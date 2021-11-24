// Copyright 2021 Andrew Karasyov
//
// Copyright 2021 Google LLC
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

#include "cloudstorageapi/internal/file_write_streambuf.h"
#include "cloudstorageapi/file_metadata.h"
#include "mock_cloud_storage_client.h"
#include "mock_resumable_upload_session.h"
#include "util/const_buffer_equality.h"
#include "util/status_matchers.h"
#include <gmock/gmock.h>

namespace csa {
namespace internal {

using ::csa::testing::util::Equal;
using ::csa::testing::util::StatusIs;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::InSequence;
using ::testing::InvokeWithoutArgs;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRef;

namespace {
// used only for test.
constexpr std::size_t ChunkSizeQuantumTest = 1 * 1024UL;
}  // namespace

/// @test Verify that uploading an empty stream creates a single chunk.
TEST(FileWriteStreambufTest, EmptyStream)
{
    auto mock = std::make_unique<MockResumableUploadSession>();
    EXPECT_CALL(*mock, Done).WillRepeatedly(Return(false));
    EXPECT_CALL(*mock, GetFileChunkSizeQuantum).WillRepeatedly(Return(ChunkSizeQuantumTest));

    auto const quantum = mock->GetFileChunkSizeQuantum();

    int count = 0;
    EXPECT_CALL(*mock, UploadFinalChunk).WillOnce([&](ConstBufferSequence const& p, std::uint64_t s) {
        ++count;
        EXPECT_EQ(1, count);
        EXPECT_EQ(0, TotalBytes(p));
        EXPECT_EQ(0, s);
        return MakeStatusOrVal(
            ResumableUploadResponse{"{}", 0, {}, ResumableUploadResponse::UploadState::InProgress, {}});
    });
    EXPECT_CALL(*mock, GetNextExpectedByte()).WillOnce(Return(0));

    FileWriteStream stream(std::make_unique<FileWriteStreambuf>(std::move(mock), quantum, AutoFinalizeConfig::Enabled));
    stream.Close();
    EXPECT_STATUS_OK(stream.GetLastStatus());
}

/// @test Verify that streams auto-finalize if enabled.
TEST(FileWriteStreambufTest, AutoFinalizeEnabled)
{
    auto mock = std::make_unique<MockResumableUploadSession>();
    EXPECT_CALL(*mock, Done).WillRepeatedly(Return(false));
    EXPECT_CALL(*mock, GetFileChunkSizeQuantum).WillRepeatedly(Return(ChunkSizeQuantumTest));

    auto const quantum = mock->GetFileChunkSizeQuantum();

    int count = 0;
    EXPECT_CALL(*mock, UploadFinalChunk).WillOnce([&](ConstBufferSequence const& p, std::uint64_t s) {
        ++count;
        EXPECT_EQ(1, count);
        EXPECT_EQ(0, TotalBytes(p));
        EXPECT_EQ(0, s);
        return MakeStatusOrVal(ResumableUploadResponse{{}, 0, {}, ResumableUploadResponse::UploadState::Done, {}});
    });
    EXPECT_CALL(*mock, GetNextExpectedByte()).WillOnce(Return(0));

    {
        FileWriteStream stream(
            std::make_unique<FileWriteStreambuf>(std::move(mock), quantum, AutoFinalizeConfig::Enabled));
    }
}

/// @test Verify that streams do not auto-finalize if so configured.
TEST(FileWriteStreambufTest, AutoFinalizeDisabled)
{
    auto mock = std::make_unique<MockResumableUploadSession>();
    EXPECT_CALL(*mock, Done).WillRepeatedly(Return(false));
    EXPECT_CALL(*mock, GetFileChunkSizeQuantum).WillRepeatedly(Return(ChunkSizeQuantumTest));

    auto const quantum = mock->GetFileChunkSizeQuantum();

    EXPECT_CALL(*mock, UploadFinalChunk).Times(0);
    EXPECT_CALL(*mock, GetNextExpectedByte()).Times(0);

    {
        FileWriteStream stream(
            std::make_unique<FileWriteStreambuf>(std::move(mock), quantum, AutoFinalizeConfig::Disabled));
    }
}

/// @test Verify that uploading a small stream creates a single chunk.
TEST(FileWriteStreambufTest, SmallStream)
{
    auto mock = std::make_unique<MockResumableUploadSession>();
    EXPECT_CALL(*mock, Done).WillRepeatedly(Return(false));
    EXPECT_CALL(*mock, GetFileChunkSizeQuantum).WillRepeatedly(Return(ChunkSizeQuantumTest));

    auto const quantum = mock->GetFileChunkSizeQuantum();
    std::string const payload = "small test payload";

    int count = 0;
    EXPECT_CALL(*mock, UploadFinalChunk).WillOnce([&](ConstBufferSequence const& p, std::uint64_t s) {
        ++count;
        EXPECT_EQ(1, count);
        ;
        EXPECT_TRUE(Equal(p, ConstBufferSequence{ConstBuffer{payload}}));
        EXPECT_EQ(payload.size(), s);
        auto last_committed_byte = payload.size() - 1;
        return MakeStatusOrVal(ResumableUploadResponse{
            "{}", last_committed_byte, {}, ResumableUploadResponse::UploadState::InProgress, {}});
    });
    EXPECT_CALL(*mock, GetNextExpectedByte()).WillOnce(Return(0));

    FileWriteStreambuf streambuf(std::move(mock), quantum, AutoFinalizeConfig::Enabled);

    streambuf.sputn(payload.data(), payload.size());
    auto response = streambuf.Close();
    EXPECT_STATUS_OK(response);
}

/// @test Verify that uploading a stream which ends on a upload chunk quantum
/// works as expected.
TEST(FileWriteStreambufTest, EmptyTrailer)
{
    auto mock = std::make_unique<MockResumableUploadSession>();
    EXPECT_CALL(*mock, Done).WillRepeatedly(Return(false));
    EXPECT_CALL(*mock, GetFileChunkSizeQuantum).WillRepeatedly(Return(ChunkSizeQuantumTest));

    auto const quantum = mock->GetFileChunkSizeQuantum();
    std::string const payload(quantum, '*');

    int count = 0;
    size_t next_byte = 0;
    EXPECT_CALL(*mock, UploadChunk).WillOnce([&](ConstBufferSequence const& p) {
        ++count;
        EXPECT_EQ(1, count);
        EXPECT_TRUE(Equal(p, ConstBufferSequence{ConstBuffer{payload}}));
        auto last_committed_byte = payload.size() - 1;
        next_byte = last_committed_byte + 1;
        return MakeStatusOrVal(
            ResumableUploadResponse{"", last_committed_byte, {}, ResumableUploadResponse::UploadState::InProgress, {}});
    });
    EXPECT_CALL(*mock, UploadFinalChunk).WillOnce([&](ConstBufferSequence const& p, std::uint64_t s) {
        ++count;
        EXPECT_EQ(2, count);
        EXPECT_EQ(0, TotalBytes(p));
        EXPECT_EQ(quantum, s);
        auto last_committed_byte = quantum - 1;
        return MakeStatusOrVal(ResumableUploadResponse{
            "{}", last_committed_byte, {}, ResumableUploadResponse::UploadState::InProgress, {}});
    });
    EXPECT_CALL(*mock, GetNextExpectedByte()).WillRepeatedly([&]() { return next_byte; });

    FileWriteStreambuf streambuf(std::move(mock), quantum, AutoFinalizeConfig::Enabled);

    streambuf.sputn(payload.data(), payload.size());
    auto response = streambuf.Close();
    EXPECT_STATUS_OK(response);
}

MATCHER_P(ConstBufSeqIs, seq, "ConstBufferSequence has expected elements") { return Equal(arg, seq); }

/// @test Verify that a stream sends a single message for large payloads.
TEST(FileWriteStreambufTest, FlushAfterLargePayload)
{
    auto mock = std::make_unique<MockResumableUploadSession>();
    EXPECT_CALL(*mock, Done).WillRepeatedly(Return(false));
    EXPECT_CALL(*mock, GetFileChunkSizeQuantum).WillRepeatedly(Return(ChunkSizeQuantumTest));

    auto const quantum = mock->GetFileChunkSizeQuantum();
    std::string const payload_1(3 * quantum, '*');
    std::string const payload_2("trailer");
    size_t next_byte = 0;
    {
        InSequence seq;
        EXPECT_CALL(*mock, UploadChunk).WillOnce(InvokeWithoutArgs([&]() {
            next_byte = payload_1.size();
            return MakeStatusOrVal(ResumableUploadResponse{
                "", payload_1.size() - 1, {}, ResumableUploadResponse::UploadState::InProgress, {}});
        }));
        EXPECT_CALL(*mock, UploadFinalChunk(ConstBufSeqIs(ConstBufferSequence{{payload_2}}),
                                            payload_1.size() + payload_2.size()))
            .WillOnce(Return(MakeStatusOrVal(ResumableUploadResponse{"{}",
                                                                     payload_1.size() + payload_2.size() - 1,
                                                                     {},
                                                                     ResumableUploadResponse::UploadState::InProgress,
                                                                     {}})));
    }
    EXPECT_CALL(*mock, GetNextExpectedByte()).WillRepeatedly([&]() { return next_byte; });

    FileWriteStreambuf streambuf(std::move(mock), 3 * quantum, AutoFinalizeConfig::Enabled);

    streambuf.sputn(payload_1.data(), payload_1.size());
    streambuf.sputn(payload_2.data(), payload_2.size());
    auto response = streambuf.Close();
    EXPECT_STATUS_OK(response);
}

/// @test Verify that a stream flushes when a full quantum is available.
TEST(FileWriteStreambufTest, FlushAfterFullQuantum)
{
    auto mock = std::make_unique<MockResumableUploadSession>();
    EXPECT_CALL(*mock, Done).WillRepeatedly(Return(false));
    EXPECT_CALL(*mock, GetFileChunkSizeQuantum).WillRepeatedly(Return(ChunkSizeQuantumTest));

    auto const quantum = mock->GetFileChunkSizeQuantum();
    std::string const payload_1("header");
    std::string const payload_2(quantum, '*');

    int count = 0;
    size_t next_byte = 0;
    EXPECT_CALL(*mock, UploadChunk).WillOnce([&](ConstBufferSequence const& p) {
        ++count;
        EXPECT_EQ(1, count);
        auto trailer = payload_2.substr(0, quantum - payload_1.size());
        EXPECT_TRUE(Equal(p, ConstBufferSequence{ConstBuffer{payload_1}, ConstBuffer{trailer}}));
        next_byte += TotalBytes(p);
        return MakeStatusOrVal(
            ResumableUploadResponse{"", quantum - 1, {}, ResumableUploadResponse::UploadState::InProgress, {}});
    });
    EXPECT_CALL(*mock, UploadFinalChunk).WillOnce([&](ConstBufferSequence const& p, std::uint64_t s) {
        ++count;
        EXPECT_EQ(2, count);
        auto expected = payload_2.substr(payload_2.size() - payload_1.size());
        EXPECT_TRUE(Equal(p, ConstBufferSequence{ConstBuffer{expected}}));
        EXPECT_EQ(payload_1.size() + payload_2.size(), s);
        auto last_committed_byte = payload_1.size() + payload_2.size() - 1;
        return MakeStatusOrVal(ResumableUploadResponse{
            "{}", last_committed_byte, {}, ResumableUploadResponse::UploadState::InProgress, {}});
    });
    EXPECT_CALL(*mock, GetNextExpectedByte()).WillRepeatedly([&]() { return next_byte; });

    FileWriteStreambuf streambuf(std::move(mock), quantum, AutoFinalizeConfig::Enabled);

    streambuf.sputn(payload_1.data(), payload_1.size());
    streambuf.sputn(payload_2.data(), payload_2.size());
    auto response = streambuf.Close();
    EXPECT_STATUS_OK(response);
}

/// @test Verify that a stream flushes when adding one character at a time.
TEST(FileWriteStreambufTest, OverflowFlushAtFullQuantum)
{
    auto mock = std::make_unique<MockResumableUploadSession>();
    EXPECT_CALL(*mock, GetFileChunkSizeQuantum).WillRepeatedly(Return(ChunkSizeQuantumTest));

    auto const quantum = mock->GetFileChunkSizeQuantum();
    std::string const payload(quantum, '*');

    int count = 0;
    std::size_t next_byte = 0;
    EXPECT_CALL(*mock, GetNextExpectedByte()).WillRepeatedly([&]() { return next_byte; });
    bool mock_done = false;
    EXPECT_CALL(*mock, Done).WillRepeatedly([&] { return mock_done; });
    EXPECT_CALL(*mock, UploadChunk).WillOnce([&](ConstBufferSequence const& p) {
        ++count;
        EXPECT_EQ(1, count);
        EXPECT_THAT(p, ConstBufSeqIs(ConstBufferSequence{ConstBuffer{payload}}));
        next_byte += TotalBytes(p);
        return MakeStatusOrVal(
            ResumableUploadResponse{"", next_byte - 1, {}, ResumableUploadResponse::UploadState::InProgress, {}});
    });
    EXPECT_CALL(*mock, UploadFinalChunk).WillOnce([&](ConstBufferSequence const& p, std::uint64_t s) {
        ++count;
        EXPECT_EQ(2, count);
        std::string expected = " ";
        EXPECT_THAT(p, ConstBufSeqIs(ConstBufferSequence{ConstBuffer{expected}}));
        next_byte += 1;
        EXPECT_EQ(next_byte, s);
        mock_done = true;
        return MakeStatusOrVal(
            ResumableUploadResponse{"{}", next_byte - 1, {}, ResumableUploadResponse::UploadState::Done, {}});
    });

    FileWriteStreambuf streambuf(std::move(mock), quantum, AutoFinalizeConfig::Enabled);

    for (auto const& c : payload)
    {
        EXPECT_EQ(c, streambuf.sputc(c));
    }
    EXPECT_EQ(' ', streambuf.sputc(' '));
    auto response = streambuf.Close();
    EXPECT_STATUS_OK(response);
    EXPECT_EQ(FileWriteStreambuf::traits_type::eof(), streambuf.sputc(' '));
}

/// @test verify that bytes not accepted by GCS will be re-uploaded next Flush.
TEST(FileWriteStreambufTest, SomeBytesNotAccepted)
{
    auto mock = std::make_unique<MockResumableUploadSession>();
    EXPECT_CALL(*mock, GetFileChunkSizeQuantum).WillRepeatedly(Return(ChunkSizeQuantumTest));

    auto const quantum = mock->GetFileChunkSizeQuantum();
    std::string const payload = std::string(quantum - 2, '*') + "abcde";

    size_t next_byte = 0;
    EXPECT_CALL(*mock, UploadChunk).WillOnce([&](ConstBufferSequence const& p) {
        auto expected = payload.substr(0, quantum);
        EXPECT_THAT(p, ConstBufSeqIs(ConstBufferSequence{ConstBuffer{expected}}));
        next_byte += quantum;
        return MakeStatusOrVal(
            ResumableUploadResponse{"", next_byte - 1, {}, ResumableUploadResponse::UploadState::InProgress, {}});
    });
    EXPECT_CALL(*mock, UploadFinalChunk).WillOnce([&](ConstBufferSequence const& p, std::uint64_t s) {
        auto const content = payload.substr(quantum);
        EXPECT_THAT(p, ConstBufSeqIs(ConstBufferSequence{ConstBuffer{content}}));
        EXPECT_EQ(payload.size(), s);
        next_byte += content.size();
        return MakeStatusOrVal(
            ResumableUploadResponse{"{}", next_byte - 1, {}, ResumableUploadResponse::UploadState::InProgress, {}});
    });
    EXPECT_CALL(*mock, GetNextExpectedByte()).WillRepeatedly([&]() { return next_byte; });
    EXPECT_CALL(*mock, Done).WillRepeatedly(Return(false));

    FileWriteStreambuf streambuf(std::move(mock), quantum, AutoFinalizeConfig::Enabled);

    std::ostream output(&streambuf);
    output << payload;
    auto response = streambuf.Close();
    EXPECT_STATUS_OK(response);
}

/// @test verify that the upload steam transitions to a bad state if the next
/// expected byte jumps.
TEST(FileWriteStreambufTest, NextExpectedByteJumpsAhead)
{
    auto mock = std::make_unique<MockResumableUploadSession>();
    EXPECT_CALL(*mock, GetFileChunkSizeQuantum).WillRepeatedly(Return(ChunkSizeQuantumTest));

    auto const quantum = mock->GetFileChunkSizeQuantum();
    std::string const payload = std::string(quantum * 2, '*');

    size_t next_byte = 0;
    EXPECT_CALL(*mock, UploadChunk).WillOnce([&](ConstBufferSequence const& p) {
        auto expected = payload.substr(0, 2 * quantum);
        EXPECT_THAT(p, ConstBufSeqIs(ConstBufferSequence{ConstBuffer{expected}}));
        // Simulate a condition where the server reports more bytes committed
        // than expected
        next_byte += quantum * 3;
        return MakeStatusOrVal(
            ResumableUploadResponse{"", next_byte - 1, {}, ResumableUploadResponse::UploadState::InProgress, {}});
    });
    EXPECT_CALL(*mock, GetNextExpectedByte()).WillRepeatedly([&]() { return next_byte; });
    EXPECT_CALL(*mock, Done).WillRepeatedly(Return(false));
    std::string id = "id";
    EXPECT_CALL(*mock, GetSessionId).WillOnce(ReturnRef(id));

    FileWriteStreambuf streambuf(std::move(mock), quantum, AutoFinalizeConfig::Enabled);
    std::ostream output(&streambuf);
    output << payload;
    EXPECT_FALSE(output.good());
    EXPECT_THAT(streambuf.GetLastStatus(), StatusIs(StatusCode::Aborted));
}

/// @test verify that the upload steam transitions to a bad state if the next
/// expected byte decreases.
TEST(FileWriteStreambufTest, NextExpectedByteDecreases)
{
    auto mock = std::make_unique<MockResumableUploadSession>();
    EXPECT_CALL(*mock, GetFileChunkSizeQuantum).WillRepeatedly(Return(ChunkSizeQuantumTest));

    auto const quantum = mock->GetFileChunkSizeQuantum();
    std::string const payload = std::string(quantum * 2, '*');

    auto next_byte = quantum;
    EXPECT_CALL(*mock, UploadChunk).WillOnce(InvokeWithoutArgs([&]() {
        next_byte--;
        return MakeStatusOrVal(
            ResumableUploadResponse{"", next_byte - 1, {}, ResumableUploadResponse::UploadState::InProgress, {}});
    }));
    EXPECT_CALL(*mock, GetNextExpectedByte()).WillRepeatedly([&]() { return next_byte; });
    EXPECT_CALL(*mock, Done).WillRepeatedly(Return(false));
    std::string id = "id";
    EXPECT_CALL(*mock, GetSessionId).WillOnce(ReturnRef(id));

    FileWriteStreambuf streambuf(std::move(mock), quantum, AutoFinalizeConfig::Enabled);
    std::ostream output(&streambuf);
    output << payload;
    EXPECT_FALSE(output.good());
    EXPECT_THAT(streambuf.GetLastStatus(), StatusIs(StatusCode::Aborted));
}

/// @test Verify that a stream flushes when mixing operations that add one
/// character at a time and operations that add buffers.
TEST(FileWriteStreambufTest, MixPutcPutn)
{
    auto mock = std::make_unique<MockResumableUploadSession>();
    EXPECT_CALL(*mock, Done).WillRepeatedly(Return(false));
    EXPECT_CALL(*mock, GetFileChunkSizeQuantum).WillRepeatedly(Return(ChunkSizeQuantumTest));

    auto const quantum = mock->GetFileChunkSizeQuantum();
    std::string const payload_1("header");
    std::string const payload_2(quantum, '*');

    int count = 0;
    size_t next_byte = 0;
    EXPECT_CALL(*mock, UploadChunk).WillOnce([&](ConstBufferSequence const& p) {
        ++count;
        EXPECT_EQ(1, count);
        auto expected = payload_2.substr(0, quantum - payload_1.size());
        EXPECT_THAT(p, ConstBufSeqIs(ConstBufferSequence{ConstBuffer{payload_1}, ConstBuffer{expected}}));
        next_byte += TotalBytes(p);
        return MakeStatusOrVal(
            ResumableUploadResponse{"", quantum - 1, {}, ResumableUploadResponse::UploadState::InProgress, {}});
    });
    EXPECT_CALL(*mock, UploadFinalChunk).WillOnce([&](ConstBufferSequence const& p, std::uint64_t s) {
        ++count;
        EXPECT_EQ(2, count);
        auto expected = payload_2.substr(payload_2.size() - payload_1.size());
        EXPECT_THAT(p, ConstBufSeqIs(ConstBufferSequence{ConstBuffer{expected}}));
        EXPECT_EQ(payload_1.size() + payload_2.size(), s);
        auto last_committed_byte = payload_1.size() + payload_2.size() - 1;
        return MakeStatusOrVal(ResumableUploadResponse{
            "{}", last_committed_byte, {}, ResumableUploadResponse::UploadState::InProgress, {}});
    });
    EXPECT_CALL(*mock, GetNextExpectedByte()).WillRepeatedly([&]() { return next_byte; });

    FileWriteStreambuf streambuf(std::move(mock), quantum, AutoFinalizeConfig::Enabled);

    for (auto const& c : payload_1)
    {
        streambuf.sputc(c);
    }
    streambuf.sputn(payload_2.data(), payload_2.size());
    auto response = streambuf.Close();
    EXPECT_STATUS_OK(response);
}

/// @test Verify that a stream created for a finished upload starts out as
/// closed.
TEST(FileWriteStreambufTest, CreatedForFinalizedUpload)
{
    auto mock = std::make_unique<MockResumableUploadSession>();
    EXPECT_CALL(*mock, Done).WillRepeatedly(Return(true));
    EXPECT_CALL(*mock, GetFileChunkSizeQuantum).WillRepeatedly(Return(ChunkSizeQuantumTest));
    auto last_upload_response =
        MakeStatusOrVal(ResumableUploadResponse{"url-for-test", 0, {}, ResumableUploadResponse::UploadState::Done, {}});
    EXPECT_CALL(*mock, GetLastResponse).WillOnce(ReturnRef(last_upload_response));

    FileWriteStreambuf streambuf(std::move(mock), ChunkSizeQuantumTest, AutoFinalizeConfig::Enabled);
    EXPECT_EQ(streambuf.IsOpen(), false);
    StatusOrVal<ResumableUploadResponse> close_result = streambuf.Close();
    ASSERT_STATUS_OK(close_result);
    EXPECT_EQ("url-for-test", close_result.Value().m_uploadSessionUrl);
}

/// @test Verify that last error status is accessible for small payload.
TEST(FileWriteStreambufTest, ErroneousStream)
{
    auto mock = std::make_unique<MockResumableUploadSession>();
    EXPECT_CALL(*mock, Done).WillRepeatedly(Return(false));
    EXPECT_CALL(*mock, GetFileChunkSizeQuantum).WillRepeatedly(Return(ChunkSizeQuantumTest));

    auto const quantum = mock->GetFileChunkSizeQuantum();
    std::string const payload = "small test payload";

    int count = 0;
    EXPECT_CALL(*mock, UploadFinalChunk).WillOnce([&](ConstBufferSequence const& p, std::uint64_t n) {
        ++count;
        EXPECT_EQ(1, count);
        EXPECT_THAT(p, ConstBufSeqIs(ConstBufferSequence{ConstBuffer{payload}}));
        EXPECT_EQ(payload.size(), n);
        return Status(StatusCode::InvalidArgument, "Invalid Argument");
    });
    EXPECT_CALL(*mock, GetNextExpectedByte()).WillOnce(Return(0));

    FileWriteStreambuf streambuf(std::move(mock), quantum, AutoFinalizeConfig::Enabled);

    streambuf.sputn(payload.data(), payload.size());
    auto response = streambuf.Close();
    EXPECT_THAT(response, StatusIs(StatusCode::InvalidArgument));
    EXPECT_THAT(streambuf.GetLastStatus(), StatusIs(StatusCode::InvalidArgument));
}

/// @test Verify that last error status is accessible for large payloads.
TEST(FileWriteStreambufTest, ErrorInLargePayload)
{
    auto mock = std::make_unique<NiceMock<MockResumableUploadSession>>();
    EXPECT_CALL(*mock, Done).WillRepeatedly(Return(false));
    EXPECT_CALL(*mock, GetFileChunkSizeQuantum).WillRepeatedly(Return(ChunkSizeQuantumTest));

    auto const quantum = mock->GetFileChunkSizeQuantum();
    std::string const payload_1(3 * quantum, '*');
    std::string const payload_2("trailer");
    std::string const session_id = "upload_id";

    ON_CALL(*mock, GetNextExpectedByte()).WillByDefault(Return(0));
    EXPECT_CALL(*mock, UploadChunk).WillOnce([&](ConstBufferSequence const& p) {
        EXPECT_EQ(3 * quantum, TotalBytes(p));
        return Status(StatusCode::InvalidArgument, "Invalid Argument");
    });
    EXPECT_CALL(*mock, GetSessionId).WillOnce(ReturnRef(session_id));
    FileWriteStreambuf streambuf(std::move(mock), quantum, AutoFinalizeConfig::Enabled);

    streambuf.sputn(payload_1.data(), payload_1.size());
    EXPECT_THAT(streambuf.GetLastStatus(), StatusIs(StatusCode::InvalidArgument));
    EXPECT_EQ(streambuf.GetResumableSessionId(), session_id);

    streambuf.sputn(payload_2.data(), payload_2.size());
    EXPECT_THAT(streambuf.GetLastStatus(), StatusIs(StatusCode::InvalidArgument));

    auto response = streambuf.Close();
    EXPECT_THAT(response, StatusIs(StatusCode::InvalidArgument));
}

/// @test Verify that uploads of known size work.
TEST(FileWriteStreambufTest, KnownSizeUpload)
{
    auto mock = std::make_unique<MockResumableUploadSession>();
    EXPECT_CALL(*mock, GetFileChunkSizeQuantum).WillRepeatedly(Return(ChunkSizeQuantumTest));

    auto const quantum = mock->GetFileChunkSizeQuantum();
    std::string const payload(2 * quantum, '*');

    std::size_t mock_next_byte = 0;
    EXPECT_CALL(*mock, GetNextExpectedByte()).WillRepeatedly([&]() { return mock_next_byte; });
    bool mock_is_done = false;
    EXPECT_CALL(*mock, Done()).WillRepeatedly([&]() { return mock_is_done; });
    std::string const mock_session_id = "session-id";
    EXPECT_CALL(*mock, GetSessionId()).WillRepeatedly(ReturnRef(mock_session_id));
    EXPECT_CALL(*mock, UploadChunk)
        .WillOnce([&](ConstBufferSequence const& p) {
            EXPECT_THAT(p, ConstBufSeqIs(ConstBufferSequence{ConstBuffer{payload}}));
            mock_next_byte += TotalBytes(p);
            return MakeStatusOrVal(
                ResumableUploadResponse{"", 2 * quantum - 1, {}, ResumableUploadResponse::UploadState::InProgress, {}});
        })
        .WillOnce([&](ConstBufferSequence const& p) {
            EXPECT_THAT(p, ConstBufSeqIs(ConstBufferSequence{ConstBuffer{payload}}));
            mock_next_byte += TotalBytes(p);
            return MakeStatusOrVal(
                ResumableUploadResponse{"", 4 * quantum - 1, {}, ResumableUploadResponse::UploadState::InProgress, {}});
        })
        .WillOnce([&](ConstBufferSequence const& p) {
            EXPECT_THAT(p, ConstBufSeqIs(ConstBufferSequence{ConstBuffer{payload.data(), quantum}}));
            mock_next_byte += TotalBytes(p);
            // When using X-Upload-Content-Length GCS finalizes the upload when
            // enough data is sent, regardless of whether we use UploadChunk() or
            // UploadFinalChunk(). Furthermore the response has a last committed
            // byte of "0".
            mock_is_done = true;
            return MakeStatusOrVal(ResumableUploadResponse{"", 0, {}, ResumableUploadResponse::UploadState::Done, {}});
        });

    FileWriteStreambuf streambuf(std::move(mock), quantum, AutoFinalizeConfig::Enabled);

    streambuf.sputn(payload.data(), 2 * quantum);
    streambuf.sputn(payload.data(), 2 * quantum);
    streambuf.sputn(payload.data(), quantum);
    EXPECT_EQ(5 * quantum, streambuf.GetNextExpectedByte());
    EXPECT_FALSE(streambuf.IsOpen());
    EXPECT_STATUS_OK(streambuf.GetLastStatus());
    auto response = streambuf.Close();
    EXPECT_STATUS_OK(response);
}

/// @test Verify flushing partially full buffers works.
TEST(FileWriteStreambufTest, Pubsync)
{
    auto mock = std::make_unique<MockResumableUploadSession>();
    EXPECT_CALL(*mock, GetFileChunkSizeQuantum).WillRepeatedly(Return(ChunkSizeQuantumTest));

    auto const quantum = mock->GetFileChunkSizeQuantum();
    std::string const payload(quantum, '*');

    std::size_t mock_next_byte = 0;
    EXPECT_CALL(*mock, GetNextExpectedByte()).WillRepeatedly([&]() { return mock_next_byte; });
    bool mock_is_done = false;
    EXPECT_CALL(*mock, Done()).WillRepeatedly([&]() { return mock_is_done; });
    std::string const mock_session_id = "session-id";
    EXPECT_CALL(*mock, GetSessionId()).WillRepeatedly(ReturnRef(mock_session_id));
    EXPECT_CALL(*mock, UploadChunk).WillOnce([&](ConstBufferSequence const& p) {
        EXPECT_THAT(p, ConstBufSeqIs(ConstBufferSequence{ConstBuffer{payload}}));
        mock_next_byte += TotalBytes(p);
        return MakeStatusOrVal(
            ResumableUploadResponse{"", mock_next_byte - 1, {}, ResumableUploadResponse::UploadState::InProgress, {}});
    });
    EXPECT_CALL(*mock, UploadFinalChunk).WillOnce([&](ConstBufferSequence const& p, std::uint64_t) {
        EXPECT_THAT(p, ConstBufSeqIs(ConstBufferSequence{ConstBuffer{payload}}));
        mock_next_byte += TotalBytes(p);
        mock_is_done = true;
        return MakeStatusOrVal(
            ResumableUploadResponse{"", mock_next_byte - 1, {}, ResumableUploadResponse::UploadState::Done, {}});
    });

    FileWriteStreambuf streambuf(std::move(mock), 2 * quantum, AutoFinalizeConfig::Enabled);

    EXPECT_EQ(quantum, streambuf.sputn(payload.data(), quantum));
    EXPECT_EQ(0, streambuf.pubsync());
    EXPECT_EQ(quantum, streambuf.sputn(payload.data(), quantum));
    auto response = streambuf.Close();
    EXPECT_STATUS_OK(response);
}

/// @test Verify flushing too small a buffer does nothing.
TEST(FileWriteStreambufTest, PubsyncTooSmall)
{
    auto mock = std::make_unique<MockResumableUploadSession>();
    EXPECT_CALL(*mock, GetFileChunkSizeQuantum).WillRepeatedly(Return(ChunkSizeQuantumTest));

    auto const quantum = mock->GetFileChunkSizeQuantum();
    auto const half = quantum / 2;
    std::string const p0(half, '0');
    std::string const p1(half, '1');
    std::string const p2(half, '2');

    std::size_t mock_next_byte = 0;
    EXPECT_CALL(*mock, GetNextExpectedByte()).WillRepeatedly([&]() { return mock_next_byte; });
    bool mock_is_done = false;
    EXPECT_CALL(*mock, Done()).WillRepeatedly([&]() { return mock_is_done; });
    std::string const mock_session_id = "session-id";
    EXPECT_CALL(*mock, GetSessionId()).WillRepeatedly(ReturnRef(mock_session_id));

    // Write some data and flush it, because there are no EXPECT_CALLS for
    // UploadChunk yet this fails if we flush too early.
    FileWriteStreambuf streambuf(std::move(mock), 2 * quantum, AutoFinalizeConfig::Enabled);

    EXPECT_EQ(half, streambuf.sputn(p0.data(), half));
    EXPECT_EQ(0, streambuf.pubsync());
}

}  // namespace internal
}  // namespace csa
