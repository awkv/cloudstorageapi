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

#include "cloudstorageapi/internal/folder_requests.h"
#include <sstream>
#include <iterator>

namespace csa {
namespace internal {

std::ostream& operator<<(std::ostream& os, ListFolderRequest const& r)
{
    os << "ListFolderRequest={folder_name=" << r.GetObjectId();
    r.DumpOptions(os, ", ");
    return os << "}";
}

std::ostream& operator<<(std::ostream& os, ListFolderResponse const& r)
{
    os << "ListFolderResponse={next_page_token=" << r.m_nextPageToken
        << ", items={";
    for (auto& item : r.m_items)
    {
        std::visit([&os](auto&& metaItem) { os << metaItem; }, item);
    }
    return os << "}}";
}

std::ostream& operator<<(std::ostream& os, GetFolderMetadataRequest const& r)
{
    os << "GetFolderMetadataRequest={folder_name=" << r.GetObjectId();
    r.DumpOptions(os, ", ");
    return os << "}";
}

std::ostream& operator<<(std::ostream& os, CreateFolderRequest const& r)
{
    os << "CreateFolderRequest={parent_id=" << r.GetParent()
        << ", folder_name=" << r.GetName();
    r.DumpOptions(os, ", ");
    return os << "}";
}

std::ostream& operator<<(std::ostream& os, PatchFolderMetadataRequest const& r)
{
    os << "PatchFolderMetadataRequest={file_id=" << r.GetObjectId()
        << ", original_metadata=" << r.GetOriginalMetadata()
        << ", updated_metadata=" << r.GetUpdatedMetadata();
    r.DumpOptions(os, ", ");
    return os << "}";
}

} // namespace internal
} // namespace csa
