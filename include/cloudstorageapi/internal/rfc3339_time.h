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
#include <chrono>
#include <string>

namespace csa {
namespace internal {

/**
 * Parses @p timestamp assuming it is in RFC-3339 format.
 *
 * Parse RFC-3339 timestamps and convert to `std::chrono::system_clock::time_point`,
 * the C++ class used to represent timestamps.  Depending on the underlying C++
 * implementation the timestamp may lose precision. C++ does not specify the
 * precision of the system clock, though most implementations have sub-second
 * precision, and nanoseconds is common. The RFC-3339 spec allows for arbitrary
 * precision in fractional seconds.
 *
 * @see https://tools.ietf.org/html/rfc3339
 */
StatusOrVal<std::chrono::system_clock::time_point> ParseRfc3339(std::string const& timestamp);

/**
 * Formats @p tp as a RFC-3339 timestamp.
 *
 * This function is used convert from `std::chrono::system_clock::time_point`
 * to the RFC-3339 format.
 *
 * There are many possible formats for RFC-3339 timestamps, this function always
 * uses YYYY-MM-DDTHH:MM:SS.FFFZ. The fractional seconds always represent the
 * full precision of `std::chrono::system_clock::time_point`. Note, however,
 * that C++ does not specify the minimum precision of these time points. In
 * most platforms time points have sub-second precision, and microseconds are
 * common.
 *
 * @see https://tools.ietf.org/html/rfc3339
 */
std::string FormatRfc3339(std::chrono::system_clock::time_point tp);

}  // namespace internal
}  // namespace csa
