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
#include <optional>

namespace csa {
namespace internal {
/**
 * A complex option is a request optional parameter that is neither a header
 * nor a query parameter.
 *
 * The majority of the request options either change a header (or group of
 * headers) or they set a query parameter. They are modeled using
 * `WellKnownParameter` or `WellKnownHeader`. A few options do neither, they
 * affect how the query is performed. For example
 * user can provide pre-computed values for the MD5 hash and CRC32C values of
 * an upload or download.
 *
 * @tparam Derived the type we will use to represent the option.
 * @tparam T the C++ type of the option value.
 */
template <typename Derived, typename T>
class ComplexOption
{
public:
    ComplexOption() : m_value{} {}
    explicit ComplexOption(T value) : m_value(std::move(value)) {}

    char const* OptionName() const { return Derived::name(); }
    bool HasValue() const { return m_value.has_value(); }
    T const& Value() const { return m_value.value(); }

private:
    std::optional<T> m_value;
};

template <typename Derived, typename T>
std::ostream& operator<<(std::ostream& os, ComplexOption<Derived, T> const& rhs)
{
    if (rhs.HasValue())
    {
        return os << rhs.OptionName() << "=" << rhs.Value();
    }
    return os << rhs.OptionName() << "=<not set>";
}

}  // namespace internal
}  // namespace csa
