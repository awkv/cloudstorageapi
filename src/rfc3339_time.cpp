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

#include "cloudstorageapi/internal/rfc3339_time.h"
#include "cloudstorageapi/status.h"
#include <array>
#include <cctype>
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace {

csa::Status ReportError(std::string const& timestamp, char const* msg)
{
    return csa::Status(csa::StatusCode::InvalidArgument,
                       std::string("Error parsing RFC 3339 timestamp: ") + msg +
                           " Valid format is YYYY-MM-DD[Tt]HH:MM:SS[.s+](Z|[+-]HH:MM), got=" + timestamp);
}

bool IsLeapYear(int year) { return (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)); }

namespace {
auto constexpr MonthsInYear = 12;
auto constexpr HoursInDay = 24;
auto constexpr MinutesInHour = std::chrono::seconds(std::chrono::minutes(1)).count();
auto constexpr SecondsInMinute = std::chrono::minutes(std::chrono::hours(1)).count();

}  // namespace

csa::StatusOrVal<std::chrono::system_clock::time_point> ParseDateTime(char const*& buffer, std::string const& timestamp)
{
    // Use std::mktime to compute the number of seconds because RFC 3339 requires
    // working with civil time, including the annoying leap seconds, and mktime
    // does.
    int year, month, day;
    char date_time_separator;
    int hours, minutes, seconds;

    int pos;
    auto count = std::sscanf(buffer, "%4d-%2d-%2d%c%2d:%2d:%2d%n", &year, &month, &day, &date_time_separator, &hours,
                             &minutes, &seconds, &pos);
    // All the fields up to this point have fixed width, so total width must be:
    auto constexpr ExpectedWidth = 19;
    auto constexpr ExpectedFields = 7;
    if (count != ExpectedFields || pos != ExpectedWidth)
    {
        return ReportError(timestamp,
                           "Invalid format for RFC 3339 timestamp detected while parsing"
                           " the base date and time portion.");
    }
    if (date_time_separator != 'T' && date_time_separator != 't')
    {
        return ReportError(timestamp, "Invalid date-time separator, expected 'T' or 't'.");
    }

    // Double braces are needed to workaround a clang-3.8 bug.
    std::array<int, MonthsInYear> constexpr MaxDaysInMonth{{
        31,  // January
        29,  // February (non-leap years checked below)
        31,  // March
        30,  // April
        31,  // May
        30,  // June
        31,  // July
        31,  // August
        30,  // September
        31,  // October
        30,  // November
        31,  // December
    }};
    auto constexpr kMkTimeBaseYear = 1900;
    if (month < 1 || month > MonthsInYear)
    {
        return ReportError(timestamp, "Out of range month.");
    }
    if (day < 1 || day > MaxDaysInMonth[month - 1])
    {
        return ReportError(timestamp, "Out of range day for given month.");
    }

    if (2 == month && day > MaxDaysInMonth[1] - 1 && !IsLeapYear(year))
    {
        return ReportError(timestamp, "Out of range day for given month.");
    }
    if (hours < 0 || hours >= HoursInDay)
    {
        return ReportError(timestamp, "Out of range hour.");
    }
    if (minutes < 0 || minutes >= MinutesInHour)
    {
        return ReportError(timestamp, "Out of range minute.");
    }
    // RFC-3339 points out that the seconds field can only assume value '60' for
    // leap seconds, so theoretically, we should validate that (furthermore, we
    // should valid that `seconds` is smaller than 59 for negative leap seconds).
    // This would require loading a table, and adds too much complexity for little
    // value.
    if (seconds < 0 || seconds > SecondsInMinute)
    {
        return ReportError(timestamp, "Out of range second.");
    }
    // Advance the pointer for all the characters read.
    buffer += pos;

    std::tm tm{};
    tm.tm_year = year - kMkTimeBaseYear;
    tm.tm_mon = month - 1;
    tm.tm_mday = day;
    tm.tm_hour = hours;
    tm.tm_min = minutes;
    tm.tm_sec = seconds;
    return std::chrono::system_clock::from_time_t(std::mktime(&tm));
}

