// Copyright 2021 Andrew Karasyov
//
// Copyright 2018 Google LLC
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

#include "cloudstorageapi/internal/raw_client.h"
#include "cloudstorageapi/internal/resumable_upload_session.h"
#include "cloudstorageapi/retry_policy.h"
#include <string>

namespace csa {
namespace internal {
/**
 * Decorates a `RawClient` to retry each operation.
 */
class RetryClient : public RawClient, public std::enable_shared_from_this<RetryClient>
{
public:
    explicit RetryClient(std::shared_ptr<RawClient> client, Options const& options);

    ~RetryClient() override = default;

    Options const& GetOptions() const override;
    std::string GetProviderName() const override;
    StatusOrVal<UserInfo> GetUserInfo() override;
    std::size_t GetFileChunkQuantum() const override;

    StatusOrVal<EmptyResponse> Delete(DeleteRequest const& request) override;
    StatusOrVal<ListFolderResponse> ListFolder(ListFolderRequest const& request) override;
    StatusOrVal<FolderMetadata> GetFolderMetadata(GetFolderMetadataRequest const& request) override;
    StatusOrVal<FolderMetadata> CreateFolder(CreateFolderRequest const& request);
    StatusOrVal<FolderMetadata> RenameFolder(RenameRequest const& request) override;
    StatusOrVal<FolderMetadata> PatchFolderMetadata(PatchFolderMetadataRequest const& request) override;

    StatusOrVal<FileMetadata> GetFileMetadata(GetFileMetadataRequest const& request) override;
    StatusOrVal<FileMetadata> PatchFileMetadata(PatchFileMetadataRequest const& request) override;
    StatusOrVal<FileMetadata> RenameFile(RenameRequest const& request) override;
    StatusOrVal<FileMetadata> InsertFile(InsertFileRequest const& request) override;
    /// Call ReadFile() but do not wrap the result in a RetryObjectReadSource.
    StatusOrVal<std::unique_ptr<ObjectReadSource>> ReadFileNotWrapped(ReadFileRangeRequest const&, csa::RetryPolicy&,
                                                                      csa::BackoffPolicy&);
    StatusOrVal<std::unique_ptr<ObjectReadSource>> ReadFile(ReadFileRangeRequest const& request) override;
    StatusOrVal<std::unique_ptr<ResumableUploadSession>> CreateResumableSession(
        ResumableUploadRequest const& request) override;
    StatusOrVal<std::unique_ptr<ResumableUploadSession>> RestoreResumableSession(std::string const& sessionId) override;
    StatusOrVal<EmptyResponse> DeleteResumableUpload(DeleteResumableUploadRequest const& request) override;
    StatusOrVal<FileMetadata> CopyFileObject(CopyFileRequest const& request) override;

    StatusOrVal<StorageQuota> GetQuota() override;

private:
    std::shared_ptr<RawClient> m_client;
    std::shared_ptr<csa::RetryPolicy const> m_retryPolicyPrototype;
    std::shared_ptr<csa::BackoffPolicy const> m_backoffPolicyPrototype;
};

}  // namespace internal
}  // namespace csa
