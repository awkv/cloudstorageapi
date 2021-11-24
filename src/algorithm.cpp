// Copyright 2021 Andrew Karasyov
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cloudstorageapi/internal/algorithm.h"
#include <numeric>

namespace csa {
namespace internal {

std::vector<std::string> StrSplit(const std::string& str, char delim)
{
    std::vector<std::string> result;
    std::istringstream input{str};
    for (std::string line; std::getline(input, line, delim);)
    {
        result.emplace_back(std::move(line));
    }

    return result;
}

std::string StrJoin(const std::vector<std::string>& arr, const std::string& delim)
{
    if (arr.empty())
        return "";
    return std::accumulate(std::next(arr.begin()), arr.end(), *(arr.begin()),
                           [delim](auto res, const auto& s) { return std::move(res) + delim + s; });
}

}  // namespace internal
}  // namespace csa
