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

#include "util/const_buffer_equality.h"
#include <gmock/gmock.h>

namespace csa {
namespace internal {
using csa::testing::util::Equal;

TEST(ConstBufferEqTest, ConstBufEquality)
{
    ConstBuffer a{"1", 1};
    ConstBuffer b{"1", 1};
    EXPECT_TRUE(Equal(a, b));
    ConstBuffer c{"2", 1};
    EXPECT_FALSE(Equal(a, c));
}

TEST(ConstBufferEqTest, ConstBufSequanceEquality)
{
    ConstBufferSequence a{
        ConstBuffer{"1", 1},
        ConstBuffer{"ab", 2},
        ConstBuffer{"ABC", 3},
    };
    ConstBufferSequence b{
        ConstBuffer{"1", 1},
        ConstBuffer{"ab", 2},
        ConstBuffer{"ABC", 3},
    };
    ConstBufferSequence c{
        ConstBuffer{"ab", 2},
        ConstBuffer{"ABC", 3},
    };
    EXPECT_TRUE(Equal(a, b));
    EXPECT_FALSE(Equal(a, c));
}
}  // namespace internal
}  // namespace csa
