// Copyright 2021 Andrew Karasyov
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

#include "cloudstorageapi/internal/json_utils.h"
#include "util/status_matchers.h"
#include <gmock/gmock.h>
#include <limits>

namespace csa {
namespace internal {

using ::csa::testing::util::StatusIs;

/// @test Verify that we parse boolean values in JSON objects.
TEST(JsonUtilsTest, ParseBool)
{
    std::string text = R"""({
      "flag1": true,
      "flag2": false
})""";
    auto jsonObject = nlohmann::json::parse(text);
    EXPECT_TRUE(JsonUtils::ParseBool(jsonObject, "flag1").Value());
    EXPECT_FALSE(JsonUtils::ParseBool(jsonObject, "flag2").Value());
}

/// @test Verify that we parse boolean values in JSON objects.
TEST(JsonUtilsTest, ParseBoolFromString)
{
    std::string text = R"""({
      "flag1": "true",
      "flag2": "false"
})""";
    auto jsonObject = nlohmann::json::parse(text);
    EXPECT_TRUE(JsonUtils::ParseBool(jsonObject, "flag1").Value());
    EXPECT_FALSE(JsonUtils::ParseBool(jsonObject, "flag2").Value());
}

/// @test Verify that we parse missing boolean values in JSON objects.
TEST(JsonUtilsTest, ParseMissingBool)
{
    std::string text = R"""({
      "flag": true
})""";
    auto jsonObject = nlohmann::json::parse(text);
    auto actual = JsonUtils::ParseBool(jsonObject, "some-other-flag").Value();
    EXPECT_FALSE(actual);
}

/// @test Verify that we raise an exception with invalid boolean fields.
TEST(JsonUtilsTest, ParseInvalidBoolValue)
{
    std::string text = R"""({"flag": "not-a-boolean"})""";
    auto jsonObject = nlohmann::json::parse(text);

    EXPECT_THAT(JsonUtils::ParseBool(jsonObject, "flag"), StatusIs(StatusCode::InvalidArgument));
}

/// @test Verify that we raise an exception with invalid boolean fields.
TEST(JsonUtilsTest, ParseInvalidBoolType)
{
    std::string text = R"""({
      "flag": [0, 1, 2]
})""";
    auto jsonObject = nlohmann::json::parse(text);

    EXPECT_THAT(JsonUtils::ParseBool(jsonObject, "flag"), StatusIs(StatusCode::InvalidArgument));
}

/// @test Verify that we parse RFC-3339 timestamps in JSON objects.
TEST(JsonUtilsTest, ParseTimestampField)
{
    std::string text = R"""({
      "timeCreated": "2018-05-19T19:31:14Z",
      "updated": "2018-05-19T19:31:24Z"
})""";
    auto jsonObject = nlohmann::json::parse(text);
    auto actual = JsonUtils::ParseRFC3339Timestamp(jsonObject, "timeCreated");
    ASSERT_STATUS_OK(actual);

    // Use `date -u +%s --date='2018-05-19T19:31:14Z'` to get the magic number:
    using std::chrono::duration_cast;
    EXPECT_EQ(1526758274L, duration_cast<std::chrono::seconds>(actual->time_since_epoch()).count());
}

/// @test Verify that we parse RFC-3339 timestamps in JSON objects.
TEST(JsonUtilsTest, ParseMissingTimestamp)
{
    std::string text = R"""({
      "updated": "2018-05-19T19:31:24Z"
})""";
    auto jsonObject = nlohmann::json::parse(text);
    auto actual = JsonUtils::ParseRFC3339Timestamp(jsonObject, "timeCreated");
    ASSERT_STATUS_OK(actual);

    using std::chrono::duration_cast;
    EXPECT_EQ(0L, duration_cast<std::chrono::seconds>(actual->time_since_epoch()).count());
}

TEST(JsonUtilsTest, ParseTimestampInvalidType)
{
    std::string text = R"""({
      "updated": [0, 1, 2]
})""";
    auto jsonObject = nlohmann::json::parse(text);
    auto actual = JsonUtils::ParseRFC3339Timestamp(jsonObject, "updated");
    EXPECT_THAT(actual, StatusIs(StatusCode::InvalidArgument));
}

template <typename Integer>
void CheckParseNormal(std::function<StatusOrVal<Integer>(nlohmann::json const&, char const*)> tested)
{
    std::string text = R"""({ "field": 42 })""";
    auto jsonObject = nlohmann::json::parse(text);
    auto actual = tested(jsonObject, "field");
    EXPECT_EQ(Integer(42), actual.Value());
}

/// @test Verify Parse* can parse regular values.
TEST(JsonUtilsTest, ParseIntegralNormal)
{
    CheckParseNormal<std::int32_t>(&JsonUtils::ParseInt);
    CheckParseNormal<std::uint32_t>(&JsonUtils::ParseUnsignedInt);
    CheckParseNormal<std::int64_t>(&JsonUtils::ParseLong);
    CheckParseNormal<std::uint64_t>(&JsonUtils::ParseUnsignedLong);
}

template <typename Integer>
void CheckParseFromString(std::function<StatusOrVal<Integer>(nlohmann::json const&, char const*)> tested)
{
    std::string text = R"""({ "field": "1234" })""";
    auto jsonObject = nlohmann::json::parse(text);
    auto actual = tested(jsonObject, "field");
    EXPECT_EQ(Integer(1234), actual.Value());
}

/// @test Verify Parse* can parse string values.
TEST(JsonUtilsTest, ParseIntegralFieldString)
{
    CheckParseFromString<std::int32_t>(&JsonUtils::ParseInt);
    CheckParseFromString<std::uint32_t>(&JsonUtils::ParseUnsignedInt);
    CheckParseFromString<std::int64_t>(&JsonUtils::ParseLong);
    CheckParseFromString<std::uint64_t>(&JsonUtils::ParseUnsignedLong);
}

template <typename Integer>
void CheckParseFullRange(std::function<StatusOrVal<Integer>(nlohmann::json const&, char const*)> tested)
{
    auto min = tested(nlohmann::json{{"field", std::to_string(std::numeric_limits<Integer>::min())}}, "field");
    EXPECT_EQ(std::numeric_limits<Integer>::min(), min.Value());
    auto max = tested(nlohmann::json{{"field", std::to_string(std::numeric_limits<Integer>::max())}}, "field");
    EXPECT_EQ(std::numeric_limits<Integer>::max(), max.Value());
}

/// @test Verify Parse* can parse string values.
TEST(JsonUtilsTest, ParseIntegralFullRange)
{
    CheckParseFullRange<std::int32_t>(&JsonUtils::ParseInt);
    CheckParseFullRange<std::uint32_t>(&JsonUtils::ParseUnsignedInt);
    CheckParseFullRange<std::int64_t>(&JsonUtils::ParseLong);
    CheckParseFullRange<std::uint64_t>(&JsonUtils::ParseUnsignedLong);
}

template <typename Integer>
void CheckParseMissing(std::function<StatusOrVal<Integer>(nlohmann::json const&, char const*)> tested)
{
    std::string text = R"""({ "field": "1234" })""";
    auto jsonObject = nlohmann::json::parse(text);
    auto actual = tested(jsonObject, "some-other-field");
    EXPECT_EQ(Integer(0), actual.Value());
}

/// @test Verify Parse* can parse string values.
TEST(JsonUtilsTest, ParseIntegralMissing)
{
    CheckParseMissing<std::int32_t>(&JsonUtils::ParseInt);
    CheckParseMissing<std::uint32_t>(&JsonUtils::ParseUnsignedInt);
    CheckParseMissing<std::int64_t>(&JsonUtils::ParseLong);
    CheckParseMissing<std::uint64_t>(&JsonUtils::ParseUnsignedLong);
}

template <typename Integer>
void CheckParseInvalid(std::function<StatusOrVal<Integer>(nlohmann::json const&, char const*)> tested)
{
    std::string text = R"""({ "field_name": "not-a-number" })""";
    auto jsonObject = nlohmann::json::parse(text);
    EXPECT_THAT(tested(jsonObject, "field_name"), StatusIs(StatusCode::InvalidArgument));
}

/// @test Verify Parse* detects invalid values.
TEST(JsonUtilsTest, ParseIntegralInvalid)
{
    CheckParseInvalid<std::int32_t>(&JsonUtils::ParseInt);
    CheckParseInvalid<std::uint32_t>(&JsonUtils::ParseUnsignedInt);
    CheckParseInvalid<std::int64_t>(&JsonUtils::ParseLong);
    CheckParseInvalid<std::uint64_t>(&JsonUtils::ParseUnsignedLong);
}

template <typename Integer>
void CheckParseInvalidType(std::function<StatusOrVal<Integer>(nlohmann::json const&, char const*)> tested)
{
    std::string text = R"""({ "field_name": [0, 1, 2] })""";
    auto jsonObject = nlohmann::json::parse(text);
    EXPECT_THAT(tested(jsonObject, "field_name"), StatusIs(StatusCode::InvalidArgument));
}

/// @test Verify Parse* detects invalid types.
TEST(JsonUtilsTest, ParseIntegralInvalidType)
{
    CheckParseInvalidType<std::int32_t>(&JsonUtils::ParseInt);
    CheckParseInvalidType<std::uint32_t>(&JsonUtils::ParseUnsignedInt);
    CheckParseInvalidType<std::int64_t>(&JsonUtils::ParseLong);
    CheckParseInvalidType<std::uint64_t>(&JsonUtils::ParseUnsignedLong);
}

}  // namespace internal
}  // namespace csa
