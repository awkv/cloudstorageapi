// Copyright 2020 Andrew Karasyov
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

#include "cloudstorageapi/auth/default_credentials_file.h"
#include "util/scoped_environment.h"
#include <gmock/gmock.h>

namespace csa {
namespace {

using csa::testing::internal::ScopedEnvironment;
using ::testing::HasSubstr;

class DefaultCredentialFileTest : public ::testing::Test
{
public:
    DefaultCredentialFileTest()
        : m_homeEnvVar(csa::auth::HomeEnvVar(), {}),
        m_csaEnvVar(csa::auth::DefaultCredentialsEnvVar(), {}),
        m_csaTestEnvVar(csa::auth::DefaultTestCredentialsEnvVar(), {})
    {
    }

protected:
    ScopedEnvironment m_homeEnvVar;
    ScopedEnvironment m_csaEnvVar;
    ScopedEnvironment m_csaTestEnvVar;
};

/// @test Verify that the specified path is given when the credentials env var is set.
TEST_F(DefaultCredentialFileTest, CredentialsEnvVariableSet)
{
    ScopedEnvironment credEnvVar(csa::auth::DefaultCredentialsEnvVar(), "/foo/bar/baz");
    EXPECT_EQ("/foo/bar/baz", csa::auth::DefaultCredentialsFilePathFromEnvVarOrEmpty());
}

/// @test Verify that an empty string is given when the credentials env var is unset.
TEST_F(DefaultCredentialFileTest, CredentialsEnvVariableNotSet)
{
    EXPECT_EQ(csa::auth::DefaultCredentialsFilePathFromEnvVarOrEmpty(), "");
}

/// @test Verify that the credentials file path can be overridden for testing.
TEST_F(DefaultCredentialFileTest, CredentialsTestPathOverrideViaEnvVar)
{
    ScopedEnvironment gcloud_path_override_env_var(csa::auth::DefaultTestCredentialsEnvVar(),
        "/foo/bar/baz");
    EXPECT_EQ(csa::auth::DefaultCredentialsFilePathFromWellKnownPathOrEmpty(), "/foo/bar/baz");
}

/// @test Verify that the credentials file path is given when HOME is set.
TEST_F(DefaultCredentialFileTest, HomeSet)
{
    ScopedEnvironment home_env_var(csa::auth::HomeEnvVar(), "/foo/bar/baz");

    auto actual = csa::auth::DefaultCredentialsFilePathFromWellKnownPathOrEmpty();

    EXPECT_THAT(actual, HasSubstr("/foo/bar/baz"));
    // The rest of the path differs depending on the OS; just make sure that we
    // appended this suffix of the path to the path prefix set above.
    EXPECT_THAT(actual, HasSubstr("csa/default_credentials.json"));
}

/// @test Verify that the credentials file path is not given when HOME is unset.
TEST_F(DefaultCredentialFileTest, HomeNotSet)
{
    EXPECT_EQ(csa::auth::DefaultCredentialsFilePathFromWellKnownPathOrEmpty(), "");
}

} // namespace
} // namespace csa