csa::StatusOrVal<std::chrono::system_clock::duration> ParseFractionalSeconds(char const*& buffer,
                                                                             std::string const& timestamp)
{
    if (buffer[0] != '.')
    {
        return std::chrono::system_clock::duration(0);
    }
    ++buffer;

    long fractional_seconds;
    int pos;
    auto count = std::sscanf(buffer, "%9ld%n", &fractional_seconds, &pos);
    if (count != 1)
    {
        return ReportError(timestamp, "Invalid fractional seconds component.");
    }
    auto constexpr MaxNanosecondDigits = 9;
    auto constexpr NanosecondsBase = 10;
    // Normalize the fractional seconds to nanoseconds.
    for (int digits = pos; digits < MaxNanosecondDigits; ++digits)
    {
        fractional_seconds *= NanosecondsBase;
    }
    // Skip any other digits. This loses precision for sub-nanosecond timestamps,
    // but we do not consider this a problem for Internet timestamps.
    buffer += pos;
    while (std::isdigit(buffer[0]) != 0)
    {
        ++buffer;
    }
    return std::chrono::duration_cast<std::chrono::system_clock::duration>(
        std::chrono::nanoseconds(fractional_seconds));
}

csa::StatusOrVal<std::chrono::seconds> ParseOffset(char const*& buffer, std::string const& timestamp)
{
    if (buffer[0] == '+' || buffer[0] == '-')
    {
        bool positive = (buffer[0] == '+');
        ++buffer;
        // Parse the HH:MM offset.
        int hours, minutes, pos;
        auto count = std::sscanf(buffer, "%2d:%2d%n", &hours, &minutes, &pos);
        auto constexpr ExpectedOffsetWidth = 5;
        auto constexpr ExpectedOffsetFields = 2;
        if (count != ExpectedOffsetFields || pos != ExpectedOffsetWidth)
        {
            return ReportError(timestamp, "Invalid timezone offset, expected [+-]HH:MM.");
        }
        if (hours < 0 || hours >= HoursInDay)
        {
            return ReportError(timestamp, "Out of range offset hour.");
        }
        if (minutes < 0 || minutes >= MinutesInHour)
        {
            return ReportError(timestamp, "Out of range offset minute.");
        }
        buffer += pos;
        using std::chrono::duration_cast;
        if (positive)
        {
            return duration_cast<std::chrono::seconds>(std::chrono::hours(hours) + std::chrono::minutes(minutes));
        }
        return duration_cast<std::chrono::seconds>(-std::chrono::hours(hours) - std::chrono::minutes(minutes));
    }
    if (buffer[0] != 'Z' && buffer[0] != 'z')
    {
        return ReportError(timestamp, "Invalid timezone offset, expected 'Z' or 'z'.");
    }
    ++buffer;
    return std::chrono::seconds(0);
}

std::string FormatFractional(std::chrono::nanoseconds ns)
{
    if (ns.count() == 0)
    {
        return "";
    }

    using std::chrono::milliseconds;
    using std::chrono::nanoseconds;
    using std::chrono::seconds;
    auto constexpr MaxNanosecondsDigits = 9;
    auto constexpr BufferSize = 16;
    static_assert(BufferSize > (MaxNanosecondsDigits  // digits
                                + 1                   // period
                                + 1),                 // NUL terminator
                  "Buffer is not large enough for printing nanoseconds");
    auto constexpr NanosecondsPerMillisecond = nanoseconds(milliseconds(1)).count();
    auto constexpr MillisecondsPerSecond = milliseconds(seconds(1)).count();

    std::array<char, BufferSize> buffer{};

    // If the fractional seconds can be just expressed as milliseconds, do that,
    // we do not want to print 1.123000000
    auto d = std::lldiv(ns.count(), NanosecondsPerMillisecond);
    if (d.rem == 0)
    {
        std::snprintf(buffer.data(), buffer.size(), ".%03lld", d.quot);
        return buffer.data();
    }
    d = std::lldiv(ns.count(), MillisecondsPerSecond);
    if (d.rem == 0)
    {
        std::snprintf(buffer.data(), buffer.size(), ".%06lld", d.quot);
        return buffer.data();
    }

    std::snprintf(buffer.data(), buffer.size(), ".%09lld", static_cast<long long>(ns.count()));
    return buffer.data();
}

