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

#include "cloudstorageapi/file_metadata.h"
#include <gmock/gmock.h>

namespace csa {
namespace internal {
using ::testing::HasSubstr;
namespace {
constexpr auto fileSizeTest = 9 * 1024;
FileMetadata CreateFileMetadataForTest()
{
    FileMetadata fm;
    fm.SetCloudId("File-cloud-id-1");
    fm.SetName("File-1");
    fm.SetParentId("Folder-parent-id-1");
    fm.SetSize(fileSizeTest);
    const auto modifiedTime = std::chrono::system_clock::now();
    fm.SetChangeTime(modifiedTime);
    fm.SetModifyTime(modifiedTime);
    fm.SetAccessTime(modifiedTime);
    // fm.SetMimeTypeOpt(); intentionally left unset
    fm.SetDownloadable(true);

    return fm;
}
}  // namespace

TEST(FileMetadataTest, Fields)
{
    FileMetadata fm = CreateFileMetadataForTest();
    EXPECT_EQ("File-cloud-id-1", fm.GetCloudId());
    EXPECT_EQ("File-1", fm.GetName());
    EXPECT_EQ("Folder-parent-id-1", fm.GetParentId());
    EXPECT_EQ(fileSizeTest, fm.GetSize());
    const auto modifiedTime = std::chrono::system_clock::now();
    fm.SetChangeTime(modifiedTime);
    fm.SetModifyTime(modifiedTime);
    fm.SetAccessTime(modifiedTime);
    EXPECT_EQ(modifiedTime, fm.GetChangeTime());
    EXPECT_EQ(modifiedTime, fm.GetModifyTime());
    EXPECT_EQ(modifiedTime, fm.GetAccessTime());
    EXPECT_FALSE(fm.GetMimeTypeOpt().has_value());
    EXPECT_TRUE(fm.IsDownloadable());
}

TEST(FileMetadataTest, IOStream)
{
    FileMetadata fm = CreateFileMetadataForTest();
    const auto modifiedTime = std::chrono::system_clock::now();
    fm.SetChangeTime(modifiedTime);
    fm.SetModifyTime(modifiedTime);
    fm.SetAccessTime(modifiedTime);

    std::ostringstream os;
    os << fm;
    auto actual = os.str();
    EXPECT_THAT(actual, HasSubstr("File-cloud-id-1"));
    EXPECT_THAT(actual, HasSubstr("File-1"));
    EXPECT_THAT(actual, HasSubstr("Folder-parent-id-1"));
    EXPECT_THAT(actual, HasSubstr(std::to_string(fileSizeTest)));
    auto timestampStr = std::to_string(fm.GetChangeTime().time_since_epoch().count());
    EXPECT_THAT(actual, HasSubstr(timestampStr));
    EXPECT_THAT(actual, HasSubstr("true"));
    EXPECT_THAT(actual, HasSubstr("N/A"));
}

TEST(FileMetadataTest, Equality)
{
    FileMetadata fm = CreateFileMetadataForTest();
    FileMetadata copy = fm;
    ASSERT_EQ(fm, copy);
    copy.SetName("different name");
    ASSERT_NE(fm, copy);
}
}  // namespace internal
}  // namespace csa
