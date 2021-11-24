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

#include "cloudstorageapi/internal/clients/curl_googledrive_client.h"
#include "cloudstorageapi/internal/clients/google_metadata_parser.h"
#include "cloudstorageapi/internal/clients/google_response_parser.h"
#include "cloudstorageapi/internal/clients/google_utils.h"
#include "cloudstorageapi/internal/curl_request_builder.h"
#include "cloudstorageapi/internal/curl_resumable_upload_session.h"
#include "cloudstorageapi/internal/object_read_source.h"
#include "cloudstorageapi/internal/resumable_upload_session.h"
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
constexpr auto AboutEndPoint = "https://www.googleapis.com/drive/v3/about";
constexpr auto ObjectMetadataFields =
    "kind,id,name,mimeType,parents,capabilities/canDownload,capabilities/canAddChildren,size,modifiedTime";
constexpr auto QuotaFields = "storageQuota/limit, storageQuota/usageInDrive";
constexpr auto UserFields = "user/displayName, user/emailAddress";

// Chunks must be multiples of 256 KiB:
// https://developers.google.com/drive/api/v3/manage-uploads#resumable
constexpr std::size_t ChunkSizeQuantum = 256 * 1024UL;

template <typename ParseFunctor>
auto CheckAndParse(ParseFunctor f, StatusOrVal<HttpResponse> response) -> decltype(f(response->m_payload))
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
    using ParseResponseFunc = StatusOrVal<ReturnType> (*)(std::string const&);
    return CheckAndParse(std::bind(static_cast<ParseResponseFunc>(&GoogleResponseParser::ParseResponse<ReturnType>),
                                   std::placeholders::_1),
                         response);
}

StatusOrVal<FileMetadata> ParseFileMetadata(StatusOrVal<HttpResponse> response)
{
    using ParseFileMetadataFunc = StatusOrVal<FileMetadata> (*)(std::string const&);
    return CheckAndParse(
        std::bind(static_cast<ParseFileMetadataFunc>(&GoogleMetadataParser::ParseFileMetadata), std::placeholders::_1),
        response);
}

StatusOrVal<FolderMetadata> ParseFolderMetadata(StatusOrVal<HttpResponse> response)
{
    using ParseFolderMetadataFunc = StatusOrVal<FolderMetadata> (*)(std::string const&);
    return CheckAndParse(std::bind(static_cast<ParseFolderMetadataFunc>(&GoogleMetadataParser::ParseFolderMetadata),
                                   std::placeholders::_1),
                         response);
}

StatusOrVal<StorageQuota> ParseQuota(StatusOrVal<HttpResponse> response)
{
    auto ParseQuotaFunc = [](std::string const& jsonStr) -> StatusOrVal<StorageQuota> {
        auto json = nlohmann::json::parse(jsonStr, nullptr, false);
        if (json.is_discarded() || !json.is_object())
        {
            return Status(StatusCode::InvalidArgument,
                          "Invalid About drive object. "
                          "Failed to parse json.");
        }

        constexpr auto quotaKey = "storageQuota";
        constexpr auto limitKey = "limit";
        constexpr auto usageKey = "usageInDrive";
        if (json.contains(quotaKey))
        {
            auto& jsonQuota = json[quotaKey];
            if (jsonQuota.count(usageKey))
            {
                // `limit` key may be absent. It means user has unlimited storage.
                std::int64_t total = (std::numeric_limits<std::int64_t>::max)();
                if (jsonQuota.count(limitKey))
                {
                    auto totalTmp = JsonUtils::ParseLong(jsonQuota, limitKey);
                    if (!totalTmp)
                        return std::move(totalTmp).GetStatus();
                    total = *totalTmp;
                }

                auto usage = JsonUtils::ParseLong(jsonQuota, usageKey);
                if (!usage)
                    return std::move(usage).GetStatus();

                return StorageQuota{total, *usage};
            }
        }
        return Status(StatusCode::InvalidArgument, "Correct storageQuota not found in About reponse.");
    };

    return CheckAndParse(std::bind(ParseQuotaFunc, std::placeholders::_1), response);
}

