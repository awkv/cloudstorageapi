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

#include "cloudstorageapi/internal/http_response.h"
#include <iostream>

namespace csa {
namespace internal {

Status AsStatus(HttpResponse const& http_response)
{
    // The code here is organized by increasing range (or value) of the errors,
    // just to make it readable. There are probably shorter (and/or more
    // efficient) ways to write this, but we went for readability.

    if (http_response.m_statusCode < 100)
    {
        return Status(StatusCode::Unknown, http_response.m_payload);
    }
    if (http_response.m_statusCode < 200)
    {
        // We treat the 100s (e.g. 100 Continue) as OK results. They normally are
        // ignored by libcurl, so we do not really expect to see them.
        return Status(StatusCode::Ok, std::string{});
    }
    if (http_response.m_statusCode < 300)
    {
        // We treat the 200s as Okay results.
        return Status(StatusCode::Ok, std::string{});
    }
    if (http_response.m_statusCode == 308)
    {
        // 308 - Resume Incomplete: this one is terrible. When performing a PUT
        // for a resumable upload this means "The client and server are out of sync
        // in this resumable upload, please reset". Unfortunately, during a
        // "reset" this means "The reset worked, here is the next committed byte,
        // keep in mind that the server is still doing work".  The second is more
        // like a kOk, the first is more like a kFailedPrecondition.
        // This level of complexity / detail is something that the caller should
        // handle, i.e., the mapping depends on the operation.
        return Status(StatusCode::FailedPrecondition, http_response.m_payload);
    }
    if (http_response.m_statusCode < 400)
    {
        // The 300s should be handled by libcurl, we should not get them.
        return Status(StatusCode::Unknown, http_response.m_payload);
    }
    if (http_response.m_statusCode == 400)
    {
        // 400 - Bad Request
        return Status(StatusCode::InvalidArgument, http_response.m_payload);
    }
    if (http_response.m_statusCode == 401)
    {
        // 401 - Unauthorized
        return Status(StatusCode::Unauthenticated, http_response.m_payload);
    }
    if (http_response.m_statusCode == 403)
    {
        // 403 - Forbidden
        return Status(StatusCode::PermissionDenied, http_response.m_payload);
    }
    if (http_response.m_statusCode == 404)
    {
        // 404 - Not Found
        return Status(StatusCode::NotFound, http_response.m_payload);
    }
    if (http_response.m_statusCode == 405)
    {
        // 405 - Method Not Allowed
        return Status(StatusCode::PermissionDenied, http_response.m_payload);
    }
    if (http_response.m_statusCode == 409)
    {
        // 409 - Conflict
        return Status(StatusCode::Aborted, http_response.m_payload);
    }
    if (http_response.m_statusCode == 410)
    {
        // 410 - Gone
        return Status(StatusCode::NotFound, http_response.m_payload);
    }
    if (http_response.m_statusCode == 411)
    {
        // 411 - Length Required
        return Status(StatusCode::InvalidArgument, http_response.m_payload);
    }
    if (http_response.m_statusCode == 412)
    {
        // 412 - Precondition Failed
        return Status(StatusCode::FailedPrecondition, http_response.m_payload);
    }
    if (http_response.m_statusCode == 413)
    {
        // 413 - Payload Too Large
        return Status(StatusCode::OutOfRange, http_response.m_payload);
    }
    if (http_response.m_statusCode == 416)
    {
        // 416 - Request Range Not Satisfiable
        return Status(StatusCode::OutOfRange, http_response.m_payload);
    }
    if (http_response.m_statusCode == 429)
    {
        // 429 - Too Many Requests
        return Status(StatusCode::Unavailable, http_response.m_payload);
    }
    if (http_response.m_statusCode < 500)
    {
        // 4XX - A request error.
        return Status(StatusCode::InvalidArgument, http_response.m_payload);
    }
    if (http_response.m_statusCode == 500)
    {
        // 500 - Internal Server Error
        return Status(StatusCode::Unavailable, http_response.m_payload);
    }
    if (http_response.m_statusCode == 502)
    {
        // 502 - Bad Gateway
        return Status(StatusCode::Unavailable, http_response.m_payload);
    }
    if (http_response.m_statusCode == 503)
    {
        // 503 - Service Unavailable
        return Status(StatusCode::Unavailable, http_response.m_payload);
    }
    if (http_response.m_statusCode < 600)
    {
        // 5XX - server errors are mapped to kInternal.
        return Status(StatusCode::Internal, http_response.m_payload);
    }
    return Status(StatusCode::Unknown, http_response.m_payload);
}

std::ostream& operator<<(std::ostream& os, HttpResponse const& rhs)
{
    os << "m_statusCode=" << rhs.m_statusCode << ", {";
    char const* sep = "";
    for (auto const& kv : rhs.m_headers)
    {
        os << sep << kv.first << ": " << kv.second;
        sep = ", ";
    }
    return os << "}, payload=<" << rhs.m_payload << ">";
}

}  // namespace internal
}  // namespace csa
