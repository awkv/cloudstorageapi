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

#pragma once

#include "cloudstorageapi/auth/oauth2_credentials.h"
#include "cloudstorageapi/internal/nljson.h"

namespace csa {
namespace auth {

class GoogleAuthHandler
{
public:
    GoogleAuthHandler() = delete;

    static constexpr auto OAuthEndPoint = "https://accounts.google.com/o/oauth2";
    static constexpr auto OAuthRefreshEndPoint = "https://accounts.google.com/o/oauth2/token";
    // There is another oauth2 token refresh end ponit "https://www.googleapis.com/oauth2/v4/token";

    static std::string BuildRequestPayload(OAuth2CredentialsInfo const& info);
    static StatusOrVal<RefreshingCredentialsWrapper::TemporaryToken> ParseOAuth2RefreshResponse(
        internal::HttpResponse const& response, std::chrono::system_clock::time_point now);
    static StatusOrVal<OAuth2CredentialsInfo> ParseOAuth2Credentials(internal::nl::json const& jsonCreds,
                                                                     std::string const& source);
};

using GoogleOAuth2Credentials = OAuth2Credentials<GoogleAuthHandler>;

}  // namespace auth
}  // namespace csa
