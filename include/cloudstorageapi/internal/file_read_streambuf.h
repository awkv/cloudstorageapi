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

#pragma once

#include "cloudstorageapi/internal/http_response.h"
#include "cloudstorageapi/internal/object_read_source.h"
#include "cloudstorageapi/internal/resumable_upload_session.h"
#include "cloudstorageapi/status_or_val.h"
#include <iostream>
#include <map>
#include <memory>
#include <vector>

namespace csa {
class FileMetadata;

namespace internal {

class ReadFileRangeRequest;

/**
 * Defines a compilation barrier for libcurl.
 *
 * We do not want to expose the libcurl objects through `FileReadStream`,
 * this class abstracts away the implementation so applications are not impacted
 * by the implementation details.
 */
class FileReadStreambuf : public std::basic_streambuf<char>
{
public:
    FileReadStreambuf(ReadFileRangeRequest const& request, std::unique_ptr<ObjectReadSource> source,
                      std::streamoff pos_in_stream);

    /// Create a streambuf in a permanent error status.
    FileReadStreambuf(ReadFileRangeRequest const& request, Status status);

    ~FileReadStreambuf() override = default;

    FileReadStreambuf(FileReadStreambuf&&) noexcept = delete;
    FileReadStreambuf& operator=(FileReadStreambuf&&) noexcept = delete;
    FileReadStreambuf(FileReadStreambuf const&) = delete;
    FileReadStreambuf& operator=(FileReadStreambuf const&) = delete;

    pos_type seekpos(pos_type pos, std::ios_base::openmode which) override;
    pos_type seekoff(off_type off, std::ios_base::seekdir dir, std::ios_base::openmode which) override;

    bool IsOpen() const;
    void Close();

    Status const& GetStatus() const { return m_status; }
    std::multimap<std::string, std::string> const& GetHeaders() const { return m_headers; }

private:
    int_type ReportError(Status status);
    bool CheckPreconditions(char const* function_name);

    int_type underflow() override;
    std::streamsize xsgetn(char* s, std::streamsize count) override;

    std::unique_ptr<ObjectReadSource> m_source;
    std::streamoff m_sourcePos;
    std::vector<char> m_currentIosBuffer;
    Status m_status;
    std::multimap<std::string, std::string> m_headers;
};

}  // namespace internal
}  // namespace csa
