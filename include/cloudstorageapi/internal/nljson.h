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

/**
 * @file
 *
 * Include internal nlohmann/json headers but renaming the namespace to avoid
 * any dependency conflicts if user include another version of this library.
 *
 * This library always includes nlohmann/json through this header, and rename its top-level
 * namespace to avoid conflicts. Because nlohmann/json is a header-only library
 * no further action is needed.
 *
 * It is necessary to change original file and delete inline operators "".
 *
 * @see https://github.com/nlohmann/json.git
 */

// Remove the include guards because third-parties may have included their own
// version of nlohmann::json. This is safe because this file has include guard
// which is enough.

#undef INCLUDE_NLOHMANN_JSON_HPP_
#undef INCLUDE_NLOHMANN_JSON_FWD_HPP_

#define nlohmann csa_internal_nlohmann_3_7_0
#include "cloudstorageapi/internal/nlohmann_json.hpp"

// Remove other include guards so third-parties can include their own version of
// nlohmann::json.
#undef NLOHMANN_BASIC_JSON_TPL
#undef NLOHMANN_BASIC_JSON_TPL_DECLARATION
#undef INCLUDE_NLOHMANN_JSON_HPP_
#undef INCLUDE_NLOHMANN_JSON_FWD_HPP_
#undef NLOHMANN_JSON_SERIALIZE_ENUM
#undef NLOHMANN_JSON_VERSION_MAJOR
#undef NLOHMANN_JSON_VERSION_MINOR
#undef NLOHMANN_JSON_VERSION_PATCH

namespace nlohmann {
//
// Google Test uses PrintTo (with many overloads) to print the results of failed
// comparisons. Most of the time the default overloads for PrintTo() provided
// by Google Test magically do the job picking a good way to print things, but
// in this case there is a bug:
//     https://github.com/nlohmann/json/issues/709
// explicitly defining an overload, which must be in the namespace of the class,
// works around the problem, see ADL for why it must be in the right namespace:
//     https://en.wikipedia.org/wiki/Argument-dependent_name_lookup
//
/// Prints json objects to output streams from within Google Test.
inline void PrintTo(json const& j, std::ostream* os) { *os << j.dump(); }
}  // namespace nlohmann
#undef nlohmann

namespace csa {
namespace internal {
namespace nl = ::csa_internal_nlohmann_3_7_0;
}  // namespace internal
}  // namespace csa
