// Copyright 2021 Andrew Karasyov
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

#include "cloudstorageapi/internal/clients/google_response_parser.h"
#include "util/status_matchers.h"
#include <gmock/gmock.h>
#include <nlohmann/json.hpp>

namespace csa {
namespace internal {

using ::csa::testing::util::IsOk;
using ::testing::HasSubstr;
using ::testing::Not;

namespace {
std::string GetFolderMeta()
{
    return R"""({
    "kind": "drive#file",
    "id": "my-folder-id",
    "name": "my-folder-name",
    "mimeType": "application/vnd.google-apps.folder",
    "description": "some-description",
    "starred": true,
    "trashed": false,
    "explicitlyTrashed": false,
    "parents": [
        "my-parent-id"
    ],
    "version": 123,
    "webContentLink": "webLink",
    "webViewLink": "webViewLink",
    "modifiedTime": "2018-05-18t14:42:03z",
    "capabilities": {
        "canAddChildren": true,
        "canDownload": true,
        "canEdit": true
    },
    "md5Checksum": "54321decf",
    "size": 4096
})""";
}

std::string GetFileMeta()
{
    return R"""({
    "kind": "drive#file",
    "id": "my-file-id",
    "name": "my-file-name",
    "mimeType": "my-mime-type",
    "description": "some-description",
    "starred": true,
    "trashed": false,
    "explicitlyTrashed": false,
    "parents": [
        "my-parent-id"
    ],
    "version": 123,
    "webContentLink": "webLink",
    "webViewLink": "webViewLink",
    "modifiedTime": "2018-05-18t14:42:03z",
    "capabilities": {
        "canAddChildren": true,
        "canDownload": true,
        "canEdit": true
    },
    "md5Checksum": "54321decf",
    "size": 543345
    })""";
}

}  // namespace

TEST(GoogleResponseParserTest, ParseFailure)
{
    auto actual = GoogleResponseParser::ParseResponse<ListFolderResponse>(std::string{"{123"});
    EXPECT_THAT(actual, Not(IsOk()));
    auto actual2 = GoogleResponseParser::ParseResponse<ResumableUploadResponse>(
        HttpResponse{HttpStatusCode::Ok, std::string{"{123"}, {}});
    EXPECT_THAT(actual2, Not(IsOk()));
}

TEST(GoogleResponseParserTest, ParseListFolderResponse)
{
    auto fileMeta = GetFileMeta();
    auto folderMeta = GetFolderMeta();
    std::string payload = R"""({
    "kind": "drive#fileList",
    "nextPageToken": "my-next-page-token",
    "files": [)""" + folderMeta +
                          "," + fileMeta +
                          R"""(]
    })""";

    auto actual = GoogleResponseParser::ParseResponse<ListFolderResponse>(payload);
    auto file = GoogleMetadataParser::ParseFileMetadata(fileMeta).Value();
    auto folder = GoogleMetadataParser::ParseFolderMetadata(folderMeta).Value();
    EXPECT_THAT(actual, IsOk());
    EXPECT_EQ("my-next-page-token", (*actual).m_nextPageToken);
    EXPECT_THAT((*actual).m_items, ::testing::ElementsAre(folder, file));
}

TEST(GoogleResponseParserTest, ParseResumableUploadDoneResponse)
{
    std::string loc{"https://www.googleapis.com/upload/drive/v3/files?uploadType=resumable&upload_id=xa298sd_sdlkj2"};
    HttpResponse httpResp{HttpStatusCode::Ok, GetFileMeta(), {{"location", loc}}};
    auto actual = GoogleResponseParser::ParseResponse<ResumableUploadResponse>(httpResp);
    EXPECT_THAT(actual, IsOk());
    auto session = actual.Value();
    EXPECT_EQ(loc, session.m_uploadSessionUrl);
    EXPECT_EQ(0, session.m_lastCommittedByte);
    auto file = GoogleMetadataParser::ParseFileMetadata(GetFileMeta()).Value();
    EXPECT_EQ(file, session.m_payload.value());
    EXPECT_EQ(ResumableUploadResponse::UploadState::Done, session.m_uploadState);
    EXPECT_THAT(session.m_annotations, HasSubstr("missing range header"));
}

TEST(GoogleResponseParserTest, ParseResumableUploadInProgressResponse)
{
    std::string loc{"https://www.googleapis.com/upload/drive/v3/files?uploadType=resumable&upload_id=xa298sd_sdlkj2"};
    HttpResponse httpResp{
        HttpStatusCode::ResumeIncomplete, GetFileMeta(), {{"location", loc}, {"Range", "bytes=0-42"}}};
    auto actual = GoogleResponseParser::ParseResponse<ResumableUploadResponse>(httpResp);
    EXPECT_THAT(actual, IsOk());
    auto session = actual.Value();
    EXPECT_EQ(loc, session.m_uploadSessionUrl);
    EXPECT_EQ(0, session.m_lastCommittedByte);
    auto file = GoogleMetadataParser::ParseFileMetadata(GetFileMeta()).Value();
    EXPECT_FALSE(session.m_payload.has_value());
    EXPECT_EQ(ResumableUploadResponse::UploadState::InProgress, session.m_uploadState);
    EXPECT_THAT(session.m_annotations, HasSubstr("missing range header"));
}

}  // namespace internal
}  // namespace csa
