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

#include "cloudstorageapi/status.h"
#include <chrono>
#include <memory>

namespace csa {
namespace internal {

/**
 * Define the interface for retry policies.
 */
class RetryPolicy
{
public:
    virtual ~RetryPolicy() = default;

    //@{
    /**
     * @name Control retry loop duration.
     *
     * This functions are typically used in a retry loop, where they control
     * whether to continue, whether a failure should be retried, and finally
     * how to format the error message.
     *
     * @code
     * std::unique_ptr<RetryPolicy> policy = ....;
     * Status status;
     * while (!policy->IsExhausted()) {
     *   auto response = try_rpc();  // typically `response` is StatusOrVal<T>
     *   if (response.Ok()) return response;
     *   status = std::move(response).GetStatus();
     *   if (!policy->OnFailure(response->GetStatus())) {
     *     if (policy->IsPermanentFailure(response->GetStatus()) {
     *       return StatusModifiedToSayPermanentFailureCausedTheProblem(status);
     *     }
     *     return StatusModifiedToSayPolicyExhaustionCausedTheProblem(status);
     *   }
     *   // sleep, which may exhaust the policy, even if it was not exhausted in
     *   // the last call.
     * }
     * return StatusModifiedToSayPolicyExhaustionCausedTheProblem(status);
     * @endcode
     */
    virtual bool OnFailure(Status const&) = 0;
    virtual bool IsExhausted() const = 0;
    virtual bool IsPermanentFailure(Status const&) const = 0;
    //@}
};

/**
 * Trait based RetryPolicy.
 *
 * @tparam RetryableTraitsP the traits to decide if a status represents a
 *     permanent failure.
 */
template <typename RetryableTraitsP>
class TraitBasedRetryPolicy : public RetryPolicy
{
public:
    ///@{
    /**
     * @name type traits
     */
    /// The traits describing which errors are permanent failures
    using RetryableTraits = RetryableTraitsP;

    /// The status type used by the retry policy
    using StatusType = csa::Status;
    ///@}

    ~TraitBasedRetryPolicy() override = default;

    virtual std::unique_ptr<TraitBasedRetryPolicy> Clone() const = 0;

    bool IsPermanentFailure(Status const& status) const override { return RetryableTraits::IsPermanentFailure(status); }

    bool OnFailure(Status const& status) override
    {
        if (RetryableTraits::IsPermanentFailure(status))
        {
            return false;
        }
        OnFailureImpl();
        return !IsExhausted();
    }

protected:
    virtual void OnFailureImpl() = 0;
};

/**
 * Implement a simple "count errors and then stop" retry policy.
 *
 * @tparam RetryablePolicy the policy to decide if a status represents a
 *     permanent failure.
 */
template <typename RetryablePolicyTraits>
class LimitedErrorCountRetryPolicy : public TraitBasedRetryPolicy<RetryablePolicyTraits>
{
public:
    using BaseType = TraitBasedRetryPolicy<RetryablePolicyTraits>;

    explicit LimitedErrorCountRetryPolicy(int maximumFailures) : m_failureCount(0), m_maximumFailures(maximumFailures)
    {
    }

    LimitedErrorCountRetryPolicy(LimitedErrorCountRetryPolicy&& rhs) noexcept
        : LimitedErrorCountRetryPolicy(rhs.m_maximumFailures)
    {
    }
    LimitedErrorCountRetryPolicy(LimitedErrorCountRetryPolicy const& rhs) noexcept
        : LimitedErrorCountRetryPolicy(rhs.m_maximumFailures)
    {
    }

    std::unique_ptr<BaseType> Clone() const override
    {
        return std::unique_ptr<BaseType>(new LimitedErrorCountRetryPolicy(m_maximumFailures));
    }
    bool IsExhausted() const override { return m_failureCount > m_maximumFailures; }

protected:
    void OnFailureImpl() override { ++m_failureCount; }

private:
    int m_failureCount;
    int m_maximumFailures;
};

/**
 * Implement a simple "keep trying for this time" retry policy.
 *
 * @tparam RetryablePolicyTraits the policy to decide if a status represents a
 *     permanent failure.
 */
template <typename RetryablePolicyTraits>
class LimitedTimeRetryPolicy : public TraitBasedRetryPolicy<RetryablePolicyTraits>
{
public:
    using BaseType = TraitBasedRetryPolicy<RetryablePolicyTraits>;

    /**
     * Constructor given a `std::chrono::duration<>` object.
     *
     * @tparam DurationRep a placeholder to match the `Rep` tparam for @p
     *     duration's type. The semantics of this template parameter are
     *     documented in `std::chrono::duration<>` (in brief, the underlying
     *     arithmetic type used to store the number of ticks), for our purposes it
     *     is simply a formal parameter.
     * @tparam DurationPeriod a placeholder to match the `Period` tparam for @p
     *     duration's type. The semantics of this template parameter are
     *     documented in `std::chrono::duration<>` (in brief, the length of the
     *     tick in seconds, expressed as a `std::ratio<>`), for our purposes it is
     *     simply a formal parameter.
     * @param maximumDuration the maximum time allowed before the policy expires,
     *     while the application can express this time in any units they desire,
     *     the class truncates to milliseconds.
     */
    template <typename DurationRep, typename DurationPeriod>
    explicit LimitedTimeRetryPolicy(std::chrono::duration<DurationRep, DurationPeriod> maximumDuration)
        : m_maximumDuration(std::chrono::duration_cast<std::chrono::milliseconds>(maximumDuration)),
          m_deadline(std::chrono::system_clock::now() + m_maximumDuration)
    {
    }

    LimitedTimeRetryPolicy(LimitedTimeRetryPolicy&& rhs) noexcept : LimitedTimeRetryPolicy(rhs.m_maximumDuration) {}
    LimitedTimeRetryPolicy(LimitedTimeRetryPolicy const& rhs) : LimitedTimeRetryPolicy(rhs.m_maximumDuration) {}

    std::unique_ptr<BaseType> Clone() const override
    {
        return std::unique_ptr<BaseType>(new LimitedTimeRetryPolicy(m_maximumDuration));
    }
    bool IsExhausted() const override { return std::chrono::system_clock::now() >= m_deadline; }

    std::chrono::system_clock::time_point deadline() const { return m_deadline; }

protected:
    void OnFailureImpl() override {}

private:
    std::chrono::milliseconds m_maximumDuration;
    std::chrono::system_clock::time_point m_deadline;
};

}  // namespace internal
}  // namespace csa