StatusOrVal<UserInfo> ParseUser(StatusOrVal<HttpResponse> response)
{
    auto ParseUserFunc = [](std::string const& jsonStr) -> StatusOrVal<UserInfo> {
        auto json = nlohmann::json::parse(jsonStr, nullptr, false);
        if (json.is_discarded() || !json.is_object())
        {
            return Status(StatusCode::InvalidArgument,
                          "Invalid About drive object. "
                          "Failed to parse json.");
        }

        constexpr auto userKey = "user";
        constexpr auto nameKey = "displayName";
        constexpr auto emailKey = "emailAddress";

        if (json.contains(userKey))
        {
            auto& jsonUser = json[userKey];
            if (jsonUser.count(nameKey))
            {
                const std::string name = jsonUser[nameKey];
                // This may not be present in certain contexts
                // if the user has not made their email address visible to the requester.
                const std::string email = jsonUser.count(emailKey) ? jsonUser[emailKey] : "";

                return UserInfo{email, name};
            }
        }
        return Status(StatusCode::InvalidArgument, "Correct User object not found in About response.");
    };

    return CheckAndParse(std::bind(ParseUserFunc, std::placeholders::_1), response);
}

StatusOrVal<EmptyResponse> ReturnEmptyResponse(StatusOrVal<HttpResponse> response)
{
    auto emptyResponseReturner = [](std::string const&) { return StatusOrVal(EmptyResponse{}); };
    return CheckAndParse(emptyResponseReturner, response);
}

FolderMetadata CreateDefaultFolderMetadata(std::string const& parent, std::string const& name)
{
    auto now = std::chrono::system_clock::now();
    FolderMetadata fmeta;
    fmeta.SetModifyTime(now);
    fmeta.SetAccessTime(now);
    fmeta.SetChangeTime(now);
    fmeta.SetParentId(parent);
    fmeta.SetName(name);
    return fmeta;
}
}  // namespace

CurlGoogleDriveClient::CurlGoogleDriveClient(Options options) : CurlClientBase(std::move(options)) {}

StatusOrVal<UserInfo> CurlGoogleDriveClient::GetUserInfo()
{
    CurlRequestBuilder builder(AboutEndPoint, m_storageFactory);
    auto status = SetupBuilderCommon(builder, "GET");
    if (!status.Ok())
    {
        return status;
    }
    builder.AddQueryParameter("fields", UserFields);
    return ParseUser(builder.BuildRequest().MakeRequest(std::string{}));
}

StatusOrVal<ResumableUploadResponse> CurlGoogleDriveClient::UploadChunk(UploadChunkRequest const& request)
{
    CurlRequestBuilder builder(request.GetUploadSessionUrl(), m_uploadFactory);
    auto status = SetupBuilder(builder, request, "PUT");
    if (!status.Ok())
    {
        return status;
    }
    builder.AddHeader(GoogleUtils::GetRangeHeader(request));
    builder.AddHeader("Content-Type: application/octet-stream");
    builder.AddHeader("Content-Length: " + std::to_string(request.GetPayloadSize()));
    // We need to explicitly disable chunked transfer encoding. libcurl uses is by
    // default (at least in this case), and that wastes bandwidth as the content
    // length is known.
    builder.AddHeader("Transfer-Encoding:");
    auto response = builder.BuildRequest().MakeUploadRequest(request.GetPayload());
    if (!response.Ok())
    {
        return std::move(response).GetStatus();
    }
    if (response->m_statusCode < HttpStatusCode::MinNotSuccess ||
        response->m_statusCode == HttpStatusCode::ResumeIncomplete)
    {
        return GoogleResponseParser::ParseResponse<ResumableUploadResponse>(*std::move(response));
    }
    return AsStatus(*response);
}

StatusOrVal<ResumableUploadResponse> CurlGoogleDriveClient::QueryResumableUpload(
    QueryResumableUploadRequest const& request)
{
    CurlRequestBuilder builder(request.GetUploadSessionUrl(), m_uploadFactory);
    auto status = SetupBuilder(builder, request, "PUT");
    if (!status.Ok())
    {
        return status;
    }
    builder.AddHeader("Content-Range: bytes */*");
    builder.AddHeader("Content-Type: application/octet-stream");
    builder.AddHeader("Content-Length: 0");
    auto response = builder.BuildRequest().MakeRequest(std::string{});
    if (!response.Ok())
    {
        return std::move(response).GetStatus();
    }
    if (response->m_statusCode < HttpStatusCode::MinNotSuccess ||
        response->m_statusCode == HttpStatusCode::ResumeIncomplete)
    {
        return GoogleResponseParser::ParseResponse<ResumableUploadResponse>(*std::move(response));
    }
    return AsStatus(*response);
}

