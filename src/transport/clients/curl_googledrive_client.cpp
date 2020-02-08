// Copyright 2019 Andrew Karasyov
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cloudstorageapi/internal/clients/google_metadata_parser.h"
#include "cloudstorageapi/internal/clients/google_response_parser.h"
#include "cloudstorageapi/internal/clients/curl_googledrive_client.h"
#include "cloudstorageapi/internal/curl_request_builder.h"
#include "cloudstorageapi/internal/utils.h"

#include <functional>
#include <sstream>

namespace csa {
namespace internal {
namespace {
    constexpr auto EndPoint = "https://www.googleapis.com/drive/v3";
    constexpr auto FilesEndPoint = "https://www.googleapis.com/drive/v3/files";
    constexpr auto FilesUploadEndPoint = "https://www.googleapis.com/upload/drive/v3/files";
    constexpr auto UserInfoEndPoint = "https://www.googleapis.com/oauth2/v1/userinfo";
    constexpr auto OAuthRoot = "https://accounts.google.com/o/oauth2";
    constexpr auto ObjectMetadataFields = "kind,id,name,mimeType,parents,capabilities/canDownload,capabilities/canAddChildren,size,modifiedTime";

    template <typename ParseFunctor>
    auto CheckAndParse(ParseFunctor f, StatusOrVal<HttpResponse> response)
        -> decltype(f(response->m_payload))
    {
        if (!response.Ok())
        {
            return std::move(response).GetStatus();
        }
        if (response->m_statusCode >= 300)
        {
            return AsStatus(*response);
        }

        return f(response->m_payload);
    }

    template <typename ReturnType>
    StatusOrVal<ReturnType> ParseFromHttpResponse(StatusOrVal<HttpResponse> response)
    {
        using ParseResponseFunc = StatusOrVal<ReturnType>(*)(std::string const&);
        return CheckAndParse(std::bind(static_cast<ParseResponseFunc>(&GoogleResponseParser::ParseResponse<ReturnType>),
            std::placeholders::_1), response);
    }

    StatusOrVal<FileMetadata> ParseFileMetadata(StatusOrVal<HttpResponse> response)
    {
        using ParseFileMetadataFunc = StatusOrVal<FileMetadata>(*)(std::string const&);
        return CheckAndParse(std::bind(static_cast<ParseFileMetadataFunc>(&GoogleMetadataParser::ParseFileMetadata),
            std::placeholders::_1), response);
    }

    StatusOrVal<FolderMetadata> ParseFolderMetadata(StatusOrVal<HttpResponse> response)
    {
        using ParseFolderMetadataFunc = StatusOrVal<FolderMetadata>(*)(std::string const&);
        return CheckAndParse(std::bind(static_cast<ParseFolderMetadataFunc>(&GoogleMetadataParser::ParseFolderMetadata),
            std::placeholders::_1), response);
    }

