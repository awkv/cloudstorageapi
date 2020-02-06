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

#include "cloudstorageapi/internal/file_requests.h"
#include "cloudstorageapi/internal/folder_requests.h"
#include "cloudstorageapi/client_options.h"
#include "cloudstorageapi/status.h"
#include "cloudstorageapi/status_or_val.h"

namespace csa {
namespace internal {
/**
 * Defines the interface used to communicate with Google Cloud Storage.
 */
class RawClient {
public:
    virtual ~RawClient() = default;

    virtual ClientOptions const& GetClientOptions() const = 0;
    virtual std::string GetProviderName() const = 0;

    //{@
    // @name Folder operations
    virtual StatusOrVal<ListFolderResponse> ListFolder(ListFolderRequest const& request) = 0;
    virtual StatusOrVal<FolderMetadata> GetFolderMetadata(GetFolderMetadataRequest const& request) = 0;
    //@}

    //{@
    // @name File operations
     virtual StatusOrVal<FileMetadata> GetFileMetadata(GetFileMetadataRequest const& request) = 0;
     virtual StatusOrVal<FileMetadata> RenameFile(RenameFileRequest const& request) = 0;
     virtual StatusOrVal<FileMetadata> InsertFile(InsertFileRequest const& request) = 0;
    //@}
};

}  // namespace internal
}  // namespace csa