std::size_t CurlGoogleDriveClient::GetFileChunkQuantum() const { return ChunkSizeQuantum; }

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
    builder.AddQueryParameter("fields",
                              "kind,nextPageToken,incompleteSearch,"
                              "files(" +
                                  std::string(ObjectMetadataFields) + ")");
    std::string pageToken = request.GetPageToken();
    if (!pageToken.empty())
    {
        builder.AddQueryParameter("pageToken", std::move(pageToken));
    }

    return ParseFromHttpResponse<ListFolderResponse>(builder.BuildRequest().MakeRequest(std::string{}));
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

StatusOrVal<FolderMetadata> CurlGoogleDriveClient::CreateFolder(CreateFolderRequest const& request)
{
    CurlRequestBuilder builder(FilesEndPoint, m_uploadFactory);
    auto status = SetupBuilder(builder, request, "POST");
    if (!status.Ok())
    {
        return status;
    }

    FolderMetadata fmeta;
    if (request.HasOption<WithFolderMetadata>())
    {
        fmeta = request.GetOption<WithFolderMetadata>().Value();
        const auto& name = request.GetName();
        if (!name.empty())
            fmeta.SetName(name);
        const auto& parentId = request.GetParent();
        if (!parentId.empty())
            fmeta.SetParentId(parentId);
    }
    else
    {
        fmeta = CreateDefaultFolderMetadata(request.GetParent(), request.GetName());
    }

    auto jmeta = GoogleMetadataParser::ComposeFolderMetadata(fmeta);
    if (!jmeta.Ok())
        return jmeta.GetStatus();

    builder.AddHeader("content-type: application/json");
    builder.AddQueryParameter("fields", ObjectMetadataFields);
    auto response = builder.BuildRequest().MakeRequest(jmeta.Value().dump());

    return ParseFolderMetadata(response);
}

StatusOrVal<FolderMetadata> CurlGoogleDriveClient::RenameFolder(RenameRequest const& request)
{
    return ParseFolderMetadata(RenameGeneric(request));
}

