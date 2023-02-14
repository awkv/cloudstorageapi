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

#include "cloudstorageapi/internal/logging_resumable_upload_session.h"
#include "cloudstorageapi/internal/log.h"
#include "testing_util/mock_cloud_storage_client.h"
#include "testing_util/mock_resumable_upload_session.h"
#include "testing_util/const_buffer_equality.h"
#include "testing_util/contains_once.h"
#include "testing_util/scoped_log.h"
#include "testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace csa {
namespace internal {

using ::csa::testing::util::ContainsOnce;
using ::csa::testing::util::Equal;
using ::csa::testing::util::StatusIs;
using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::ReturnRef;

class LoggingResumableUploadSessionTest : public ::testing::Test
{
protected:
    ::csa::testing::internal::ScopedLog m_logbackend;
};

TEST_F(LoggingResumableUploadSessionTest, UploadChunk)
{
    auto mock = std::make_unique<MockResumableUploadSession>();

    std::string const payload = "test-payload-data";
    EXPECT_CALL(*mock, UploadChunk).WillOnce([&](ConstBufferSequence const& p) {
        EXPECT_TRUE(Equal(p, ConstBufferSequence{ConstBuffer{payload}}));
        return StatusOrVal<ResumableUploadResponse>(AsStatus(HttpResponse{503, "uh oh", {}}));
    });

    LoggingResumableUploadSession session(std::move(mock));

    auto result = session.UploadChunk({{payload}});
    EXPECT_THAT(result, StatusIs(StatusCode::Unavailable, "uh oh"));

    EXPECT_THAT(m_logbackend.ExtractLines(), ContainsOnce(HasSubstr("[UNAVAILABLE]")));
}

TEST_F(LoggingResumableUploadSessionTest, UploadFinalChunk)
{
    auto mock = std::make_unique<MockResumableUploadSession>();

    std::string const payload = "test-payload-data";
    EXPECT_CALL(*mock, UploadFinalChunk).WillOnce([&](ConstBufferSequence const& p, std::uint64_t s) {
        EXPECT_TRUE(Equal(p, ConstBufferSequence{ConstBuffer{payload}}));
        EXPECT_EQ(513 * 1024, s);
        return StatusOrVal<ResumableUploadResponse>(AsStatus(HttpResponse{503, "uh oh", {}}));
    });

    LoggingResumableUploadSession session(std::move(mock));

    auto result = session.UploadFinalChunk({{payload}}, 513 * 1024);
    EXPECT_THAT(result, StatusIs(StatusCode::Unavailable, "uh oh"));

    auto const log_lines = m_logbackend.ExtractLines();
    EXPECT_THAT(log_lines, ContainsOnce(HasSubstr("upload_size=" + std::to_string(513 * 1024))));
    EXPECT_THAT(log_lines, ContainsOnce(HasSubstr("[UNAVAILABLE]")));
}

TEST_F(LoggingResumableUploadSessionTest, ResetSession)
{
    auto mock = std::make_unique<MockResumableUploadSession>();

    EXPECT_CALL(*mock, ResetSession()).WillOnce([&]() {
        return StatusOrVal<ResumableUploadResponse>(Status(AsStatus(HttpResponse{308, "uh oh", {}})));
    });

    LoggingResumableUploadSession session(std::move(mock));

    auto result = session.ResetSession();
    EXPECT_THAT(result, StatusIs(StatusCode::FailedPrecondition, "uh oh"));

    EXPECT_THAT(m_logbackend.ExtractLines(), ContainsOnce(HasSubstr("[FAILED_PRECONDITION]")));
}

TEST_F(LoggingResumableUploadSessionTest, NextExpectedByte)
{
    auto mock = std::make_unique<MockResumableUploadSession>();

    EXPECT_CALL(*mock, GetNextExpectedByte()).WillOnce([&]() { return 512 * 1024U; });

    LoggingResumableUploadSession session(std::move(mock));

    auto result = session.GetNextExpectedByte();
    EXPECT_EQ(512 * 1024, result);

    EXPECT_THAT(m_logbackend.ExtractLines(), ContainsOnce(HasSubstr(std::to_string(512 * 1024))));
}

TEST_F(LoggingResumableUploadSessionTest, LastResponseOk)
{
    auto mock = std::make_unique<MockResumableUploadSession>();

    const StatusOrVal<ResumableUploadResponse> lastResponse(
        ResumableUploadResponse{"upload url", 1, {}, ResumableUploadResponse::UploadState::InProgress, {}});
    EXPECT_CALL(*mock, GetLastResponse()).WillOnce(ReturnRef(lastResponse));

    LoggingResumableUploadSession session(std::move(mock));

    auto result = session.GetLastResponse();
    ASSERT_STATUS_OK(result);
    EXPECT_EQ(result.Value(), lastResponse.Value());
    auto const logLines = m_logbackend.ExtractLines();
    EXPECT_THAT(logLines, ContainsOnce(HasSubstr("upload url")));
    EXPECT_THAT(logLines, ContainsOnce(HasSubstr("payload={}")));
}

TEST_F(LoggingResumableUploadSessionTest, LastResponseBadStatus)
{
    auto mock = std::make_unique<MockResumableUploadSession>();

    const StatusOrVal<ResumableUploadResponse> lastResponse(Status(StatusCode::FailedPrecondition, "something bad"));
    EXPECT_CALL(*mock, GetLastResponse()).WillOnce(ReturnRef(lastResponse));

    LoggingResumableUploadSession session(std::move(mock));

    EXPECT_THAT(session.GetLastResponse(), StatusIs(StatusCode::FailedPrecondition, "something bad"));

    EXPECT_THAT(m_logbackend.ExtractLines(), ContainsOnce(HasSubstr("[FAILED_PRECONDITION]")));
}

}  // namespace internal
}  // namespace csa
