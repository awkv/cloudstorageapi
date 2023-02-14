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

#include "cloudstorageapi/auth/oauth2_credentials.h"
#include "cloudstorageapi/auth/credential_constants.h"
#include "testing_util/mock_fake_clock.h"
#include "testing_util/mock_http_request.h"
#include "testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <nlohmann/json.hpp>
#include <cstring>

namespace csa {
namespace internal {

// using FakeClock;
// using HttpResponse;
// using MockHttpRequest;
// using MockHttpRequestBuilder;
using ::csa::testing::util::IsOk;
using ::csa::testing::util::StatusIs;
using ::testing::AllOf;
using ::testing::An;
using ::testing::HasSubstr;
using ::testing::Not;
using ::testing::Return;
using ::testing::StrEq;

class OAuth2CredentialsTest : public ::testing::Test
{
protected:
    void SetUp() override { MockHttpRequestBuilder::m_mock = std::make_shared<MockHttpRequestBuilder::Impl>(); }
    void TearDown() override { MockHttpRequestBuilder::m_mock.reset(); }
};

// This class is supposed to be provider specific. It is simplified for testing
// and checks below relies on it.
class AuthHandlerTest
{
public:
    AuthHandlerTest() = delete;

    static constexpr auto OAuthEndPoint = "https://storage.provider.com/oauth2";
    static constexpr auto OAuthRefreshEndPoint = "https://storage.provider.com/oauth2/token";

    static std::string BuildRequestPayload(csa::auth::OAuth2CredentialsInfo const& info)
    {
        std::string payload("grant_type=refresh_token");
        payload += "&client_id=" + info.m_clientId;
        payload += "&client_secret=" + info.m_clientSecret;
        payload += "&refresh_token=" + info.m_refreshToken;

        return payload;
    }

    static StatusOrVal<csa::auth::RefreshingCredentialsWrapper::TemporaryToken> ParseOAuth2RefreshResponse(
        internal::HttpResponse const& response, std::chrono::system_clock::time_point now)
    {
        auto refreshJson = nlohmann::json::parse(response.m_payload, nullptr, false);
        if (refreshJson.is_discarded() || refreshJson.count("access_token") == 0 ||
            refreshJson.count("expires_in") == 0 || refreshJson.count("token_type") == 0)
        {
            return AsStatus(internal::HttpResponse{
                response.m_statusCode, "Could not find all required fields in response", response.m_headers});
        }

        std::string header = "Authorization: ";
        header += refreshJson.value("token_type", "");
        header += ' ';
        header += refreshJson.value("access_token", "");
        auto expiresIn = std::chrono::seconds(refreshJson.value("expires_in", int(0)));
        auto newExpiration = now + expiresIn;
        return csa::auth::RefreshingCredentialsWrapper::TemporaryToken{std::move(header), newExpiration};
    }

