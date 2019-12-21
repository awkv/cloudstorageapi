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

#include "cloudstorageapi/internal/json_utils.h"
#include "cloudstorageapi/internal/rfc3339_timestamp_parser.h"
#include <stdexcept>
#include <sstream>

namespace csa {
namespace internal {

bool JsonUtils::ParseBool(nl::json const& json, char const* fieldName)
{
    if (json.count(fieldName) == 0)
    {
        return false;
    }
    auto const& f = json[fieldName];
    if (f.is_boolean())
    {
        return f.get<bool>();
    }
    if (f.is_string())
    {
        auto v = f.get<std::string>();
        if (v == "true")
        {
            return true;
        }
        if (v == "false")
        {
            return false;
        }
    }
    std::ostringstream os;
    os << "Error parsing field <" << fieldName
    << "> as a boolean, json=" << json;
    throw std::invalid_argument(os.str());
}

std::int32_t JsonUtils::ParseInt(nl::json const& json, char const* fieldName)
{
    if (json.count(fieldName) == 0)
    {
        return 0;
    }
    auto const& f = json[fieldName];
    if (f.is_number())
    {
        return f.get<std::int32_t>();
    }
    if (f.is_string())
    {
        return std::stol(f.get_ref<std::string const&>());
    }
    std::ostringstream os;
    os << "Error parsing field <" << fieldName
        << "> as an std::int32_t, json=" << json;
    throw std::invalid_argument(os.str());
}

std::uint32_t JsonUtils::ParseUnsignedInt(nl::json const& json,
    char const* fieldName)
{
    if (json.count(fieldName) == 0)
    {
        return 0;
    }
    auto const& f = json[fieldName];
    if (f.is_number())
    {
        return f.get<std::uint32_t>();
    }
    if (f.is_string())
    {
        return std::stoul(f.get_ref<std::string const&>());
    }
    std::ostringstream os;
    os << "Error parsing field <" << fieldName
        << "> as an std::uint32_t, json=" << json;
    throw std::invalid_argument(os.str());
}

std::int64_t JsonUtils::ParseLong(nl::json const& json, char const* fieldName)
{
    if (json.count(fieldName) == 0)
    {
        return 0;
    }
    auto const& f = json[fieldName];
    if (f.is_number())
    {
        return f.get<std::int64_t>();
    }
    if (f.is_string())
    {
        return std::stoll(f.get_ref<std::string const&>());
    }
    std::ostringstream os;
    os << "Error parsing field <" << fieldName
        << "> as an std::int64_t, json=" << json;
    throw std::invalid_argument(os.str());
}

std::uint64_t JsonUtils::ParseUnsignedLong(nl::json const& json,
    char const* fieldName)
{
    if (json.count(fieldName) == 0)
    {
        return 0;
    }
    auto const& f = json[fieldName];
    if (f.is_number())
    {
        return f.get<std::uint64_t>();
    }
    if (f.is_string())
    {
        return std::stoull(f.get_ref<std::string const&>());
    }
    std::ostringstream os;
    os << "Error parsing field <" << fieldName
        << "> as an std::uint64_t, json=" << json;
    throw std::invalid_argument(os.str());
}

std::chrono::system_clock::time_point JsonUtils::ParseRFC3339Timestamp(
    nl::json const& json, char const* fieldName)
{
    if (json.count(fieldName) == 0)
    {
        return std::chrono::system_clock::time_point{};
    }
    return ParseRfc3339(json[fieldName]);
}

}  // namespace internal
}  // namespace csa
