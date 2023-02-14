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

#include "cloudstorageapi/internal/retry_resumable_upload_session.h"
#include "canonical_errors.h"
#include "testing_util/mock_cloud_storage_client.h"
#include "testing_util/mock_resumable_upload_session.h"
#include "testing_util/chrono_literals.h"
#include "testing_util/const_buffer_equality.h"
#include "testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace csa {
namespace internal {

using ::csa::testing::util::chrono_literals::operator"" _us;
using ::csa::internal::canonical_errors::PermanentError;
using ::csa::internal::canonical_errors::TransientError;
using ::csa::testing::util::Equal;
using ::csa::testing::util::IsOk;
using ::csa::testing::util::StatusIs;
using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::Not;
using ::testing::Return;
using ::testing::ReturnRef;

namespace {
// used only for test.
constexpr std::size_t ChunkSizeQuantumTest = 1 * 1024UL;
}  // namespace

class RetryResumableUploadSessionTest : public ::testing::Test
{
protected:
    void SetUp() override {}
    void TearDown() override {}
};

std::unique_ptr<::csa::BackoffPolicy> TestBackoffPolicy()
{
    return ::csa::ExponentialBackoffPolicy(1_us, 2_us, 2).Clone();
}

/// @test Verify that transient failures are handled as expected.
TEST_F(RetryResumableUploadSessionTest, HandleTransient)
{
    auto mock = std::make_unique<MockResumableUploadSession>();

    std::string const payload(ChunkSizeQuantumTest, '0');

    // Keep track of the sequence of calls.
    int count = 0;
    // The sequence of messages is split across two EXPECT_CALL() and hard to see,
    // basically we want this to happen:
    //
    // RetryResumableUploadSession::UploadChunk() is called
    // 1. GetNextExpectedByte() -> returns 0
    // 2. GetNextExpectedByte() -> returns 0
    // 3. UploadChunk() -> returns transient error
    // 4. ResetSession() -> returns transient error
    // 5. ResetSession() -> returns success (0 bytes committed)
    // 6  GetNextExpectedByte() -> returns 0
    // 7. UploadChunk() -> returns success (quantum bytes committed)
    // 8. GetNextExpectedByte() -> returns quantum
    // RetryResumableUploadSession::UploadChunk() is called
    // 9. GetNextExpectedByte() -> returns quantum
    // 10. GetNextExpectedByte() -> returns quantum
    // 11. UploadChunk() -> returns transient error
    // 12. ResetSession() -> returns success (quantum bytes committed)
    // 13. GetNextExpectedByte() -> returns quantum
    // 14. UploadChunk() -> returns success (2 * quantum bytes committed)
    // 15. GetNextExpectedByte() -> returns 2 * quantum
    // RetryResumableUploadSession::UploadChunk() is called
    // 16. GetNextExpectedByte() -> returns 2 * quantum
    // 17. GetNextExpectedByte() -> returns 2 * quantum
    // 18. UploadChunk() -> returns success (3 * quantum bytes committed)
    // 18. GetNextExpectedByte() -> returns 3 * quantum
    //
    EXPECT_CALL(*mock, UploadChunk)
        .WillOnce([&](ConstBufferSequence const& p) {
            ++count;
            EXPECT_EQ(3, count);
            EXPECT_TRUE(Equal(p, ConstBufferSequence{ConstBuffer{payload}}));
            return StatusOrVal<ResumableUploadResponse>(TransientError());
        })
        .WillOnce([&](ConstBufferSequence const& p) {
            ++count;
            EXPECT_EQ(7, count);
            EXPECT_TRUE(Equal(p, ConstBufferSequence{ConstBuffer{payload}}));
            return MakeStatusOrVal(ResumableUploadResponse{
                "", ChunkSizeQuantumTest - 1, {}, ResumableUploadResponse::UploadState::InProgress, {}});
        })
        .WillOnce([&](ConstBufferSequence const& p) {
            ++count;
            EXPECT_EQ(11, count);
            EXPECT_TRUE(Equal(p, ConstBufferSequence{ConstBuffer{payload}}));
            return StatusOrVal<ResumableUploadResponse>(TransientError());
        })
        .WillOnce([&](ConstBufferSequence const& p) {
            ++count;
            EXPECT_EQ(14, count);
            EXPECT_TRUE(Equal(p, ConstBufferSequence{ConstBuffer{payload}}));
            return MakeStatusOrVal(ResumableUploadResponse{
                "", 2 * ChunkSizeQuantumTest - 1, {}, ResumableUploadResponse::UploadState::InProgress, {}});
        })
        .WillOnce([&](ConstBufferSequence const& p) {
            ++count;
            EXPECT_EQ(18, count);
            EXPECT_TRUE(Equal(p, ConstBufferSequence{ConstBuffer{payload}}));
            return MakeStatusOrVal(ResumableUploadResponse{
                "", 3 * ChunkSizeQuantumTest - 1, {}, ResumableUploadResponse::UploadState::InProgress, {}});
        });

    EXPECT_CALL(*mock, ResetSession())
        .WillOnce([&]() {
            ++count;
            EXPECT_EQ(4, count);
            return StatusOrVal<ResumableUploadResponse>(TransientError());
        })
        .WillOnce([&]() {
            ++count;
            EXPECT_EQ(5, count);
            return MakeStatusOrVal(
                ResumableUploadResponse{"", 0, {}, ResumableUploadResponse::UploadState::InProgress, {}});
        })
        .WillOnce([&]() {
            ++count;
            EXPECT_EQ(12, count);
            return MakeStatusOrVal(ResumableUploadResponse{
                "", ChunkSizeQuantumTest - 1, {}, ResumableUploadResponse::UploadState::InProgress, {}});
        });

    EXPECT_CALL(*mock, GetNextExpectedByte())
        .WillOnce([&]() {
            EXPECT_EQ(1, ++count);
            return 0;
        })
        .WillOnce([&]() {
            EXPECT_EQ(2, ++count);
            return 0;
        })
        .WillOnce([&]() {
            EXPECT_EQ(6, ++count);
            return 0;
        })
        .WillOnce([&]() {
            EXPECT_EQ(8, ++count);
            return ChunkSizeQuantumTest;
        })
        .WillOnce([&]() {
            EXPECT_EQ(9, ++count);
            return ChunkSizeQuantumTest;
        })
        .WillOnce([&]() {
            EXPECT_EQ(10, ++count);
            return ChunkSizeQuantumTest;
        })
        .WillOnce([&]() {
            EXPECT_EQ(13, ++count);
            return ChunkSizeQuantumTest;
        })
        .WillOnce([&]() {
            EXPECT_EQ(15, ++count);
            return 2 * ChunkSizeQuantumTest;
        })
        .WillOnce([&]() {
            EXPECT_EQ(16, ++count);
            return 2 * ChunkSizeQuantumTest;
        })
        .WillOnce([&]() {
            EXPECT_EQ(17, ++count);
            return 2 * ChunkSizeQuantumTest;
        })
        .WillOnce([&]() {
            EXPECT_EQ(19, ++count);
            return 3 * ChunkSizeQuantumTest;
        });

    RetryResumableUploadSession session(std::move(mock), ::csa::LimitedErrorCountRetryPolicy(10).Clone(),
                                        TestBackoffPolicy());

    StatusOrVal<ResumableUploadResponse> response;
    response = session.UploadChunk({{payload}});
    EXPECT_STATUS_OK(response);
    EXPECT_EQ(ChunkSizeQuantumTest - 1, response->m_lastCommittedByte);

    response = session.UploadChunk({{payload}});
    EXPECT_STATUS_OK(response);
    EXPECT_EQ(2 * ChunkSizeQuantumTest - 1, response->m_lastCommittedByte);

    response = session.UploadChunk({{payload}});
    EXPECT_STATUS_OK(response);
    EXPECT_EQ(3 * ChunkSizeQuantumTest - 1, response->m_lastCommittedByte);
}

/// @test Verify that a permanent error on UploadChunk results in a failure.
TEST_F(RetryResumableUploadSessionTest, PermanentErrorOnUpload)
{
    auto mock = std::make_unique<MockResumableUploadSession>();

    auto const quantum = ChunkSizeQuantumTest;
    std::string const payload(quantum, '0');

    // Keep track of the sequence of calls.
    int count = 0;
    // The sequence of messages is split across two EXPECT_CALL() and hard to see,
    // basically we want this to happen:
    //
    // Ignore GetNextExpectedByte() in tests - it will always return 0.
    // 1. UploadChunk() -> returns permanent error, the request aborts.
    //
    EXPECT_CALL(*mock, UploadChunk).WillOnce([&](ConstBufferSequence const& p) {
        ++count;
        EXPECT_EQ(1, count);
        EXPECT_TRUE(Equal(p, ConstBufferSequence{ConstBuffer{payload}}));
        return StatusOrVal<ResumableUploadResponse>(PermanentError());
    });

    EXPECT_CALL(*mock, GetNextExpectedByte()).WillRepeatedly(Return(0));

    // We only tolerate 4 transient errors, the first call to UploadChunk() will
    // consume the full budget.
    RetryResumableUploadSession session(std::move(mock), ::csa::LimitedErrorCountRetryPolicy(10).Clone(),
                                        TestBackoffPolicy());

    StatusOrVal<ResumableUploadResponse> response = session.UploadChunk({{payload}});
    EXPECT_THAT(response, Not(IsOk()));
}

/// @test Verify that a permanent error on ResetSession results in a failure.
TEST_F(RetryResumableUploadSessionTest, PermanentErrorOnReset)
{
    auto mock = std::make_unique<MockResumableUploadSession>();

    auto const quantum = ChunkSizeQuantumTest;
    std::string const payload(quantum, '0');

    // Keep track of the sequence of calls.
    int count = 0;
    // The sequence of messages is split across two EXPECT_CALL() and hard to see,
    // basically we want this to happen:
    //
    // Ignore GetNextExpectedByte() in tests - it will always return 0.
    // 1. UploadChunk() -> returns transient error
    // 2. ResetSession() -> returns permanent, the request aborts.
    //
    EXPECT_CALL(*mock, UploadChunk).WillOnce([&](ConstBufferSequence const& p) {
        ++count;
        EXPECT_EQ(1, count);
        EXPECT_TRUE(Equal(p, ConstBufferSequence{ConstBuffer{payload}}));
        return StatusOrVal<ResumableUploadResponse>(TransientError());
    });

    EXPECT_CALL(*mock, ResetSession()).WillOnce([&]() {
        ++count;
        EXPECT_EQ(2, count);
        return StatusOrVal<ResumableUploadResponse>(PermanentError());
    });

    EXPECT_CALL(*mock, GetNextExpectedByte()).WillRepeatedly(Return(0));

    // We only tolerate 4 transient errors, the first call to UploadChunk() will
    // consume the full budget.
    RetryResumableUploadSession session(std::move(mock), ::csa::LimitedErrorCountRetryPolicy(10).Clone(),
                                        TestBackoffPolicy());

    StatusOrVal<ResumableUploadResponse> response = session.UploadChunk({{payload}});
    EXPECT_THAT(response, Not(IsOk()));
}

/// @test Verify that too many transients on ResetSession results in a failure.
TEST_F(RetryResumableUploadSessionTest, TooManyTransientOnUploadChunk)
{
    auto mock = std::make_unique<MockResumableUploadSession>();

    auto const quantum = ChunkSizeQuantumTest;
    std::string const payload(quantum, '0');

    // Keep track of the sequence of calls.
    int count = 0;
    // The sequence of messages is split across two EXPECT_CALL() and hard to see,
    // basically we want this to happen:
    //
    // Ignore GetNextExpectedByte() in tests - it will always return 0.
    // 1. UploadChunk() -> returns transient error
    // 2. ResetSession() -> returns success (0 bytes committed)
    // 3. UploadChunk() -> returns transient error
    // 4. ResetSession() -> returns success (0 bytes committed)
    // 5. UploadChunk() -> returns transient error, the policy is exhausted.
    //
    EXPECT_CALL(*mock, UploadChunk)
        .WillOnce([&](ConstBufferSequence const& p) {
            ++count;
            EXPECT_EQ(1, count);
            EXPECT_TRUE(Equal(p, ConstBufferSequence{ConstBuffer{payload}}));
            return StatusOrVal<ResumableUploadResponse>(TransientError());
        })
        .WillOnce([&](ConstBufferSequence const& p) {
            ++count;
            EXPECT_EQ(3, count);
            EXPECT_TRUE(Equal(p, ConstBufferSequence{ConstBuffer{payload}}));
            return StatusOrVal<ResumableUploadResponse>(TransientError());
        })
        .WillOnce([&](ConstBufferSequence const& p) {
            ++count;
            EXPECT_EQ(5, count);
            EXPECT_TRUE(Equal(p, ConstBufferSequence{ConstBuffer{payload}}));
            return StatusOrVal<ResumableUploadResponse>(TransientError());
        });

    EXPECT_CALL(*mock, ResetSession())
        .WillOnce([&]() {
            ++count;
            EXPECT_EQ(2, count);
            return MakeStatusOrVal(
                ResumableUploadResponse{"", 0, {}, ResumableUploadResponse::UploadState::InProgress, {}});
        })
        .WillOnce([&]() {
            ++count;
            EXPECT_EQ(4, count);
            return MakeStatusOrVal(
                ResumableUploadResponse{"", 0, {}, ResumableUploadResponse::UploadState::InProgress, {}});
        });

    EXPECT_CALL(*mock, GetNextExpectedByte()).WillRepeatedly(Return(0));

    // We only tolerate 4 transient errors, the first call to UploadChunk() will
    // consume the full budget.
    RetryResumableUploadSession session(std::move(mock), ::csa::LimitedErrorCountRetryPolicy(2).Clone(),
                                        TestBackoffPolicy());

    StatusOrVal<ResumableUploadResponse> response = session.UploadChunk({{payload}});
    EXPECT_THAT(response, StatusIs(TransientError().Code(), HasSubstr("Retry policy exhausted")));
}

/// @test Verify that too many transients on ResetSession result in a failure.
TEST_F(RetryResumableUploadSessionTest, TooManyTransientOnReset)
{
    auto mock = std::make_unique<MockResumableUploadSession>();

    auto const quantum = ChunkSizeQuantumTest;
    std::string const payload(quantum, '0');

    // Keep track of the sequence of calls.
    int count = 0;
    // The sequence of messages is split across two EXPECT_CALL() and hard to see,
    // basically we want this to happen:
    //
    // RetryResumableUploadSession::UploadChunk() is called
    // 1. GetNextExpectedByte() -> returns 0
    // 2. GetNextExpectedByte() -> returns 0
    // 3. UploadChunk() -> returns transient error
    // 4. ResetSession() -> returns transient error
    // 5. ResetSession() -> returns transient error, the policy is exhausted
    //
    EXPECT_CALL(*mock, UploadChunk).WillOnce([&](ConstBufferSequence const& p) {
        ++count;
        EXPECT_EQ(3, count);
        EXPECT_TRUE(Equal(p, ConstBufferSequence{ConstBuffer{payload}}));
        return StatusOrVal<ResumableUploadResponse>(TransientError());
    });

    EXPECT_CALL(*mock, ResetSession())
        .WillOnce([&]() {
            ++count;
            EXPECT_EQ(4, count);
            return StatusOrVal<ResumableUploadResponse>(TransientError());
        })
        .WillOnce([&]() {
            ++count;
            EXPECT_EQ(5, count);
            return StatusOrVal<ResumableUploadResponse>(TransientError());
        });

    EXPECT_CALL(*mock, GetNextExpectedByte())
        .WillOnce([&]() {
            EXPECT_EQ(1, ++count);
            return 0;
        })
        .WillOnce([&]() {
            EXPECT_EQ(2, ++count);
            return 0;
        });

    // We only tolerate 2 transient errors, the third causes a permanent failure.
    // As described above, the first call to UploadChunk() will consume the full
    // budget.
    RetryResumableUploadSession session(std::move(mock), ::csa::LimitedErrorCountRetryPolicy(2).Clone(),
                                        TestBackoffPolicy());

    StatusOrVal<ResumableUploadResponse> response = session.UploadChunk({{payload}});
    EXPECT_THAT(response, Not(IsOk()));
}

/// @test Verify that transients (or elapsed time) from different chunks do not
/// accumulate.
TEST_F(RetryResumableUploadSessionTest, HandleTransiensOnSeparateChunks)
{
    auto mock = std::make_unique<MockResumableUploadSession>();

    auto const quantum = ChunkSizeQuantumTest;
    std::string const payload(quantum, '0');

    // Keep track of the sequence of calls.
    int count = 0;
    std::uint64_t GetNextExpectedByte = 0;

    // In this test we do not care about how many times or when next
    // expected_byte() is called, but it does need to return the right values,
    // the other mock functions set the correct return value using a local
    // variable.
    EXPECT_CALL(*mock, GetNextExpectedByte()).WillRepeatedly([&]() { return GetNextExpectedByte; });

    // The sequence of messages is split across two EXPECT_CALL() and hard to see,
    // basically we want this to happen:
    //
    // Ignore GetNextExpectedByte() in tests - it will always return 0.
    // 1. UploadChunk() -> returns transient error
    // 2. ResetSession() -> returns success (0 bytes committed)
    // 3. UploadChunk() -> returns success
    // 4. UploadChunk() -> returns transient error
    // 5. ResetSession() -> returns success (0 bytes committed)
    // 6. UploadChunk() -> returns success
    // 7. UploadChunk() -> returns transient error
    // 8. ResetSession() -> returns success (0 bytes committed)
    // 9. UploadChunk() -> returns success
    //
    EXPECT_CALL(*mock, UploadChunk)
        .WillOnce([&](ConstBufferSequence const& p) {
            ++count;
            EXPECT_EQ(1, count);
            EXPECT_TRUE(Equal(p, ConstBufferSequence{ConstBuffer{payload}}));
            return StatusOrVal<ResumableUploadResponse>(TransientError());
        })
        .WillOnce([&](ConstBufferSequence const& p) {
            ++count;
            EXPECT_EQ(3, count);
            EXPECT_TRUE(Equal(p, ConstBufferSequence{ConstBuffer{payload}}));
            GetNextExpectedByte = quantum;
            return MakeStatusOrVal(ResumableUploadResponse{
                "", GetNextExpectedByte - 1, {}, ResumableUploadResponse::UploadState::InProgress, {}});
        })
        .WillOnce([&](ConstBufferSequence const& p) {
            ++count;
            EXPECT_EQ(4, count);
            EXPECT_TRUE(Equal(p, ConstBufferSequence{ConstBuffer{payload}}));
            return StatusOrVal<ResumableUploadResponse>(TransientError());
        })
        .WillOnce([&](ConstBufferSequence const& p) {
            ++count;
            EXPECT_EQ(6, count);
            EXPECT_TRUE(Equal(p, ConstBufferSequence{ConstBuffer{payload}}));
            GetNextExpectedByte = 2 * quantum;
            return MakeStatusOrVal(ResumableUploadResponse{
                "", GetNextExpectedByte - 1, {}, ResumableUploadResponse::UploadState::InProgress, {}});
        })
        .WillOnce([&](ConstBufferSequence const& p) {
            ++count;
            EXPECT_EQ(7, count);
            EXPECT_TRUE(Equal(p, ConstBufferSequence{ConstBuffer{payload}}));
            return StatusOrVal<ResumableUploadResponse>(TransientError());
        })
        .WillOnce([&](ConstBufferSequence const& p) {
            ++count;
            EXPECT_EQ(9, count);
            EXPECT_TRUE(Equal(p, ConstBufferSequence{ConstBuffer{payload}}));
            GetNextExpectedByte = 3 * quantum;
            return MakeStatusOrVal(ResumableUploadResponse{
                "", GetNextExpectedByte - 1, {}, ResumableUploadResponse::UploadState::InProgress, {}});
        });

    EXPECT_CALL(*mock, ResetSession())
        .WillOnce([&]() {
            ++count;
            EXPECT_EQ(2, count);
            return MakeStatusOrVal(
                ResumableUploadResponse{"", 0, {}, ResumableUploadResponse::UploadState::InProgress, {}});
        })
        .WillOnce([&]() {
            ++count;
            EXPECT_EQ(5, count);
            return MakeStatusOrVal(ResumableUploadResponse{
                "", GetNextExpectedByte - 1, {}, ResumableUploadResponse::UploadState::InProgress, {}});
        })
        .WillOnce([&]() {
            ++count;
            EXPECT_EQ(8, count);
            return MakeStatusOrVal(ResumableUploadResponse{
                "", GetNextExpectedByte - 1, {}, ResumableUploadResponse::UploadState::InProgress, {}});
        });

    // Configure a session that tolerates 2 transient errors per call. None of
    // the calls to UploadChunk() should use more than these.
    RetryResumableUploadSession session(std::move(mock), ::csa::LimitedErrorCountRetryPolicy(2).Clone(),
                                        TestBackoffPolicy());

    StatusOrVal<ResumableUploadResponse> response = session.UploadChunk({{payload}});
    EXPECT_STATUS_OK(response);
    EXPECT_EQ(response->m_lastCommittedByte, quantum - 1);

    response = session.UploadChunk({{payload}});
    EXPECT_STATUS_OK(response);
    EXPECT_EQ(response->m_lastCommittedByte, 2 * quantum - 1);

    response = session.UploadChunk({{payload}});
    EXPECT_STATUS_OK(response);
    EXPECT_EQ(response->m_lastCommittedByte, 3 * quantum - 1);
}

/// @test Verify that a permanent error on UploadFinalChunk results in a
/// failure.
TEST_F(RetryResumableUploadSessionTest, PermanentErrorOnUploadFinalChunk)
{
    auto mock = std::make_unique<MockResumableUploadSession>();

    auto quantum = ChunkSizeQuantumTest;
    std::string const payload(quantum, '0');

    // Keep track of the sequence of calls.
    int count = 0;
    // The sequence of messages is split across two EXPECT_CALL() and hard to see,
    // basically we want this to happen:
    //
    // Ignore GetNextExpectedByte() in tests - it will always return 0.
    // 1. UploadChunk() -> returns permanent error, the request aborts.
    //
    EXPECT_CALL(*mock, UploadFinalChunk).WillOnce([&](ConstBufferSequence const& p, std::uint64_t s) {
        ++count;
        EXPECT_EQ(1, count);
        EXPECT_TRUE(Equal(p, ConstBufferSequence{ConstBuffer{payload}}));
        EXPECT_EQ(quantum, s);
        return StatusOrVal<ResumableUploadResponse>(PermanentError());
    });

    EXPECT_CALL(*mock, GetNextExpectedByte()).WillRepeatedly(Return(0));

    // We only tolerate 4 transient errors, the first call to UploadChunk() will
    // consume the full budget.
    RetryResumableUploadSession session(std::move(mock), ::csa::LimitedErrorCountRetryPolicy(10).Clone(),
                                        TestBackoffPolicy());

    auto response = session.UploadFinalChunk({{payload}}, quantum);
    EXPECT_THAT(response, StatusIs(PermanentError().Code()));
}

/// @test Verify that too many transients on UploadFinalChunk result in a
/// failure.
TEST_F(RetryResumableUploadSessionTest, TooManyTransientOnUploadFinalChunk)
{
    auto mock = std::make_unique<MockResumableUploadSession>();

    auto quantum = ChunkSizeQuantumTest;
    std::string const payload(quantum, '0');

    // Keep track of the sequence of calls.
    int count = 0;
    // The sequence of messages is split across two EXPECT_CALL() and hard to see,
    // basically we want this to happen:
    //
    // Ignore GetNextExpectedByte() in tests - it will always return 0.
    // 1. UploadFinalChunk() -> returns transient error
    // 2. ResetSession() -> returns success (0 bytes committed)
    // 3. UploadFinalChunk() -> returns transient error
    // 4. ResetSession() -> returns success (0 bytes committed)
    // 5. UploadFinalChunk() -> returns transient error, the policy is exhausted.
    //
    EXPECT_CALL(*mock, UploadFinalChunk)
        .WillOnce([&](ConstBufferSequence const& p, std::uint64_t s) {
            ++count;
            EXPECT_EQ(1, count);
            EXPECT_TRUE(Equal(p, ConstBufferSequence{ConstBuffer{payload}}));
            EXPECT_EQ(quantum, s);
            return StatusOrVal<ResumableUploadResponse>(TransientError());
        })
        .WillOnce([&](ConstBufferSequence const& p, std::uint64_t s) {
            ++count;
            EXPECT_EQ(3, count);
            EXPECT_TRUE(Equal(p, ConstBufferSequence{ConstBuffer{payload}}));
            EXPECT_EQ(quantum, s);
            return StatusOrVal<ResumableUploadResponse>(TransientError());
        })
        .WillOnce([&](ConstBufferSequence const& p, std::uint64_t s) {
            ++count;
            EXPECT_EQ(5, count);
            EXPECT_TRUE(Equal(p, ConstBufferSequence{ConstBuffer{payload}}));
            EXPECT_EQ(quantum, s);
            return StatusOrVal<ResumableUploadResponse>(TransientError());
        });

    EXPECT_CALL(*mock, ResetSession())
        .WillOnce([&]() {
            ++count;
            EXPECT_EQ(2, count);
            return MakeStatusOrVal(
                ResumableUploadResponse{"", 0, {}, ResumableUploadResponse::UploadState::InProgress, {}});
        })
        .WillOnce([&]() {
            ++count;
            EXPECT_EQ(4, count);
            return MakeStatusOrVal(
                ResumableUploadResponse{"", 0, {}, ResumableUploadResponse::UploadState::InProgress, {}});
        });

    EXPECT_CALL(*mock, GetNextExpectedByte()).WillRepeatedly(Return(0));

    // We only tolerate 4 transient errors, the first call to UploadChunk() will
    // consume the full budget.
    RetryResumableUploadSession session(std::move(mock), ::csa::LimitedErrorCountRetryPolicy(2).Clone(),
                                        TestBackoffPolicy());

    auto response = session.UploadFinalChunk({{payload}}, quantum);
    EXPECT_THAT(response, Not(IsOk()));
}

TEST(RetryResumableUploadSession, Done)
{
    auto mock = std::make_unique<MockResumableUploadSession>();
    EXPECT_CALL(*mock, Done()).WillOnce(Return(true));

    RetryResumableUploadSession session(std::move(mock), ::csa::LimitedTimeRetryPolicy(std::chrono::seconds(0)).Clone(),
                                        TestBackoffPolicy());
    EXPECT_TRUE(session.Done());
}

TEST(RetryResumableUploadSession, LastResponse)
{
    auto mock = std::make_unique<MockResumableUploadSession>();
    StatusOrVal<ResumableUploadResponse> const last_response(
        ResumableUploadResponse{"url", 1, {}, ResumableUploadResponse::UploadState::Done, {}});
    EXPECT_CALL(*mock, GetLastResponse()).WillOnce(ReturnRef(last_response));

    RetryResumableUploadSession session(std::move(mock), ::csa::LimitedTimeRetryPolicy(std::chrono::seconds(0)).Clone(),
                                        TestBackoffPolicy());
    StatusOrVal<ResumableUploadResponse> const& result = session.GetLastResponse();
    ASSERT_STATUS_OK(result);
    EXPECT_EQ(result.Value(), last_response.Value());
}

TEST(RetryResumableUploadSession, UploadChunkPolicyExhaustedOnStart)
{
    auto mock = std::make_unique<MockResumableUploadSession>();
    EXPECT_CALL(*mock, GetNextExpectedByte()).WillRepeatedly(Return(0));
    RetryResumableUploadSession session(std::move(mock), ::csa::LimitedTimeRetryPolicy(std::chrono::seconds(0)).Clone(),
                                        TestBackoffPolicy());

    std::string data(ChunkSizeQuantumTest, 'X');
    auto res = session.UploadChunk({{data}});
    EXPECT_THAT(res, StatusIs(StatusCode::DeadlineExceeded, HasSubstr("Retry policy exhausted before first attempt")));
}

TEST(RetryResumableUploadSession, UploadFinalChunkPolicyExhaustedOnStart)
{
    auto mock = std::make_unique<MockResumableUploadSession>();
    EXPECT_CALL(*mock, GetNextExpectedByte()).WillRepeatedly(Return(0));
    RetryResumableUploadSession session(std::move(mock), ::csa::LimitedTimeRetryPolicy(std::chrono::seconds(0)).Clone(),
                                        TestBackoffPolicy());

    auto res = session.UploadFinalChunk({ConstBuffer{"blah", 4}}, 4);
    EXPECT_THAT(res, StatusIs(StatusCode::DeadlineExceeded, HasSubstr("Retry policy exhausted before first attempt")));
}

TEST(RetryResumableUploadSession, ResetSessionPolicyExhaustedOnStart)
{
    auto mock = std::make_unique<MockResumableUploadSession>();
    RetryResumableUploadSession session(std::move(mock), ::csa::LimitedTimeRetryPolicy(std::chrono::seconds(0)).Clone(),
                                        TestBackoffPolicy());
    auto res = session.ResetSession();
    EXPECT_THAT(res, StatusIs(StatusCode::DeadlineExceeded, HasSubstr("Retry policy exhausted before first attempt")));
}

/// @test Verify that transient failures which move next_bytes are handled
TEST_F(RetryResumableUploadSessionTest, HandleTransientPartialFailures)
{
    auto mock = std::make_unique<MockResumableUploadSession>();

    auto const quantum = ChunkSizeQuantumTest;
    std::string const payload(std::string(quantum, 'X') + std::string(quantum, 'Y') + std::string(quantum, 'Z'));

    std::string const payload_final(std::string(quantum, 'A') + std::string(quantum, 'B') + std::string(quantum, 'C'));

    // Keep track of the sequence of calls.
    int count = 0;
    // The sequence of messages is split across two EXPECT_CALL() and hard to see,
    // basically we want this to happen:
    //
    // 1. GetNextExpectedByte() -> returns 0
    // 2. GetNextExpectedByte() -> returns 0
    // 3. UploadChunk() -> returns transient error
    // 4. ResetSession() -> returns success (quantum bytes committed)
    // 5. GetNextExpectedByte() -> returns quantum
    // 6. UploadChunk() -> returns transient error
    // 7. ResetSession() -> returns success (2 * quantum bytes committed)
    // 8. GetNextExpectedByte() -> returns 2 * quantum
    // 9. UploadChunk() -> returns success (3 * quantum bytes committed)
    // 10. GetNextExpectedByte() -> returns 3 * quantum
    //
    // 11. GetNextExpectedByte() -> returns 3 * quantum
    // 12. GetNextExpectedByte() -> returns 3 * quantum
    // 13. UploadFinalChunk() -> returns transient error
    // 14. ResetSession() -> returns success (4 * quantum bytes committed)
    // 15. GetNextExpectedByte() -> returns 4 * quantum
    // 16. UploadFinalChunk() -> returns transient error
    // 17. ResetSession() -> returns success (5 * quantum bytes committed)
    // 18. GetNextExpectedByte() -> returns 5 * quantum
    // 19. UploadFinalChunk() -> returns success (6 * quantum bytes committed)
    // 20. GetNextExpectedByte() -> returns 6 * quantum
    EXPECT_CALL(*mock, UploadChunk)
        .WillOnce([&](ConstBufferSequence const& p) {
            ++count;
            EXPECT_EQ(3, count);
            EXPECT_EQ(3 * quantum, TotalBytes(p));
            EXPECT_EQ('X', p[0][0]);
            return StatusOrVal<ResumableUploadResponse>(TransientError());
        })
        .WillOnce([&](ConstBufferSequence const& p) {
            ++count;
            EXPECT_EQ(6, count);
            EXPECT_EQ(2 * quantum, TotalBytes(p));
            EXPECT_EQ('Y', p[0][0]);
            return StatusOrVal<ResumableUploadResponse>(TransientError());
        })
        .WillOnce([&](ConstBufferSequence const& p) {
            ++count;
            EXPECT_EQ(9, count);
            EXPECT_EQ(quantum, TotalBytes(p));
            EXPECT_EQ('Z', p[0][0]);
            return MakeStatusOrVal(
                ResumableUploadResponse{"", 3 * quantum - 1, {}, ResumableUploadResponse::UploadState::InProgress, {}});
        });
    EXPECT_CALL(*mock, UploadFinalChunk)
        .WillOnce([&](ConstBufferSequence const& p, std::uint64_t) {
            ++count;
            EXPECT_EQ(13, count);
            EXPECT_EQ(3 * quantum, TotalBytes(p));
            EXPECT_EQ('A', p[0][0]);
            return StatusOrVal<ResumableUploadResponse>(TransientError());
        })
        .WillOnce([&](ConstBufferSequence const& p, std::uint64_t) {
            ++count;
            EXPECT_EQ(16, count);
            EXPECT_EQ(2 * quantum, TotalBytes(p));
            EXPECT_EQ('B', p[0][0]);
            return StatusOrVal<ResumableUploadResponse>(TransientError());
        })
        .WillOnce([&](ConstBufferSequence const& p, std::uint64_t) {
            ++count;
            EXPECT_EQ(19, count);
            EXPECT_EQ(quantum, TotalBytes(p));
            EXPECT_EQ('C', p[0][0]);
            return MakeStatusOrVal(
                ResumableUploadResponse{"", 6 * quantum - 1, {}, ResumableUploadResponse::UploadState::Done, {}});
        });

    EXPECT_CALL(*mock, ResetSession())
        .WillOnce([&]() {
            ++count;
            EXPECT_EQ(4, count);
            return MakeStatusOrVal(
                ResumableUploadResponse{"", quantum - 1, {}, ResumableUploadResponse::UploadState::InProgress, {}});
        })
        .WillOnce([&]() {
            ++count;
            EXPECT_EQ(7, count);
            return MakeStatusOrVal(
                ResumableUploadResponse{"", 2 * quantum - 1, {}, ResumableUploadResponse::UploadState::InProgress, {}});
        })
        .WillOnce([&]() {
            ++count;
            EXPECT_EQ(14, count);
            return MakeStatusOrVal(
                ResumableUploadResponse{"", 4 * quantum - 1, {}, ResumableUploadResponse::UploadState::InProgress, {}});
        })
        .WillOnce([&]() {
            ++count;
            EXPECT_EQ(17, count);
            return MakeStatusOrVal(
                ResumableUploadResponse{"", 5 * quantum - 1, {}, ResumableUploadResponse::UploadState::InProgress, {}});
        });

    EXPECT_CALL(*mock, GetNextExpectedByte())
        .WillOnce([&]() {
            EXPECT_EQ(1, ++count);
            return 0;
        })
        .WillOnce([&]() {
            EXPECT_EQ(2, ++count);
            return 0;
        })
        .WillOnce([&]() {
            EXPECT_EQ(5, ++count);
            return quantum;
        })
        .WillOnce([&]() {
            EXPECT_EQ(8, ++count);
            return 2 * quantum;
        })
        .WillOnce([&]() {
            EXPECT_EQ(10, ++count);
            return 3 * quantum;
        })
        .WillOnce([&]() {
            EXPECT_EQ(11, ++count);
            return 3 * quantum;
        })
        .WillOnce([&]() {
            EXPECT_EQ(12, ++count);
            return 3 * quantum;
        })
        .WillOnce([&]() {
            EXPECT_EQ(15, ++count);
            return 4 * quantum;
        })
        .WillOnce([&]() {
            EXPECT_EQ(18, ++count);
            return 5 * quantum;
        });

    RetryResumableUploadSession session(std::move(mock), ::csa::LimitedErrorCountRetryPolicy(10).Clone(),
                                        TestBackoffPolicy());

    StatusOrVal<ResumableUploadResponse> response;
    response = session.UploadChunk({{payload}});
    EXPECT_STATUS_OK(response);
    EXPECT_EQ(3 * quantum - 1, response->m_lastCommittedByte);

    response = session.UploadFinalChunk({{payload_final}}, 6 * quantum);
    EXPECT_STATUS_OK(response);
    EXPECT_EQ(6 * quantum - 1, response->m_lastCommittedByte);
}

/// @test Verify that erroneous server behavior (uncommitting data) is handled.
TEST_F(RetryResumableUploadSessionTest, UploadFinalChunkUncommitted)
{
    auto mock = std::make_unique<MockResumableUploadSession>();

    auto const quantum = ChunkSizeQuantumTest;
    std::string const payload(std::string(quantum, 'X'));

    // Keep track of the sequence of calls.
    int count = 0;
    // The sequence of messages is split across two EXPECT_CALL() and hard to see,
    // basically we want this to happen:
    //
    // 1. GetNextExpectedByte() -> returns 0
    // 2. GetNextExpectedByte() -> returns 0
    // 3. UploadChunk() -> returns success (quantum bytes committed)
    // 4. GetNextExpectedByte() -> returns quantum
    //
    // 5. GetNextExpectedByte() -> returns quantum
    // 6. GetNextExpectedByte() -> returns quantum
    // 7. UploadFinalChunk() -> returns transient error
    // 8. ResetSession() -> returns success (0 bytes committed)
    // 9. GetNextExpectedByte() -> returns 0
    EXPECT_CALL(*mock, UploadChunk).WillOnce([&](ConstBufferSequence const&) {
        ++count;
        EXPECT_EQ(3, count);
        return MakeStatusOrVal(
            ResumableUploadResponse{"", quantum - 1, {}, ResumableUploadResponse::UploadState::InProgress, {}});
    });
    EXPECT_CALL(*mock, UploadFinalChunk).WillOnce([&](ConstBufferSequence const&, std::uint64_t) {
        ++count;
        EXPECT_EQ(7, count);
        return StatusOrVal<ResumableUploadResponse>(TransientError());
    });

    EXPECT_CALL(*mock, ResetSession()).WillOnce([&]() {
        ++count;
        EXPECT_EQ(8, count);
        return MakeStatusOrVal(
            ResumableUploadResponse{"", 0, {}, ResumableUploadResponse::UploadState::InProgress, {}});
    });

    EXPECT_CALL(*mock, GetNextExpectedByte())
        .WillOnce([&]() {
            EXPECT_EQ(1, ++count);
            return 0;
        })
        .WillOnce([&]() {
            EXPECT_EQ(2, ++count);
            return 0;
        })
        .WillOnce([&]() {
            EXPECT_EQ(4, ++count);
            return quantum;
        })
        .WillOnce([&]() {
            EXPECT_EQ(5, ++count);
            return quantum;
        })
        .WillOnce([&]() {
            EXPECT_EQ(6, ++count);
            return quantum;
        })
        .WillOnce([&]() {
            EXPECT_EQ(9, ++count);
            return 0;
        });

    StatusOrVal<ResumableUploadResponse> response;
    EXPECT_CALL(*mock, GetLastResponse()).WillOnce([&response]() -> StatusOrVal<ResumableUploadResponse> const& {
        return response;
    });

    RetryResumableUploadSession session(std::move(mock), ::csa::LimitedErrorCountRetryPolicy(10).Clone(),
                                        TestBackoffPolicy());

    response = session.UploadChunk({{payload}});
    EXPECT_STATUS_OK(response);
    EXPECT_EQ(quantum - 1, response->m_lastCommittedByte);

    response = session.UploadFinalChunk({{payload}}, 2 * quantum);
    ASSERT_FALSE(response);
    EXPECT_THAT(response, StatusIs(StatusCode::Internal, HasSubstr("github")));
}

/// @test Verify that retry exhaustion following a short write fails.
TEST_F(RetryResumableUploadSessionTest, ShortWriteRetryExhausted)
{
    auto mock = std::make_unique<MockResumableUploadSession>();

    auto const quantum = ChunkSizeQuantumTest;
    std::string const payload(std::string(quantum * 2, 'X'));

    // Keep track of the sequence of calls.
    int count = 0;
    std::uint64_t neb = 0;
    EXPECT_CALL(*mock, GetNextExpectedByte()).WillRepeatedly([&]() { return neb; });

    // The sequence of messages is split across two EXPECT_CALL() and hard to see,
    // basically we want this to happen:
    //
    // 1. UploadChunk() -> success (quantum committed instead of 2*quantum)
    // 2. UploadChunk() -> transient
    // 3. Retry policy is exhausted.
    //
    EXPECT_CALL(*mock, UploadChunk)
        .WillOnce([&](ConstBufferSequence const&) {
            ++count;
            EXPECT_EQ(1, count);
            return MakeStatusOrVal(
                ResumableUploadResponse{"", neb - 1, {}, ResumableUploadResponse::UploadState::InProgress, {}});
        })
        .WillOnce([&](ConstBufferSequence const& p) {
            ++count;
            EXPECT_EQ(2, count);
            EXPECT_EQ(TotalBytes(p), payload.size() - neb);
            return StatusOrVal<ResumableUploadResponse>(TransientError());
        })
        .WillOnce([&](ConstBufferSequence const& p) {
            ++count;
            EXPECT_EQ(3, count);
            EXPECT_EQ(TotalBytes(p), payload.size() - neb);
            return StatusOrVal<ResumableUploadResponse>(TransientError());
        })
        .WillOnce([&](ConstBufferSequence const& p) {
            ++count;
            EXPECT_EQ(4, count);
            EXPECT_EQ(TotalBytes(p), payload.size() - neb);
            return StatusOrVal<ResumableUploadResponse>(TransientError());
        });

    EXPECT_CALL(*mock, ResetSession()).WillRepeatedly([&]() {
        return MakeStatusOrVal(
            ResumableUploadResponse{"", neb - 1, {}, ResumableUploadResponse::UploadState::InProgress, {}});
    });

    RetryResumableUploadSession session(std::move(mock), ::csa::LimitedErrorCountRetryPolicy(2).Clone(),
                                        TestBackoffPolicy());

    StatusOrVal<ResumableUploadResponse> response;
    response = session.UploadChunk({{payload}});
    EXPECT_THAT(response, StatusIs(StatusCode::Unavailable));
}

/// @test Verify that short writes are retried.
TEST_F(RetryResumableUploadSessionTest, ShortWriteRetrySucceeds)
{
    auto mock = std::make_unique<MockResumableUploadSession>();

    auto const quantum = ChunkSizeQuantumTest;
    std::string const payload(std::string(quantum * 2, 'X'));

    // Keep track of the sequence of calls.
    int count = 0;
    std::uint64_t neb = 0;
    EXPECT_CALL(*mock, GetNextExpectedByte()).WillRepeatedly([&]() { return neb; });

    // The sequence of messages is split across two EXPECT_CALL() and hard to see,
    // basically we want this to happen:
    //
    // 1. UploadChunk() -> success (quantum committed instead of 2*quantum)
    // 2. UploadChunk() -> success (2* quantum committed)
    //
    EXPECT_CALL(*mock, UploadChunk)
        .WillOnce([&](ConstBufferSequence const&) {
            ++count;
            EXPECT_EQ(1, count);
            neb = quantum;
            return MakeStatusOrVal(
                ResumableUploadResponse{"", neb - 1, {}, ResumableUploadResponse::UploadState::InProgress, {}});
        })
        .WillOnce([&](ConstBufferSequence const&) {
            ++count;
            EXPECT_EQ(2, count);
            neb = 2 * quantum;
            return MakeStatusOrVal(
                ResumableUploadResponse{"", neb - 1, {}, ResumableUploadResponse::UploadState::InProgress, {}});
        });

    EXPECT_CALL(*mock, ResetSession()).WillRepeatedly([&]() {
        return MakeStatusOrVal(
            ResumableUploadResponse{"", neb - 1, {}, ResumableUploadResponse::UploadState::InProgress, {}});
    });

    RetryResumableUploadSession session(std::move(mock), ::csa::LimitedErrorCountRetryPolicy(10).Clone(),
                                        TestBackoffPolicy());

    StatusOrVal<ResumableUploadResponse> response;
    response = session.UploadChunk({{payload}});
    ASSERT_STATUS_OK(response);
    EXPECT_EQ(2 * quantum - 1, response->m_lastCommittedByte);
}

}  // namespace internal
}  // namespace csa
