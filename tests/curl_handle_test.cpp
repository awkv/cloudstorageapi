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
    } expected_codes[]
    {
        {CURLE_OK, StatusCode::Ok},
        {CURLE_RECV_ERROR, StatusCode::Unavailable},
        {CURLE_SEND_ERROR, StatusCode::Unavailable},
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
        {CURLE_AGAIN, StatusCode::Unknown},
    };

    for (auto const& codes : expected_codes)
    {
        auto const expected = Status(codes.expected, "ignored");
        auto const actual = CurlHandle::AsStatus(codes.curl, "in-test");
        EXPECT_EQ(expected.Code(), actual.Code());
        if (!actual.Ok())
        {
            EXPECT_THAT(actual.Message(), HasSubstr("in-test"));
            EXPECT_THAT(actual.Message(), HasSubstr(curl_easy_strerror(codes.curl)));
        }
    }
}

}  // namespace
}  // namespace internal
}  // namespace csa
