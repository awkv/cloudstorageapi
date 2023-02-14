// Copyright 2021 Andrew Karasyov
//
// Copyright 2020 Google LLC
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

#include "testing_util/status_matchers.h"
#include "cloudstorageapi/status_or_val.h"
#include <gtest/gtest-spi.h>
#include <gtest/gtest.h>

namespace csa {
namespace internal {

using ::csa::testing::util::IsOk;
using ::csa::testing::util::StatusIs;
using ::testing::_;
using ::testing::AnyOf;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Not;

TEST(AssertOkTest, AssertionOk)
{
    Status status;
    ASSERT_STATUS_OK(status);
}

TEST(AssertOkTest, AssertionOkStatusOr)
{
    StatusOrVal<int> status_or(42);
    ASSERT_STATUS_OK(status_or);
}

TEST(AssertOkTest, AssertionOkDescription)
{
    Status status;
    ASSERT_STATUS_OK(status) << "OK is not OK?";
}

TEST(AssertOkTest, AssertionOkDescriptionStatusOr)
{
    StatusOrVal<int> status_or(42);
    ASSERT_STATUS_OK(status_or) << "OK is not OK?";
}

TEST(AssertOkTest, AssertionFailed)
{
    EXPECT_FATAL_FAILURE(
        {
            Status status(StatusCode::Internal, "oh no!");
            ASSERT_STATUS_OK(status);
        },
        "\n  Actual: oh no! [INTERNAL]");
}

TEST(AssertOkTest, AssertionFailedStatusOr)
{
    EXPECT_FATAL_FAILURE(
        {
            StatusOrVal<int> status_or(Status(StatusCode::Internal, "oh no!"));
            ASSERT_STATUS_OK(status_or);
        },
        ", whose status is oh no! [INTERNAL]");
}

TEST(AssertOkTest, AssertionFailedDescription)
{
    EXPECT_FATAL_FAILURE(
        {
            Status status(StatusCode::Internal, "oh no!");
            ASSERT_STATUS_OK(status) << "my assertion failed";
        },
        "\nmy assertion failed");
}

TEST(AssertOkTest, AssertionFailedDescriptionStatusOr)
{
    EXPECT_FATAL_FAILURE(
        {
            StatusOrVal<int> status_or(Status(StatusCode::Internal, "oh no!"));
            ASSERT_STATUS_OK(status_or) << "my assertion failed";
        },
        "\nmy assertion failed");
}

TEST(ExpectOkTest, ExpectOk)
{
    Status status;
    EXPECT_STATUS_OK(status);
}

TEST(ExpectOkTest, ExpectOkStatusOr)
{
    StatusOrVal<int> status_or(42);
    EXPECT_STATUS_OK(status_or);
}

TEST(ExpectOkTest, ExpectationOkDescription)
{
    Status status;
    EXPECT_STATUS_OK(status) << "OK is not OK?";
}

TEST(ExpectOkTest, ExpectationOkDescriptionStatusOr)
{
    StatusOrVal<int> status_or(42);
    EXPECT_STATUS_OK(status_or) << "OK is not OK?";
}

TEST(ExpectOkTest, ExpectationFailed)
{
    EXPECT_NONFATAL_FAILURE(
        {
            Status status(StatusCode::Internal, "oh no!");
            EXPECT_STATUS_OK(status);
        },
        "\n  Actual: oh no! [INTERNAL]");
}

TEST(ExpectOkTest, ExpectationFailedStatusOr)
{
    EXPECT_NONFATAL_FAILURE(
        {
            StatusOrVal<int> status_or(Status(StatusCode::Internal, "oh no!"));
            EXPECT_STATUS_OK(status_or);
        },
        ", whose status is oh no! [INTERNAL]");
}

TEST(ExpectOkTest, ExpectationFailedDescription)
{
    EXPECT_NONFATAL_FAILURE(
        {
            Status status(StatusCode::Internal, "oh no!");
            EXPECT_STATUS_OK(status) << "my assertion failed";
        },
        "\nmy assertion failed");
}

TEST(ExpectOkTest, ExpectationFailedDescriptionStatusOr)
{
    EXPECT_NONFATAL_FAILURE(
        {
            StatusOrVal<int> status_or(Status(StatusCode::Internal, "oh no!"));
            EXPECT_STATUS_OK(status_or) << "my assertion failed";
        },
        "\nmy assertion failed");
}

TEST(StatusIsTest, OkStatus)
{
    Status status;
    EXPECT_THAT(status, IsOk());
    EXPECT_THAT(status, StatusIs(StatusCode::Ok));
    EXPECT_THAT(status, StatusIs(_));
    EXPECT_THAT(status, StatusIs(StatusCode::Ok, IsEmpty()));
    EXPECT_THAT(status, StatusIs(_, _));
}

TEST(StatusIsTest, FailureStatus)
{
    Status status(StatusCode::Unknown, "hello");
    EXPECT_THAT(status, StatusIs(StatusCode::Unknown, "hello"));
    EXPECT_THAT(status, StatusIs(StatusCode::Unknown, Eq("hello")));
    EXPECT_THAT(status, StatusIs(StatusCode::Unknown, HasSubstr("ello")));
    EXPECT_THAT(status, StatusIs(_, AnyOf("hello", "goodbye")));
    EXPECT_THAT(status, StatusIs(AnyOf(StatusCode::Aborted, StatusCode::Unknown), _));
    EXPECT_THAT(status, StatusIs(StatusCode::Unknown, _));
    EXPECT_THAT(status, StatusIs(_, "hello"));
    EXPECT_THAT(status, StatusIs(_, _));
}

TEST(StatusIsTest, FailureStatusNegation)
{
    Status status(StatusCode::NotFound, "not found");

    // code doesn't match
    EXPECT_THAT(status, Not(StatusIs(StatusCode::Unknown, "not found")));

    // message doesn't match
    EXPECT_THAT(status, Not(StatusIs(StatusCode::NotFound, "found")));

    // both don't match
    EXPECT_THAT(status, Not(StatusIs(StatusCode::Cancelled, "goodbye")));

    // combine with a few other matchers
    EXPECT_THAT(status, Not(StatusIs(AnyOf(StatusCode::InvalidArgument, StatusCode::Internal))));
    EXPECT_THAT(status, Not(StatusIs(StatusCode::Unknown, Eq("goodbye"))));
    EXPECT_THAT(status, StatusIs(Not(StatusCode::Unknown), Not(IsEmpty())));
}

TEST(StatusIsTest, OkStatusOr)
{
    StatusOrVal<std::string> status("StatusOrVal string value");
    EXPECT_THAT(status, IsOk());
    EXPECT_THAT(status, StatusIs(StatusCode::Ok));
    EXPECT_THAT(status, StatusIs(_));
    EXPECT_THAT(status, StatusIs(StatusCode::Ok, IsEmpty()));
    EXPECT_THAT(status, StatusIs(_, _));
}

TEST(StatusIsTest, FailureStatusOr)
{
    StatusOrVal<int> status(Status(StatusCode::Unknown, "hello"));
    EXPECT_THAT(status, StatusIs(StatusCode::Unknown, "hello"));
    EXPECT_THAT(status, StatusIs(StatusCode::Unknown, Eq("hello")));
    EXPECT_THAT(status, StatusIs(StatusCode::Unknown, HasSubstr("ello")));
    EXPECT_THAT(status, StatusIs(_, AnyOf("hello", "goodbye")));
    EXPECT_THAT(status, StatusIs(AnyOf(StatusCode::Aborted, StatusCode::Unknown), _));
    EXPECT_THAT(status, StatusIs(StatusCode::Unknown, _));
    EXPECT_THAT(status, StatusIs(_, "hello"));
    EXPECT_THAT(status, StatusIs(_, _));
}

TEST(StatusIsTest, FailureStatusOrNegation)
{
    StatusOrVal<float> status(Status(StatusCode::NotFound, "not found"));

    // code doesn't match
    EXPECT_THAT(status, Not(StatusIs(StatusCode::Unknown, "not found")));

    // message doesn't match
    EXPECT_THAT(status, Not(StatusIs(StatusCode::NotFound, "found")));

    // both don't match
    EXPECT_THAT(status, Not(StatusIs(StatusCode::Cancelled, "goodbye")));

    // combine with a few other matchers
    EXPECT_THAT(status, Not(StatusIs(AnyOf(StatusCode::InvalidArgument, StatusCode::Internal))));
    EXPECT_THAT(status, Not(StatusIs(StatusCode::Unknown, Eq("goodbye"))));
    EXPECT_THAT(status, StatusIs(Not(StatusCode::Unknown), Not(IsEmpty())));
}

}  // namespace internal
}  // namespace csa
