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

#include "cloudstorageapi/internal/file_requests.h"
#include "testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <nlohmann/json.hpp>

namespace csa {
namespace internal {

using ::csa::testing::util::IsOk;
using ::testing::HasSubstr;
using ::testing::Not;

TEST(FileRequestsTest, GetFileMetadata)
{
    GetFileMetadataRequest request("my-file-id");
    std::ostringstream os;
    os << request;
    auto str = os.str();
    EXPECT_THAT(str, HasSubstr("my-file-id"));
}

TEST(FileRequestsTest, PatchFileMetadata)
{
    FileMetadata fmOld;
    fmOld.SetName("test-file-old");
    FileMetadata fmNew;
    fmNew.SetName("test-file-new");
    PatchFileMetadataRequest request("test-file-id", std::move(fmOld), std::move(fmNew));
    EXPECT_EQ("test-file-id", request.GetObjectId());

    std::ostringstream os;
    os << request;
    std::string actual = os.str();
    EXPECT_THAT(actual, HasSubstr("test-file-id"));
    EXPECT_THAT(actual, HasSubstr("test-file-old"));
    EXPECT_THAT(actual, HasSubstr("test-file-new"));
}

TEST(FileRequestsTest, InsertFile)
{
    InsertFileRequest request("my-folder-id", "my-file-name", "file contents");
    request.SetMultipleOptions(ContentEncoding("media"));
    std::ostringstream os;
    os << request;
    auto str = os.str();
    EXPECT_THAT(str, HasSubstr("InsertFileRequest"));
    EXPECT_THAT(str, HasSubstr("my-folder-id"));
    EXPECT_THAT(str, HasSubstr("my-file-name"));
    EXPECT_THAT(str, HasSubstr("contentEncoding=media"));
}
TEST(FileRequestsTest, InsertFileUpdateContents)
{
    InsertFileRequest request("my-folder-id", "my-file-name", "file contents");
    EXPECT_EQ("file contents", request.GetContent());
    request.SetContent("new contents");
    EXPECT_EQ("new contents", request.GetContent());
}

TEST(FileRequestsTest, Delete)
{
    DeleteRequest request("my-file-id");
    std::ostringstream os;
    os << request;
    EXPECT_THAT(os.str(), HasSubstr("my-file-id"));
}

TEST(FileRequestsTest, ResumableUpload)
{
    FileMetadata fm;
    fm.SetMimeTypeOpt("text/plain");
    ResumableUploadRequest request("source-folder-id", "source-file-name");
    EXPECT_EQ("source-folder-id", request.GetObjectId());
    request.SetMultipleOptions(WithFileMetadata(fm));

    std::ostringstream os;
    os << request;
    std::string actual = os.str();
    EXPECT_THAT(actual, HasSubstr("source-folder-id"));
    EXPECT_THAT(actual, HasSubstr("source-file-name"));
    EXPECT_THAT(actual, HasSubstr("text/plain"));
}

TEST(FileRequestsTest, DeleteResumableUpload)
{
    DeleteResumableUploadRequest request("source-upload-session-url");
    EXPECT_EQ("source-upload-session-url", request.GetUploadSessionUrl());

    std::ostringstream os;
    os << request;
    std::string actual = os.str();
    EXPECT_THAT(actual, HasSubstr("source-upload-session-url"));
}

TEST(FileRequestsTest, UploadChunk)
{
    std::string const url =
        "https://storage.server.com/upload/storage/v1/b/"
        "myBucket/o?uploadType=resumable"
        "&upload_id=xa298sd_sdlkj2";
    UploadChunkRequest request(url, 0, {ConstBuffer{"abc123", 6}}, 2048);
    EXPECT_EQ(url, request.GetUploadSessionUrl());
    EXPECT_EQ(0, request.GetRangeBegin());
    EXPECT_EQ(5, request.GetRangeEnd());
    EXPECT_EQ(2048, request.GetSourceSize());

    std::ostringstream os;
    os << request;
    std::string actual = os.str();
    EXPECT_THAT(actual, HasSubstr(url));
}

TEST(FileRequestsTest, QueryResumableUpload)
{
    std::string const url =
        "https://storage.server.com/upload/storage/v1/b/"
        "myBucket/o?uploadType=resumable"
        "&upload_id=xa298sd_sdlkj2";
    QueryResumableUploadRequest request(url);
    EXPECT_EQ(url, request.GetUploadSessionUrl());

    std::ostringstream os;
    os << request;
    std::string actual = os.str();
    EXPECT_THAT(actual, HasSubstr(url));
}

TEST(FileRequestsTest, ReadFileRange)
{
    ReadFileRangeRequest request("my-file-id");

    EXPECT_EQ("my-file-id", request.GetObjectId());

    request.SetMultipleOptions(ReadRange(0, 1024));

    std::ostringstream os;
    os << request;
    std::string actual = os.str();
    EXPECT_THAT(actual, HasSubstr("my-file-id"));
    EXPECT_THAT(actual, HasSubstr("begin=0"));
    EXPECT_THAT(actual, HasSubstr("end=1024"));
}

TEST(FileRequestsTest, ReadFileRangeRequiresRangeHeader)
{
    EXPECT_FALSE(ReadFileRangeRequest("my-file-id").RequiresRangeHeader());
    EXPECT_TRUE(ReadFileRangeRequest("my-file-id").SetMultipleOptions(ReadRange(0, 2048)).RequiresRangeHeader());
    EXPECT_TRUE(ReadFileRangeRequest("my-file-id").SetMultipleOptions(ReadFromOffset(1024)).RequiresRangeHeader());
    EXPECT_FALSE(ReadFileRangeRequest("my-file-id").SetMultipleOptions(ReadFromOffset(0)).RequiresRangeHeader());
    EXPECT_TRUE(ReadFileRangeRequest("my-file-id")
                    .SetMultipleOptions(ReadRange(0, 2048), ReadFromOffset(1024))
                    .RequiresRangeHeader());
    EXPECT_TRUE(ReadFileRangeRequest("my-file-id").SetMultipleOptions(ReadLast(1024)).RequiresRangeHeader());
    EXPECT_TRUE(ReadFileRangeRequest("my-file-id").SetMultipleOptions(ReadLast(0)).RequiresRangeHeader());
}

TEST(FileRequestsTest, CopyFile)
{
    FileMetadata fm;
    fm.SetMimeTypeOpt("text/plain");
    CopyFileRequest request("source-file-id", "my-folder-id", "my-file-name");
    EXPECT_EQ("source-file-id", request.GetObjectId());
    EXPECT_EQ("my-folder-id", request.GetDestinationParentId());
    EXPECT_EQ("my-file-name", request.GetDestinationFileName());
    request.SetMultipleOptions(WithFileMetadata(fm));

    std::ostringstream os;
    os << request;
    std::string actual = os.str();
    EXPECT_THAT(actual, HasSubstr("source-file-id"));
    EXPECT_THAT(actual, HasSubstr("my-folder-id"));
    EXPECT_THAT(actual, HasSubstr("my-file-name"));
    EXPECT_THAT(actual, HasSubstr("text/plain"));
}

}  // namespace internal
}  // namespace csa
