// Copyright 2020 Andrew Karasyov
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

namespace csa {
namespace internal {

/**
 * A decorator for raw client that logs each operation.
 */
class LoggingClient : public RawClient
{
public:
    LoggingClient(std::shared_ptr<RawClient> client);
    ~LoggingClient() override = default;

    ClientOptions const& GetClientOptions() const override;
    std::string GetProviderName() const override;
    std::size_t GetFileChunkQuantum() const override;

    StatusOrVal<EmptyResponse> Delete(DeleteRequest const& request) override;
    StatusOrVal<ListFolderResponse> ListFolder(ListFolderRequest const& request) override;
    StatusOrVal<FolderMetadata> GetFolderMetadata(GetFolderMetadataRequest const& request) override;
    StatusOrVal<FileMetadata> GetFileMetadata(GetFileMetadataRequest const& request) override;
    StatusOrVal<std::unique_ptr<ObjectReadSource>> ReadFile(
        ReadFileRangeRequest const& request) override;

    StatusOrVal<FileMetadata> RenameFile(RenameFileRequest const& request) override;
    StatusOrVal<FileMetadata> InsertFile(InsertFileRequest const& request) override;
    StatusOrVal<std::unique_ptr<ResumableUploadSession>>
        CreateResumableSession(ResumableUploadRequest const& request) override;
    StatusOrVal<std::unique_ptr<ResumableUploadSession>>
        RestoreResumableSession(std::string const& sessionId) override;
private:
    std::shared_ptr<RawClient> m_client;
};

} // namespace internal
} // namespace csa
