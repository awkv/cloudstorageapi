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
#include "cloudstorageapi/internal/generic_object_requests.h"
#include "cloudstorageapi/internal/http_response.h"
#include "cloudstorageapi/well_known_parameters.h"
#include <iosfwd>
#include <variant>

namespace csa {
namespace internal {

/**
 * Requests the list of folders.
 */
class ListFolderRequest : public GenericObjectRequest<ListFolderRequest, MaxResults>
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
class GetFolderMetadataRequest : public GenericObjectRequest<GetFolderMetadataRequest>
{
public:
    using GenericObjectRequest::GenericObjectRequest;
};

std::ostream& operator<<(std::ostream& os, GetFolderMetadataRequest const& r);

/**
 *
 */
class CreateFolderRequest : public GenericObjectRequest<CreateFolderRequest, WithFolderMetadata>
{
public:
    CreateFolderRequest(std::string parentId, std::string newName)
        : m_parentId(std::move(parentId)), m_name(std::move(newName))
    {
    }

    std::string GetParent() const { return m_parentId; }
    std::string GetName() const { return m_name; }

private:
    std::string m_parentId;
    std::string m_name;
};

std::ostream& operator<<(std::ostream& os, CreateFolderRequest const& r);

/**
 * Represents a request to the `Files: update` API.
 */
class PatchFolderMetadataRequest : public GenericObjectRequest<PatchFolderMetadataRequest>
{
public:
    PatchFolderMetadataRequest() = default;
    explicit PatchFolderMetadataRequest(std::string folderId, FolderMetadata original, FolderMetadata updated)
        : GenericObjectRequest(std::move(folderId)),
          m_OriginalMeta(std::move(original)),
          m_UpdatedMeta(std::move(updated))
    {
    }

    FolderMetadata const& GetOriginalMetadata() const { return m_OriginalMeta; }
    FolderMetadata const& GetUpdatedMetadata() const { return m_UpdatedMeta; }

private:
    FolderMetadata m_OriginalMeta;
    FolderMetadata m_UpdatedMeta;
};

std::ostream& operator<<(std::ostream& os, PatchFolderMetadataRequest const& r);

}  // namespace internal
}  // namespace csa
