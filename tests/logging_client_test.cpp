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

#include "cloudstorageapi/internal/logging_client.h"
#include "cloudstorageapi/file_metadata.h"
#include "cloudstorageapi/folder_metadata.h"
#include "cloudstorageapi/internal/log.h"
#include "canonical_errors.h"
#include "testing_util/mock_cloud_storage_client.h"
#include <gmock/gmock.h>

namespace csa {
namespace internal {

using canonical_errors::TransientError;
using ::testing::HasSubstr;
using ::testing::Return;

namespace {
FolderMetadata GetFolderMetadata(const std::string& cloudId, const std::string& name, const std::string& parentId)
{
    FolderMetadata fm;
    fm.SetCloudId(cloudId);
    fm.SetName(name);
    fm.SetParentId(parentId);
    fm.SetSize(4096);
    const auto modifiedTime = std::chrono::system_clock::now();
    fm.SetChangeTime(modifiedTime);
    fm.SetModifyTime(modifiedTime);
    fm.SetAccessTime(modifiedTime);
    fm.SetCanCreateFolders(true);
    fm.SetCanUploadFile(true);
    return fm;
}

FileMetadata GetFileMetadata(const std::string& cloudId, const std::string& name, const std::string& parentId)
{
    FileMetadata fm;
    fm.SetCloudId(cloudId);
    fm.SetName(name);
    fm.SetParentId(parentId);
    fm.SetSize(1000);
    const auto modifiedTime = std::chrono::system_clock::now();
    fm.SetChangeTime(modifiedTime);
    fm.SetModifyTime(modifiedTime);
    fm.SetAccessTime(modifiedTime);
    // fm.SetMimeTypeOpt(); intentionally left unset
    fm.SetDownloadable(true);
    return fm;
}
}  // namespace

class MockLogSink : public SinkBase
{
public:
    MOCK_METHOD(void, SinkRecord, (LogRecord const&), (override));
    MOCK_METHOD(void, Flush, (), (override));
};

class LoggingClientTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        m_logSink = std::make_shared<MockLogSink>();
        m_logSinkId = GetLogger()->AddSink(m_logSink);
    }
    void TearDown() override
    {
        GetLogger()->RemoveSink(m_logSinkId);
        m_logSinkId = 0;
        m_logSink.reset();
    }

    std::shared_ptr<MockLogSink> m_logSink = nullptr;
    long m_logSinkId = 0;
};

TEST_F(LoggingClientTest, GetFolderMetadata)
{
    FolderMetadata fm = GetFolderMetadata("Folder-cloud-id-1", "Folder-1", "Folder-parent-id-1");

    auto mock = std::make_shared<MockClient>(EProvider::GoogleDrive);
    EXPECT_CALL(*mock, GetFolderMetadata).WillOnce(Return(fm));

    // We want to test that the key elements are logged, but do not want a
    // "change detection test", so this is intentionally not exhaustive.
    EXPECT_CALL(*m_logSink, SinkRecord)
        .WillOnce([](LogRecord const& lr) {
            EXPECT_THAT(lr.m_message, HasSubstr(" << "));
            EXPECT_THAT(lr.m_message, HasSubstr("GetFolderMetadataRequest={"));
            EXPECT_THAT(lr.m_message, HasSubstr("Folder-cloud-id-1"));
        })
        .WillOnce([](LogRecord const& lr) {
            EXPECT_THAT(lr.m_message, HasSubstr(" >> "));
            EXPECT_THAT(lr.m_message, HasSubstr("payload={"));
            EXPECT_THAT(lr.m_message, HasSubstr("Folder-1"));
        });

    LoggingClient client(mock);
    client.GetFolderMetadata(GetFolderMetadataRequest(fm.GetCloudId()));
}

