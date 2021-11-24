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

    if (http_response.m_statusCode < HttpStatusCode::MinContinue)
    {
        return Status(StatusCode::Unknown, http_response.m_payload);
    }
    if (HttpStatusCode::MinContinue <= http_response.m_statusCode &&
        http_response.m_statusCode < HttpStatusCode::MinSuccess)
    {
        // We treat the 100s (e.g. 100 Continue) as OK results. They normally are
        // ignored by libcurl, so we do not really expect to see them.
        return Status(StatusCode::Ok, std::string{});
    }
    if (HttpStatusCode::MinSuccess <= http_response.m_statusCode &&
        http_response.m_statusCode < HttpStatusCode::MinRedirects)
    {
        // We treat the 200s as Okay results.
        return Status(StatusCode::Ok, std::string{});
    }
    if (http_response.m_statusCode == HttpStatusCode::ResumeIncomplete)
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
    if (http_response.m_statusCode == HttpStatusCode::NotModified)
    {
        // 304 - Not Modified: evidently some storages (googledirve) return 304 for some failed
        // pre-conditions. It is somewhat strange that it also returns this error
        // code for downloads, which is always read-only and was not going to modify
        // anything. In any case, it seems too confusing to return anything other
        // than FailedPrecondition here.
        return Status(StatusCode::FailedPrecondition, http_response.m_payload);
    }
    if (HttpStatusCode::MinRedirects <= http_response.m_statusCode &&
        http_response.m_statusCode < HttpStatusCode::MinRequestErrors)
    {
        // The 300s should be handled by libcurl, we should not get them.
        // E.g. according to the Google Cloud Storage documentation these are:
        // 302 - Found
        // 303 - See Other
        // 307 - Temporary Redirect
        return Status(StatusCode::Unknown, http_response.m_payload);
    }
    if (http_response.m_statusCode == HttpStatusCode::BadRequest)
    {
        // 400 - Bad Request
        return Status(StatusCode::InvalidArgument, http_response.m_payload);
    }
    if (http_response.m_statusCode == HttpStatusCode::Unauthorized)
    {
        // 401 - Unauthorized
        return Status(StatusCode::Unauthenticated, http_response.m_payload);
    }
    if (http_response.m_statusCode == HttpStatusCode::Forbidden)
    {
        // 403 - Forbidden
        return Status(StatusCode::PermissionDenied, http_response.m_payload);
    }
    if (http_response.m_statusCode == HttpStatusCode::NotFound)
    {
        // 404 - Not Found
        return Status(StatusCode::NotFound, http_response.m_payload);
    }
    if (http_response.m_statusCode == HttpStatusCode::MethodNotAllowed)
    {
        // 405 - Method Not Allowed
        return Status(StatusCode::PermissionDenied, http_response.m_payload);
    }
    if (http_response.m_statusCode == HttpStatusCode::RequestTimeout)
    {
        // Some storages (google) uses a 408 to signal that an upload has suffered a broken
        // connection, and that the client should retry.
        return Status(StatusCode::Unavailable, http_response.m_payload);
    }
    if (http_response.m_statusCode == HttpStatusCode::Conflict)
    {
        // 409 - Conflict
        return Status(StatusCode::Aborted, http_response.m_payload);
    }
    if (http_response.m_statusCode == HttpStatusCode::Gone)
    {
        // 410 - Gone
        return Status(StatusCode::NotFound, http_response.m_payload);
    }
    if (http_response.m_statusCode == HttpStatusCode::LengthRequired)
    {
        // 411 - Length Required
        return Status(StatusCode::InvalidArgument, http_response.m_payload);
    }
    if (http_response.m_statusCode == HttpStatusCode::PreconditionFailed)
    {
        // 412 - Precondition Failed
        return Status(StatusCode::FailedPrecondition, http_response.m_payload);
    }
    if (http_response.m_statusCode == HttpStatusCode::PayloadTooLarge)
    {
        // 413 - Payload Too Large
        return Status(StatusCode::OutOfRange, http_response.m_payload);
    }
    if (http_response.m_statusCode == HttpStatusCode::RequestRangeNotSatisfiable)
    {
        // 416 - Request Range Not Satisfiable
        return Status(StatusCode::OutOfRange, http_response.m_payload);
    }
    if (http_response.m_statusCode == HttpStatusCode::TooManyRequests)
    {
        // 429 - Too Many Requests
        return Status(StatusCode::Unavailable, http_response.m_payload);
    }
    if (HttpStatusCode::MinRequestErrors <= http_response.m_statusCode &&
        http_response.m_statusCode < HttpStatusCode::MinInternalErrors)
    {
        // 4XX - A request error.
        return Status(StatusCode::InvalidArgument, http_response.m_payload);
    }
    if (http_response.m_statusCode == HttpStatusCode::InternalServerError)
    {
        // 500 - Internal Server Error
        return Status(StatusCode::Unavailable, http_response.m_payload);
    }
    if (http_response.m_statusCode == HttpStatusCode::BadGateway)
    {
        // 502 - Bad Gateway
        return Status(StatusCode::Unavailable, http_response.m_payload);
    }
    if (http_response.m_statusCode == HttpStatusCode::ServiceUnavailable)
    {
        // 503 - Service Unavailable
        return Status(StatusCode::Unavailable, http_response.m_payload);
    }
    if (HttpStatusCode::MinInternalErrors <= http_response.m_statusCode &&
        http_response.m_statusCode < HttpStatusCode::MinInvalidCode)
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
