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

extern "C" void test_handler(int) {}

/// @test Verify that configuring the library to disable the SIGPIPE handler
/// works as expected.
TEST(CurlWrappers, SigpipeHandlerDisabledTest)
{
#if defined(__has_feature)
#if __has_feature(memory_sanitizer)
    // The memory sanitizer seems to intercept SIGPIPE, simply disable the test
    // in this case.
    return;
#endif  // __has_feature(memory_sanitizer)
#endif  // __has_fature

#if !defined(SIGPIPE)
    return;  // nothing to do
#elif LIBCURL_VERSION_NUM > 0x072900
    // libcurl <= 7.29.0 installs its own signal handler for SIGPIPE during
    // curl_global_init(). Unfortunately 7.29.0 is the default on CentOS-7, and
    // the tests here fails. We simply skip the test with this ancient library.
    auto initialHandler = std::signal(SIGPIPE, &test_handler);
    EProvider provider = EProvider::GoogleDrive;
    CurlInitializeOnce(ClientOptions(provider, auth::CredentialFactory::CreateAnonymousCredentials(provider))
                           .SetEnableSigpipeHandler(false));
    auto actual = std::signal(SIGPIPE, initialHandler);
    EXPECT_EQ(actual, &test_handler);
#endif  // defined(SIGPIPE)
}

}  // namespace
}  // namespace internal
}  // namespace csa
