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

#include "cloudstorageapi/status.h"
#include <sstream>

namespace csa {
namespace {
std::string StatusWhat(Status const& status)
{
  std::ostringstream os;
  os << status;
  return std::move(os).str();
}
}  // namespace

std::string StatusCodeToString(StatusCode code)
{
  switch (code)
  {
    case StatusCode::Ok:
      return "OK";
    case StatusCode::Cancelled:
      return "CANCELLED";
    case StatusCode::Unknown:
      return "UNKNOWN";
    case StatusCode::InvalidArgument:
      return "INVALID_ARGUMENT";
    case StatusCode::DeadlineExceeded:
      return "DEADLINE_EXCEEDED";
    case StatusCode::NotFound:
      return "NOT_FOUND";
    case StatusCode::AlreadyExists:
      return "ALREADY_EXISTS";
    case StatusCode::PermissionDenied:
      return "PERMISSION_DENIED";
    case StatusCode::Unauthenticated:
      return "UNAUTHENTICATED";
    case StatusCode::ResourceExhausted:
      return "RESOURCE_EXHAUSTED";
    case StatusCode::FailedPrecondition:
      return "FAILED_PRECONDITION";
    case StatusCode::Aborted:
      return "ABORTED";
    case StatusCode::OutOfRange:
      return "OUT_OF_RANGE";
    case StatusCode::Unimplemented:
      return "UNIMPLEMENTED";
    case StatusCode::Internal:
      return "INTERNAL";
    case StatusCode::Unavailable:
      return "UNAVAILABLE";
    case StatusCode::DataLoss:
      return "DATA_LOSS";
    default:
      return "UNEXPECTED_STATUS_CODE=" + std::to_string(static_cast<int>(code));
  }
}

std::ostream& operator<<(std::ostream& os, StatusCode code)
{
  return os << StatusCodeToString(code);
}

RuntimeStatusError::RuntimeStatusError(Status status)
    : std::runtime_error(StatusWhat(status)), m_status(std::move(status))
{
}

}  // namespace csa
