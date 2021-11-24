// Copyright 2021 Andrew Karasyov
//
// Copyright 2021 Google LLC
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

#include "cloudstorageapi/options.h"
#include "cloudstorageapi/internal/log.h"
#include <iterator>
#include <set>
#include <unordered_map>

namespace csa {

namespace internal {

namespace {
// Returns true if the container @p c contains the value @p v.
template <typename C, typename V>
bool Contains(C&& c, V&& v)
{
    auto const e = std::end(std::forward<C>(c));
    return e != std::find(std::begin(std::forward<C>(c)), e, std::forward<V>(v));
}
}  // namespace

void CheckExpectedOptionsImpl(std::set<std::type_index> const& expected, Options const& opts, char const* const caller)
{
    for (auto const& p : opts.m_map)
    {
        if (!Contains(expected, p.first))
        {
            CSA_LOG_WARNING("{}: Unexpected option (mangled name): {}", caller, p.first.name());
        }
    }
}

Options MergeOptions(Options a, Options b)
{
    a.m_map.insert(std::make_move_iterator(b.m_map.begin()), std::make_move_iterator(b.m_map.end()));
    return a;
}

}  // namespace internal

}  // namespace csa
