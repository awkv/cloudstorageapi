// Copyright 2021 Andrew Karasyov
//
// Licensed under the Apache License, Version 2.0 (the "License");
/// Copyright 2018 Google LLC
//
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

#include "cloudstorageapi/internal/folder_requests.h"
#include "util/status_matchers.h"
#include <gmock/gmock.h>
#include <nlohmann/json.hpp>

namespace csa {
namespace internal {

using ::csa::testing::util::IsOk;
using ::testing::HasSubstr;
using ::testing::Not;

TEST(FolderRequestsTest, ListFolder)
{
    ListFolderRequest request("my-folder-id");
    EXPECT_EQ("my-folder-id", request.GetObjectId());
    request.SetMultipleOptions(MaxResults(109));

    std::ostringstream os;
    os << request;
    std::string actual = os.str();
    EXPECT_THAT(actual, HasSubstr("my-folder-id"));
    EXPECT_THAT(actual, HasSubstr("109"));
}

TEST(FolderRequestsTest, GetFolderMetadata)
{
    GetFolderMetadataRequest request("my-folder-id");
    std::ostringstream os;
    os << request;
    auto str = os.str();
    EXPECT_THAT(str, HasSubstr("my-folder-id"));
}

TEST(FolderRequestsTest, CreateFolder)
{
    CreateFolderRequest request("my-parent-folder-id", "my-folder-name");
    std::ostringstream os;
    os << request;
    EXPECT_THAT(os.str(), HasSubstr("my-parent-folder-id"));
    EXPECT_THAT(os.str(), HasSubstr("my-folder-name"));
}

TEST(FolderRequestsTest, PatchFolderMetadata)
{
    FolderMetadata fmOld;
    fmOld.SetName("test-folder-old");
    FolderMetadata fmNew;
    fmNew.SetName("test-folder-new");
    PatchFolderMetadataRequest request("test-folder-id", std::move(fmOld), std::move(fmNew));
    EXPECT_EQ("test-folder-id", request.GetObjectId());

    std::ostringstream os;
    os << request;
    std::string actual = os.str();
    EXPECT_THAT(actual, HasSubstr("test-folder-id"));
    EXPECT_THAT(actual, HasSubstr("test-folder-old"));
    EXPECT_THAT(actual, HasSubstr("test-folder-new"));
}

}  // namespace internal
}  // namespace csa
