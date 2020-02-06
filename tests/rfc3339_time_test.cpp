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
#include <gmock/gmock.h>
#include <ctime>

namespace csa {
namespace internal {
namespace {

using ::std::chrono::duration_cast;
using ::std::chrono::milliseconds;
using ::std::chrono::nanoseconds;
using ::std::chrono::seconds;
using ::testing::HasSubstr;

// Test parsing

TEST(ParseRfc3339Test, ParseEpoch) {
  auto timestamp = ParseRfc3339("1970-01-01T00:00:00Z");
  // The C++ 11, 14 and 17 standards do not guarantee that the system clock's
  // epoch is actually the same as the Unix Epoch. Luckily, the platforms we
  // support actually have that property, and C++ 20 fixes things. If this test
  // breaks because somebody is unlucky enough to have a C++ compiler + library
  // with a different Epoch then at least we know what the problem is.
  EXPECT_EQ(0, timestamp.time_since_epoch().count())
      << "Your C++ compiler, library, and/or your operating system does\n"
      << "not use the Unix epoch (1970-01-01T00:00:00Z) as its epoch.\n"
      << "The cloudstorageapi library doesn't support this case.\n"
      << "Please let developers know at https://github.com/awkv/cloudstorageapi.";
}

TEST(ParseRfc3339Test, ParseSimpleZulu)
{
    auto timestamp = ParseRfc3339("2018-05-18T14:42:03Z");
    // Use `date -u +%s --date='2018-05-18T14:42:03'` to get the magic value:
    EXPECT_EQ(1526654523L,
        duration_cast<seconds>(timestamp.time_since_epoch()).count());
}

TEST(ParseRfc3339Test, ParseAlternativeSeparators)
{
    auto timestamp = ParseRfc3339("2018-05-18t14:42:03z");
    // Use `date -u +%s --date='2018-05-18T14:42:03'` to get the magic value:
    EXPECT_EQ(1526654523L,
        duration_cast<seconds>(timestamp.time_since_epoch()).count());
}

TEST(ParseRfc3339Test, ParseFractional)
{
    auto timestamp = ParseRfc3339("2018-05-18T14:42:03.123456789Z");
    // Use `date -u +%s --date='2018-05-18T14:42:03'` to get the magic value:
    auto actual_seconds = duration_cast<seconds>(timestamp.time_since_epoch());
    EXPECT_EQ(1526654523L, actual_seconds.count());

    bool const system_clock_has_nanos = std::ratio_greater_equal<
        std::nano, std::chrono::system_clock::duration::period>::value;
    if (system_clock_has_nanos)
    {
        auto actual_nanoseconds = duration_cast<nanoseconds>(
            timestamp.time_since_epoch() - actual_seconds);
        EXPECT_EQ(123456789L, actual_nanoseconds.count());
    }
    else
    {
        // On platforms where the system clock has less than nanosecond precision
        // just check for milliseconds, we could check at the highest possible
        // precision but that is overkill.
        auto actual_milliseconds = duration_cast<milliseconds>(
            timestamp.time_since_epoch() - actual_seconds);
        EXPECT_EQ(123L, actual_milliseconds.count());
    }
}

TEST(ParseRfc3339Test, ParseFractionalMoreThanNanos)
{
    auto timestamp = ParseRfc3339("2018-05-18T14:42:03.1234567890123Z");
    // Use `date -u +%s --date='2018-05-18T14:42:03'` to get the magic value:
    auto actual_seconds = duration_cast<seconds>(timestamp.time_since_epoch());
    EXPECT_EQ(1526654523L, actual_seconds.count());
    bool const system_clock_has_nanos = std::ratio_greater_equal<
        std::nano, std::chrono::system_clock::duration::period>::value;
    if (system_clock_has_nanos)
    {
        auto actual_nanoseconds = duration_cast<nanoseconds>(
            timestamp.time_since_epoch() - actual_seconds);
        EXPECT_EQ(123456789L, actual_nanoseconds.count());
    }
    else
    {
        // On platforms where the system clock has less than nanosecond precision
        // just check for milliseconds, we could check at the highest possible
        // precision but that is overkill.
        auto actual_milliseconds = duration_cast<milliseconds>(
            timestamp.time_since_epoch() - actual_seconds);
        EXPECT_EQ(123L, actual_milliseconds.count());
    }
}

TEST(ParseRfc3339Test, ParseFractionalLessThanNanos)
{
    auto timestamp = ParseRfc3339("2018-05-18T14:42:03.123456Z");
    // Use `date -u +%s --date='2018-05-18T14:42:03'` to get the magic value:
    auto actual_seconds = duration_cast<seconds>(timestamp.time_since_epoch());
    EXPECT_EQ(1526654523L, actual_seconds.count());
    auto actual_nanoseconds =
        duration_cast<nanoseconds>(timestamp.time_since_epoch() - actual_seconds);
    EXPECT_EQ(123456000L, actual_nanoseconds.count());
}

TEST(ParseRfc3339Test, ParseWithOffset)
{
    auto timestamp = ParseRfc3339("2018-05-18T14:42:03+08:00");
    // Use `date -u +%s --date='2018-05-18T14:42:03+08:00'` to get the magic
    // value.
    auto actual_seconds = duration_cast<seconds>(timestamp.time_since_epoch());
    EXPECT_EQ(1526625723L, actual_seconds.count());
}

TEST(ParseRfc3339Test, ParseFull)
{
    auto timestamp = ParseRfc3339("2018-05-18T14:42:03.5-01:05");
    // Use `date -u +%s --date='2018-05-18T14:42:03.5-01:05'` to get the magic
    // value.
    auto actual_seconds = duration_cast<seconds>(timestamp.time_since_epoch());
    EXPECT_EQ(1526658423L, actual_seconds.count());
    auto actual_milliseconds = duration_cast<milliseconds>(
        timestamp.time_since_epoch() - actual_seconds);
    EXPECT_EQ(500, actual_milliseconds.count());
}

TEST(ParseRfc3339Test, DetectInvalidSeparator)
{
    EXPECT_THROW(ParseRfc3339("2018-05-18x14:42:03Z"), std::invalid_argument);
    EXPECT_THROW(ParseRfc3339("2018-05-18T14:42:03x"), std::invalid_argument);
}

TEST(ParseRfc3339Test, DetectLongYear)
{
    EXPECT_THROW(ParseRfc3339("52018-05-18T14:42:03Z"), std::invalid_argument);
}

TEST(ParseRfc3339Test, DetectShortYear)
{
    EXPECT_THROW(ParseRfc3339("218-05-18T14:42:03Z"), std::invalid_argument);
}

TEST(ParseRfc3339Test, DetectLongMonth)
{
    EXPECT_THROW(ParseRfc3339("2018-123-18T14:42:03Z"), std::invalid_argument);
}

TEST(ParseRfc3339Test, DetectShortMonth)
{
    EXPECT_THROW(ParseRfc3339("2018-1-18T14:42:03Z"), std::invalid_argument);
}

TEST(ParseRfc3339Test, DetectOutOfRangeMonth)
{
    EXPECT_THROW(ParseRfc3339("2018-33-18T14:42:03Z"), std::invalid_argument);
}

TEST(ParseRfc3339Test, DetectLongMDay)
{
    EXPECT_THROW(ParseRfc3339("2018-05-181T14:42:03Z"), std::invalid_argument);
}

TEST(ParseRfc3339Test, DetectShortMDay)
{
    EXPECT_THROW(ParseRfc3339("2018-05-1T14:42:03Z"), std::invalid_argument);
}

TEST(ParseRfc3339Test, DetectOutOfRangeMDay)
{
    EXPECT_THROW(ParseRfc3339("2018-05-55T14:42:03Z"), std::invalid_argument);
}

TEST(ParseRfc3339Test, DetectOutOfRangeMDay30)
{
    EXPECT_THROW(ParseRfc3339("2018-06-31T14:42:03Z"), std::invalid_argument);
}

TEST(ParseRfc3339Test, DetectOutOfRangeMDayFebLeap)
{
    EXPECT_THROW(ParseRfc3339("2016-02-30T14:42:03Z"), std::invalid_argument);
}

TEST(ParseRfc3339Test, DetectOutOfRangeMDayFebNonLeap)
{
    EXPECT_THROW(ParseRfc3339("2017-02-29T14:42:03Z"), std::invalid_argument);
}

TEST(ParseRfc3339Test, DetectLongHour)
{
    EXPECT_THROW(ParseRfc3339("2018-05-18T144:42:03Z"), std::invalid_argument);
}

TEST(ParseRfc3339Test, DetectShortHour)
{
    EXPECT_THROW(ParseRfc3339("2018-05-18T1:42:03Z"), std::invalid_argument);
}

TEST(ParseRfc3339Test, DetectOutOfRangeHour)
{
    EXPECT_THROW(ParseRfc3339("2018-05-18T24:42:03Z"), std::invalid_argument);
}

TEST(ParseRfc3339Test, DetectLongMinute)
{
    EXPECT_THROW(ParseRfc3339("2018-05-18T14:442:03Z"), std::invalid_argument);
}

TEST(ParseRfc3339Test, DetectShortMinute)
{
    EXPECT_THROW(ParseRfc3339("2018-05-18T14:2:03Z"), std::invalid_argument);
}

TEST(ParseRfc3339Test, DetectOutOfRangeMinute)
{
    EXPECT_THROW(ParseRfc3339("2018-05-18T22:60:03Z"), std::invalid_argument);
}

TEST(ParseRfc3339Test, DetectLongSecond)
{
    EXPECT_THROW(ParseRfc3339("2018-05-18T14:42:003Z"), std::invalid_argument);
}

TEST(ParseRfc3339Test, DetectShortSecond)
{
    EXPECT_THROW(ParseRfc3339("2018-05-18T14:42:3Z"), std::invalid_argument);
}

TEST(ParseRfc3339Test, DetectOutOfRangeSecond)
{
    EXPECT_THROW(ParseRfc3339("2018-05-18T22:42:61Z"), std::invalid_argument);
}

TEST(ParseRfc3339Test, DetectLongOffsetHour)
{
    EXPECT_THROW(ParseRfc3339("2018-05-18T14:42:03+008:00"),
               std::invalid_argument);
}

TEST(ParseRfc3339Test, DetectShortOffsetHour)
{
    EXPECT_THROW(ParseRfc3339("2018-05-18T14:42:03+8:00"), std::invalid_argument);
}

TEST(ParseRfc3339Test, DetectOutOfRangeOffsetHour)
{
    EXPECT_THROW(ParseRfc3339("2018-05-18T14:42:03+24:00"),
               std::invalid_argument);
}

TEST(ParseRfc3339Test, DetectLongOffsetMinute)
{
    EXPECT_THROW(ParseRfc3339("2018-05-18T14:42:03+08:001"),
               std::invalid_argument);
}

TEST(ParseRfc3339Test, DetectShortOffsetMinute)
{
    EXPECT_THROW(ParseRfc3339("2018-05-18T14:42:03+08:1"), std::invalid_argument);
}

TEST(ParseRfc3339Test, DetectOutOfRangeOffsetMinute)
{
    EXPECT_THROW(ParseRfc3339("2018-05-18T14:42:03+08:60"),
               std::invalid_argument);
}

// Test formatting

TEST(FormatRfc3339Test, NoFractional)
{
    auto timestamp = ParseRfc3339("2018-08-02T01:02:03Z");
    std::string actual = FormatRfc3339(timestamp);
    EXPECT_EQ("2018-08-02T01:02:03Z", actual);
}

TEST(FormatRfc3339Test, FractionalMillis)
{
    auto timestamp = ParseRfc3339("2018-08-02T01:02:03.123Z");
    std::string actual = FormatRfc3339(timestamp);
    EXPECT_EQ("2018-08-02T01:02:03.123Z", actual);
}

TEST(FormatRfc3339Test, FractionalMillsSmall)
{
    auto timestamp = ParseRfc3339("2018-08-02T01:02:03.001Z");
    std::string actual = FormatRfc3339(timestamp);
    EXPECT_EQ("2018-08-02T01:02:03.001Z", actual);
}

TEST(FormatRfc3339Test, FractionalMicros)
{
    auto timestamp = ParseRfc3339("2018-08-02T01:02:03.123456Z");
    std::string actual = FormatRfc3339(timestamp);

    bool system_clock_has_micros = std::ratio_greater_equal<
        std::micro, std::chrono::system_clock::duration::period>::value;
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
    std::string actual = FormatRfc3339(timestamp);

    bool system_clock_has_nanos = std::ratio_greater_equal<
        std::nano, std::chrono::system_clock::duration::period>::value;
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
