// Copyright 2021 Andrew Karasyov
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
#include "cloudstorageapi/auth/credential_constants.h"
#include "mock_fake_clock.h"
#include "mock_http_request.h"
#include "util/status_matchers.h"
#include <gmock/gmock.h>
#include <nlohmann/json.hpp>
#include <cstring>

namespace csa {
namespace internal {

// using FakeClock;
// using HttpResponse;
// using MockHttpRequest;
// using MockHttpRequestBuilder;
using ::csa::auth::GoogleAuthHandler;
using ::csa::testing::util::IsOk;
using ::csa::testing::util::StatusIs;
using ::testing::AllOf;
using ::testing::An;
using ::testing::HasSubstr;
using ::testing::Not;
using ::testing::Return;
using ::testing::StrEq;

class GoogleOAuth2CredentialsTest : public ::testing::Test
{
protected:
    void SetUp() override { MockHttpRequestBuilder::m_mock = std::make_shared<MockHttpRequestBuilder::Impl>(); }
    void TearDown() override { MockHttpRequestBuilder::m_mock.reset(); }
};

/// @test Verify that parsing an authorized user account JSON string works.
TEST_F(GoogleOAuth2CredentialsTest, ParseSimple)
{
    std::string config = R"""({
      "client_id": "a-client-id.example.com",
      "client_secret": "a-123456ABCDEF",
      "refresh_token": "1/THETOKEN",
      "token_uri": "https://oauth2.googleapis.com/test_endpoint",
      "type": "magic_type"
    })""";

    auto actual = GoogleAuthHandler::ParseOAuth2Credentials(config, "test-data");
    ASSERT_STATUS_OK(actual);
    EXPECT_EQ("a-client-id.example.com", actual->m_clientId);
    EXPECT_EQ("a-123456ABCDEF", actual->m_clientSecret);
    EXPECT_EQ("1/THETOKEN", actual->m_refreshToken);
    EXPECT_EQ("https://oauth2.googleapis.com/test_endpoint", actual->m_tokenUri);
}

/// @test Verify that parsing an authorized user account JSON string works.
TEST_F(GoogleOAuth2CredentialsTest, ParseUsesExplicitDefaultTokenUri)
{
    // No token_uri attribute here, so the default passed below should be used.
    std::string config = R"""({
      "client_id": "a-client-id.example.com",
      "client_secret": "a-123456ABCDEF",
      "refresh_token": "1/THETOKEN",
      "type": "magic_type"
    })""";

    auto actual = GoogleAuthHandler::ParseOAuth2Credentials(config, "test-data");
    ASSERT_STATUS_OK(actual);
    EXPECT_EQ("a-client-id.example.com", actual->m_clientId);
    EXPECT_EQ("a-123456ABCDEF", actual->m_clientSecret);
    EXPECT_EQ("1/THETOKEN", actual->m_refreshToken);
    EXPECT_EQ(std::string(GoogleAuthHandler::OAuthRefreshEndPoint), actual->m_tokenUri);
}

/// @test Verify that parsing an authorized user account JSON string works.
TEST_F(GoogleOAuth2CredentialsTest, ParseUsesImplicitDefaultTokenUri)
{
    // No token_uri attribute here.
    std::string config = R"""({
      "client_id": "a-client-id.example.com",
      "client_secret": "a-123456ABCDEF",
      "refresh_token": "1/THETOKEN",
      "type": "magic_type"
    })""";

    // No token_uri passed in here, either.
    auto actual = GoogleAuthHandler::ParseOAuth2Credentials(config, "test-data");
    ASSERT_STATUS_OK(actual);
    EXPECT_EQ("a-client-id.example.com", actual->m_clientId);
    EXPECT_EQ("a-123456ABCDEF", actual->m_clientSecret);
    EXPECT_EQ("1/THETOKEN", actual->m_refreshToken);
    EXPECT_EQ(std::string(GoogleAuthHandler::OAuthRefreshEndPoint), actual->m_tokenUri);
}

/// @test Verify that invalid contents result in a readable error.
TEST_F(GoogleOAuth2CredentialsTest, ParseInvalidContentsFails)
{
    std::string config = R"""( not-a-valid-json-string })""";

    auto info = GoogleAuthHandler::ParseOAuth2Credentials(config, "test-as-a-source");
    EXPECT_THAT(info, StatusIs(Not(StatusCode::Ok),
                               AllOf(HasSubstr("Invalid OAuth2Credentials"), HasSubstr("test-as-a-source"))));
}

/// @test Parsing a service account JSON string should detect empty fields.
TEST_F(GoogleOAuth2CredentialsTest, ParseEmptyFieldFails)
{
    std::string contents = R"""({
      "client_id": "a-client-id.example.com",
      "client_secret": "a-123456ABCDEF",
      "refresh_token": "1/THETOKEN",
      "type": "magic_type"
    })""";

    for (auto const& field : {"client_id", "client_secret", "refresh_token"})
    {
        auto json = nlohmann::json::parse(contents);
        json[field] = "";
        auto info = GoogleAuthHandler::ParseOAuth2Credentials(json.dump(), "test-data");
        EXPECT_THAT(info, StatusIs(Not(StatusCode::Ok),
                                   AllOf(HasSubstr(field), HasSubstr(" field is empty"), HasSubstr("test-data"))));
    }
}

/// @test Parsing a service account JSON string should detect missing fields.
TEST_F(GoogleOAuth2CredentialsTest, ParseMissingFieldFails)
{
    std::string contents = R"""({
      "client_id": "a-client-id.example.com",
      "client_secret": "a-123456ABCDEF",
      "refresh_token": "1/THETOKEN",
      "type": "magic_type"
})""";

    for (auto const& field : {"client_id", "client_secret", "refresh_token"})
    {
        auto json = nlohmann::json::parse(contents);
        json.erase(field);
        auto info = GoogleAuthHandler::ParseOAuth2Credentials(json.dump(), "test-data");
        EXPECT_THAT(info, StatusIs(Not(StatusCode::Ok),
                                   AllOf(HasSubstr(field), HasSubstr(" field is missing"), HasSubstr("test-data"))));
    }
}

/// @test Parsing a refresh response with missing fields results in failure.
TEST_F(GoogleOAuth2CredentialsTest, ParseAuthorizedUserRefreshResponseMissingFields)
{
    std::string r1 = R"""({})""";
    // Does not have access_token.
    std::string r2 = R"""({
    "token_type": "Type",
    "id_token": "id-token-value",
    "expires_in": 1000
    })""";

    FakeClock::reset_clock(1000);
    auto status = GoogleAuthHandler::ParseOAuth2RefreshResponse(HttpResponse{400, r1, {}}, FakeClock::now());
    EXPECT_THAT(status, StatusIs(StatusCode::InvalidArgument, HasSubstr("Could not find all required fields")));

    status = GoogleAuthHandler::ParseOAuth2RefreshResponse(HttpResponse{400, r2, {}}, FakeClock::now());
    EXPECT_THAT(status, StatusIs(StatusCode::InvalidArgument, HasSubstr("Could not find all required fields")));
}

/// @test Parsing a refresh response yields a TemporaryToken.
TEST_F(GoogleOAuth2CredentialsTest, ParseAuthorizedUserRefreshResponse)
{
    std::string r1 = R"""({
    "token_type": "Type",
    "access_token": "access-token-r1",
    "id_token": "id-token-value",
    "expires_in": 1000
    })""";

    auto expires_in = 1000;
    FakeClock::reset_clock(2000);
    auto status = GoogleAuthHandler::ParseOAuth2RefreshResponse(HttpResponse{200, r1, {}}, FakeClock::now());
    EXPECT_STATUS_OK(status);
    auto token = *status;
    EXPECT_EQ(std::chrono::time_point_cast<std::chrono::seconds>(token.m_expirationTime).time_since_epoch().count(),
              FakeClock::m_nowValue + expires_in);
    EXPECT_EQ(token.m_token, "Authorization: Type access-token-r1");
}

}  // namespace internal
}  // namespace csa
