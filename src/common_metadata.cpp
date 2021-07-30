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

#include "cloudstorageapi/common_metadata.h"
#include "cloudstorageapi/internal/ios_flags_saver.h"

namespace csa {

bool operator==(CommonMetadata const& lhs, CommonMetadata const& rhs)
{
    return lhs.m_cloudId == rhs.m_cloudId && lhs.m_name == rhs.m_name && lhs.m_parentId == rhs.m_parentId &&
           lhs.m_size == rhs.m_size && lhs.m_ctime == rhs.m_ctime && lhs.m_mtime == rhs.m_mtime &&
           lhs.m_atime == rhs.m_atime;
}

std::ostream& operator<<(std::ostream& os, CommonMetadata const& rhs)
{
    os << "cloudId=" << rhs.GetCloudId() << ", name=" << rhs.GetName() << ", parentId=" << rhs.GetParentId()
       << ", size=" << rhs.GetSize() << ", change time=" << rhs.GetChangeTime().time_since_epoch().count()
       << ", modify time=" << rhs.GetModifyTime().time_since_epoch().count()
       << ", access time=" << rhs.GetAccessTime().time_since_epoch().count();

    return os;
}

}  // namespace csa
