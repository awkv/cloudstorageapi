// Copyright 2021 Andrew Karasyov
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

#include <algorithm>
#include <sstream>
#include <string>
#include <vector>

namespace csa {
namespace internal {

std::vector<std::string> StrSplit(const std::string& str, char delim);
std::string StrJoin(const std::vector<std::string>& arr, const std::string& delim);

template <typename C, typename V>
bool Contains(C&& c, V&& v)
{
    auto const e = std::end(std::forward<C>(c));
    return e != std::find(std::begin(std::forward<C>(c)), e, std::forward<V>(v));
}

}  // namespace internal
}  // namespace csa
