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

#pragma once

#include "cloudstorageapi/file_metadata.h"
#include "cloudstorageapi/internal/curl_client_base.h"
#include <nlohmann/json.hpp>

namespace csa {
namespace internal {
class CurlRequestBuilder;
class UploadChunkRequest;
class QueryResumableUploadRequest;
struct ResumableUploadResponse;
class ObjectReadSource;

/**
 * Implements the low-level RPCs to Cloud Storage using libcurl.
 */
class CurlGoogleDriveClient final : public CurlClientBase, public std::enable_shared_from_this<CurlGoogleDriveClient>
{
public:
    static std::shared_ptr<CurlGoogleDriveClient> Create(Options options)
    {
        // Cannot use std::make_shared because the constructor is private.
        return std::shared_ptr<CurlGoogleDriveClient>(new CurlGoogleDriveClient(std::move(options)));
    }

    static std::shared_ptr<CurlGoogleDriveClient> Create(std::shared_ptr<auth::Credentials> credentials)
    {
        return Create(
            Options{}.Set<ProviderOption>(EProvider::GoogleDrive).Set<Oauth2CredentialsOption>(std::move(credentials)));
    }

    CurlGoogleDriveClient(CurlGoogleDriveClient const& rhs) = delete;
    CurlGoogleDriveClient(CurlGoogleDriveClient&& rhs) = delete;
    CurlGoogleDriveClient& operator=(CurlGoogleDriveClient const& rhs) = delete;
    CurlGoogleDriveClient& operator=(CurlGoogleDriveClient&& rhs) = delete;

    std::string GetProviderName() const override { return ProviderNames.at(EProvider::GoogleDrive); }

    StatusOrVal<UserInfo> GetUserInfo() override;

    StatusOrVal<ResumableUploadResponse> UploadChunk(UploadChunkRequest const&) override;
    StatusOrVal<ResumableUploadResponse> QueryResumableUpload(QueryResumableUploadRequest const&) override;
    std::size_t GetFileChunkQuantum() const override;

    StatusOrVal<EmptyResponse> Delete(DeleteRequest const& request) override;

    StatusOrVal<ListFolderResponse> ListFolder(ListFolderRequest const& request) override;
    StatusOrVal<FolderMetadata> GetFolderMetadata(GetFolderMetadataRequest const& request) override;
    StatusOrVal<FolderMetadata> CreateFolder(CreateFolderRequest const& request) override;
    StatusOrVal<FolderMetadata> RenameFolder(RenameRequest const& request) override;
    StatusOrVal<FolderMetadata> PatchFolderMetadata(PatchFolderMetadataRequest const& request) override;

    StatusOrVal<FileMetadata> GetFileMetadata(GetFileMetadataRequest const& request) override;
    StatusOrVal<FileMetadata> PatchFileMetadata(PatchFileMetadataRequest const& request) override;
    StatusOrVal<FileMetadata> RenameFile(RenameRequest const& request) override;
    StatusOrVal<FileMetadata> InsertFile(InsertFileRequest const& request) override;
    StatusOrVal<std::unique_ptr<ObjectReadSource>> ReadFile(ReadFileRangeRequest const& request) override;
    StatusOrVal<std::unique_ptr<ResumableUploadSession>> CreateResumableSession(
        ResumableUploadRequest const& request) override;
    StatusOrVal<std::unique_ptr<ResumableUploadSession>> RestoreResumableSession(
        std::string const& session_id) override;
    StatusOrVal<EmptyResponse> DeleteResumableUpload(DeleteResumableUploadRequest const& request) override;
    StatusOrVal<FileMetadata> CopyFileObject(CopyFileRequest const& request) override;

    StatusOrVal<StorageQuota> GetQuota() override;

private:
    // The constructor is protected because the class must always be created
    // as a shared_ptr<>.
    explicit CurlGoogleDriveClient(Options options);

    std::string PickBoundary(std::string const& textToAvoid);
    StatusOrVal<FileMetadata> InsertFileSimple(InsertFileRequest const& request);
    StatusOrVal<FileMetadata> InsertFileMultipart(InsertFileRequest const& request);

    template <typename RequestType>
    StatusOrVal<std::unique_ptr<ResumableUploadSession>> CreateResumableSessionGeneric(RequestType const& request);

    StatusOrVal<HttpResponse> RenameGeneric(RenameRequest const& request);
    template <typename RequestType>
    StatusOrVal<HttpResponse> PatchMetadataGeneric(RequestType const& request, nlohmann::json const& patch);
};

}  // namespace internal
}  // namespace csa
