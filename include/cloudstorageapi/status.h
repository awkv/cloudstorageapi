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

#pragma once

#include <iostream>
#include <stdexcept>
#include <tuple>

namespace csa {
/**
 * Well-known status codes.
 *
 */
enum class StatusCode
{
    // Not an error; returned on success.
    Ok = 0,

    Cancelled = 1,
    Unknown = 2,
    InvalidArgument = 3,
    DeadlineExceeded = 4,
    NotFound = 5,
    AlreadyExists = 6,
    PermissionDenied = 7,
    Unauthenticated = 16,
    ResourceExhausted = 8,
    FailedPrecondition = 9,
    Aborted = 10,
    OutOfRange = 11,
    Unimplemented = 12,
    Internal = 13,
    Unavailable = 14,
    DataLoss = 15,
};

std::string StatusCodeToString(StatusCode code);
std::ostream& operator<<(std::ostream& os, StatusCode code);

/**
 * Reports error code and details from a remote request.
 *
 * It contains the status code and error message (if applicable) from a JSON request.
 */
class Status 
{
public:
    Status() = default;

    explicit Status(StatusCode statusCode, std::string message)
        : m_code(statusCode), m_message(std::move(message)) {}

    bool Ok() const { return m_code == StatusCode::Ok; }

    StatusCode Code() const { return m_code; }
    std::string const& Message() const { return m_message; }

private:
    StatusCode m_code = StatusCode::Ok;
    std::string m_message;
};

inline std::ostream& operator<<(std::ostream& os, Status const& rhs) {
  return os << rhs.Message() << " [" << StatusCodeToString(rhs.Code()) << "]";
}

inline bool operator==(Status const& lhs, Status const& rhs) {
  return lhs.Code() == rhs.Code() && lhs.Message() == rhs.Message();
}

inline bool operator!=(Status const& lhs, Status const& rhs) {
  return !(lhs == rhs);
}

class RuntimeStatusError : public std::runtime_error
{
 public:
    explicit RuntimeStatusError(Status status);

    Status const& GetStatus() const { return m_status; }

private:
    Status m_status;
};

}  // namespace csa