std::tm AsUtcTm(std::chrono::system_clock::time_point tp)
{
    std::time_t time = std::chrono::system_clock::to_time_t(tp);
    std::tm tm{};
    // The standard C++ function to convert time_t to a struct tm is not thread
    // safe (it holds global storage), use some OS specific stuff here:
#if _WIN32
    gmtime_s(&tm, &time);
#else
    gmtime_r(&time, &tm);
#endif  // _WIN32
    return tm;
}

}  // anonymous namespace

namespace csa {
namespace internal {

StatusOrVal<std::chrono::system_clock::time_point> ParseRfc3339(std::string const& timestamp)
{
    // TODO: dynamically change the timezone offset.
    // Because this computation is a bit expensive, assume the timezone offset
    // does not change during the lifetime of the program.  This function takes
    // the current time, converts to UTC and then convert back to a time_t.  The
    // difference is the offset.
    static std::chrono::seconds const LocalTimeOffset = []() {
        auto now = std::time(nullptr);
        std::tm lcl;
        // The standard C++ function to convert time_t to a struct tm is not thread
        // safe (it holds global storage), use some OS specific stuff here:
#if _WIN32
        gmtime_s(&lcl, &now);
#else
        gmtime_r(&now, &lcl);
#endif  // _WIN32
        return std::chrono::seconds(mktime(&lcl) - now);
    }();

    char const* buffer = timestamp.c_str();
    auto timePoint = ParseDateTime(buffer, timestamp);
    if (!timePoint)
        return std::move(timePoint).GetStatus();
    auto fractionalSeconds = ParseFractionalSeconds(buffer, timestamp);
    if (!fractionalSeconds)
        return std::move(fractionalSeconds).GetStatus();
    auto offset = ParseOffset(buffer, timestamp);
    if (!offset)
        return std::move(offset).GetStatus();

    if (buffer[0] != '\0')
    {
        return ReportError(timestamp, "Additional text after RFC 3339 date.");
    }

    *timePoint += *fractionalSeconds;
    *timePoint -= *offset;
    *timePoint -= LocalTimeOffset;
    return timePoint;
}

std::string FormatRfc3339(std::chrono::system_clock::time_point tp)
{
    auto constexpr TimestampFormatSize = 256;
    static_assert(TimestampFormatSize > ((4 + 1)    // YYYY-
                                         + (2 + 1)  // MM-
                                         + 2        // DD
                                         + 1        // T
                                         + (2 + 1)  // HH:
                                         + (2 + 1)  // MM:
                                         + 2        // SS
                                         + 1),      // Z
                  "Buffer size not large enough for YYYY-MM-DDTHH:MM:SSZ format");

    std::tm tm = AsUtcTm(tp);
    std::array<char, TimestampFormatSize> buffer{};
    std::strftime(buffer.data(), buffer.size(), "%Y-%m-%dT%H:%M:%S", &tm);

    std::string result(buffer.data());
    // Add the fractional seconds...
    auto duration = tp.time_since_epoch();
    using std::chrono::duration_cast;
    auto fractional = duration_cast<std::chrono::nanoseconds>(duration - duration_cast<std::chrono::seconds>(duration));
    result += FormatFractional(fractional);
    result += "Z";
    return result;
}

}  // namespace internal
}  // namespace csa
