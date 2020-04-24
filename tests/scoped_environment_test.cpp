// Copyright 2020 Andrew Karasyov
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

#include "util/scoped_environment.h"
#include "cloudstorageapi/internal/utils.h"
#include <gtest/gtest.h>

namespace csa {
namespace testing {
namespace internal {
auto constexpr VarName = "SCOPED_ENVIRONMENT_TEST";

TEST(ScopedEnvironment, SetOverSet)
{
    ScopedEnvironment envOuter(VarName, "foo");
    EXPECT_EQ(*csa::internal::GetEnv(VarName), "foo");
    {
        ScopedEnvironment envInner(VarName, "bar");
        EXPECT_EQ(*csa::internal::GetEnv(VarName), "bar");
    }
    EXPECT_EQ(*csa::internal::GetEnv(VarName), "foo");
}

TEST(ScopedEnvironment, SetOverUnset)
{
    ScopedEnvironment envOuter(VarName, {});
    EXPECT_FALSE(csa::internal::GetEnv(VarName).has_value());
    {
        ScopedEnvironment envInner(VarName, "bar");
        EXPECT_EQ(*csa::internal::GetEnv(VarName), "bar");
    }
    EXPECT_FALSE(csa::internal::GetEnv(VarName).has_value());
}

TEST(ScopedEnvironment, UnsetOverSet)
{
    ScopedEnvironment envOuter(VarName, "foo");
    EXPECT_EQ(*csa::internal::GetEnv(VarName), "foo");
    {
        ScopedEnvironment envInner(VarName, {});
        EXPECT_FALSE(csa::internal::GetEnv(VarName).has_value());
    }
    EXPECT_EQ(*csa::internal::GetEnv(VarName), "foo");
}
}  // namespace internal
}  // namespace testing
}  // namespace csa
