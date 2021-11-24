// Copyright 2021 Andrew Karasyov
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

#include "cloudstorageapi/well_known_headers.h"
#include <gmock/gmock.h>

namespace csa {
namespace {

using ::testing::HasSubstr;

/// @test Verify that CustomHeader works as expected.
TEST(WellKnownHeader, CustomHeader)
{
    CustomHeader header("my-custom-header", "do-stuff");
    std::ostringstream os;
    os << header;
    EXPECT_THAT(os.str(), HasSubstr("do-stuff"));
    EXPECT_THAT(os.str(), HasSubstr("my-custom-header"));
}

TEST(WellKnownHeader, ValueOrEmptyCase)
{
    CustomHeader header;
    ASSERT_FALSE(header.HasValue());
    EXPECT_EQ("foo", header.ValueOr("foo"));
}

TEST(WellKnownHeader, ValueOrNonEmptyCase)
{
    CustomHeader header("header", "value");
    ASSERT_TRUE(header.HasValue());
    EXPECT_EQ("value", header.ValueOr("foo"));
}

}  // namespace
}  // namespace csa
