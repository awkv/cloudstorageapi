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

#include <iostream>

namespace csa {

/**
 * This structure represent storage quota in bytes.
 */
struct StorageQuota
{
    /** Total available space in bytes */
    std::int64_t m_total;

    /** Used space in bytes */
    std::int64_t m_usage;
};

inline std::ostream& operator<<(std::ostream& os, StorageQuota const& q)
{
    os << "storage_quota={"
       << "total=" << q.m_total
       << ", usage=" << q.m_usage;
    return os << "}";
}

} // namespace csa
