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

#include "cloudstorageapi/status.h"
#include "cloudstorageapi/status_or_val.h"
#include <gtest/gtest.h>

namespace csa {
namespace testing {
namespace internal {

// A unary predicate-formatter for csa::Status.
::testing::AssertionResult IsOkPredFormat(char const* expr,
                                        ::csa::Status const& status);

// A unary predicate-formatter for csa::StatusOrVal<T>.
template <typename T>
::testing::AssertionResult IsOkPredFormat(
    char const* expr, ::csa::StatusOrVal<T> const& status_or)
{
    if (status_or)
    {
        return ::testing::AssertionSuccess();
    }
    return IsOkPredFormat(expr, status_or.GetStatus());
}

}  // namespace internal
}  // namespace testing
}  // namespace csa

#define ASSERT_STATUS_OK(val) \
  ASSERT_PRED_FORMAT1(csa::testing::internal::IsOkPredFormat, val)

#define EXPECT_STATUS_OK(val) \
  EXPECT_PRED_FORMAT1(csa::testing::internal::IsOkPredFormat, val)
