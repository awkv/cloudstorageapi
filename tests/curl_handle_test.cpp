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

#include "cloudstorageapi/internal/curl_handle.h"
#include <gmock/gmock.h>

namespace csa {
namespace internal {
namespace {

using ::testing::HasSubstr;

TEST(CurlHandleTest, AsStatus)
{
    struct
    {
        CURLcode curl;
        StatusCode expected;
    } expected_codes[]{
        {CURLE_OK, StatusCode::Ok},
        {CURLE_RECV_ERROR, StatusCode::Unavailable},
        {CURLE_SEND_ERROR, StatusCode::Unavailable},
        {CURLE_PARTIAL_FILE, StatusCode::Unavailable},
        {CURLE_SSL_CONNECT_ERROR, StatusCode::Unavailable},
        {CURLE_COULDNT_RESOLVE_HOST, StatusCode::Unavailable},
        {CURLE_COULDNT_RESOLVE_PROXY, StatusCode::Unavailable},
        {CURLE_COULDNT_CONNECT, StatusCode::Unavailable},
        {CURLE_REMOTE_ACCESS_DENIED, StatusCode::PermissionDenied},
        {CURLE_OPERATION_TIMEDOUT, StatusCode::DeadlineExceeded},
        {CURLE_RANGE_ERROR, StatusCode::Unimplemented},
        {CURLE_BAD_DOWNLOAD_RESUME, StatusCode::InvalidArgument},
        {CURLE_ABORTED_BY_CALLBACK, StatusCode::Aborted},
        {CURLE_REMOTE_FILE_NOT_FOUND, StatusCode::NotFound},
        {CURLE_FAILED_INIT, StatusCode::Unknown},
        {CURLE_FTP_PORT_FAILED, StatusCode::Unknown},
        {CURLE_GOT_NOTHING, StatusCode::Unavailable},
        {CURLE_AGAIN, StatusCode::Unknown},
        {CURLE_HTTP2, StatusCode::Unavailable},
    };

    for (auto const& codes : expected_codes)
    {
        auto const expected = Status(codes.expected, "ignored");
        auto const actual = CurlHandle::AsStatus(codes.curl, "in-test");
        EXPECT_EQ(expected.Code(), actual.Code()) << "CURL code=" << codes.curl;
        if (!actual.Ok())
        {
            EXPECT_THAT(actual.Message(), HasSubstr("in-test"));
            EXPECT_THAT(actual.Message(), HasSubstr(curl_easy_strerror(codes.curl)));
        }
    }
}

TEST(AssertOptionSuccess, StringWithError)
{
    EXPECT_THROW(
        try {
            AssertOptionSuccess(CURLE_NOT_BUILT_IN, CURLOPT_CAINFO, "test-function", "some-path");
        } catch (std::exception const& ex) {
            EXPECT_THAT(ex.what(), HasSubstr("test-function"));
            EXPECT_THAT(ex.what(), HasSubstr("some-path"));
            throw;
        },
        std::logic_error);
}

TEST(AssertOptionSuccess, IntWithError)
{
    EXPECT_THROW(
        try {
            AssertOptionSuccess(CURLE_NOT_BUILT_IN, CURLOPT_CAINFO, "test-function", 1234);
        } catch (std::exception const& ex) {
            EXPECT_THAT(ex.what(), HasSubstr("test-function"));
            EXPECT_THAT(ex.what(), HasSubstr("1234"));
            throw;
        },
        std::logic_error);
}

TEST(AssertOptionSuccess, NullptrWithError)
{
    EXPECT_THROW(
        try {
            AssertOptionSuccess(CURLE_NOT_BUILT_IN, CURLOPT_CAINFO, "test-function", nullptr);
        } catch (std::exception const& ex) {
            EXPECT_THAT(ex.what(), HasSubstr("test-function"));
            EXPECT_THAT(ex.what(), HasSubstr("nullptr"));
            throw;
        },
        std::logic_error);
}

int TestFunction() { return 42; }

TEST(AssertOptionSuccess, FunctionPtrWithError)
{
    EXPECT_EQ(42, TestFunction());
    EXPECT_THROW(
        try {
            AssertOptionSuccess(CURLE_NOT_BUILT_IN, CURLOPT_CAINFO, "test-function", &TestFunction);
        } catch (std::exception const& ex) {
            EXPECT_THAT(ex.what(), HasSubstr("test-function"));
            EXPECT_THAT(ex.what(), HasSubstr("a value of type="));
            throw;
        },
        std::logic_error);
}

}  // namespace
}  // namespace internal
}  // namespace csa
