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
#include <type_traits>

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
std::string BinaryDataAsDebugString(char const* data, std::size_t size, std::size_t max_output_bytes = 0);

/**
 * Generate a string that is not found in @p message.
 *
 * When sending messages over multipart MIME payloads we need a separator that
 * is not found in the body of the message *and* that is not too large (it is
 * trivial to generate a string not found in @p message, just append some
 * characters to the message itself).
 *
 * The algorithm is to generate a short random string, and search for it in the
 * message, if the message has that string, append some more random characters
 * and keep searching.
 *
 * This function is a template because the string generator is typically a
 * lambda that captures state variables (such as the random number generator),
 * of the class that uses it.
 *
 * @param message a message body, typically the payload of a HTTP request that
 *     will need to be encoded as a MIME multipart message.
 * @param random_string_generator a callable to generate random strings.
 *     Typically a lambda that captures the generator and any locks necessary
 *     to generate the string. This is also used in testing to verify things
 *     work when the initial string is found.
 * @param initial_size the length for the initial string.
 * @param growth_size how fast to grow the random string.
 * @return a string not found in @p message.
 */
template <typename RandomStringGenerator,
          typename std::enable_if<std::is_invocable<RandomStringGenerator, int>::value, int>::type = 0>
std::string GenerateMessageBoundary(std::string const& message, RandomStringGenerator&& random_string_generator,
                                    int initial_size, int growth_size)
{
    std::string candidate = random_string_generator(initial_size);
    for (std::string::size_type i = message.find(candidate, 0); i != std::string::npos; i = message.find(candidate, i))
    {
        candidate += random_string_generator(growth_size);
    }
    return candidate;
}

/**
 * Some cloud storages require file chunks to be a multiple of some quantum in size.
 * This function rounds up given chunk size to be multiply of quantum.
 */
std::size_t RoundUpToQuantum(std::size_t maxChunkSize, std::size_t quantumSize);

}  // namespace internal
}  // namespace csa
