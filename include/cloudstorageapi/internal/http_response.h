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

#include "cloudstorageapi/status.h"
#include <iosfwd>
#include <map>
#include <string>

namespace csa {
namespace internal {

enum HttpStatusCode
{
    MinContinue = 100,
    MinSuccess = 200,
    MinRedirects = 300,
    MinRequestErrors = 400,
    MinInternalErrors = 500,
    MinInvalidCode = 600,

    Continue = 100,

    Ok = 200,
    Created = 201,

    // Google's resumable upload protocol abuses 308 (Permanent Redirect) as
    // "Resume Incomplete".
    ResumeIncomplete = 308,

    // The libcurl library handles (most) redirects, so anything above 300 is
    // actually an error.
    MinNotSuccess = 300,
    // This is returned in some download requests instead of 412.
    NotModified = 304,

    BadRequest = 400,
    Unauthorized = 401,
    Forbidden = 403,
    NotFound = 404,
    MethodNotAllowed = 405,
    RequestTimeout = 408,
    Conflict = 409,
    Gone = 410,
    LengthRequired = 411,
    PreconditionFailed = 412,
    PayloadTooLarge = 413,
    RequestRangeNotSatisfiable = 416,
    TooManyRequests = 429,

    InternalServerError = 500,
    BadGateway = 502,
    ServiceUnavailable = 503,
};

/**
 * Contains the results of a HTTP request.
 */
struct HttpResponse
{
    long m_statusCode;
    std::string m_payload;
    std::multimap<std::string, std::string> m_headers;
};

/**
 * Maps a HTTP response to a `Status`.
 *
 * HTTP responses have a wide range of status codes (100 to 599), and we have
 * a much more limited number of `StatusCode` values. This function performs
 * the mapping between the two.
 *
 * The general principles in this mapping are:
 * - A "code" outside the valid code for HTTP (from 100 to 599 both
 *   inclusive) is always kUnknown.
 * - Codes that are not specifically documented in
 *     https://cloud.google.com/storage/docs/json_api/v1/status-codes
 *   are mapped by these rules:
 *     [100,300) -> Ok because they are all success status codes.
 *     [300,400) -> Unknown because libcurl should handle the redirects, so
 *                  getting one is fairly strange.
 *     [400,500) -> InvalidArgument because these are generally "the client
 *                  sent an invalid request" errors.
 *     [500,600) -> Internal because these are "server errors".
 *
 * @return A status with the code corresponding to @p http_response.status_code,
 *     the error message in the status is initialized with
 *     @p http_response.payload.
 */
Status AsStatus(HttpResponse const& http_response);

std::ostream& operator<<(std::ostream& os, HttpResponse const& rhs);

}  // namespace internal
}  // namespace csa
