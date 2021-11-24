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

#pragma once

#include "cloudstorageapi/internal/backoff_policy_internal.h"
#include "cloudstorageapi/internal/retry_policy_internal.h"
#include "cloudstorageapi/status.h"

namespace csa {
namespace internal {
/// Defines what error codes are permanent errors.
struct StatusTraits
{
    static bool IsPermanentFailure(Status const& status)
    {
        return status.Code() != StatusCode::DeadlineExceeded && status.Code() != StatusCode::Internal &&
               status.Code() != StatusCode::ResourceExhausted && status.Code() != StatusCode::Unavailable;
    }
};
}  // namespace internal

/// The retry policy base class
using RetryPolicy = csa::internal::TraitBasedRetryPolicy<internal::StatusTraits>;

/// Keep retrying until some time has expired.
using LimitedTimeRetryPolicy = csa::internal::LimitedTimeRetryPolicy<internal::StatusTraits>;

/// Keep retrying until the error count has been exceeded.
using LimitedErrorCountRetryPolicy = csa::internal::LimitedErrorCountRetryPolicy<internal::StatusTraits>;

/// The backoff policy base class.
using BackoffPolicy = csa::internal::BackoffPolicy;

/// Implement truncated exponential backoff with randomization.
using ExponentialBackoffPolicy = csa::internal::ExponentialBackoffPolicy;

}  // namespace csa
