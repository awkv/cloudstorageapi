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

#include "cloudstorageapi/auto_finalize.h"
#include "cloudstorageapi/internal/const_buffer.h"
#include "cloudstorageapi/internal/resumable_upload_session.h"
#include "cloudstorageapi/status_or_val.h"
#include <iostream>
#include <memory>

namespace csa {
class FileWriteStream;

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

    FileWriteStreambuf(std::unique_ptr<ResumableUploadSession> uploadSession, std::size_t maxBufferSize,
                       AutoFinalizeConfig autoFinalize);

    ~FileWriteStreambuf() override = default;

    FileWriteStreambuf(FileWriteStreambuf&& rhs) noexcept = delete;
    FileWriteStreambuf& operator=(FileWriteStreambuf&& rhs) noexcept = delete;
    FileWriteStreambuf(FileWriteStreambuf const&) = delete;
    FileWriteStreambuf& operator=(FileWriteStreambuf const&) = delete;

    virtual StatusOrVal<ResumableUploadResponse> Close();
    virtual bool IsOpen() const;

    /// The session id, if applicable, it is empty for non-resumable uploads.
    virtual std::string const& GetResumableSessionId() const { return m_uploadSession->GetSessionId(); }

    /// The next expected byte, if applicable, always 0 for non-resumable uploads.
    virtual std::uint64_t GetNextExpectedByte() const { return m_uploadSession->GetNextExpectedByte(); }

    virtual Status GetLastStatus() const { return m_lastResponse.GetStatus(); }

protected:
    int sync() override;
    std::streamsize xsputn(char const* s, std::streamsize count) override;
    int_type overflow(int_type ch) override;

private:
    friend class csa::FileWriteStream;

    /**
     * Automatically finalize the upload unless configured to not do so.
     *
     * Called by the FileWriteStream destructor, some applications prefer to
     * explicitly finalize an upload. For example, they may start an upload,
     * checkpoint the upload id, then upload in chunks and may *not* want to
     * finalize the upload in the presence of exceptions that destroy any
     * FileWriteStream.
     */
    void AutoFlushFinal();

    /// Flush any data if possible.
    void Flush();

    /// Flush any remaining data and finalize the upload.
    void FlushFinal();

    /// Upload a round chunk
    void FlushRoundChunk(ConstBufferSequence buffers);

    /// The current used bytes in the put area (aka m_currentIosBuffer)
    std::size_t PutAreaSize() const { return pptr() - pbase(); }

    std::unique_ptr<ResumableUploadSession> m_uploadSession;
    std::vector<char> m_currentIosBuffer;
    std::size_t m_maxBufferSize;
    AutoFinalizeConfig m_autoFinalize = AutoFinalizeConfig::Disabled;
    StatusOrVal<ResumableUploadResponse> m_lastResponse;
};

}  // namespace internal
}  // namespace csa
