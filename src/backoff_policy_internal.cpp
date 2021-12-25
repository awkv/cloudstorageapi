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

#include "cloudstorageapi/internal/backoff_policy_internal.h"

namespace csa {
namespace internal {

std::unique_ptr<BackoffPolicy> ExponentialBackoffPolicy::Clone() const
{
    return std::make_unique<ExponentialBackoffPolicy>(m_initialDelay, m_maximumDelay, m_scaling);
}

std::chrono::milliseconds ExponentialBackoffPolicy::OnCompletion()
{
    using std::chrono::duration_cast;
    using std::chrono::microseconds;
    using std::chrono::milliseconds;
    // We do not want to copy the seed in `Clone()` because then all operations
    // will have the same sequence of backoffs. Nor do we want to use a shared
    // PRNG because that would require locking and some more complicated lifecycle
    // management of the shared PRNG.
    //
    // Instead we exploit the following observation: most requests never need to
    // backoff, they succeed on the first call.
    //
    // So we delay the initialization of the PRNG until the first call that needs
    // to, that is here:
    if (!m_generator)
    {
        m_generator = csa::internal::MakeDefaultPRNG();
    }
    std::uniform_int_distribution<microseconds::rep> rng_distribution(m_currentDelayRange.count() / 2,
                                                                      m_currentDelayRange.count());
    // Randomized sleep period because it is possible that after some time all
    // client have same sleep period if we use only exponential backoff policy.
    auto delay = microseconds(rng_distribution(*m_generator));
    m_currentDelayRange =
        microseconds(static_cast<microseconds::rep>(static_cast<double>(m_currentDelayRange.count()) * m_scaling));
    if (m_currentDelayRange >= m_maximumDelay)
    {
        m_currentDelayRange = m_maximumDelay;
    }
    return duration_cast<milliseconds>(delay);
}

}  // namespace internal
}  // namespace csa
