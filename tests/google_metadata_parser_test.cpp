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

#include "cloudstorageapi/internal/clients/google_metadata_parser.h"
#include "cloudstorageapi/internal/rfc3339_time.h"
#include "testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <nlohmann/json.hpp>

namespace csa {
namespace internal {

using ::csa::testing::util::IsOk;
using ::std::chrono::duration_cast;
using ::testing::HasSubstr;
using ::testing::Not;

TEST(GoogleMetadataParserTest, ParseFailure)
{
    auto actual = GoogleMetadataParser::ParseFileMetadata(std::string{"{123"});
    EXPECT_THAT(actual, Not(IsOk()));
    auto actual2 = GoogleMetadataParser::ParseFolderMetadata(std::string{"{123"});
    EXPECT_THAT(actual2, Not(IsOk()));
}

TEST(GoogleMetadataParserTest, ParseFile)
{
    std::string object1 = R"""({
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

    auto file1 = GoogleMetadataParser::ParseFileMetadata(object1);
    EXPECT_STATUS_OK(file1);
    auto f1 = file1.Value();
    EXPECT_EQ(f1.GetCloudId(), std::string{"my-file-id"});
    EXPECT_EQ(f1.GetName(), std::string{"my-file-name"});
    EXPECT_EQ(f1.GetMimeTypeOpt().value(), std::string{"my-mime-type"});
    EXPECT_EQ(f1.GetParentId(), std::string{"my-parent-id"});
    EXPECT_EQ(duration_cast<std::chrono::seconds>(f1.GetModifyTime().time_since_epoch()).count(), 1526654523L);
    EXPECT_TRUE(f1.IsDownloadable());
    EXPECT_EQ(f1.GetSize(), 543345);
}

TEST(GoogleMetadataParserTest, ParseFolder)
{
    std::string object1 = R"""({
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

    auto folder1 = GoogleMetadataParser::ParseFolderMetadata(object1);
    EXPECT_STATUS_OK(folder1);
    auto f1 = folder1.Value();
    EXPECT_EQ(f1.GetCloudId(), std::string{"my-folder-id"});
    EXPECT_EQ(f1.GetName(), std::string{"my-folder-name"});
    EXPECT_EQ(f1.GetParentId(), std::string{"my-parent-id"});
    EXPECT_EQ(duration_cast<std::chrono::seconds>(f1.GetModifyTime().time_since_epoch()).count(), 1526654523L);
    EXPECT_TRUE(f1.GetCanUploadFile());
    EXPECT_TRUE(f1.GetCanCreateFolders());
    EXPECT_EQ(f1.GetSize(), 4096);
}

TEST(GoogleMetadataParserTest, ComposeFile)
{
    FileMetadata fm;
    fm.SetCloudId("File-cloud-id-1");
    fm.SetName("File-1");
    fm.SetParentId("Folder-parent-id-1");
    fm.SetSize(5678);
    const auto modifiedTime = ParseRfc3339("2018-08-02T01:02:03.001Z");
    EXPECT_STATUS_OK(modifiedTime);
    fm.SetChangeTime(*modifiedTime);
    fm.SetModifyTime(*modifiedTime);
    fm.SetAccessTime(*modifiedTime);
    fm.SetMimeTypeOpt("my-mime-type");
    fm.SetDownloadable(true);
    auto status_or_jfm = GoogleMetadataParser::ComposeFileMetadata(fm);
    EXPECT_THAT(status_or_jfm, IsOk());
    auto jfm = status_or_jfm.Value();
    EXPECT_TRUE(jfm.contains("kind"));
    EXPECT_EQ(jfm["kind"], std::string{"drive#file"});
    EXPECT_TRUE(jfm.contains("id"));
    EXPECT_EQ(jfm["id"], std::string{"File-cloud-id-1"});
    EXPECT_TRUE(jfm.contains("name"));
    EXPECT_EQ(jfm["name"], std::string{"File-1"});
    EXPECT_TRUE(jfm.contains("parents"));
    EXPECT_EQ(jfm["parents"].front(), std::string{"Folder-parent-id-1"});
    EXPECT_TRUE(jfm.contains("modifiedTime"));
    EXPECT_EQ(jfm["modifiedTime"], std::string{"2018-08-02T01:02:03.001Z"});
    EXPECT_TRUE(jfm.contains("mimeType"));
    EXPECT_EQ(jfm["mimeType"], std::string{"my-mime-type"});
    EXPECT_FALSE(jfm.contains("size"));
}

TEST(GoogleMetadataParserTest, ComposeFolder)
{
    FolderMetadata fm;
    fm.SetCloudId("Folder-cloud-id-1");
    fm.SetName("Folder-1");
    fm.SetParentId("Folder-parent-id-1");
    fm.SetSize(4096);
    const auto modifiedTime = ParseRfc3339("2018-08-02T01:02:03.001Z");
    EXPECT_STATUS_OK(modifiedTime);
    fm.SetChangeTime(*modifiedTime);
    fm.SetModifyTime(*modifiedTime);
    fm.SetAccessTime(*modifiedTime);
    fm.SetCanCreateFolders(true);
    fm.SetCanUploadFile(true);
    auto statusOrJfm = GoogleMetadataParser::ComposeFolderMetadata(fm);
    auto jfm = statusOrJfm.Value();
    EXPECT_TRUE(jfm.contains("kind"));
    EXPECT_EQ(jfm["kind"], std::string{"drive#file"});
    EXPECT_TRUE(jfm.contains("id"));
    EXPECT_EQ(jfm["id"], std::string{"Folder-cloud-id-1"});
    EXPECT_TRUE(jfm.contains("name"));
    EXPECT_EQ(jfm["name"], std::string{"Folder-1"});
    EXPECT_TRUE(jfm.contains("parents"));
    EXPECT_EQ(jfm["parents"].front(), std::string{"Folder-parent-id-1"});
    EXPECT_TRUE(jfm.contains("modifiedTime"));
    EXPECT_EQ(jfm["modifiedTime"], std::string{"2018-08-02T01:02:03.001Z"});
    EXPECT_TRUE(jfm.contains("mimeType"));
    EXPECT_EQ(jfm["mimeType"], std::string{"application/vnd.google-apps.folder"});
    EXPECT_FALSE(jfm.contains("size"));
}

TEST(GoogleMetadataParserTest, PatchFile)
{
    FileMetadata fm1;
    fm1.SetCloudId("File-cloud-id-1");
    fm1.SetName("File-1");
    fm1.SetParentId("Folder-parent-id-1");
    fm1.SetSize(5678);
    const auto modifiedTime = ParseRfc3339("2018-08-02T01:02:03.001Z");
    EXPECT_STATUS_OK(modifiedTime);
    fm1.SetChangeTime(*modifiedTime);
    fm1.SetModifyTime(*modifiedTime);
    fm1.SetAccessTime(*modifiedTime);
    fm1.SetMimeTypeOpt("my-mime-type");
    fm1.SetDownloadable(true);
    FileMetadata fm2{fm1};
    const auto modifiedTime2 = ParseRfc3339("2019-08-02T02:03:04.002Z");
    EXPECT_STATUS_OK(modifiedTime2);
    fm2.SetModifyTime(*modifiedTime2);
    fm2.SetName("File-1-modified");
    // Patch works only on relevant fields (very limited subset). See gdrive online documentation.
    auto pfm = GoogleMetadataParser::PatchFileMetadata(fm1, fm2).Value();
    EXPECT_EQ(2, pfm.size());
    EXPECT_TRUE(pfm.contains("name"));
    EXPECT_EQ(pfm["name"], std::string{"File-1-modified"});
    EXPECT_TRUE(pfm.contains("modifiedTime"));
    EXPECT_EQ(pfm["modifiedTime"], std::string{"2019-08-02T02:03:04.002Z"});
}

TEST(GoogleMetadataParserTest, PatchFolder)
{
    FolderMetadata fm1;
    fm1.SetCloudId("Folder-cloud-id-1");
    fm1.SetName("Folder-1");
    fm1.SetParentId("Folder-parent-id-1");
    fm1.SetSize(4096);
    const auto modifiedTime = ParseRfc3339("2018-08-02T01:02:03.001Z");
    EXPECT_STATUS_OK(modifiedTime);
    fm1.SetChangeTime(*modifiedTime);
    fm1.SetModifyTime(*modifiedTime);
    fm1.SetAccessTime(*modifiedTime);
    fm1.SetCanCreateFolders(true);
    fm1.SetCanUploadFile(true);
    FolderMetadata fm2{fm1};
    fm2.SetName("Folder-1-modified");
    // Patch works only on relevant fields (very limited subset). See gdrive online documentation.
    auto pfm = GoogleMetadataParser::PatchFolderMetadata(fm1, fm2).Value();
    EXPECT_EQ(1, pfm.size());
    EXPECT_TRUE(pfm.contains("name"));
    EXPECT_EQ(pfm["name"], std::string{"Folder-1-modified"});
}

}  // namespace internal
}  // namespace csa
