// Copyright 2019 Andrew Karasyov
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

#include "cloudstorageapi/auth/google_oauth2_credentials.h"
#include "cloudstorageapi/internal/curl_handle.h"

namespace csa {
namespace auth {

std::string GoogleAuthHandler::BuildRequestPayload(OAuth2CredentialsInfo const& info)
{
    internal::CurlHandle curl;

    std::string payload("grant_type=refresh_token");
    payload += "&client_id=";
    payload += curl.MakeEscapedString(info.m_clientId).get();
    payload += "&client_secret=";
    payload += curl.MakeEscapedString(info.m_clientSecret).get();
    payload += "&refresh_token=";
    payload += curl.MakeEscapedString(info.m_refreshToken).get();

    return payload;
}

StatusOrVal<RefreshingCredentialsWrapper::TemporaryToken> GoogleAuthHandler::ParseOAuth2RefreshResponse(
    internal::HttpResponse const& response, std::chrono::system_clock::time_point now)
{
    auto refreshJson = internal::nl::json::parse(response.m_payload, nullptr, false);
    if (refreshJson.is_discarded() || refreshJson.count("access_token") == 0 || refreshJson.count("expires_in") == 0 ||
        refreshJson.count("token_type") == 0)
    {
        auto payload = response.m_payload +
                       "Could not find all required fields in response (access_token,"
                       " expires_in, token_type).";
        return AsStatus(internal::HttpResponse{response.m_statusCode, payload, response.m_headers});
    }

    std::string header = "Authorization: ";
    header += refreshJson.value("token_type", "");
    header += ' ';
    header += refreshJson.value("access_token", "");
    auto expiresIn = std::chrono::seconds(refreshJson.value("expires_in", int(0)));
    auto newExpiration = now + expiresIn;
    return RefreshingCredentialsWrapper::TemporaryToken{std::move(header), newExpiration};
}

StatusOrVal<OAuth2CredentialsInfo> GoogleAuthHandler::ParseOAuth2Credentials(internal::nl::json const& jsonCreds,
                                                                             std::string const& source)
{
    constexpr char const clientIdKey[] = "client_id";
    constexpr char const clientSecretKey[] = "client_secret";
    constexpr char const refreshTokenKey[] = "refresh_token";
    constexpr char const tokenTypeKey[] = "token_type";

    for (auto const& key : {clientIdKey, clientSecretKey, refreshTokenKey})
    {
        if (jsonCreds.count(key) == 0)
        {
            return Status(StatusCode::InvalidArgument, "Invalid OAuth2Credentials, the " + std::string(key) +
                                                           " field is missing on data loaded from " + source);
        }

        if (jsonCreds.value(key, "").empty())
        {
            return Status(StatusCode::InvalidArgument, "Invalid OAuth2Credentials, the " + std::string(key) +
                                                           " field is empty on data loaded from " + source);
        }
    }

    return OAuth2CredentialsInfo{jsonCreds.value(clientIdKey, ""), jsonCreds.value(clientSecretKey, ""),
                                 jsonCreds.value(refreshTokenKey, ""), OAuthRefreshEndPoint};
}

}  // namespace auth
}  // namespace csa
