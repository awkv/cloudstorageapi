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

#include "cloudstorageapi/internal/utils.h"
#ifdef _WIN32
// We need _dupenv_s()
#include <stdlib.h>
#else
// On Unix-like systems we need setenv()/unsetenv(), which are defined here:
#include <cstdlib>
#endif  // _WIN32
#include <memory>

namespace csa {
namespace internal {

std::optional<std::string> GetEnv(char const* variable)
{
#if _WIN32
    // On Windows, std::getenv() is not thread-safe. It returns a pointer that
    // can be invalidated by _putenv_s(). We must use the thread-safe alternative,
    // which unfortunately allocates the buffer using malloc():
    char* buffer;
    std::size_t size;
    _dupenv_s(&buffer, &size, variable);
    std::unique_ptr<char, decltype(&free)> release(buffer, &free);
#else
    char* buffer = std::getenv(variable);
#endif  // _WIN32
    if (buffer == nullptr)
        return std::optional<std::string>();

    return std::optional<std::string>(std::string{buffer});
}

void UnsetEnv(char const* variable)
{
#ifdef _WIN32
    (void)_putenv_s(variable, "");
#else
    unsetenv(variable);
#endif  // _WIN32
}

void SetEnv(char const* variable, char const* value)
{
    if (value == nullptr)
    {
        UnsetEnv(variable);
        return;
    }
#ifdef _WIN32
    (void)_putenv_s(variable, value);
#else
    (void)setenv(variable, value, 1);
#endif  // _WIN32
}

void SetEnv(char const* variable, std::optional<std::string> value)
{
    if (!value.has_value())
    {
        UnsetEnv(variable);
        return;
    }
    SetEnv(variable, value->data());
}

std::string BinaryDataAsDebugString(char const* data, std::size_t size,
    std::size_t max_output_bytes)
{
    std::string result;
    std::size_t text_width = 24;
    std::string text_column(text_width, ' ');
    std::string hex_column(2 * text_width, ' ');

    auto flush = [&result, &text_column, &hex_column, text_width]
    {
        result += text_column;
        result += ' ';
        result += hex_column;
        result += '\n';
        text_column = std::string(text_width, ' ');
        hex_column = std::string(2 * text_width, ' ');
    };

    // Limit the output to the first `max_output_bytes`.
    std::size_t n = size;
    if (max_output_bytes > 0 && max_output_bytes < size)
    {
        n = max_output_bytes;
    }

    std::size_t count = 0;
    for (char const* c = data; c != data + n; ++c)
    {
        // std::isprint() actually takes an int argument, signed, without this
        // explicit conversion MSVC in Debug mode asserts an invalid argument, and
        // pops up a nice dialog box that breaks the CI builds.
        int cval = static_cast<unsigned char>(*c);
        if (std::isprint(cval) != 0)
        {
            text_column[count] = *c;
        }
        else
        {
            text_column[count] = '.';
        }
        char buf[3];
        snprintf(buf, sizeof(buf), "%02x", cval);
        hex_column[2 * count] = buf[0];
        hex_column[2 * count + 1] = buf[1];
        ++count;
        if (count == text_width)
        {
            flush();
            count = 0;
        }
    }
    if (count != 0)
    {
        flush();
    }
    return result;
}

std::size_t RoundUpToQuantum(std::size_t maxChunkSize, std::size_t quantumSize)
{
    // If you are tempted to use bit manipulation to do this, modern compilers
    // known how to optimize this (tested at godbolt.org):
    //   https://godbolt.org/z/xxUsjg
    if (maxChunkSize % quantumSize == 0)
    {
        return maxChunkSize;
    }
    auto n = maxChunkSize / quantumSize;
    return (n + 1) * quantumSize;
}

}  // namespace internal
}  // namespace csa
