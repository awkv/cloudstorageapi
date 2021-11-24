// Copyright 2021 Andrew Karasyov
//
// Copyright 2020 Google LLC
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

#include "cloudstorageapi/internal/complex_option.h"
#include <gmock/gmock.h>

namespace csa {
namespace internal {

struct PlaceholderOption : public ComplexOption<PlaceholderOption, int>
{
    using ComplexOption<PlaceholderOption, int>::ComplexOption;
};

TEST(ComplexOptionTest, ValueOrEmptyCase)
{
    PlaceholderOption d;
    ASSERT_FALSE(d.HasValue());
    EXPECT_EQ(5, d.ValueOr(5));
}

TEST(ComplexOptionTest, ValueOrNonEmptyCase)
{
    PlaceholderOption d(10);
    ASSERT_TRUE(d.HasValue());
    EXPECT_EQ(10, d.ValueOr(5));
}

}  // namespace internal
}  // namespace csa
