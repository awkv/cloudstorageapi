// Copyright 2020 Andrew Karasyov
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

#include "cloudstorageapi/internal/rfc3339_time.h"
#include "testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <ctime>

namespace csa {
namespace internal {
namespace {

using ::csa::testing::util::IsOk;
using ::csa::testing::util::StatusIs;
using ::std::chrono::duration_cast;
using ::std::chrono::milliseconds;
using ::std::chrono::nanoseconds;
using ::std::chrono::seconds;
using ::testing::HasSubstr;

// Test parsing

TEST(ParseRfc3339Test, ParseEpoch)
{
    auto timestamp = ParseRfc3339("1970-01-01T00:00:00Z");
    EXPECT_STATUS_OK(timestamp);
    // The C++ 11, 14 and 17 standards do not guarantee that the system clock's
    // epoch is actually the same as the Unix Epoch. Luckily, the platforms we
    // support actually have that property, and C++ 20 fixes things. If this test
    // breaks because somebody is unlucky enough to have a C++ compiler + library
    // with a different Epoch then at least we know what the problem is.
    EXPECT_EQ(0, (*timestamp).time_since_epoch().count())
        << "Your C++ compiler, library, and/or your operating system does\n"
        << "not use the Unix epoch (1970-01-01T00:00:00Z) as its epoch.\n"
        << "The cloudstorageapi library doesn't support this case.\n"
        << "Please let developers know at https://github.com/awkv/cloudstorageapi.";
}

TEST(ParseRfc3339Test, ParseSimpleZulu)
{
    struct Timestamps
    {
        std::string input;
        std::time_t expected;
    } tests[] = {
        // Use `date -u +%s --date='....'` to get the expected values.
        {"2018-05-18T14:42:03Z", 1526654523L}, {"2020-01-01T00:00:00Z", 1577836800L},
        {"2020-01-31T00:00:00Z", 1580428800L}, {"2020-02-29T00:00:00Z", 1582934400L},
        {"2020-03-31T00:00:00Z", 1585612800L}, {"2020-04-30T00:00:00Z", 1588204800L},
        {"2020-05-31T00:00:00Z", 1590883200L}, {"2020-06-30T00:00:00Z", 1593475200L},
        {"2020-07-31T00:00:00Z", 1596153600L}, {"2020-08-31T00:00:00Z", 1598832000L},
        {"2020-09-30T00:00:00Z", 1601424000L}, {"2020-10-31T00:00:00Z", 1604102400L},
        {"2020-11-20T00:00:00Z", 1605830400L}, {"2020-12-31T00:00:00Z", 1609372800L},
        {"2020-01-01T00:00:59Z", 1577836859L}, {"2020-01-01T00:59:59Z", 1577840399L},
        {"2020-01-01T23:59:59Z", 1577923199L},
    };
    for (auto const& test : tests)
    {
        auto timestamp = ParseRfc3339(test.input);
        EXPECT_STATUS_OK(timestamp);
        auto actual = std::chrono::system_clock::to_time_t(*timestamp);
        EXPECT_EQ(actual, test.expected) << " when testing with input=" << test.input;
    }
}

TEST(ParseRfc3339Test, ParseAlternativeSeparators)
{
    auto timestamp = ParseRfc3339("2018-05-18t14:42:03z");
    EXPECT_STATUS_OK(timestamp);
    // Use `date -u +%s --date='2018-05-18T14:42:03'` to get the magic value:
    EXPECT_EQ(1526654523L, duration_cast<seconds>((*timestamp).time_since_epoch()).count());
}

TEST(ParseRfc3339Test, ParseFractional)
{
    auto timestamp = ParseRfc3339("2018-05-18T14:42:03.123456789Z");
    EXPECT_STATUS_OK(timestamp);
    // Use `date -u +%s --date='2018-05-18T14:42:03'` to get the magic value:
    auto actual_seconds = duration_cast<seconds>((*timestamp).time_since_epoch());
    EXPECT_EQ(1526654523L, actual_seconds.count());

    bool const system_clock_has_nanos =
        std::ratio_greater_equal<std::nano, std::chrono::system_clock::duration::period>::value;
    if (system_clock_has_nanos)
    {
        auto actual_nanoseconds = duration_cast<nanoseconds>((*timestamp).time_since_epoch() - actual_seconds);
        EXPECT_EQ(123456789L, actual_nanoseconds.count());
    }
    else
    {
        // On platforms where the system clock has less than nanosecond precision
        // just check for milliseconds, we could check at the highest possible
        // precision but that is overkill.
        auto actual_milliseconds = duration_cast<milliseconds>((*timestamp).time_since_epoch() - actual_seconds);
        EXPECT_EQ(123L, actual_milliseconds.count());
    }
}

TEST(ParseRfc3339Test, ParseFractionalMoreThanNanos)
{
    auto timestamp = ParseRfc3339("2018-05-18T14:42:03.1234567890123Z");
    EXPECT_STATUS_OK(timestamp);
    // Use `date -u +%s --date='2018-05-18T14:42:03'` to get the magic value:
    auto actual_seconds = duration_cast<seconds>((*timestamp).time_since_epoch());
    EXPECT_EQ(1526654523L, actual_seconds.count());
    bool const system_clock_has_nanos =
        std::ratio_greater_equal<std::nano, std::chrono::system_clock::duration::period>::value;

    if (system_clock_has_nanos)
    {
        auto actual_nanoseconds = duration_cast<nanoseconds>((*timestamp).time_since_epoch() - actual_seconds);
        EXPECT_EQ(123456789L, actual_nanoseconds.count());
    }
    else
    {
        // On platforms where the system clock has less than nanosecond precision
        // just check for milliseconds, we could check at the highest possible
        // precision but that is overkill.
        auto actual_milliseconds = duration_cast<milliseconds>((*timestamp).time_since_epoch() - actual_seconds);
        EXPECT_EQ(123L, actual_milliseconds.count());
    }
}

TEST(ParseRfc3339Test, ParseFractionalLessThanNanos)
{
    auto timestamp = ParseRfc3339("2018-05-18T14:42:03.123456Z");
    EXPECT_STATUS_OK(timestamp);
    // Use `date -u +%s --date='2018-05-18T14:42:03'` to get the magic value:
    auto actual_seconds = duration_cast<seconds>((*timestamp).time_since_epoch());
    EXPECT_EQ(1526654523L, actual_seconds.count());
    auto actual_nanoseconds = duration_cast<nanoseconds>((*timestamp).time_since_epoch() - actual_seconds);
    EXPECT_EQ(123456000L, actual_nanoseconds.count());
}

TEST(ParseRfc3339Test, ParseWithOffset)
{
    auto timestamp = ParseRfc3339("2018-05-18T14:42:03+08:00");
    EXPECT_STATUS_OK(timestamp);
    // Use `date -u +%s --date='2018-05-18T14:42:03+08:00'` to get the magic
    // value.
    auto actual_seconds = duration_cast<seconds>((*timestamp).time_since_epoch());
    EXPECT_EQ(1526625723L, actual_seconds.count());
}

TEST(ParseRfc3339Test, ParseFull)
{
    auto timestamp = ParseRfc3339("2018-05-18T14:42:03.5-01:05");
    EXPECT_STATUS_OK(timestamp);
    // Use `date -u +%s --date='2018-05-18T14:42:03.5-01:05'` to get the magic
    // value.
    auto actual_seconds = duration_cast<seconds>((*timestamp).time_since_epoch());
    EXPECT_EQ(1526658423L, actual_seconds.count());
    auto actual_milliseconds = duration_cast<milliseconds>((*timestamp).time_since_epoch() - actual_seconds);
    EXPECT_EQ(500, actual_milliseconds.count());
}

TEST(ParseRfc3339Test, DetectInvalidSeparator)
{
    EXPECT_THAT(ParseRfc3339("2018-05-18x14:42:03Z"), StatusIs(StatusCode::InvalidArgument));
    EXPECT_THAT(ParseRfc3339("2018-05-18T14:42:03x"), StatusIs(StatusCode::InvalidArgument));
}

TEST(ParseRfc3339Test, DetectLongYear)
{
    EXPECT_THAT(ParseRfc3339("52018-05-18T14:42:03Z"), StatusIs(StatusCode::InvalidArgument));
}

TEST(ParseRfc3339Test, DetectShortYear)
{
    EXPECT_THAT(ParseRfc3339("218-05-18T14:42:03Z"), StatusIs(StatusCode::InvalidArgument));
}

TEST(ParseRfc3339Test, DetectLongMonth)
{
    EXPECT_THAT(ParseRfc3339("2018-123-18T14:42:03Z"), StatusIs(StatusCode::InvalidArgument));
}

TEST(ParseRfc3339Test, DetectShortMonth)
{
    EXPECT_THAT(ParseRfc3339("2018-1-18T14:42:03Z"), StatusIs(StatusCode::InvalidArgument));
}

TEST(ParseRfc3339Test, DetectOutOfRangeMonth)
{
    EXPECT_THAT(ParseRfc3339("2018-33-18T14:42:03Z"), StatusIs(StatusCode::InvalidArgument));
}

TEST(ParseRfc3339Test, DetectLongMDay)
{
    EXPECT_THAT(ParseRfc3339("2018-05-181T14:42:03Z"), StatusIs(StatusCode::InvalidArgument));
}

TEST(ParseRfc3339Test, DetectShortMDay)
{
    EXPECT_THAT(ParseRfc3339("2018-05-1T14:42:03Z"), StatusIs(StatusCode::InvalidArgument));
}

TEST(ParseRfc3339Test, DetectOutOfRangeMDay)
{
    EXPECT_THAT(ParseRfc3339("2018-05-55T14:42:03Z"), StatusIs(StatusCode::InvalidArgument));
}

TEST(ParseRfc3339Test, DetectOutOfRangeMDay30)
{
    EXPECT_THAT(ParseRfc3339("2018-06-31T14:42:03Z"), StatusIs(StatusCode::InvalidArgument));
}

TEST(ParseRfc3339Test, DetectOutOfRangeMDayFebLeap)
{
    EXPECT_THAT(ParseRfc3339("2016-02-30T14:42:03Z"), StatusIs(StatusCode::InvalidArgument));
}

TEST(ParseRfc3339Test, DetectOutOfRangeMDayFebNonLeap)
{
    EXPECT_THAT(ParseRfc3339("2017-02-29T14:42:03Z"), StatusIs(StatusCode::InvalidArgument));
}

TEST(ParseRfc3339Test, DetectLongHour)
{
    EXPECT_THAT(ParseRfc3339("2018-05-18T144:42:03Z"), StatusIs(StatusCode::InvalidArgument));
}

TEST(ParseRfc3339Test, DetectShortHour)
{
    EXPECT_THAT(ParseRfc3339("2018-05-18T1:42:03Z"), StatusIs(StatusCode::InvalidArgument));
}

TEST(ParseRfc3339Test, DetectOutOfRangeHour)
{
    EXPECT_THAT(ParseRfc3339("2018-05-18T24:42:03Z"), StatusIs(StatusCode::InvalidArgument));
}

TEST(ParseRfc3339Test, DetectLongMinute)
{
    EXPECT_THAT(ParseRfc3339("2018-05-18T14:442:03Z"), StatusIs(StatusCode::InvalidArgument));
}

TEST(ParseRfc3339Test, DetectShortMinute)
{
    EXPECT_THAT(ParseRfc3339("2018-05-18T14:2:03Z"), StatusIs(StatusCode::InvalidArgument));
}

TEST(ParseRfc3339Test, DetectOutOfRangeMinute)
{
    EXPECT_THAT(ParseRfc3339("2018-05-18T22:60:03Z"), StatusIs(StatusCode::InvalidArgument));
}

TEST(ParseRfc3339Test, DetectLongSecond)
{
    EXPECT_THAT(ParseRfc3339("2018-05-18T14:42:003Z"), StatusIs(StatusCode::InvalidArgument));
}

TEST(ParseRfc3339Test, DetectShortSecond)
{
    EXPECT_THAT(ParseRfc3339("2018-05-18T14:42:3Z"), StatusIs(StatusCode::InvalidArgument));
}

TEST(ParseRfc3339Test, DetectOutOfRangeSecond)
{
    EXPECT_THAT(ParseRfc3339("2018-05-18T22:42:61Z"), StatusIs(StatusCode::InvalidArgument));
}

TEST(ParseRfc3339Test, DetectLongOffsetHour)
{
    EXPECT_THAT(ParseRfc3339("2018-05-18T14:42:03+008:00"), StatusIs(StatusCode::InvalidArgument));
}

TEST(ParseRfc3339Test, DetectShortOffsetHour)
{
    EXPECT_THAT(ParseRfc3339("2018-05-18T14:42:03+8:00"), StatusIs(StatusCode::InvalidArgument));
}

TEST(ParseRfc3339Test, DetectOutOfRangeOffsetHour)
{
    EXPECT_THAT(ParseRfc3339("2018-05-18T14:42:03+24:00"), StatusIs(StatusCode::InvalidArgument));
}

TEST(ParseRfc3339Test, DetectLongOffsetMinute)
{
    EXPECT_THAT(ParseRfc3339("2018-05-18T14:42:03+08:001"), StatusIs(StatusCode::InvalidArgument));
}

TEST(ParseRfc3339Test, DetectShortOffsetMinute)
{
    EXPECT_THAT(ParseRfc3339("2018-05-18T14:42:03+08:1"), StatusIs(StatusCode::InvalidArgument));
}

TEST(ParseRfc3339Test, DetectOutOfRangeOffsetMinute)
{
    EXPECT_THAT(ParseRfc3339("2018-05-18T14:42:03+08:60"), StatusIs(StatusCode::InvalidArgument));
}

// Test formatting

TEST(FormatRfc3339Test, NoFractional)
{
    auto timestamp = ParseRfc3339("2018-08-02T01:02:03Z");
    EXPECT_STATUS_OK(timestamp);
    std::string actual = FormatRfc3339(*timestamp);
    EXPECT_EQ("2018-08-02T01:02:03Z", actual);
}

TEST(FormatRfc3339Test, FractionalMillis)
{
    auto timestamp = ParseRfc3339("2018-08-02T01:02:03.123Z");
    EXPECT_STATUS_OK(timestamp);
    std::string actual = FormatRfc3339(*timestamp);
    EXPECT_EQ("2018-08-02T01:02:03.123Z", actual);
}

TEST(FormatRfc3339Test, FractionalMillsSmall)
{
    auto timestamp = ParseRfc3339("2018-08-02T01:02:03.001Z");
    EXPECT_STATUS_OK(timestamp);
    std::string actual = FormatRfc3339(*timestamp);
    EXPECT_EQ("2018-08-02T01:02:03.001Z", actual);
}

TEST(FormatRfc3339Test, FractionalMicros)
{
    auto timestamp = ParseRfc3339("2018-08-02T01:02:03.123456Z");
    EXPECT_STATUS_OK(timestamp);
    std::string actual = FormatRfc3339(*timestamp);

    bool system_clock_has_micros =
        std::ratio_greater_equal<std::micro, std::chrono::system_clock::duration::period>::value;
    if (system_clock_has_micros)
    {
        EXPECT_EQ("2018-08-02T01:02:03.123456Z", actual);
    }
    else
    {
        // On platforms where the system clock has less than microsecond precision
        // just check for milliseconds.
        EXPECT_THAT(actual, HasSubstr("2018-08-02T01:02:03.123"));
    }
}

TEST(FormatRfc3339Test, FractionalNanos)
{
    auto timestamp = ParseRfc3339("2018-08-02T01:02:03.123456789Z");
    EXPECT_STATUS_OK(timestamp);
    std::string actual = FormatRfc3339(*timestamp);

    bool system_clock_has_nanos =
        std::ratio_greater_equal<std::nano, std::chrono::system_clock::duration::period>::value;
    if (system_clock_has_nanos)
    {
        EXPECT_EQ("2018-08-02T01:02:03.123456789Z", actual);
    }
    else
    {
        // On platforms where the system clock has less than nanosecond precision
        // just check for milliseconds.
        EXPECT_THAT(actual, HasSubstr("2018-08-02T01:02:03.123"));
    }
}

}  // namespace
}  // namespace internal
}  // namespace csa
