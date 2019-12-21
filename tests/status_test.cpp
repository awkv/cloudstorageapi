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

#include "cloudstorageapi/status_or_val.h"
#include <gmock/gmock.h>

namespace csa {
namespace {
using ::testing::HasSubstr;

TEST(Status, StatusCodeToString) {
  EXPECT_EQ("OK", StatusCodeToString(StatusCode::Ok));
  EXPECT_EQ("CANCELLED", StatusCodeToString(StatusCode::Cancelled));
  EXPECT_EQ("UNKNOWN", StatusCodeToString(StatusCode::Unknown));
  EXPECT_EQ("INVALID_ARGUMENT",
            StatusCodeToString(StatusCode::InvalidArgument));
  EXPECT_EQ("DEADLINE_EXCEEDED",
            StatusCodeToString(StatusCode::DeadlineExceeded));
  EXPECT_EQ("NOT_FOUND", StatusCodeToString(StatusCode::NotFound));
  EXPECT_EQ("ALREADY_EXISTS", StatusCodeToString(StatusCode::AlreadyExists));
  EXPECT_EQ("PERMISSION_DENIED",
            StatusCodeToString(StatusCode::PermissionDenied));
  EXPECT_EQ("UNAUTHENTICATED",
            StatusCodeToString(StatusCode::Unauthenticated));
  EXPECT_EQ("RESOURCE_EXHAUSTED",
            StatusCodeToString(StatusCode::ResourceExhausted));
  EXPECT_EQ("FAILED_PRECONDITION",
            StatusCodeToString(StatusCode::FailedPrecondition));
  EXPECT_EQ("ABORTED", StatusCodeToString(StatusCode::Aborted));
  EXPECT_EQ("OUT_OF_RANGE", StatusCodeToString(StatusCode::OutOfRange));
  EXPECT_EQ("UNIMPLEMENTED", StatusCodeToString(StatusCode::Unimplemented));
  EXPECT_EQ("INTERNAL", StatusCodeToString(StatusCode::Internal));
  EXPECT_EQ("UNAVAILABLE", StatusCodeToString(StatusCode::Unavailable));
  EXPECT_EQ("DATA_LOSS", StatusCodeToString(StatusCode::DataLoss));
  EXPECT_EQ("UNEXPECTED_STATUS_CODE=42",
            StatusCodeToString(static_cast<StatusCode>(42)));
}

}  // namespace
}  // namespace csa
