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
#include "cloudstorageapi/folder_metadata.h"
#include "cloudstorageapi/internal/generic_object_request.h"
#include "cloudstorageapi/internal/http_response.h"
#include "cloudstorageapi/well_known_parameters.h"
#include <variant>
#include <iosfwd>

namespace csa {
namespace internal {

/**
 * Requests the list of folders.
 */
class ListFolderRequest
    : public GenericObjectRequest<ListFolderRequest, PageSize>
{
public:
    using GenericObjectRequest::GenericObjectRequest;

    std::string const& GetPageToken() const { return m_pageToken; }
    ListFolderRequest& SetPageToken(std::string page_token)
    {
        m_pageToken = std::move(page_token);
        return *this;
    }

private:
    std::string m_pageToken;
};

std::ostream& operator<<(std::ostream& os, ListFolderRequest const& r);

struct ListFolderResponse
{
    using MetadataItem = std::variant<FolderMetadata, FileMetadata>;

    std::string m_nextPageToken;
    std::vector<MetadataItem> m_items;
};

std::ostream& operator<<(std::ostream& os, ListFolderResponse const& r);

/**
 * Requests the metadata for a bucket.
 */
class GetFolderMetadataRequest
    : public GenericObjectRequest<GetFolderMetadataRequest>
{
public:
    using GenericObjectRequest::GenericObjectRequest;
};

std::ostream& operator<<(std::ostream& os, GetFolderMetadataRequest const& r);

}  // namespace internal
}  // namespace csa
