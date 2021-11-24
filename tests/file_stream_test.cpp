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

#include "cloudstorageapi/file_stream.h"
#include "cloudstorageapi/internal/raw_client.h"
#include "util/status_matchers.h"
#include <gmock/gmock.h>

namespace csa {
namespace internal {
using ::csa::testing::util::IsOk;
using ::csa::testing::util::StatusIs;
using ::testing::NotNull;

TEST(FileStream, ReadMoveConstructor)
{
    FileReadStream reader;
    ASSERT_THAT(reader.rdbuf(), NotNull());
    reader.setstate(std::ios::badbit | std::ios::eofbit);
    EXPECT_TRUE(reader.bad());
    EXPECT_TRUE(reader.eof());

    FileReadStream copy(std::move(reader));
    ASSERT_THAT(copy.rdbuf(), NotNull());
    EXPECT_TRUE(copy.bad());
    EXPECT_TRUE(copy.eof());
    EXPECT_THAT(copy.GetStatus(), StatusIs(StatusCode::Unimplemented));

    // NOLINTNEXTLINE(bugprone-use-after-move)
    EXPECT_THAT(reader.GetStatus(), Not(IsOk()));
    EXPECT_THAT(reader.rdbuf(), NotNull());
}

TEST(FileStream, ReadMoveAssignment)
{
    FileReadStream reader;
    ASSERT_THAT(reader.rdbuf(), NotNull());
    reader.setstate(std::ios::badbit | std::ios::eofbit);

    FileReadStream copy;

    copy = std::move(reader);
    ASSERT_THAT(copy.rdbuf(), NotNull());
    EXPECT_TRUE(copy.bad());
    EXPECT_TRUE(copy.eof());
    EXPECT_THAT(copy.GetStatus(), StatusIs(StatusCode::Unimplemented));

    // NOLINTNEXTLINE(bugprone-use-after-move)
    ASSERT_THAT(reader.rdbuf(), NotNull());
    EXPECT_THAT(reader.GetStatus(), Not(IsOk()));
}

TEST(FileStream, WriteMoveConstructor)
{
    FileWriteStream writer;
    ASSERT_THAT(writer.rdbuf(), NotNull());
    EXPECT_THAT(writer.GetMetadata(), StatusIs(StatusCode::Unimplemented));

    FileWriteStream copy(std::move(writer));
    ASSERT_THAT(copy.rdbuf(), NotNull());
    EXPECT_TRUE(copy.bad());
    EXPECT_TRUE(copy.eof());
    EXPECT_THAT(copy.GetMetadata(), StatusIs(StatusCode::Unimplemented));

    // NOLINTNEXTLINE(bugprone-use-after-move)
    ASSERT_THAT(writer.rdbuf(), NotNull());
    EXPECT_THAT(writer.GetLastStatus(), Not(IsOk()));
}

TEST(FileStream, WriteMoveAssignment)
{
    FileWriteStream writer;
    ASSERT_THAT(writer.rdbuf(), NotNull());
    EXPECT_THAT(writer.GetMetadata(), StatusIs(StatusCode::Unimplemented));

    FileWriteStream copy;

    copy = std::move(writer);
    ASSERT_THAT(copy.rdbuf(), NotNull());
    EXPECT_TRUE(copy.bad());
    EXPECT_TRUE(copy.eof());
    EXPECT_THAT(copy.GetMetadata(), StatusIs(StatusCode::Unimplemented));

    // NOLINTNEXTLINE(bugprone-use-after-move)
    ASSERT_THAT(writer.rdbuf(), NotNull());
    EXPECT_THAT(writer.GetLastStatus(), Not(IsOk()));
}

TEST(FileStream, Suspend)
{
    FileWriteStream writer;
    ASSERT_THAT(writer.rdbuf(), NotNull());
    EXPECT_THAT(writer.GetMetadata(), StatusIs(StatusCode::Unimplemented));

    std::move(writer).Suspend();
    // NOLINTNEXTLINE(bugprone-use-after-move)
    ASSERT_THAT(writer.rdbuf(), NotNull());
    EXPECT_THAT(writer.GetLastStatus(), Not(IsOk()));
}

}  // namespace internal
}  // namespace csa
