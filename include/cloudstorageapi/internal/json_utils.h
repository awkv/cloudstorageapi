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

#include "cloudstorageapi/internal/nljson.h"
#include <chrono>

namespace csa {
namespace internal {

class JsonUtils
{
public:
    /**
     * Parses a boolean field, even if it is represented by a string type in the
     * JSON object.
     */
    static bool ParseBool(nl::json const& json, char const* fieldName);

    /**
     * Parses an integer field, even if it is represented by a string type in the
     * JSON object.
     */
    static std::int32_t ParseInt(nl::json const& json, char const* field_nName);

    /**
     * Parses an unsigned integer field, even if it is represented by a string type
     * in the JSON object.
     */
    static std::uint32_t ParseUnsignedInt(nl::json const& json, char const* fieldName);

    /**
     * Parses a long integer field, even if it is represented by a string type in
     * the JSON object.
     */
    static std::int64_t ParseLong(nl::json const& json, char const* fieldName);

    /**
     * Parses an unsigned long integer field, even if it is represented by a string
     * type in the JSON object.
     */
    static std::uint64_t ParseUnsignedLong(nl::json const& json, char const* fieldName);

    /**
     * Parses a RFC 3339 timestamp.
     *
     * @return the value of @p field_name in @p json, or the epoch if the field is
     * not present.
     */
    static std::chrono::system_clock::time_point ParseRFC3339Timestamp(nl::json const& json, char const* fieldName);
};

}  // namespace internal
}  // namespace csa