    static StatusOrVal<csa::auth::OAuth2CredentialsInfo> ParseOAuth2Credentials(std::string const& strCreds,
                                                                                std::string const& source)
    {
        auto jsonCreds = nlohmann::json::parse(strCreds, nullptr, false);
        if (jsonCreds.is_discarded())
        {
            return Status(StatusCode::InvalidArgument,
                          "Invalid OAuth2CredentialsInfo, parsing failed on data from " + source);
        }
        constexpr char const clientIdKey[] = "client_id";
        constexpr char const clientSecretKey[] = "client_secret";
        constexpr char const refreshTokenKey[] = "refresh_token";
        constexpr char const tokenTypeKey[] = "token_type";

        for (auto const& key : {clientIdKey, clientSecretKey, refreshTokenKey})
        {
            if (jsonCreds.count(key) == 0)
            {
                return Status(StatusCode::InvalidArgument, "Invalid OAuth2Credentials, the " + std::string(key) +
                                                               " field is missing on data "
                                                               "loaded from " +
                                                               source);
            }

            if (jsonCreds.value(key, "").empty())
            {
                return Status(StatusCode::InvalidArgument, "Invalid OAuth2Credentials, the " + std::string(key) +
                                                               " field is empty on data "
                                                               "loaded from " +
                                                               source);
            }
        }

        return csa::auth::OAuth2CredentialsInfo{jsonCreds.value(clientIdKey, ""), jsonCreds.value(clientSecretKey, ""),
                                                jsonCreds.value(refreshTokenKey, ""), OAuthRefreshEndPoint};
    }
};

/// @test Verify that we can create credentials from a JWT string.
TEST_F(OAuth2CredentialsTest, Simple)
{
    std::string response = R"""({
    "token_type": "Type",
    "access_token": "access-token-value",
    "id_token": "id-token-value",
    "expires_in": 1234
})""";
    auto mockRequest = std::make_shared<MockHttpRequest::Impl>();
    EXPECT_CALL(*mockRequest, MakeRequest).WillOnce([response](std::string const& payload) {
        EXPECT_THAT(payload, HasSubstr("grant_type=refresh_token"));
        EXPECT_THAT(payload, HasSubstr("client_id=a-client-id.example.com"));
        EXPECT_THAT(payload, HasSubstr("client_secret=a-123456ABCDEF"));
        EXPECT_THAT(payload, HasSubstr("refresh_token=1/THETOKEN"));
        return HttpResponse{200, response, {}};
    });

    auto mockBuilder = MockHttpRequestBuilder::m_mock;
    EXPECT_CALL(*mockBuilder, Constructor(StrEq(AuthHandlerTest::OAuthRefreshEndPoint))).Times(1);
    EXPECT_CALL(*mockBuilder, BuildRequest()).WillOnce([mockRequest]() {
        MockHttpRequest result;
        result.mock = mockRequest;
        return result;
    });
    EXPECT_CALL(*mockBuilder, MakeEscapedString(An<std::string const&>())).WillRepeatedly([](std::string const& s) {
        auto t = std::unique_ptr<char[]>(new char[s.size() + 1]);
        std::copy(s.begin(), s.end(), t.get());
        t[s.size()] = '\0';
        return t;
    });

    std::string config = R"""({
      "client_id": "a-client-id.example.com",
      "client_secret": "a-123456ABCDEF",
      "refresh_token": "1/THETOKEN",
      "type": "magic_type"
})""";

    auto info = AuthHandlerTest::ParseOAuth2Credentials(config, "test");
    ASSERT_STATUS_OK(info);
    csa::auth::OAuth2Credentials<AuthHandlerTest, MockHttpRequestBuilder> credentials(*info);
    EXPECT_EQ("Authorization: Type access-token-value", credentials.AuthorizationHeader().Value());
}

/// @test Verify that we can refresh service account credentials.
TEST_F(OAuth2CredentialsTest, Refresh)
{
    // Prepare two responses, the first one is used but becomes immediately
    // expired, resulting in another refresh next time the caller tries to get
    // an authorization header.
    std::string r1 = R"""({
    "token_type": "Type",
    "access_token": "access-token-r1",
    "id_token": "id-token-value",
    "expires_in": 0
})""";
    std::string r2 = R"""({
    "token_type": "Type",
    "access_token": "access-token-r2",
    "id_token": "id-token-value",
    "expires_in": 1000
})""";
    auto mockRequest = std::make_shared<MockHttpRequest::Impl>();
    EXPECT_CALL(*mockRequest, MakeRequest)
        .WillOnce(Return(HttpResponse{200, r1, {}}))
        .WillOnce(Return(HttpResponse{200, r2, {}}));

    // Now setup the builder to return those responses.
    auto mockBuilder = MockHttpRequestBuilder::m_mock;
    EXPECT_CALL(*mockBuilder, BuildRequest()).WillOnce([mockRequest] {
        MockHttpRequest request;
        request.mock = mockRequest;
        return request;
    });
    EXPECT_CALL(*mockBuilder, Constructor(AuthHandlerTest::OAuthRefreshEndPoint)).Times(1);
    EXPECT_CALL(*mockBuilder, MakeEscapedString(An<std::string const&>())).WillRepeatedly([](std::string const& s) {
        auto t = std::unique_ptr<char[]>(new char[s.size() + 1]);
        std::copy(s.begin(), s.end(), t.get());
        t[s.size()] = '\0';
        return t;
    });

    std::string config = R"""({
      "client_id": "a-client-id.example.com",
      "client_secret": "a-123456ABCDEF",
      "refresh_token": "1/THETOKEN",
      "type": "magic_type"
})""";
    auto info = AuthHandlerTest::ParseOAuth2Credentials(config, "test");
    ASSERT_STATUS_OK(info);
    csa::auth::OAuth2Credentials<AuthHandlerTest, MockHttpRequestBuilder> credentials(*info);
    EXPECT_EQ("Authorization: Type access-token-r1", credentials.AuthorizationHeader().Value());
    EXPECT_EQ("Authorization: Type access-token-r2", credentials.AuthorizationHeader().Value());
    EXPECT_EQ("Authorization: Type access-token-r2", credentials.AuthorizationHeader().Value());
}

/// @test Mock a failed refresh response.
TEST_F(OAuth2CredentialsTest, FailedRefresh)
{
    auto mockRequest = std::make_shared<MockHttpRequest::Impl>();
    EXPECT_CALL(*mockRequest, MakeRequest)
        .WillOnce(Return(Status(StatusCode::Aborted, "Fake Curl error")))
        .WillOnce(Return(HttpResponse{400, "", {}}));

    // Now setup the builder to return those responses.
    auto mockBuilder = MockHttpRequestBuilder::m_mock;
    EXPECT_CALL(*mockBuilder, BuildRequest()).WillOnce([mockRequest] {
        MockHttpRequest request;
        request.mock = mockRequest;
        return request;
    });
    EXPECT_CALL(*mockBuilder, Constructor(AuthHandlerTest::OAuthRefreshEndPoint)).Times(1);
    EXPECT_CALL(*mockBuilder, MakeEscapedString(An<std::string const&>())).WillRepeatedly([](std::string const& s) {
        auto t = std::unique_ptr<char[]>(new char[s.size() + 1]);
        std::copy(s.begin(), s.end(), t.get());
        t[s.size()] = '\0';
        return t;
    });

    std::string config = R"""({
      "client_id": "a-client-id.example.com",
      "client_secret": "a-123456ABCDEF",
      "refresh_token": "1/THETOKEN",
      "type": "magic_type"
})""";
    auto info = AuthHandlerTest::ParseOAuth2Credentials(config, "test");
    ASSERT_STATUS_OK(info);
    csa::auth::OAuth2Credentials<AuthHandlerTest, MockHttpRequestBuilder> credentials(*info);
    // Response 1
    auto status = credentials.AuthorizationHeader();
    EXPECT_THAT(status, StatusIs(StatusCode::Aborted));
    // Response 2
    status = credentials.AuthorizationHeader();
    EXPECT_THAT(status, Not(IsOk()));
}

}  // namespace internal
}  // namespace csa
