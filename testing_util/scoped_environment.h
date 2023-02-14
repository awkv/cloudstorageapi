// Copyright 2020 Andrew Karasyov
//
// Copyright 2020 Google LLC
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

#include <optional>
#include <string>

namespace csa {
namespace testing {
namespace internal {

/**
 * Helper class to (un)set and restore the value of an environment variable.
 */
class ScopedEnvironment
{
public:
    /**
     * Set the @p variable environment variable to @p value. If @value is
     * an unset optional then the variable is unset. The previous value of
     * the variable will be restored upon destruction.
     */
    ScopedEnvironment(std::string variable, std::optional<std::string> const& value);

    ~ScopedEnvironment();

    // Movable, but not copyable.
    ScopedEnvironment(ScopedEnvironment const&) = delete;
    ScopedEnvironment(ScopedEnvironment&&) = default;
    ScopedEnvironment& operator=(ScopedEnvironment const&) = delete;
    ScopedEnvironment& operator=(ScopedEnvironment&&) = default;

private:
    std::string m_variable;
    std::optional<std::string> m_prevValue;
};

}  // namespace internal
}  // namespace testing
}  // namespace csa
