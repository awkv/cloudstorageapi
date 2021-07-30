// Copyright 2019 Andrew Karasyov
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

#include "util/assert_ok.h"
#include "cloudstorageapi/status.h"
#include <gtest/gtest-spi.h>
#include <gtest/gtest.h>

using ::csa::Status;
using ::csa::StatusCode;
using ::csa::StatusOrVal;

TEST(AssertOkTest, AssertionOk)
{
    Status status;
    ASSERT_STATUS_OK(status);
}

TEST(AssertOkTest, AssertionOkStatusOr)
{
    StatusOrVal<int> statusOrVal(42);
    ASSERT_STATUS_OK(statusOrVal);
}

TEST(AssertOkTest, AssertionOkDescription)
{
    Status status;
    ASSERT_STATUS_OK(status) << "OK is not OK?";
}

TEST(AssertOkTest, AssertionOkDescriptionStatusOr)
{
    StatusOrVal<int> statusOrVal(42);
    ASSERT_STATUS_OK(statusOrVal) << "OK is not OK?";
}

TEST(AssertOkTest, AssertionFailed)
{
    EXPECT_FATAL_FAILURE(
        {
            Status status(StatusCode::Internal, "oh no!");
            ASSERT_STATUS_OK(status);
        },
        "Value of: status\nExpected: is OK\nActual: oh no! [INTERNAL]");
}

TEST(AssertOkTest, AssertionFailedStatusOr)
{
    EXPECT_FATAL_FAILURE(
        {
            StatusOrVal<int> statusOrVal(Status(StatusCode::Internal, "oh no!"));
            ASSERT_STATUS_OK(statusOrVal);
        },
        "Value of: statusOrVal\nExpected: is OK\nActual: oh no! [INTERNAL]");
}

TEST(AssertOkTest, AssertionFailedDescription)
{
    EXPECT_FATAL_FAILURE(
        {
            Status status(StatusCode::Internal, "oh no!");
            ASSERT_STATUS_OK(status) << "my precious assertion failed";
        },
        "Value of: status\nExpected: is OK\nActual: oh no! [INTERNAL]\nmy "
        "precious assertion failed");
}

TEST(AssertOkTest, AssertionFailedDescriptionStatusOr)
{
    EXPECT_FATAL_FAILURE(
        {
            StatusOrVal<int> statusOrVal(Status(StatusCode::Internal, "oh no!"));
            ASSERT_STATUS_OK(statusOrVal) << "my precious assertion failed";
        },
        "Value of: statusOrVal\nExpected: is OK\nActual: oh no! [INTERNAL]\nmy "
        "precious assertion failed");
}

TEST(ExpectOkTest, ExpectOk)
{
    Status status;
    EXPECT_STATUS_OK(status);
}

TEST(ExpectOkTest, ExpectOkStatusOr)
{
    StatusOrVal<int> statusOrVal(42);
    EXPECT_STATUS_OK(statusOrVal);
}

TEST(ExpectOkTest, ExpectionOkDescription)
{
    Status status;
    EXPECT_STATUS_OK(status) << "OK is not OK?";
}

TEST(ExpectOkTest, ExpectionOkDescriptionStatusOr)
{
    StatusOrVal<int> statusOrVal(42);
    EXPECT_STATUS_OK(statusOrVal) << "OK is not OK?";
}

TEST(ExpectOkTest, ExpectionFailed)
{
    EXPECT_NONFATAL_FAILURE(
        {
            Status status(StatusCode::Internal, "oh no!");
            EXPECT_STATUS_OK(status);
        },
        "Value of: status\nExpected: is OK\nActual: oh no! [INTERNAL]");
}

TEST(ExpectOkTest, ExpectionFailedStatusOr)
{
    EXPECT_NONFATAL_FAILURE(
        {
            StatusOrVal<int> statusOrVal(Status(StatusCode::Internal, "oh no!"));
            EXPECT_STATUS_OK(statusOrVal);
        },
        "Value of: statusOrVal\nExpected: is OK\nActual: oh no! [INTERNAL]");
}

TEST(ExpectOkTest, ExpectionFailedDescription)
{
    EXPECT_NONFATAL_FAILURE(
        {
            Status status(StatusCode::Internal, "oh no!");
            EXPECT_STATUS_OK(status) << "my precious assertion failed";
        },
        "Value of: status\nExpected: is OK\nActual: oh no! [INTERNAL]\nmy "
        "precious assertion failed");
}

TEST(ExpectOkTest, ExpectionFailedDescriptionStatusOr)
{
    EXPECT_NONFATAL_FAILURE(
        {
            StatusOrVal<int> statusOrVal(Status(StatusCode::Internal, "oh no!"));
            EXPECT_STATUS_OK(statusOrVal) << "my precious assertion failed";
        },
        "Value of: statusOrVal\nExpected: is OK\nActual: oh no! [INTERNAL]\nmy "
        "precious assertion failed");
}
