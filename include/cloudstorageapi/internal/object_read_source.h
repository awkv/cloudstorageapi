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

#include "cloudstorageapi/status_or_val.h"
#include "cloudstorageapi/internal/http_response.h"
#include <string>

namespace csa {
namespace internal {
/**
 * The result of reading some data from the source.
 *
 * Reading data may result in several outcomes:
 * - There was an error trying to read the data: we wrap this object in a
 *   StatusOr for this case.
 *
 * Other reads are considered successful, even if the "read" an HTTP error code.
 * Successful reads return:
 *
 * - How much of the data requested was read: returned in the `bytes_received`
 *   field.
 * - The HTTP error code for the full download. In-progress downloads have an
 *   `response.status_code == 100` (CONTINUE).
 * - At any point the call may return one or more headers, these headers are
 *   in `response.headers`.
 * - If the `response.status_code` was an error code (i.e >= 200) then the
 *   `response.payload` *may* contain additional error payload.
 */
struct ReadSourceResult
{
    std::size_t m_bytesReceived;
    HttpResponse m_response;
};

/**
 * A data source for ObjectReadStreambuf.
 *
 * This object represents an open download stream. It is an abstract class
 * because (a) we do not want to expose CURL types in the public headers, and
 * (b) we want to break the functionality for retry vs. simple downloads in
 * different classes.
 */
class ObjectReadSource
{
public:
    virtual ~ObjectReadSource() = default;

    virtual bool IsOpen() const = 0;

    /// Actively close a download, even if not all the data has been read.
    virtual StatusOrVal<HttpResponse> Close() = 0;

    /// Read more data from the download, returning any HTTP headers and error
    /// codes.
    virtual StatusOrVal<ReadSourceResult> Read(char* buf, std::size_t n) = 0;
};

/**
 * A ObjectReadErrorStreambufSource in a permanent error state.
 */
class ObjectReadErrorSource : public ObjectReadSource
{
public:
    explicit ObjectReadErrorSource(Status status) : m_status(std::move(status)) {}

    bool IsOpen() const override { return false; }
    StatusOrVal<HttpResponse> Close() override { return m_status; }
    StatusOrVal<ReadSourceResult> Read(char*, std::size_t) override
    {
        return m_status;
    }

private:
    Status m_status;
};

}  // namespace internal
}  // namespace csa
