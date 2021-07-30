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
#include <chrono>
#include <string>
#include <utility>

namespace csa {
namespace auth {

/**
 * Wrapper for refreshable parts of a Credentials object.
 */
class RefreshingCredentialsWrapper
{
public:
    struct TemporaryToken
    {
        std::string m_token;
        std::chrono::system_clock::time_point m_expirationTime;
    };

    template <typename RefreshFunctor>
    StatusOrVal<std::string> AuthorizationHeader(std::chrono::system_clock::time_point now,
                                                 RefreshFunctor refreshFn) const
    {
        if (IsValid(now))
        {
            return m_temporaryToken.m_token;
        }

        StatusOrVal<TemporaryToken> newToken = refreshFn();
        if (newToken)
        {
            m_temporaryToken = *std::move(newToken);
            return m_temporaryToken.m_token;
        }
        return newToken.GetStatus();
    }

    /**
     * Returns whether the current access token should be considered expired.
     *
     * When determining if a Credentials object needs to be refreshed, the IsValid
     * method should be used instead; there may be cases where a Credentials is
     * not expired but should be considered invalid.
     *
     * If a Credentials is close to expiration but not quite expired, this method
     * may still return false. This helps prevent the case where an access token
     * expires between when it is obtained and when it is used.
     */
    bool IsExpired(std::chrono::system_clock::time_point now) const;

    /**
     * Returns whether the current access token should be considered valid.
     *
     * This method should be used to determine whether a Credentials object needs
     * to be refreshed.
     */
    bool IsValid(std::chrono::system_clock::time_point now) const;

private:
    mutable TemporaryToken m_temporaryToken;
};

}  // namespace auth
}  // namespace csa
