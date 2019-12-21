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

#include "cloudstorageapi/internal/curl_client_base.h"
#include "cloudstorageapi/file_metadata.h"

namespace csa {
namespace internal {
class CurlRequestBuilder;

/**
 * Implements the low-level RPCs to Cloud Storage using libcurl.
 */
class CurlGoogleDriveClient final : public CurlClientBase,
    public std::enable_shared_from_this<CurlGoogleDriveClient>
{
public:
    static std::shared_ptr<CurlGoogleDriveClient> Create(ClientOptions options)
    {
        // Cannot use std::make_shared because the constructor is private.
        return std::shared_ptr<CurlGoogleDriveClient>(new CurlGoogleDriveClient(std::move(options)));
    }

    static std::shared_ptr<CurlGoogleDriveClient> Create(std::shared_ptr<auth::Credentials> credentials)
    {
        return Create(ClientOptions(EProvider::GoogleDrive, std::move(credentials)));
    }

    CurlGoogleDriveClient(CurlGoogleDriveClient const& rhs) = delete;
    CurlGoogleDriveClient(CurlGoogleDriveClient&& rhs) = delete;
    CurlGoogleDriveClient& operator=(CurlGoogleDriveClient const& rhs) = delete;
    CurlGoogleDriveClient& operator=(CurlGoogleDriveClient&& rhs) = delete;

    std::string GetProviderName() const override { return ProviderNames.at(EProvider::GoogleDrive);  }

    StatusOrVal<ListFolderResponse> ListFolder(ListFolderRequest const& request) override;
    StatusOrVal<FolderMetadata> GetFolderMetadata(GetFolderMetadataRequest const& request) override;

    StatusOrVal<FileMetadata> GetFileMetadata(GetFileMetadataRequest const& request) override;

private:
    // The constructor is protected because the class must always be created
    // as a shared_ptr<>.
    explicit CurlGoogleDriveClient(ClientOptions options);
};

}  // namespace internal
}  // namespace csa
