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

#include "cloudstorageapi/internal/curl_resumable_upload_session.h"
#include "cloudstorageapi/auth/credential_factory.h"
#include "cloudstorageapi/internal/curl_client_base.h"
#include "testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace csa {
namespace internal {

using ::csa::testing::util::IsOk;
using ::testing::Not;

class MockCurlClient : public CurlClientBase
{
public:
    static std::shared_ptr<MockCurlClient> Create() { return std::make_shared<MockCurlClient>(); }

    MockCurlClient()
        : CurlClientBase(internal::CreateDefaultOptions(
              auth::CredentialFactory::CreateAnonymousCredentials(EProvider::GoogleDrive), {}))
    {
    }

    MOCK_METHOD(StatusOrVal<ResumableUploadResponse>, UploadChunk, (UploadChunkRequest const&), (override));
    MOCK_METHOD(StatusOrVal<ResumableUploadResponse>, QueryResumableUpload, (QueryResumableUploadRequest const&),
                (override));

    // Other methods, not important.
    MOCK_METHOD(std::string, GetProviderName, (), (const, override));
    MOCK_METHOD(StatusOrVal<UserInfo>, GetUserInfo, (), (override));
    MOCK_METHOD(std::size_t, GetFileChunkQuantum, (), (const, override));
    MOCK_METHOD(StatusOrVal<EmptyResponse>, Delete, (DeleteRequest const&), (override));
    MOCK_METHOD(StatusOrVal<ListFolderResponse>, ListFolder, (ListFolderRequest const&), (override));
    MOCK_METHOD(StatusOrVal<FolderMetadata>, GetFolderMetadata, (GetFolderMetadataRequest const&), (override));
    MOCK_METHOD(StatusOrVal<FolderMetadata>, CreateFolder, (CreateFolderRequest const&), (override));
    MOCK_METHOD(StatusOrVal<FolderMetadata>, RenameFolder, (RenameRequest const&), (override));
    MOCK_METHOD(StatusOrVal<FolderMetadata>, PatchFolderMetadata, (PatchFolderMetadataRequest const&), (override));
    MOCK_METHOD(StatusOrVal<FileMetadata>, GetFileMetadata, (GetFileMetadataRequest const&), (override));
    MOCK_METHOD(StatusOrVal<FileMetadata>, PatchFileMetadata, (PatchFileMetadataRequest const&), (override));
    MOCK_METHOD(StatusOrVal<FileMetadata>, RenameFile, (RenameRequest const&), (override));
    MOCK_METHOD(StatusOrVal<FileMetadata>, InsertFile, (InsertFileRequest const&), (override));
    MOCK_METHOD(StatusOrVal<std::unique_ptr<ObjectReadSource>>, ReadFile, (ReadFileRangeRequest const&), (override));
    MOCK_METHOD(StatusOrVal<std::unique_ptr<ResumableUploadSession>>, CreateResumableSession,
                (ResumableUploadRequest const&), (override));
    MOCK_METHOD(StatusOrVal<std::unique_ptr<ResumableUploadSession>>, RestoreResumableSession, (std::string const&),
                (override));
    MOCK_METHOD(StatusOrVal<EmptyResponse>, DeleteResumableUpload, (DeleteResumableUploadRequest const&), (override));
    MOCK_METHOD(StatusOrVal<FileMetadata>, CopyFileObject, (CopyFileRequest const&), (override));
    MOCK_METHOD(StatusOrVal<StorageQuota>, GetQuota, (), (override));
};

MATCHER_P(MatchesPayload, value, "Checks whether payload matches a value")
{
    std::string contents;
    for (auto const& s : arg)
    {
        contents += std::string{s.data(), s.size()};
    }
    return ::testing::ExplainMatchResult(::testing::Eq(contents), value, result_listener);
}

TEST(CurlResumableUploadSessionTest, Simple)
{
    auto mock = MockCurlClient::Create();
    std::string testUrl = "http://invalid.example.com/not-used-in-mock";
    CurlResumableUploadSession session(mock, testUrl);

    std::string const payload = "test payload";
    auto const size = payload.size();

    EXPECT_FALSE(session.Done());
    EXPECT_EQ(0, session.GetNextExpectedByte());
    EXPECT_CALL(*mock, UploadChunk)
        .WillOnce([&](UploadChunkRequest const& request) {
            EXPECT_EQ(testUrl, request.GetUploadSessionUrl());
            EXPECT_THAT(request.GetPayload(), MatchesPayload(payload));
            EXPECT_EQ(0, request.GetSourceSize());
            EXPECT_EQ(0, request.GetRangeBegin());
            return MakeStatusOrVal(
                ResumableUploadResponse{"", size - 1, {}, ResumableUploadResponse::UploadState::InProgress, {}});
        })
        .WillOnce([&](UploadChunkRequest const& request) {
            EXPECT_EQ(testUrl, request.GetUploadSessionUrl());
            EXPECT_THAT(request.GetPayload(), MatchesPayload(payload));
            EXPECT_EQ(2 * size, request.GetSourceSize());
            EXPECT_EQ(size, request.GetRangeBegin());
            return MakeStatusOrVal(
                ResumableUploadResponse{"", 2 * size - 1, {}, ResumableUploadResponse::UploadState::Done, {}});
        });

    auto upload = session.UploadChunk({{payload}});
    EXPECT_STATUS_OK(upload);
    EXPECT_EQ(size - 1, upload->m_lastCommittedByte);
    EXPECT_EQ(size, session.GetNextExpectedByte());
    EXPECT_FALSE(session.Done());

    upload = session.UploadFinalChunk({{payload}}, 2 * size);
    EXPECT_STATUS_OK(upload);
    EXPECT_EQ(2 * size - 1, upload->m_lastCommittedByte);
    EXPECT_EQ(2 * size, session.GetNextExpectedByte());
    EXPECT_TRUE(session.Done());
}

TEST(CurlResumableUploadSessionTest, Reset)
{
    auto mock = MockCurlClient::Create();
    std::string url1 = "http://invalid.example.com/not-used-in-mock-1";
    std::string url2 = "http://invalid.example.com/not-used-in-mock-2";
    CurlResumableUploadSession session(mock, url1);

    std::string const payload = "test payload";
    auto const size = payload.size();

    EXPECT_EQ(0, session.GetNextExpectedByte());
    EXPECT_CALL(*mock, UploadChunk)
        .WillOnce([&](UploadChunkRequest const&) {
            return MakeStatusOrVal(
                ResumableUploadResponse{"", size - 1, {}, ResumableUploadResponse::UploadState::InProgress, {}});
        })
        .WillOnce([&](UploadChunkRequest const&) {
            return StatusOrVal<ResumableUploadResponse>(AsStatus(HttpResponse{308, "uh oh", {}}));
        });
    const ResumableUploadResponse resume_response{
        url2, 2 * size - 1, {}, ResumableUploadResponse::UploadState::InProgress, {}};
    EXPECT_CALL(*mock, QueryResumableUpload).WillOnce([&](QueryResumableUploadRequest const& request) {
        EXPECT_EQ(url1, request.GetUploadSessionUrl());
        return MakeStatusOrVal(resume_response);
    });

    auto upload = session.UploadChunk({{payload}});
    EXPECT_EQ(size, session.GetNextExpectedByte());
    upload = session.UploadChunk({{payload}});
    EXPECT_THAT(upload, Not(IsOk()));
    EXPECT_EQ(size, session.GetNextExpectedByte());
    EXPECT_EQ(url1, session.GetSessionId());

    session.ResetSession();
    EXPECT_EQ(2 * size, session.GetNextExpectedByte());
    // Changes to the session id are ignored, they do not happen in production
    // anyway
    EXPECT_EQ(url1, session.GetSessionId());
    StatusOrVal<ResumableUploadResponse> const& last_response = session.GetLastResponse();
    ASSERT_STATUS_OK(last_response);
    EXPECT_EQ(last_response.Value(), resume_response);
}

TEST(CurlResumableUploadSessionTest, SessionUpdatedInChunkUpload)
{
    auto mock = MockCurlClient::Create();
    std::string url1 = "http://invalid.example.com/not-used-in-mock-1";
    std::string url2 = "http://invalid.example.com/not-used-in-mock-2";
    CurlResumableUploadSession session(mock, url1);

    std::string const payload = "test payload";
    auto const size = payload.size();

    EXPECT_EQ(0, session.GetNextExpectedByte());
    EXPECT_CALL(*mock, UploadChunk)
        .WillOnce([&](UploadChunkRequest const&) {
            return MakeStatusOrVal(
                ResumableUploadResponse{"", size - 1, {}, ResumableUploadResponse::UploadState::InProgress, {}});
        })
        .WillOnce([&](UploadChunkRequest const&) {
            return MakeStatusOrVal(
                ResumableUploadResponse{url2, 2 * size - 1, {}, ResumableUploadResponse::UploadState::InProgress, {}});
        });

    auto upload = session.UploadChunk({{payload}});
    EXPECT_EQ(size, session.GetNextExpectedByte());
    upload = session.UploadChunk({{payload}});
    EXPECT_STATUS_OK(upload);
    EXPECT_EQ(2 * size, session.GetNextExpectedByte());
    // Changes to the session id are ignored, they do not happen in production
    // anyway
    EXPECT_EQ(url1, session.GetSessionId());
}

TEST(CurlResumableUploadSessionTest, Empty)
{
    auto mock = MockCurlClient::Create();
    std::string test_url = "http://invalid.example.com/not-used-in-mock";
    CurlResumableUploadSession session(mock, test_url);

    std::string const payload{};
    auto const size = payload.size();

    EXPECT_FALSE(session.Done());
    EXPECT_EQ(0, session.GetNextExpectedByte());
    EXPECT_CALL(*mock, UploadChunk).WillOnce([&](UploadChunkRequest const& request) {
        EXPECT_EQ(test_url, request.GetUploadSessionUrl());
        EXPECT_THAT(request.GetPayload(), MatchesPayload(payload));
        EXPECT_EQ(0, request.GetSourceSize());
        EXPECT_EQ(0, request.GetRangeBegin());
        return MakeStatusOrVal(ResumableUploadResponse{"", size, {}, ResumableUploadResponse::UploadState::Done, {}});
    });

    auto upload = session.UploadFinalChunk({{payload}}, size);
    EXPECT_STATUS_OK(upload);
    EXPECT_EQ(size, upload->m_lastCommittedByte);
    EXPECT_EQ(size, session.GetNextExpectedByte());
    EXPECT_TRUE(session.Done());
}

}  // namespace internal
}  // namespace csa
