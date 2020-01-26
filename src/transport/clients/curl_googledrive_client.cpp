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

namespace csa {
namespace internal {
namespace {
    constexpr auto EndPoint = "https://www.googleapis.com/drive/v3";
    constexpr auto FilesEndPoint = "https://www.googleapis.com/drive/v3/files";
    constexpr auto FilesUploadEndPoint = "https://www.googleapis.com/upload/drive/v2/files";
    constexpr auto UserInfoEndPoint = "https://www.googleapis.com/oauth2/v1/userinfo";
    constexpr auto OAuthRoot = "https://accounts.google.com/o/oauth2";
    constexpr auto ObjectMetadataFields = "kind,id,name,mimeType,parents,capabilities/canDownload,capabilities/canAddChildren,size,modifiedTime";

    template <typename ReturnType>
    StatusOrVal<ReturnType> ParseFromHttpResponse(StatusOrVal<HttpResponse> response)
    {
        if (!response.Ok())
        {
            return std::move(response).GetStatus();
        }
        if (response->m_statusCode >= 300)
        {
            return AsStatus(*response);
        }

        return GoogleResponseParser::ParseResponse<ReturnType>(response->m_payload);
    }

} // namespace

CurlGoogleDriveClient::CurlGoogleDriveClient(ClientOptions options)
    : CurlClientBase(std::move(options))
{
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
    //TODO: extract method and wrap these checks. Do it for other functions.
    if (!response.Ok())
    {
        return std::move(response).GetStatus();
    }

    if (response->m_statusCode >= 300)
    {
        return AsStatus(*response);
    }

    return GoogleMetadataParser::ParseFolderMetadata(response->m_payload);
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
    if (!response.Ok())
    {
        return std::move(response).GetStatus();
    }
    if (response->m_statusCode >= 300)
    {
        return AsStatus(*response);
    }

    return GoogleMetadataParser::ParseFileMetadata(response->m_payload);
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
    if (!response.Ok())
    {
        return std::move(response).GetStatus();
    }
    if (response->m_statusCode >= 300)
    {
        return AsStatus(*response);
    }

    return GoogleMetadataParser::ParseFileMetadata(response->m_payload);
}

}  // namespace internal
}  // namespace csa
