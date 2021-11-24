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

#include "cloudstorageapi/folder_metadata.h"
#include <gmock/gmock.h>
#include <chrono>

namespace csa {
namespace internal {

using ::testing::HasSubstr;

namespace {
FolderMetadata CreateFolderMetadataForTest()
{
    FolderMetadata fm;
    fm.SetCloudId("Folder-cloud-id-1");
    fm.SetName("Folder-1");
    fm.SetParentId("Folder-parent-id-1");
    fm.SetSize(4096);
    const auto modifiedTime = std::chrono::system_clock::now();
    fm.SetChangeTime(modifiedTime);
    fm.SetModifyTime(modifiedTime);
    fm.SetAccessTime(modifiedTime);
    fm.SetCanCreateFolders(true);
    fm.SetCanUploadFile(true);

    return fm;
}
}  // namespace

TEST(FolderMetadataTest, Fields)
{
    FolderMetadata fm = CreateFolderMetadataForTest();
    EXPECT_EQ("Folder-cloud-id-1", fm.GetCloudId());
    EXPECT_EQ("Folder-1", fm.GetName());
    EXPECT_EQ("Folder-parent-id-1", fm.GetParentId());
    EXPECT_EQ(4096, fm.GetSize());
    const auto modifiedTime = std::chrono::system_clock::now();
    fm.SetChangeTime(modifiedTime);
    fm.SetModifyTime(modifiedTime);
    fm.SetAccessTime(modifiedTime);
    EXPECT_EQ(modifiedTime, fm.GetChangeTime());
    EXPECT_EQ(modifiedTime, fm.GetModifyTime());
    EXPECT_EQ(modifiedTime, fm.GetAccessTime());
    EXPECT_EQ(true, fm.GetCanCreateFolders());
    EXPECT_EQ(true, fm.GetCanUploadFile());
}

TEST(FolderMetadataTest, IOStream)
{
    FolderMetadata fm = CreateFolderMetadataForTest();
    const auto modifiedTime = std::chrono::system_clock::now();
    fm.SetChangeTime(modifiedTime);
    fm.SetModifyTime(modifiedTime);
    fm.SetAccessTime(modifiedTime);

    std::ostringstream os;
    os << fm;
    auto actual = os.str();
    EXPECT_THAT(actual, HasSubstr("Folder-cloud-id-1"));
    EXPECT_THAT(actual, HasSubstr("Folder-1"));
    EXPECT_THAT(actual, HasSubstr("Folder-parent-id-1"));
    EXPECT_THAT(actual, HasSubstr("4096"));
    auto timestampStr = std::to_string(fm.GetChangeTime().time_since_epoch().count());
    EXPECT_THAT(actual, HasSubstr(timestampStr));
    EXPECT_THAT(actual, HasSubstr("true"));
}

TEST(FolderMetadataTest, Equality)
{
    FolderMetadata fm = CreateFolderMetadataForTest();
    FolderMetadata copy = fm;
    ASSERT_EQ(fm, copy);
    copy.SetName("different name");
    ASSERT_NE(fm, copy);
}

}  // namespace internal
}  // namespace csa
