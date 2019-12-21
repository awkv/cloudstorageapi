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

#include <optional>
#include <string>

namespace csa {
namespace internal {

/**
 * Return the value of an environment variable, or an unset optional.
 *
 * On Windows `std::getenv()` is not thread safe. We must write a wrapper to
 * portably get the value of the environment variables.
 */
std::optional<std::string> GetEnv(char const* variable);

/// Unset (remove) an environment variable.
void UnsetEnv(char const* variable);

/**
 * Set the @p variable environment variable to @p value.
 *
 * If @value is the null pointer then the variable is unset.
 *
 * @note On Windows, due to the underlying API function, an empty @value unsets
 * the variable, while on Linux an empty environment variable is created.
 *
 * @see
 * https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/putenv-s-wputenv-s?view=vs-2019
 */
void SetEnv(char const* variable, char const* value);

/**
 * Set the @p variable environment variable to @p value.
 *
 * If @value is an unset optional then the variable is unset.
 */
void SetEnv(char const* variable, std::optional<std::string> value);

/**
 * Formats a block of data for debug printing.
 *
 * Takes a block of data, possible with non-printable characters and creates
 * a string with two columns.  The first column is 24 characters wide and has
 * the non-printable characters replaced by periods.  The second column is 48
 * characters wide and contains the hexdump of the data.  The columns are
 * separated by a single space.
 */
std::string BinaryDataAsDebugString(char const* data, std::size_t size,
    std::size_t max_output_bytes = 0);

}  // namespace internal
}  // namespace csa
