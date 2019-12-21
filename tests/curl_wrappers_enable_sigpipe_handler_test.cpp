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

#include "cloudstorageapi/auth/credential_factory.h"
#include "cloudstorageapi/client_options.h"
#include "cloudstorageapi/internal/curl_wrappers.h"
#include <gmock/gmock.h>
#include <csignal>

namespace csa {
namespace internal {
namespace {

extern "C" void testHandler(int) {}

/// @test Verify that configuring the library to enable the SIGPIPE handler
/// works as expected.
TEST(CurlWrappers, SigpipeHandlerEnabledTest)
{
#if !defined(SIGPIPE)
    return;  // nothing to do
#else
    auto initial_handler = std::signal(SIGPIPE, &test_handler);
    CurlInitializeOnce(ClientOptions(auth::CreateAnonymousCredentials())
                         .SetEnableSigpipeHandler(true));
    auto actual = std::signal(SIGPIPE, initialHandler);
    EXPECT_EQ(actual, SIG_IGN);

    // Also verify a second call has no effect.
    EProvider provider = EProvider::GoogleDrive;
    CurlInitializeOnce(ClientOptions(provider, auth::CredentialFactory::CreateAnonymousCredentials(provider))
                            .SetEnableSigpipeHandler(true));
    actual = std::signal(SIGPIPE, initial_handler);
    EXPECT_EQ(actual, initial_handler);
#endif  // defined(SIGPIPE)
}

}  // namespace
}  // namespace internal
}  // namespace csa
