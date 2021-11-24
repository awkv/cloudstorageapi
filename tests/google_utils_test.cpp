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

#include "cloudstorageapi/internal/clients/google_utils.h"
#include "util/status_matchers.h"
#include <gmock/gmock.h>
#include <nlohmann/json.hpp>

namespace csa {
namespace internal {

using ::csa::testing::util::IsOk;
using ::testing::HasSubstr;
using ::testing::Not;

TEST(GoogleUtilsTest, UploadChunkRangeHeader)
{
    std::string const url =
        "https://storage.server.com/upload/storage/v1/b/"
        "myBucket/o?uploadType=resumable"
        "&upload_id=xa298sd_sdlkj2";
    UploadChunkRequest request(url, 0, {ConstBuffer{"abc123", 6}}, 2048);
    EXPECT_EQ("Content-Range: bytes 0-5/2048", GoogleUtils::GetRangeHeader(request));
}

TEST(GoogleUtilsTest, UploadChunkRangeHeaderNotLast)
{
    std::string const url = "https://unused.googleapis.com/test-only";
    UploadChunkRequest request(url, 1024, {ConstBuffer{"1234", 4}});
    EXPECT_EQ("Content-Range: bytes 1024-1027/*", GoogleUtils::GetRangeHeader(request));
}

TEST(GoogleUtilsTest, UploadChunkRangeHeaderLast)
{
    std::string const url = "https://unused.googleapis.com/test-only";
    UploadChunkRequest request(url, 2045, {ConstBuffer{"1234", 4}}, 2048U);
    EXPECT_EQ("Content-Range: bytes 2045-2048/2048", GoogleUtils::GetRangeHeader(request));
}

TEST(GoogleUtilsTest, UploadChunkRangeHeaderEmptyPayloadNotLast)
{
    std::string const url = "https://unused.googleapis.com/test-only";
    UploadChunkRequest request(url, 1024, {});
    EXPECT_EQ("Content-Range: bytes */*", GoogleUtils::GetRangeHeader(request));
}

TEST(GoogleUtilsTest, UploadChunkRangeHeaderEmptyPayloadLast)
{
    std::string const url = "https://unused.googleapis.com/test-only";
    UploadChunkRequest request(url, 2047, {}, 2048U);
    EXPECT_EQ("Content-Range: bytes */2048", GoogleUtils::GetRangeHeader(request));
}

TEST(GoogleUtilsTest, UploadChunkRangeHeaderEmptyPayloadEmpty)
{
    std::string const url = "https://unused.googleapis.com/test-only";
    UploadChunkRequest r0(url, 1024, {}, 0U);
    EXPECT_EQ("Content-Range: bytes */0", GoogleUtils::GetRangeHeader(r0));
    UploadChunkRequest r1(url, 1024, {{}, {}, {}}, 0U);
    EXPECT_EQ("Content-Range: bytes */0", GoogleUtils::GetRangeHeader(r1));
}

TEST(GoogleUtilsTest, ReadFileRangeRangeHeader)
{
    EXPECT_EQ("", GoogleUtils::GetRangeHeader(ReadFileRangeRequest("my-file-id")));
    EXPECT_EQ("Range: bytes=0-2047",
              GoogleUtils::GetRangeHeader(ReadFileRangeRequest("my-file-id").SetMultipleOptions(ReadRange(0, 2048))));
    EXPECT_EQ("Range: bytes=1024-",
              GoogleUtils::GetRangeHeader(ReadFileRangeRequest("my-file-id").SetMultipleOptions(ReadFromOffset(1024))));
    EXPECT_EQ("",
              GoogleUtils::GetRangeHeader(ReadFileRangeRequest("my-file-id").SetMultipleOptions(ReadFromOffset(0))));
    EXPECT_EQ("Range: bytes=1024-2047",
              GoogleUtils::GetRangeHeader(
                  ReadFileRangeRequest("my-file-id").SetMultipleOptions(ReadRange(0, 2048), ReadFromOffset(1024))));
    EXPECT_EQ("Range: bytes=-1024",
              GoogleUtils::GetRangeHeader(ReadFileRangeRequest("my-file-id").SetMultipleOptions(ReadLast(1024))));
    EXPECT_EQ("Range: bytes=-0",
              GoogleUtils::GetRangeHeader(ReadFileRangeRequest("my-file-id").SetMultipleOptions(ReadLast(0))));
}

}  // namespace internal
}  // namespace csa
