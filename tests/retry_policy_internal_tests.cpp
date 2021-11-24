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

#include "cloudstorageapi/internal/retry_policy_internal.h"
#include "cloudstorageapi/status.h"
#include "util/check_predicate_becomes_false.h"
#include <gmock/gmock.h>

namespace csa {
namespace internal {

struct TestRetryablePolicy
{
    static bool IsPermanentFailure(csa::Status const& s)
    {
        return !s.Ok() && (s.Code() == csa::StatusCode::PermissionDenied);
    }
};

Status CreateTransientError() { return Status(StatusCode::Unavailable, ""); }
Status CreatePermanentError() { return Status(StatusCode::PermissionDenied, ""); }

using RetryPolicyForTest = csa::internal::TraitBasedRetryPolicy<TestRetryablePolicy>;
using LimitedTimeRetryPolicyForTest = csa::internal::LimitedTimeRetryPolicy<TestRetryablePolicy>;
using LimitedErrorCountRetryPolicyForTest = csa::internal::LimitedErrorCountRetryPolicy<TestRetryablePolicy>;

auto const LimitedTimeTestPeriod = std::chrono::milliseconds(50);
auto const LimitedTimeTolerance = std::chrono::milliseconds(10);

/**
 * @test Verify that a retry policy configured to run for 50ms works correctly.
 *
 * This eliminates some amount of code duplication in the following tests.
 */
void CheckLimitedTime(RetryPolicy& tested)
{
    csa::testing::internal::CheckPredicateBecomesFalse([&tested] { return tested.OnFailure(CreateTransientError()); },
                                                       std::chrono::system_clock::now() + LimitedTimeTestPeriod,
                                                       LimitedTimeTolerance);
}

/// @test A simple test for the LimitedTimeRetryPolicy.
TEST(LimitedTimeRetryPolicy, Simple)
{
    LimitedTimeRetryPolicyForTest tested(LimitedTimeTestPeriod);
    CheckLimitedTime(tested);
}

/// @test Test cloning for LimitedTimeRetryPolicy.
TEST(LimitedTimeRetryPolicy, Clone)
{
    LimitedTimeRetryPolicyForTest original(LimitedTimeTestPeriod);
    auto cloned = original.Clone();
    CheckLimitedTime(*cloned);
}

/// @test Verify that non-retryable errors cause an immediate failure.
TEST(LimitedTimeRetryPolicy, OnNonRetryable)
{
    LimitedTimeRetryPolicyForTest tested(std::chrono::milliseconds(10));
    EXPECT_FALSE(tested.OnFailure(CreatePermanentError()));
}

/// @test A simple test for the LimitedErrorCountRetryPolicy.
TEST(LimitedErrorCountRetryPolicy, Simple)
{
    LimitedErrorCountRetryPolicyForTest tested(3);
    EXPECT_TRUE(tested.OnFailure(CreateTransientError()));
    EXPECT_TRUE(tested.OnFailure(CreateTransientError()));
    EXPECT_TRUE(tested.OnFailure(CreateTransientError()));
    EXPECT_FALSE(tested.OnFailure(CreateTransientError()));
    EXPECT_FALSE(tested.OnFailure(CreateTransientError()));
}

/// @test Test cloning for LimitedErrorCountRetryPolicy.
TEST(LimitedErrorCountRetryPolicy, Clone)
{
    LimitedErrorCountRetryPolicyForTest original(3);
    auto tested = original.Clone();
    EXPECT_TRUE(tested->OnFailure(CreateTransientError()));
    EXPECT_TRUE(tested->OnFailure(CreateTransientError()));
    EXPECT_TRUE(tested->OnFailure(CreateTransientError()));
    EXPECT_FALSE(tested->OnFailure(CreateTransientError()));
    EXPECT_FALSE(tested->OnFailure(CreateTransientError()));
}

/// @test Verify that non-retryable errors cause an immediate failure.
TEST(LimitedErrorCountRetryPolicy, OnNonRetryable)
{
    LimitedErrorCountRetryPolicyForTest tested(3);
    EXPECT_FALSE(tested.OnFailure(CreatePermanentError()));
}

}  // namespace internal
}  // namespace csa
