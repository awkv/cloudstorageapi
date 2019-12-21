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

#include <cstdint>
#include <iostream>
#include <limits>
#include <optional>
#include <random>
#include <string>

namespace csa {
namespace internal {
/**
 * Defines well-known request headers using the CRTP.
 *
 * @tparam H the type we will use to represent the header.
 * @tparam T the C++ type of the query parameter
 */
template <typename H, typename T>
class WellKnownHeader
{
public:
    WellKnownHeader() : m_value{} {}
    explicit WellKnownHeader(T value) : m_value(std::move(value)) {}

    char const* header_name() const { return H::header_name(); }
    bool HasValue() const { return m_value.has_value(); }
    T const& value() const { return m_value.value(); }

private:
    std::optional<T> m_value;
};

template <typename H, typename T>
std::ostream& operator<<(std::ostream& os, WellKnownHeader<H, T> const& rhs)
{
    if (rhs.HasValue())
    {
        return os << rhs.header_name() << ": " << rhs.value();
    }
    return os << rhs.header_name() << ": <not set>";
}
}  // namespace internal

/**
 * Set the MIME content type of an object.
 *
 * This optional parameter sets the content-type of an object during uploads,
 * without having to configure all the other metadata attributes.
 */
struct ContentType : public internal::WellKnownHeader<ContentType, std::string>
{
    using WellKnownHeader<ContentType, std::string>::WellKnownHeader;
    static char const* header_name() { return "content-type"; }
};

/**
 * An option to inject custom headers into the request.
 *
 * In some cases it is necessary to inject a custom header into the request. For
 * example, because the protocol has added new headers and the library has not
 * been updated to support them, or because
 */
class CustomHeader : public internal::WellKnownHeader<CustomHeader, std::string>
{
public:
    CustomHeader() = default;
    explicit CustomHeader(std::string name, std::string value)
        : WellKnownHeader(std::move(value)), m_name(std::move(name)) {}

    std::string const& CustomHeaderName() const { return m_name; }

private:
    std::string m_name;
};

std::ostream& operator<<(std::ostream& os, CustomHeader const& rhs);

/**
 * A pre-condition: apply this operation only if the HTTP Entity Tag matches.
 *
 * [HTTP Entity Tags](https://en.wikipedia.org/wiki/HTTP_ETag) allow
 * applications to conditionally execute a query only if the target resource
 * matches the expected state. This can be useful, for example, to implement
 * optimistic concurrency control in the application.
 */
struct IfMatchEtag : public internal::WellKnownHeader<IfMatchEtag, std::string>
{
    using WellKnownHeader<IfMatchEtag, std::string>::WellKnownHeader;
    static char const* header_name() { return "If-Match"; }
};

/**
 * A pre-condition: apply this operation only if the HTTP Entity Tag does not
 * match.
 *
 * [HTTP Entity Tags](https://en.wikipedia.org/wiki/HTTP_ETag) allow
 * applications to conditionally execute a query only if the target resource
 * matches the expected state. This can be useful, for example, to implement
 * optimistic concurrency control in the application.
 */
struct IfNoneMatchEtag : public internal::WellKnownHeader<IfNoneMatchEtag, std::string>
{
    using WellKnownHeader<IfNoneMatchEtag, std::string>::WellKnownHeader;
    static char const* header_name() { return "If-None-Match"; }
};

}  // namespace csa
