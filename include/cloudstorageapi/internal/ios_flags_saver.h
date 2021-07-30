// Copyright 2019 Andrew Karasyov
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

#include <iostream>

namespace csa {
namespace internal {

/**
 * Save the formatting flags in a std::ios_base and restores them.
 *
 * This class is a helper for functions that need to restore all formatting
 * flags in a iostream. Typically it is used in the implementation of a ostream
 * operator to restore any flags changed to format the output.
 *
 * @par Example
 * @code
 * std::ostream& operator<<(std::ostream& os, MyType const& rhs) {
 *   IosFlagsSaver save_flags(io);
 *   os << "enabled=" << std::boolalpha << rhs.enabled;
 *   // more calls here ... potentially modifying more flags
 *   return os << "blah";
 * }
 * @endcode
 */
class IosFlagsSaver final
{
public:
    IosFlagsSaver(std::ios_base& ios) : m_ios(ios), m_flags(m_ios.flags()) {}
    ~IosFlagsSaver() { m_ios.setf(m_flags); }

private:
    std::ios_base& m_ios;
    std::ios_base::fmtflags m_flags;
};

}  // namespace internal
}  // namespace csa
