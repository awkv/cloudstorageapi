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

#include "cloudstorageapi/internal/const_buffer.h"
#include <gtest/gtest.h>

namespace csa {
namespace testing {
namespace util {

inline bool Equal(csa::internal::ConstBuffer const& a, csa::internal::ConstBuffer const& b)
{
    return std::equal(a.begin(), a.end(), b.begin(), b.end());
}

inline bool Equal(csa::internal::ConstBufferSequence const& a, csa::internal::ConstBufferSequence const& b)
{
    if (a.size() == b.size())
    {
        auto aIt = a.begin();
        auto bIt = b.begin();
        while (aIt != a.end())
        {
            if (!Equal(*aIt++, *bIt++))
                return false;
        }

        return true;
    }

    return false;
}
}  // namespace util
}  // namespace testing
}  // namespace csa
