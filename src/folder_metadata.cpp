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

#include "cloudstorageapi/folder_metadata.h"
#include "cloudstorageapi/internal/ios_flags_saver.h"

namespace csa {

bool operator==(FolderMetadata const& lhs, FolderMetadata const& rhs)
{
    return static_cast<CommonMetadata const&>(lhs) == rhs && lhs.m_CanCreateFolders == rhs.m_CanCreateFolders &&
           lhs.m_CanUploadFile == rhs.m_CanUploadFile;
}

std::ostream& operator<<(std::ostream& os, FolderMetadata const& rhs)
{
    internal::IosFlagsSaver save_format(os);
    os << std::boolalpha << "FolderMetadata={" << static_cast<CommonMetadata const&>(rhs)
       << ", CanCreateFolders=" << (rhs.GetCanCreateFolders()) << ", CanUploadFiles=" << (rhs.GetCanUploadFile())
       << "}";

    return os;
}

}  // namespace csa
