// Copyright 2020 Andrew Karasyov
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

#include "cloudstorageapi/internal/complex_option.h"
#include <cstdint>
#include <iostream>
#include <string>

namespace csa {

struct ReadRangeData
{
    std::int64_t m_begin;
    std::int64_t m_end;
};

/**
 * Request only a portion of the cloud storage file in a read operation.
 *
 * Note that the range is right-open, as it is customary in C++. That is, it
 * excludes the `end` byte.
 */
struct ReadRange : public internal::ComplexOption<ReadRange, ReadRangeData>
{
    ReadRange()
        : ComplexOption()
    {}

    explicit ReadRange(std::int64_t begin, std::int64_t end)
        : ComplexOption(ReadRangeData{ begin, end })
    {}

    static char const* name() { return "read-range"; }
};

inline std::ostream& operator<<(std::ostream& os, ReadRangeData const& rhs)
{
    return os << "ReadRangeData={begin=" << rhs.m_begin << ", end=" << rhs.m_end
        << "}";
}

/**
 * Download all the data from the cloud storage file starting at the given offset.
 */
struct ReadFromOffset
    : public internal::ComplexOption<ReadFromOffset, std::int64_t>
{
    using ComplexOption::ComplexOption;
    // GCC <= 7.0 does not use the inherited default constructor, redeclare it
    // explicitly
    ReadFromOffset() = default;
    static char const* name() { return "read-offset"; }
};

/**
 * Read last N bytes from the cloud storage file.
 */
struct ReadLast : public internal::ComplexOption<ReadLast, std::int64_t>
{
    using ComplexOption::ComplexOption;
    // GCC <= 7.0 does not use the inherited default constructor, redeclare it
    // explicitly
    ReadLast() = default;
    static char const* name() { return "read-last"; }
};

} // namespace csa