TEST_F(LoggingClientTest, GetFolderMetadataWithError)
{
    auto mock = std::make_shared<MockClient>(EProvider::GoogleDrive);
    EXPECT_CALL(*mock, GetFolderMetadata).WillOnce(Return(StatusOrVal<FolderMetadata>(TransientError())));

    // We want to test that the key elements are logged, but do not want a
    // "change detection test", so this is intentionally not exhaustive.
    EXPECT_CALL(*m_logSink, SinkRecord)
        .WillOnce([](LogRecord const& lr) {
            EXPECT_THAT(lr.m_message, HasSubstr(" << "));
            EXPECT_THAT(lr.m_message, HasSubstr("GetFolderMetadataRequest={"));
            EXPECT_THAT(lr.m_message, HasSubstr("my-folder-id"));
        })
        .WillOnce([](LogRecord const& lr) {
            EXPECT_THAT(lr.m_message, HasSubstr(" >> "));
            EXPECT_THAT(lr.m_message, HasSubstr("status={"));
        });

    LoggingClient client(mock);
    client.GetFolderMetadata(GetFolderMetadataRequest("my-folder-id"));
}

TEST_F(LoggingClientTest, InsertFile)
{
    FileMetadata fm = GetFileMetadata("File-cloud-id-1", "File-1", "Folder-parent-id-1");

    auto mock = std::make_shared<MockClient>(EProvider::GoogleDrive);
    EXPECT_CALL(*mock, InsertFile).WillOnce(Return(fm));

    // We want to test that the key elements are logged, but do not want a
    // "change detection test", so this is intentionally not exhaustive.
    EXPECT_CALL(*m_logSink, SinkRecord)
        .WillOnce([](LogRecord const& lr) {
            EXPECT_THAT(lr.m_message, HasSubstr(" << "));
            EXPECT_THAT(lr.m_message, HasSubstr("InsertFileRequest={"));
            EXPECT_THAT(lr.m_message, HasSubstr("File-1"));
            EXPECT_THAT(lr.m_message, HasSubstr("Folder-parent-id-1"));
            EXPECT_THAT(lr.m_message, HasSubstr("the contents"));
        })
        .WillOnce([](LogRecord const& lr) {
            EXPECT_THAT(lr.m_message, HasSubstr(" >> "));
            EXPECT_THAT(lr.m_message, HasSubstr("payload={"));
            EXPECT_THAT(lr.m_message, HasSubstr("File-1"));
            EXPECT_THAT(lr.m_message, HasSubstr("Folder-parent-id-1"));
        });

    LoggingClient client(mock);
    client.InsertFile(InsertFileRequest(fm.GetParentId(), fm.GetName(), "the contents"));
}

TEST_F(LoggingClientTest, ListFolder)
{
    FileMetadata fileMeta =
        GetFileMetadata("Response-folder-cloud-id-1", "Response-file-1", "Response-folder-parent-id-1");
    FolderMetadata folderMeta =
        GetFolderMetadata("Response-folder-cloud-id-2", "Response-folder-2", "Response-folder-parent-id-2");
    std::vector<ListFolderResponse::MetadataItem> items = {fileMeta, folderMeta};
    auto mock = std::make_shared<MockClient>(EProvider::GoogleDrive);
    EXPECT_CALL(*mock, ListFolder).WillOnce(Return(MakeStatusOrVal(ListFolderResponse{"a-token", items})));

    // We want to test that the key elements are logged, but do not want a
    // "change detection test", so this is intentionally not exhaustive.
    EXPECT_CALL(*m_logSink, SinkRecord)
        .WillOnce([](LogRecord const& lr) {
            EXPECT_THAT(lr.m_message, HasSubstr(" << "));
            EXPECT_THAT(lr.m_message, HasSubstr("ListFolderRequest={"));
            EXPECT_THAT(lr.m_message, HasSubstr("my-folder"));
        })
        .WillOnce([](LogRecord const& lr) {
            EXPECT_THAT(lr.m_message, HasSubstr(" >> "));
            EXPECT_THAT(lr.m_message, HasSubstr("payload={"));
            EXPECT_THAT(lr.m_message, HasSubstr("ListFolderResponse={"));
            EXPECT_THAT(lr.m_message, HasSubstr("a-token"));
            EXPECT_THAT(lr.m_message, HasSubstr("Response-file-1"));
            EXPECT_THAT(lr.m_message, HasSubstr("Response-folder-2"));
        });

    LoggingClient client(mock);
    client.ListFolder(ListFolderRequest("my-folder"));
}

}  // namespace internal
}  // namespace csa
