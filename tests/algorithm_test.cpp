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

#include "cloudstorageapi/internal/algorithm.h"
#include <gmock/gmock.h>
#include <sstream>

namespace csa {
namespace internal {

using ::testing::ElementsAre;

TEST(AlgorithmTest, StrSplit)
{
    std::string empty;
    EXPECT_TRUE(StrSplit(empty, ',').empty());

    std::string str{"abc,def,ghi,jkl"};
    EXPECT_THAT(StrSplit(str, ','), ElementsAre("abc", "def", "ghi", "jkl"));

    std::string str2{"a1b1c1"};
    EXPECT_THAT(StrSplit(str2, '1'), ElementsAre("a", "b", "c"));
}

TEST(AlgorithmTest, StrJoin)
{
    std::vector<std::string> empty;
    EXPECT_TRUE(StrJoin(empty, ",").empty());

    std::vector<std::string> v{"abc", "def", "ghi", "jkl"};
    EXPECT_EQ(StrJoin(v, ", "), std::string{"abc, def, ghi, jkl"});
}

TEST(AlgorithmTest, Contains)
{
    std::string s = "abcde";
    EXPECT_TRUE(Contains(s, 'c'));
    EXPECT_FALSE(Contains(s, 'z'));

    std::string a[] = {"foo", "bar", "baz"};
    EXPECT_TRUE(Contains(a, "foo"));
    EXPECT_FALSE(Contains(a, "OOPS"));

    std::vector<std::string> v = {"foo", "bar", "baz"};
    EXPECT_TRUE(Contains(v, "foo"));
    EXPECT_FALSE(Contains(v, "OOPS"));
}

}  // namespace internal
}  // namespace csa