    StatusOrVal<EmptyResponse> ReturnEmptyResponse(StatusOrVal<HttpResponse> response)
    {
        auto emptyResponseReturner = [](std::string const&) { return StatusOrVal(EmptyResponse{}); };
        return CheckAndParse(emptyResponseReturner, response);
    }
} // namespace

CurlGoogleDriveClient::CurlGoogleDriveClient(ClientOptions options)
    : CurlClientBase(std::move(options))
{
}

StatusOrVal<EmptyResponse> CurlGoogleDriveClient::Delete(DeleteRequest const& request)
{
    // Assume file name is verified by caller.
    CurlRequestBuilder builder(std::string(FilesEndPoint) + "/" + request.GetObjectId(), m_storageFactory);
    auto status = SetupBuilder(builder, request, "DELETE");
    if (!status.Ok())
    {
        return status;
    }
    return ReturnEmptyResponse(builder.BuildRequest().MakeRequest(std::string{}));
}

StatusOrVal<ListFolderResponse> CurlGoogleDriveClient::ListFolder(ListFolderRequest const& request)
{
    CurlRequestBuilder builder(FilesEndPoint, m_storageFactory);
    auto status = SetupBuilder(builder, request, "GET");
    if (!status.Ok())
    {
        return status;
    }

    std::string qVal = "('" + request.GetObjectId() + "' in parents) and trashed=false";
    builder.AddQueryParameter("q", qVal);
    builder.AddQueryParameter("fields", "kind,nextPageToken,incompleteSearch,"
        "files(" + std::string(ObjectMetadataFields) + ")");
    std::string pageToken = request.GetPageToken();
    if (!pageToken.empty())
    {
        builder.AddQueryParameter("pageToken", std::move(pageToken));
    }

    return ParseFromHttpResponse<ListFolderResponse>(
        builder.BuildRequest().MakeRequest(std::string{}));
}

StatusOrVal<FolderMetadata> CurlGoogleDriveClient::GetFolderMetadata(GetFolderMetadataRequest const& request)
{
    CurlRequestBuilder builder(std::string(FilesEndPoint) + "/" + request.GetObjectId(), m_storageFactory);
    auto status = SetupBuilder(builder, request, "GET");
    if (!status.Ok())
    {
        return status;
    }
    builder.AddQueryParameter("fields", ObjectMetadataFields);

    auto response = builder.BuildRequest().MakeRequest(std::string{});
    return ParseFolderMetadata(response);
}

StatusOrVal<FileMetadata> CurlGoogleDriveClient::GetFileMetadata(GetFileMetadataRequest const& request)
{
    CurlRequestBuilder builder(std::string(FilesEndPoint) + "/" + request.GetObjectId(), m_storageFactory);
    auto status = SetupBuilder(builder, request, "GET");
    if (!status.Ok())
    {
        return status;
    }

    builder.AddQueryParameter("fields", ObjectMetadataFields);
    auto response = builder.BuildRequest().MakeRequest(std::string{});
    return ParseFileMetadata(response);
}

StatusOrVal<FileMetadata> CurlGoogleDriveClient::RenameFile(RenameFileRequest const& request)
{
    CurlRequestBuilder builder(std::string(FilesEndPoint) + "/" + request.GetObjectId(), m_storageFactory);
    auto status = SetupBuilder(builder, request, "PATCH");
    if (!status.Ok())
    {
        return status;
    }

    auto& parentId = request.GetParentId();
    auto& newParentId = request.GetNewParentId();
    if (!parentId.empty() && !newParentId.empty())
    {
        builder.AddQueryParameter("addParents", request.GetNewParentId());
        builder.AddQueryParameter("removeParents", request.GetParentId());
    }
    builder.AddQueryParameter("fields", ObjectMetadataFields);
    builder.AddHeader("Content-Type: application/json");
    auto response = builder.BuildRequest().MakeRequest("{\"name\":\""+request.GetNewName()+"\"}");
    return ParseFileMetadata(response);
}

StatusOrVal<FileMetadata> CurlGoogleDriveClient::InsertFile(InsertFileRequest const& request)
{
    // If the object metadata is specified, then we need to do a multipart upload.
    if (request.HasOption<WithFileMetadata>() || !request.GetName().empty() || !request.GetFolderId().empty())
        return InsertFileMultipart(request);
    else   // Otherwise do a simple upload.
        return InsertFileSimple(request);
}


std::string CurlGoogleDriveClient::PickBoundary(std::string const& textToAvoid)
{
    // We need to find a string that is *not* found in `text_to_avoid`, we pick
    // a string at random, and see if it is in `text_to_avoid`, if it is, we grow
    // the string with random characters and start from where we last found
    // the candidate.  Eventually we will find something, though it might be
    // larger than `text_to_avoid`.  And we only make (approximately) one pass
    // over `text_to_avoid`.
    auto generateCandidate = [this](int n) {
        static std::string const charArray =
            "abcdefghijklmnopqrstuvwxyz012456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
        std::unique_lock<std::mutex> lk(m_muRNG);
        return csa::internal::Sample(m_generator, n, charArray);
    };
    constexpr int candidateInitialSize = 16;
    constexpr int candidateGrowthSize = 4;
    return GenerateMessageBoundary(textToAvoid, std::move(generateCandidate),
        candidateInitialSize, candidateGrowthSize);
}

StatusOrVal<FileMetadata> CurlGoogleDriveClient::InsertFileSimple(InsertFileRequest const& request)
{
    CurlRequestBuilder builder(std::string(FilesUploadEndPoint) + "/" + request.GetObjectId(), m_uploadFactory);
    auto status = SetupBuilder(builder, request, "POST");
    if (!status.Ok())
    {
        return status;
    }

    // Set the content type of a sensible value, the application can override this
    // in the options for the request.
    if (!request.HasOption<ContentType>()) {
        builder.AddHeader("content-type: application/octet-stream");
    }

    builder.AddQueryParameter("uploadType", "media");
    builder.AddHeader("Content-Length: " +
        std::to_string(request.GetContent().size()));
    builder.AddQueryParameter("fields", ObjectMetadataFields);
    auto response = builder.BuildRequest().MakeRequest(request.GetContent());

    return ParseFileMetadata(response);
}

StatusOrVal<FileMetadata> CurlGoogleDriveClient::InsertFileMultipart(InsertFileRequest const& request)
{
    // 1. Create a request object.
    CurlRequestBuilder builder(std::string(FilesUploadEndPoint) + "/" + request.GetObjectId(), m_uploadFactory);
    auto status = SetupBuilder(builder, request, "POST");
    if (!status.Ok())
    {
        return status;
    }

    // 2. Pick a separator that does not conflict with the request contents.
    auto boundary = PickBoundary(request.GetContent());
    builder.AddHeader("content-type: multipart/related; boundary=" + boundary);
    builder.AddQueryParameter("uploadType", "multipart");

    // 3. Perform a streaming upload because computing the size upfront is more
    //    complicated than it is worth.
    std::ostringstream writer;

    nl::json jmeta;
    if (request.HasOption<WithFileMetadata>())
    {
        auto meta = GoogleMetadataParser::ComposeFileMetadata(request.GetOption<WithFileMetadata>().Value());
        if (!meta.Ok())
            return meta.GetStatus();
        jmeta = meta.Value();
    }
    
    if (!request.GetName().empty())
        jmeta["name"] = request.GetName();

    if (!request.GetFolderId().empty())
        jmeta["parents"].emplace_back(request.GetFolderId());

    const std::string crlf = "\r\n";
    const std::string marker = "--" + boundary;

    // 4. Format the first part, including the separators and the headers.
    writer << marker << crlf << "content-type: application/json; charset=UTF-8"
        << crlf << crlf << jmeta.dump() << crlf << marker << crlf;

    // 5. Format the second part, which includes all the contents and a final
    //    separator.
    if (request.HasOption<ContentType>())
    {
        writer << "content-type: " << request.GetOption<ContentType>().value()
            << crlf;
    }
    else
    {
        writer << "content-type: application/octet-stream" << crlf;
    }
    writer << crlf << request.GetContent() << crlf << marker << "--" << crlf;

    // 6. Return the results as usual.
    auto contents = std::move(writer).str();
    builder.AddHeader("Content-Length: " + std::to_string(contents.size()));
    builder.AddQueryParameter("fields", ObjectMetadataFields);
    auto response = builder.BuildRequest().MakeRequest(contents);
    return ParseFileMetadata(response);
}

}  // namespace internal
}  // namespace csa
