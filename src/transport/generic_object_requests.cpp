// Copyright 2020 Andrew Karasyov
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

#include "cloudstorageapi/internal/generic_object_requests.h"
#include <sstream>
#include <iterator>

namespace csa {
namespace internal {

std::ostream& operator<<(std::ostream& os, RenameRequest const& r)
{
    os << "RenameRequest={object_id=" << r.GetObjectId()
        << ", new_name=" << r.GetNewName()
        << ", parent_id=" << r.GetParentId()
        << ", new_parent_id=" << r.GetNewParentId();
    r.DumpOptions(os, ", ");
    return os << "}";
}

} // namespace internal
} // namespace csa
