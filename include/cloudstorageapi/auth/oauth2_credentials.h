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

#include "cloudstorageapi/auth/credentials.h"
#include "cloudstorageapi/auth/refreshing_credentials_wrapper.h"
#include "cloudstorageapi/internal/curl_handle_factory.h"
#include "cloudstorageapi/internal/curl_request_builder.h"
#include <chrono>
#include <mutex>

namespace csa {
namespace auth {

struct OAuth2CredentialsInfo
{
    std::string m_clientId;
    std::string m_clientSecret;
    std::string m_refreshToken;
    std::string m_tokenUri;
};

/**
 * Wrapper class for OAuth 2.0 user account credentials.
 *
 * Takes a OAuth2CredentialsInfo and obtains access tokens from the
 * Authorization Service as needed.
 *
 * If the current access token is invalid or nearing expiration, this will
 * class will first obtain a new access token before returning
 * the Authorization header string.
 *
 * @tparam AuthHandler client specific handler provides functions to
 *     parse auth responses and build auth requests.
 * @tparam HttpRequestBuilderType a dependency injection point. It makes it
 *     possible to mock internal libcurl wrappers. This should generally not be
 *     overridden except for testing.
 * @tparam ClockType a dependency injection point to fetch the current time.
 *     This should generally not be overridden except for testing.
 */
template <typename AuthHandler, typename HttpRequestBuilderType = internal::CurlRequestBuilder,
          typename ClockType = std::chrono::system_clock>
class OAuth2Credentials : public Credentials
{
public:
    explicit OAuth2Credentials(OAuth2CredentialsInfo const& info) : m_clock()
    {
        HttpRequestBuilderType requestBuilder(info.m_tokenUri, internal::GetDefaultCurlHandleFactory());
        m_payload = AuthHandler::BuildRequestPayload(info);
        m_request = requestBuilder.BuildRequest();
    }

    StatusOrVal<std::string> AuthorizationHeader() override
    {
        std::unique_lock<std::mutex> lock(m_mu);
        return m_refreshingCreds.AuthorizationHeader(m_clock.now(), [this] { return Refresh(); });
    }

private:
    StatusOrVal<RefreshingCredentialsWrapper::TemporaryToken> Refresh()
    {
        auto response = m_request.MakeRequest(m_payload);
        if (!response)
        {
            return std::move(response).GetStatus();
        }
        if (response->m_statusCode >= 300)
        {
            return AsStatus(*response);
        }
        return AuthHandler::ParseOAuth2RefreshResponse(*response, m_clock.now());
    }

    ClockType m_clock;
    typename HttpRequestBuilderType::RequestType m_request;
    std::string m_payload;
    mutable std::mutex m_mu;
    RefreshingCredentialsWrapper m_refreshingCreds;
};

}  // namespace auth
}  // namespace csa