StatusOrVal<FolderMetadata> CurlGoogleDriveClient::PatchFolderMetadata(PatchFolderMetadataRequest const& request)
{
    auto metaJson =
        GoogleMetadataParser::PatchFolderMetadata(request.GetOriginalMetadata(), request.GetUpdatedMetadata());
    if (!metaJson.Ok())
        return metaJson.GetStatus();
    return ParseFolderMetadata(PatchMetadataGeneric(request, metaJson.Value()));
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

StatusOrVal<FileMetadata> CurlGoogleDriveClient::PatchFileMetadata(PatchFileMetadataRequest const& request)
{
    auto metaJson =
        GoogleMetadataParser::PatchFileMetadata(request.GetOriginalMetadata(), request.GetUpdatedMetadata());
    if (!metaJson.Ok())
        return metaJson.GetStatus();
    return ParseFileMetadata(PatchMetadataGeneric(request, metaJson.Value()));
}

StatusOrVal<FileMetadata> CurlGoogleDriveClient::RenameFile(RenameRequest const& request)
{
    return ParseFileMetadata(RenameGeneric(request));
}

StatusOrVal<FileMetadata> CurlGoogleDriveClient::InsertFile(InsertFileRequest const& request)
{
    // If the object metadata is specified, then we need to do a multipart upload.
    if (request.HasOption<WithFileMetadata>() || !request.GetName().empty() || !request.GetFolderId().empty())
        return InsertFileMultipart(request);
    else  // Otherwise do a simple upload.
        return InsertFileSimple(request);
}

StatusOrVal<std::unique_ptr<ObjectReadSource>> CurlGoogleDriveClient::ReadFile(ReadFileRangeRequest const& request)
{
    CurlRequestBuilder builder(std::string(FilesEndPoint) + "/" + request.GetObjectId(), m_storageFactory);
    auto status = SetupBuilder(builder, request, "GET");
    if (!status.Ok())
    {
        return status;
    }
    builder.AddQueryParameter("alt", "media");
    if (request.RequiresRangeHeader())
    {
        builder.AddHeader(GoogleUtils::GetRangeHeader(request));
    }
    if (request.RequiresNoCache())
    {
        builder.AddHeader("Cache-Control: no-transform");
    }

    return std::unique_ptr<ObjectReadSource>(std::move(builder).BuildDownloadRequest());
}

StatusOrVal<std::unique_ptr<ResumableUploadSession>> CurlGoogleDriveClient::CreateResumableSession(
    ResumableUploadRequest const& request)
{
    return CreateResumableSessionGeneric(request);
}

StatusOrVal<std::unique_ptr<ResumableUploadSession>> CurlGoogleDriveClient::RestoreResumableSession(
    std::string const& sessionId)
{
    auto session = std::make_unique<CurlResumableUploadSession>(shared_from_this(), sessionId);
    auto response = session->ResetSession();
    if (response.GetStatus().Ok())
    {
        return std::unique_ptr<ResumableUploadSession>(std::move(session));
    }
    return std::move(response).GetStatus();
}

StatusOrVal<EmptyResponse> CurlGoogleDriveClient::DeleteResumableUpload(DeleteResumableUploadRequest const& request)
{
    CurlRequestBuilder builder(request.GetUploadSessionUrl(), m_uploadFactory);
    auto status = SetupBuilderCommon(builder, "DELETE");
    if (!status.Ok())
    {
        return status;
    }
    auto response = builder.BuildRequest().MakeRequest(std::string{});
    if (!response.Ok())
    {
        return std::move(response).GetStatus();
    }
    if (response->m_statusCode >= HttpStatusCode::MinNotSuccess && response->m_statusCode != 499)
    {
        return AsStatus(*response);
    }
    return EmptyResponse{};
}

StatusOrVal<FileMetadata> CurlGoogleDriveClient::CopyFileObject(CopyFileRequest const& request)
{
    CurlRequestBuilder builder(std::string(FilesEndPoint) + "/" + request.GetObjectId() + "/copy", m_storageFactory);
    auto status = SetupBuilder(builder, request, "POST");
    if (!status.Ok())
    {
        return status;
    }

    FileMetadata fmeta;
    if (request.HasOption<WithFileMetadata>())
    {
        fmeta = request.GetOption<WithFileMetadata>().Value();
    }
    if (!request.GetDestinationFileName().empty())
        fmeta.SetName(request.GetDestinationFileName());
    if (!request.GetDestinationParentId().empty())
        fmeta.SetParentId(request.GetDestinationParentId());
    auto jmeta = GoogleMetadataParser::ComposeFileMetadata(fmeta);
    if (!jmeta.Ok())
        return jmeta.GetStatus();

    builder.AddQueryParameter("fields", ObjectMetadataFields);
    builder.AddHeader("Content-Type: application/json");
    auto response = builder.BuildRequest().MakeRequest(jmeta.Value().dump());
    return ParseFileMetadata(response);
}

StatusOrVal<StorageQuota> CurlGoogleDriveClient::GetQuota()
{
    CurlRequestBuilder builder(AboutEndPoint, m_storageFactory);
    auto status = SetupBuilderCommon(builder, "GET");
    if (!status.Ok())
    {
        return status;
    }
    builder.AddQueryParameter("fields", QuotaFields);
    return ParseQuota(builder.BuildRequest().MakeRequest(std::string{}));
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
        static std::string const charArray = "abcdefghijklmnopqrstuvwxyz012456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
        std::unique_lock<std::mutex> lk(m_muRNG);
        return csa::internal::Sample(m_generator, n, charArray);
    };
    constexpr int candidateInitialSize = 16;
    constexpr int candidateGrowthSize = 4;
    return GenerateMessageBoundary(textToAvoid, std::move(generateCandidate), candidateInitialSize,
                                   candidateGrowthSize);
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
    if (!request.HasOption<ContentType>())
    {
        builder.AddHeader("content-type: application/octet-stream");
    }

    builder.AddQueryParameter("uploadType", "media");
    builder.AddHeader("Content-Length: " + std::to_string(request.GetContent().size()));
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

    nlohmann::json jmeta;
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
    writer << marker << crlf << "content-type: application/json; charset=UTF-8" << crlf << crlf << jmeta.dump() << crlf
           << marker << crlf;

    // 5. Format the second part, which includes all the contents and a final
    //    separator.
    if (request.HasOption<ContentType>())
    {
        writer << "content-type: " << request.GetOption<ContentType>().value() << crlf;
    }
    else if (jmeta.count("contentType") != 0)
    {
        writer << "content-type: " << jmeta.value("contentType", "application/octet-stream") << crlf;
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

template <typename RequestType>
StatusOrVal<std::unique_ptr<ResumableUploadSession>> CurlGoogleDriveClient::CreateResumableSessionGeneric(
    RequestType const& request)
{
    if (request.template HasOption<UseResumableUploadSession>())
    {
        auto sessionId = request.template GetOption<UseResumableUploadSession>().Value();
        if (!sessionId.empty())
        {
            return RestoreResumableSession(sessionId);
        }
    }

    CurlRequestBuilder builder(FilesUploadEndPoint, m_uploadFactory);
    auto status = SetupBuilderCommon(builder, "POST");
    if (!status.Ok())
    {
        return status;
    }

    // In most cases we use `SetupBuilder()` to setup all these options in the
    // request. But in this case we cannot because that might also set
    // `Content-Type` to the wrong value. Instead we have to explicitly list all
    // the options here. Somebody could write a clever meta-function to say
    // "set all the options except `ContentType`, but I think that is going to be
    // very hard to understand.
    builder.AddOption(request.template GetOption<CustomHeader>());
    builder.AddOption(request.template GetOption<Fields>());
    builder.AddOption(request.template GetOption<IfMatchEtag>());
    builder.AddOption(request.template GetOption<IfNoneMatchEtag>());
    builder.AddOption(request.template GetOption<UploadContentLength>());

    builder.AddQueryParameter("uploadType", "resumable");
    builder.AddHeader("Content-Type: application/json; charset=UTF-8");
    nlohmann::json resource;
    // Set meta information
    {
        FileMetadata fmeta;
        if (request.template HasOption<WithFileMetadata>())
        {
            fmeta = request.template GetOption<WithFileMetadata>().Value();
        }

        if (fmeta.GetName().empty())
        {
            fmeta.SetName(request.GetFileName());
        }
        if (fmeta.GetParentId().empty())
        {
            fmeta.SetParentId(request.GetFolderId());
        }
        resource = GoogleMetadataParser::ComposeFileMetadata(fmeta).Value();
    }

    if (request.template HasOption<ContentEncoding>())
    {
        resource["contentEncoding"] = request.template GetOption<ContentEncoding>().value();
    }
    if (request.template HasOption<ContentType>())
    {
        resource["contentType"] = request.template GetOption<ContentType>().value();
    }

    std::string requestPayload;
    if (!resource.empty())
    {
        requestPayload = resource.dump();
    }
    builder.AddHeader("Content-Length: " + std::to_string(requestPayload.size()));
    builder.AddQueryParameter("fields", ObjectMetadataFields);

    auto httpResponse = builder.BuildRequest().MakeRequest(requestPayload);
    if (!httpResponse.Ok())
    {
        return std::move(httpResponse).GetStatus();
    }
    if (httpResponse->m_statusCode >= HttpStatusCode::MinNotSuccess)
    {
        return AsStatus(*httpResponse);
    }
    auto response = GoogleResponseParser::ParseResponse<ResumableUploadResponse>(*std::move(httpResponse));
    if (!response.Ok())
    {
        return std::move(response).GetStatus();
    }
    if (response->m_uploadSessionUrl.empty())
    {
        std::ostringstream os;
        os << __func__ << " - invalid server response, parsed to " << *response;
        return Status(StatusCode::Internal, std::move(os).str());
    }

    return std::unique_ptr<ResumableUploadSession>(
        std::make_unique<CurlResumableUploadSession>(shared_from_this(), std::move(response->m_uploadSessionUrl),
                                                     std::move(request.template GetOption<CustomHeader>())));
}

StatusOrVal<HttpResponse> CurlGoogleDriveClient::RenameGeneric(RenameRequest const& request)
{
    CurlRequestBuilder builder(std::string(FilesEndPoint) + "/" + request.GetObjectId(), m_storageFactory);
    auto status = SetupBuilder(builder, request, "PATCH");
    if (!status.Ok())
    {
        return status;
    }

    const auto& parentId = request.GetParentId();
    const auto& newParentId = request.GetNewParentId();
    if (!parentId.empty() && !newParentId.empty())
    {
        builder.AddQueryParameter("addParents", request.GetNewParentId());
        builder.AddQueryParameter("removeParents", request.GetParentId());
    }
    builder.AddQueryParameter("fields", ObjectMetadataFields);
    builder.AddHeader("Content-Type: application/json");
    return builder.BuildRequest().MakeRequest("{\"name\":\"" + request.GetNewName() + "\"}");
}

template <typename RequestType>
StatusOrVal<HttpResponse> CurlGoogleDriveClient::PatchMetadataGeneric(RequestType const& request,
                                                                      nlohmann::json const& patch)
{
    CurlRequestBuilder builder(std::string(FilesEndPoint) + "/" + request.GetObjectId(), m_storageFactory);
    auto status = SetupBuilder(builder, request, "PATCH");
    if (!status.Ok())
    {
        return status;
    }
    builder.AddHeader("Content-Type: application/json");
    builder.AddQueryParameter("fields", ObjectMetadataFields);
    return builder.BuildRequest().MakeRequest(patch.dump());
}

}  // namespace internal
}  // namespace csa
