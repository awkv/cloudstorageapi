// Copyright 2020 Andrew Karasyov
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

/**
 * Defines a compilation barrier for libcurl.
 *
 * We do not want to expose the libcurl objects through `FileWriteStream`,
 * this class abstracts away the implementation so applications are not impacted
 * by the implementation details.
 */
class FileWriteStreambuf : public std::basic_streambuf<char>
{
public:
    FileWriteStreambuf() = default;

    FileWriteStreambuf(std::unique_ptr<ResumableUploadSession> uploadSession,
        std::size_t maxBufferSize);

    ~FileWriteStreambuf() override = default;

    FileWriteStreambuf(FileWriteStreambuf&& rhs) noexcept = delete;
    FileWriteStreambuf& operator=(FileWriteStreambuf&& rhs) noexcept = delete;
    FileWriteStreambuf(FileWriteStreambuf const&) = delete;
    FileWriteStreambuf& operator=(FileWriteStreambuf const&) = delete;

    virtual StatusOrVal<ResumableUploadResponse> Close();
    virtual bool IsOpen() const;

    /// The session id, if applicable, it is empty for non-resumable uploads.
    virtual std::string const& GetResumableSessionId() const
    {
        return m_uploadSession->GetSessionId();
    }

    /// The next expected byte, if applicable, always 0 for non-resumable uploads.
    virtual std::uint64_t GetNextExpectedByte() const
    {
        return m_uploadSession->GetNextExpectedByte();
    }

    virtual Status GetLastStatus() const { return m_lastResponse.GetStatus(); }

protected:
    int sync() override;
    std::streamsize xsputn(char const* s, std::streamsize count) override;
    int_type overflow(int_type ch) override;

private:
    /// Flush any data if possible.
    StatusOrVal<ResumableUploadResponse> Flush();

    /// Flush any remaining data and commit the upload.
    StatusOrVal<ResumableUploadResponse> FlushFinal();

    std::unique_ptr<ResumableUploadSession> m_uploadSession;

    std::vector<char> m_currentIosBuffer;
    std::size_t m_maxBufferSize;

    StatusOrVal<ResumableUploadResponse> m_lastResponse;
};

}  // namespace internal
}  // namespace csa
