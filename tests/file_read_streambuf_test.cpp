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

#include "cloudstorageapi/internal/file_read_streambuf.h"
#include "mock_cloud_storage_client.h"
#include "mock_object_read_source.h"
#include <gmock/gmock.h>

namespace csa {
namespace internal {

using ::testing::Return;

TEST(FileReadStreambufTest, FailedTellg)
{
    FileReadStreambuf buf(ReadFileRangeRequest{}, Status(StatusCode::InvalidArgument, "some error"));
    std::istream stream(&buf);
    EXPECT_EQ(-1, stream.tellg());
}

TEST(FileReadStreambufTest, Success)
{
    auto readSource = std::make_unique<MockObjectReadSource>();
    EXPECT_CALL(*readSource, IsOpen()).WillRepeatedly(Return(true));
    EXPECT_CALL(*readSource, Read)
        .WillOnce(Return(ReadSourceResult{10, {}}))
        .WillOnce(Return(ReadSourceResult{15, {}}))
        .WillOnce(Return(ReadSourceResult{15, {}}))
        .WillOnce(Return(ReadSourceResult{128 * 1024, {}}));
    FileReadStreambuf buf(ReadFileRangeRequest{}, std::move(readSource), 0);

    std::istream stream(&buf);
    EXPECT_EQ(0, stream.tellg());

    auto read = [&](std::size_t toRead, std::streampos expectedTellg) {
        std::vector<char> v(toRead);
        stream.read(v.data(), v.size());
        EXPECT_TRUE(!!stream);
        EXPECT_EQ(expectedTellg, stream.tellg());
    };
    read(10, 10);
    read(15, 25);
    read(15, 40);
    stream.get();
    EXPECT_EQ(41, stream.tellg());
    read(1000, 1041);
    read(2000, 3041);
    // Reach eof
    std::vector<char> v(128 * 1024 - 1 - 1000 - 2000);
    stream.read(v.data(), v.size());
    EXPECT_EQ(128 * 1024 + 15 + 15 + 10, stream.tellg());
}

TEST(FileReadStreambufTest, WrongSeek)
{
    auto readSource = std::make_unique<MockObjectReadSource>();
    EXPECT_CALL(*readSource, IsOpen()).WillRepeatedly(Return(true));
    EXPECT_CALL(*readSource, Read).WillOnce(Return(ReadSourceResult{10, {}}));
    FileReadStreambuf buf(ReadFileRangeRequest{}, std::move(readSource), 0);

    std::istream stream(&buf);
    EXPECT_EQ(0, stream.tellg());

    std::vector<char> v(10);
    stream.read(v.data(), v.size());
    EXPECT_TRUE(!!stream);
    EXPECT_EQ(10, stream.tellg());
    EXPECT_FALSE(stream.fail());
    stream.seekg(10);
    EXPECT_TRUE(stream.fail());
    stream.clear();
    stream.seekg(-1, std::ios_base::cur);
    EXPECT_TRUE(stream.fail());
    stream.clear();
    stream.seekg(0, std::ios_base::beg);
    EXPECT_TRUE(stream.fail());
    stream.clear();
    stream.seekg(0, std::ios_base::end);
    EXPECT_TRUE(stream.fail());
}

}  // namespace internal
}  // namespace csa
