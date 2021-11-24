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
#include "cloudstorageapi/internal/rfc3339_time.h"
#include <sstream>
#include <stdexcept>

namespace csa {
namespace internal {

StatusOrVal<bool> JsonUtils::ParseBool(nlohmann::json const& json, char const* fieldName)
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
    os << "Error parsing field <" << fieldName << "> as a boolean, json=" << json;
    return Status(StatusCode::InvalidArgument, std::move(os).str());
}

StatusOrVal<std::int32_t> JsonUtils::ParseInt(nlohmann::json const& json, char const* fieldName)
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
        try
        {
            return std::stoi(f.get_ref<std::string const&>());
        }
        catch (...)
        {
            // Empty. Error is reported below.
        }
    }
    std::ostringstream os;
    os << "Error parsing field <" << fieldName << "> as an std::int32_t, json=" << json;
    return Status(StatusCode::InvalidArgument, std::move(os).str());
}

StatusOrVal<std::uint32_t> JsonUtils::ParseUnsignedInt(nlohmann::json const& json, char const* fieldName)
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
        try
        {
            auto v = std::stoul(f.get_ref<std::string const&>());
            if (v <= (std::numeric_limits<std::uint32_t>::max)())
            {
                return static_cast<std::uint32_t>(v);
            }
        }
        catch (...)
        {
            // Empty. Error is reported below.
        }
    }
    std::ostringstream os;
    os << "Error parsing field <" << fieldName << "> as an std::uint32_t, json=" << json;
    return Status(StatusCode::InvalidArgument, std::move(os).str());
}

StatusOrVal<std::int64_t> JsonUtils::ParseLong(nlohmann::json const& json, char const* fieldName)
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
        try
        {
            return std::stoll(f.get_ref<std::string const&>());
        }
        catch (...)
        {
            // Empty. Error is reported below.
        }
    }
    std::ostringstream os;
    os << "Error parsing field <" << fieldName << "> as an std::int64_t, json=" << json;
    return Status(StatusCode::InvalidArgument, std::move(os).str());
}

StatusOrVal<std::uint64_t> JsonUtils::ParseUnsignedLong(nlohmann::json const& json, char const* fieldName)
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
        try
        {
            return std::stoull(f.get_ref<std::string const&>());
        }
        catch (...)
        {
            // Empty. Error is reported below.
        }
    }
    std::ostringstream os;
    os << "Error parsing field <" << fieldName << "> as an std::uint64_t, json=" << json;
    return Status(StatusCode::InvalidArgument, std::move(os).str());
}

StatusOrVal<std::chrono::system_clock::time_point> JsonUtils::ParseRFC3339Timestamp(nlohmann::json const& json,
                                                                                    char const* fieldName)
{
    if (json.count(fieldName) == 0)
    {
        return std::chrono::system_clock::time_point{};
    }
    auto const& f = json[fieldName];
    if (f.is_string())
        return ParseRfc3339(f);
    std::ostringstream os;
    os << "Error parsing field <" << fieldName << "> as a timestamp, json=" << json;
    return Status(StatusCode::InvalidArgument, std::move(os).str());
}

}  // namespace internal
}  // namespace csa
