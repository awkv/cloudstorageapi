// Copyright 2021 Andrew Karasyov
//
// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include "cloudstorageapi/internal/random.h"
#include <chrono>
#include <memory>
#include <optional>

namespace csa {
namespace internal {
/**
 * Define the interface for backoff policies.
 *
 * The client libraries need to hide partial and temporary failures from the
 * application. Exponential backoff is generally considered a best practice when
 * retrying operations. However, the details of how exponential backoff is
 * implemented and tuned varies widely. We need to give the users enough
 * flexibility, and also provide sensible default implementations.
 *
 * The client library receives an object of this type, and clones a new instance
 * for each operation. That is, the application provides the library with a
 * [Prototype](https://en.wikipedia.org/wiki/Prototype_pattern) of the policy
 * that will be applied to each operation.
 *
 */
class BackoffPolicy
{
public:
    virtual ~BackoffPolicy() = default;

    /**
     * Return a new copy of this object.
     *
     * Typically implemented as
     * @code
     *   return std::unique_ptr<BackoffPolicy>(new Foo(*this));
     * @endcode
     */
    virtual std::unique_ptr<BackoffPolicy> Clone() const = 0;

    /**
     * Handle an operation completion.
     *
     * This function is typically called when an operation has failed (if it had
     * succeeded there is no reason to retry and backoff). The decision to retry
     * the operation is handled by other policies. This separates the concerns
     * of how much to retry vs. how much delay to put between retries.
     *
     * @return the delay to wait before the next retry attempt.
     */
    virtual std::chrono::milliseconds OnCompletion() = 0;
};

/**
 * Implements a truncated exponential backoff with randomization policy.
 *
 * This policy implements the truncated exponential backoff policy for
 * retrying operations. After a request fails, and subject to a separate
 * retry policy, the client library will wait for an initial delay before
 * trying again. If the second attempt fails the delay time is increased,
 * using an scaling factor. The delay time growth stops at a maximum delay
 * wait time. The policy also randomizes the delay each time, to avoid
 * [thundering herd
 * problem](https://en.wikipedia.org/wiki/Thundering_herd_problem).
 */
class ExponentialBackoffPolicy : public BackoffPolicy
{
public:
    /**
     * Constructor for an exponential backoff policy.
     *
     * Define the initial delay, maximum delay, and scaling factor for an instance
     * of the policy. While the constructor accepts `std::chrono::duration`
     * objects at any resolution, the data is kept internally in microseconds.
     * Sub-microsecond delays seem unnecessarily precise for this application.
     *
     * @code
     * using namespace std::chrono_literals; // C++14
     * auto r1 = ExponentialBackoffPolicy<S,T>(10ms, 500ms, 1.618);
     * auto r2 = ExponentialBackoffPolicy<S,T>(10min, 10min + 2s, 1.002);
     * @endcode
     *
     * @param initialDelay how long to wait after the first (unsuccessful)
     *     operation.
     * @param maximumDelay the maximum value for the delay between operations.
     * @param scaling how fast does the delay increase between iterations.
     *
     * @tparam Rep1 a placeholder to match the Rep tparam for @p initial_delay's
     *     type, the semantics of this template parameter are documented in
     *     `std::chrono::duration<>` (in brief, the underlying arithmetic type
     *     used to store the number of ticks), for our purposes it is simply a
     *     formal parameter.
     * @tparam Period1 a placeholder to match the Period tparam for
     *     @p initial_delay's type, the semantics of this template parameter are
     *     documented in `std::chrono::duration<>` (in brief, the length of the
     *     tick in seconds, expressed as a `std::ratio<>`), for our purposes it
     *     is simply a formal parameter.
     * @tparam Rep2 similar formal parameter for the type of @p maximum_delay.
     * @tparam Period2 similar formal parameter for the type of @p maximum_delay.
     *
     * @see
     * [std::chrono::duration<>](http://en.cppreference.com/w/cpp/chrono/duration)
     *     for more details.
     */
    template <typename Rep1, typename Period1, typename Rep2, typename Period2>
    ExponentialBackoffPolicy(std::chrono::duration<Rep1, Period1> initialDelay,
                             std::chrono::duration<Rep2, Period2> maximumDelay, double scaling)
        : m_initialDelay(std::chrono::duration_cast<std::chrono::microseconds>(initialDelay)),
          m_currentDelayRange(std::chrono::duration_cast<std::chrono::microseconds>(2 * initialDelay)),
          m_maximumDelay(std::chrono::duration_cast<std::chrono::microseconds>(maximumDelay)),
          m_scaling(scaling)
    {
        if (m_scaling <= 1.0)
        {
            throw std::invalid_argument("scaling factor must be > 1.0");
        }
    }

    // Do not copy the PRNG, we get two benefits:
    //  - This works around a bug triggered by MSVC + `absl::optional` (we do not
    //    know specifically which one is at fault)
    //  - We want uncorrelated data streams for each copy anyway.
    ExponentialBackoffPolicy(ExponentialBackoffPolicy const& rhs) noexcept
        : m_initialDelay(rhs.m_initialDelay),
          m_currentDelayRange(rhs.m_currentDelayRange),
          m_maximumDelay(rhs.m_maximumDelay),
          m_scaling(rhs.m_scaling)
    {
    }

    std::unique_ptr<BackoffPolicy> Clone() const override;
    std::chrono::milliseconds OnCompletion() override;

private:
    std::chrono::microseconds m_initialDelay;
    std::chrono::microseconds m_currentDelayRange;
    std::chrono::microseconds m_maximumDelay;
    double m_scaling;
    std::optional<DefaultPRNG> m_generator;
};

}  // namespace internal
}  // namespace csa
