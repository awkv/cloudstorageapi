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

#include "cloudstorageapi/internal/ios_flags_saver.h"
#include <cstdint>
#include <iomanip>
#include <optional>
#include <string>

namespace csa {
namespace internal {
/**
 * Defines well-known request headers using the CRTP.
 *
 * @tparam P the type we will use to represent the query parameter.
 * @tparam T the C++ type of the query parameter
 */
template <typename P, typename T>
class WellKnownParameter
{
public:
    WellKnownParameter() : m_value{} {}
    explicit WellKnownParameter(T&& value) : m_value(std::forward<T>(value)) {}
    explicit WellKnownParameter(T const& value) : m_value(value) {}

    char const* ParameterName() const { return P::WellKnownParameterName(); }
    bool HasValue() const { return m_value.has_value(); }
    T const& value() const { return m_value.value(); }

private:
    std::optional<T> m_value;
};

template <typename P, typename T>
std::ostream& operator<<(std::ostream& os, WellKnownParameter<P, T> const& rhs)
{
    if (rhs.HasValue())
    {
        return os << rhs.ParameterName() << "=" << rhs.value();
    }
    return os << rhs.ParameterName() << "=<not set>";
}

template <typename P>
std::ostream& operator<<(std::ostream& os, WellKnownParameter<P, bool> const& rhs)
{
    if (rhs.HasValue())
    {
        csa::internal::IosFlagsSaver saver(os);
        return os << rhs.ParameterName() << "=" << std::boolalpha << rhs.value();
    }
    return os << rhs.ParameterName() << "=<not set>";
}
}  // namespace internal

/**
 * Sets the contentEncoding option for object uploads.
 *
 * The contentEncoding option allows applications to describe how is the data
 * encoded (binary or ASCII) in upload requests.
 */
struct ContentEncoding : public internal::WellKnownParameter<ContentEncoding, std::string>
{
    using WellKnownParameter<ContentEncoding, std::string>::WellKnownParameter;
    static char const* WellKnownParameterName() { return "contentEncoding"; }
};

/**
 * Included deleted HMAC keys in list requests.
 */
struct Deleted : public internal::WellKnownParameter<Deleted, bool>
{
    using WellKnownParameter<Deleted, bool>::WellKnownParameter;
    static char const* WellKnownParameterName() { return "deleted"; }
};

/**
 * Defines the `fields` query parameter.
 *
 * The `fields` parameter can be used to limit the fields returned by a request,
 * saving bandwidth and possibly improving performance for applications that do
 * not need a full response from the server.
 *
 * @see
 * https://cloud.google.com/storage/docs/json_api/v1/how-tos/performance#partial-response
 *     for general documentation on how to use this parameter.
 */
struct Fields : public internal::WellKnownParameter<Fields, std::string>
{
    using WellKnownParameter<Fields, std::string>::WellKnownParameter;
    static char const* WellKnownParameterName() { return "fields"; }
};

/**
 * Set the version of an object to operate on.
 *
 * For objects in Buckets with `versioning` enabled, the application sometimes
 * needs to specify which version of the object should the request target. This
 * is an optional query parameter to control the version.
 *
 * @see https://cloud.google.com/storage/docs/object-versioning for more
 *     information on GCS Object versioning.
 */
struct Generation : public internal::WellKnownParameter<Generation, std::int64_t>
{
    using WellKnownParameter<Generation, std::int64_t>::WellKnownParameter;
    static char const* WellKnownParameterName() { return "generation"; }
};

/**
 * Limit the number of results per page when listing Folders and Files.
 *
 * Applications may reduce the memory requirements of the Folder and File
 * iterators by using smaller page sizes. The downside is that more requests
 * may be needed to iterate over the full range of Folders and/or Files.
 */
struct PageSize : public internal::WellKnownParameter<PageSize, std::int64_t>
{
    using WellKnownParameter<PageSize, std::int64_t>::WellKnownParameter;
    static char const* WellKnownParameterName() { return "pageSize"; }
};

/**
 * Limit the number of bytes rewritten in a `Objects: rewrite` step.
 *
 * Applications should not need for the most part. It is used during testing, to
 * ensure the code handles partial rewrites properly. Note that the value must
 * be a multiple of 1 MiB (1048576).
 */
struct MaxBytesRewrittenPerCall : public internal::WellKnownParameter<MaxBytesRewrittenPerCall, std::int64_t>
{
    using WellKnownParameter<MaxBytesRewrittenPerCall, std::int64_t>::WellKnownParameter;
    static char const* WellKnownParameterName() { return "maxBytesRewrittenPerCall"; }
};

/**
 * Control if all versions of an object should be included when listing objects.
 *
 * By default requests listing objects only included the latest (live) version
 * of each object, set this option to `true` to get all the previous versions.
 *
 * @see https://cloud.google.com/storage/docs/object-versioning for more
 *     information on GCS Object versioning.
 */
struct Versions : public internal::WellKnownParameter<Versions, bool>
{
    using WellKnownParameter<Versions, bool>::WellKnownParameter;
    static char const* WellKnownParameterName() { return "versions"; }
};

}  // namespace csa
