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
#include "cloudstorageapi/internal/generic_object_request.h"
#include "cloudstorageapi/well_known_headers.h"

namespace csa {
namespace internal {

/**
 * Represents a request to the file metadata.
 */
class GetFileMetadataRequest
    : public GenericObjectRequest<GetFileMetadataRequest>
{
public:
    using GenericObjectRequest::GenericObjectRequest;
};

std::ostream& operator<<(std::ostream& os, GetFileMetadataRequest const& r);

/**
 * Represents a request to rename/move file.
 */
class RenameFileRequest
    : public GenericObjectRequest<RenameFileRequest>
{
public:
    RenameFileRequest(std::string id, std::string newName, std::string parentId, std::string newParentId)
        : GenericObjectRequest<RenameFileRequest>(id),
        m_newName(std::move(newName)),
        m_parentId(std::move(parentId)),
        m_newParentId(std::move(newParentId))
    {}

    std::string GetParentId() const { return m_parentId; }
    std::string GetNewParentId() const { return m_newParentId; }
    std::string GetNewName() const { return m_newName; }

private:
    std::string m_newName;
    std::string m_parentId;
    std::string m_newParentId;
};

std::ostream& operator<<(std::ostream& os, RenameFileRequest const& r);

class InsertFileRequest : public GenericObjectRequest<InsertFileRequest, ContentType, WithFileMetadata>
{
public:
    InsertFileRequest(std::string folderId, std::string name, std::string content)
        : m_folderId(std::move(folderId)),
        m_name(std::move(name)),
        m_content(std::move(content))
    {}

    std::string GetFolderId() const { return m_folderId; }
    std::string GetName() const { return m_name; }
    std::string const& GetContent() const { return m_content; }

private:
    std::string m_folderId;
    std::string m_name;
    std::string m_content;
};

std::ostream& operator<<(std::ostream& os, InsertFileRequest const& r);
}  // namespace internal
}  // namespace csa
