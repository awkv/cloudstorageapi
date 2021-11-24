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

#include "cloudstorageapi/status_or_val.h"
#include <nlohmann/json.hpp>
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
    static StatusOrVal<bool> ParseBool(nlohmann::json const& json, char const* fieldName);

    /**
     * Parses an integer field, even if it is represented by a string type in the
     * JSON object.
     */
    static StatusOrVal<std::int32_t> ParseInt(nlohmann::json const& json, char const* field_nName);

    /**
     * Parses an unsigned integer field, even if it is represented by a string type
     * in the JSON object.
     */
    static StatusOrVal<std::uint32_t> ParseUnsignedInt(nlohmann::json const& json, char const* fieldName);

    /**
     * Parses a long integer field, even if it is represented by a string type in
     * the JSON object.
     */
    static StatusOrVal<std::int64_t> ParseLong(nlohmann::json const& json, char const* fieldName);

    /**
     * Parses an unsigned long integer field, even if it is represented by a string
     * type in the JSON object.
     */
    static StatusOrVal<std::uint64_t> ParseUnsignedLong(nlohmann::json const& json, char const* fieldName);

    /**
     * Parses a RFC 3339 timestamp.
     *
     * @return the value of @p field_name in @p json, or the epoch if the field is
     * not present.
     */
    static StatusOrVal<std::chrono::system_clock::time_point> ParseRFC3339Timestamp(nlohmann::json const& json,
                                                                                    char const* fieldName);
};

}  // namespace internal
}  // namespace csa
