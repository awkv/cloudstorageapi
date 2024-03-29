// Copyright 2019 Andrew Karasyov
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

#include "cloudstorageapi/internal/empty_response.h"
#include "cloudstorageapi/internal/file_requests.h"
#include "cloudstorageapi/internal/folder_requests.h"
#include "cloudstorageapi/internal/object_read_source.h"
#include "cloudstorageapi/internal/resumable_upload_session.h"
#include "cloudstorageapi/options.h"
#include "cloudstorageapi/status.h"
#include "cloudstorageapi/status_or_val.h"
#include "cloudstorageapi/storage_quota.h"
#include "cloudstorageapi/user_info.h"

namespace csa {
namespace internal {
/**
 * Defines the interface used to communicate with Google Cloud Storage.
 */
class RawClient
{
public:
    virtual ~RawClient() = default;

    virtual Options const& GetOptions() const = 0;
    virtual std::string GetProviderName() const = 0;
    virtual StatusOrVal<UserInfo> GetUserInfo() = 0;
    virtual std::size_t GetFileChunkQuantum() const = 0;

    //{@
    // @name Common operations
    virtual StatusOrVal<EmptyResponse> Delete(DeleteRequest const& request) = 0;
    //}@

    //{@
    // @name Folder operations
    virtual StatusOrVal<ListFolderResponse> ListFolder(ListFolderRequest const& request) = 0;
    virtual StatusOrVal<FolderMetadata> GetFolderMetadata(GetFolderMetadataRequest const& request) = 0;
    virtual StatusOrVal<FolderMetadata> CreateFolder(CreateFolderRequest const& request) = 0;
    virtual StatusOrVal<FolderMetadata> RenameFolder(RenameRequest const& request) = 0;
    virtual StatusOrVal<FolderMetadata> PatchFolderMetadata(PatchFolderMetadataRequest const& request) = 0;
    //@}

    //{@
    // @name File operations
    virtual StatusOrVal<FileMetadata> GetFileMetadata(GetFileMetadataRequest const& request) = 0;
    virtual StatusOrVal<FileMetadata> PatchFileMetadata(PatchFileMetadataRequest const& request) = 0;
    virtual StatusOrVal<FileMetadata> RenameFile(RenameRequest const& request) = 0;
    virtual StatusOrVal<FileMetadata> InsertFile(InsertFileRequest const& request) = 0;
    virtual StatusOrVal<std::unique_ptr<ObjectReadSource>> ReadFile(ReadFileRangeRequest const& request) = 0;
    virtual StatusOrVal<std::unique_ptr<ResumableUploadSession>> CreateResumableSession(
        ResumableUploadRequest const& request) = 0;
    virtual StatusOrVal<std::unique_ptr<ResumableUploadSession>> RestoreResumableSession(
        std::string const& session_id) = 0;
    virtual StatusOrVal<EmptyResponse> DeleteResumableUpload(DeleteResumableUploadRequest const& request) = 0;
    // TODO: The name is expected to be `CopyFile`. But there is a macro in windows.h with this name.
    virtual StatusOrVal<FileMetadata> CopyFileObject(CopyFileRequest const& request) = 0;
    //@}

    virtual StatusOrVal<StorageQuota> GetQuota() = 0;
};

}  // namespace internal
}  // namespace csa
