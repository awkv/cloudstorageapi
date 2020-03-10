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

#include "cloudstorageapi/internal/file_streambuf.h"
#include "cloudstorageapi/internal/utils.h"
#include "cloudstorageapi/internal/log.h"
#include <algorithm>
#include <sstream>

namespace csa {
namespace internal {
FileWriteStreambuf::FileWriteStreambuf(
    std::unique_ptr<ResumableUploadSession> uploadSession,
    std::size_t maxBufferSize)
    : m_uploadSession(std::move(uploadSession)),
    m_maxBufferSize(RoundUpToQuantum(maxBufferSize, m_uploadSession->GetFileChunkSizeQuantum())),
    m_lastResponse(ResumableUploadResponse{
        {}, 0, {}, ResumableUploadResponse::UploadState::InProgress, {} })
{
    m_currentIosBuffer.resize(m_maxBufferSize);
    auto pbeg = m_currentIosBuffer.data();
    auto pend = pbeg + m_currentIosBuffer.size();
    setp(pbeg, pend);
    // Sessions start in a closed state for uploads that have already been
    // finalized.
    if (m_uploadSession->Done())
    {
        m_lastResponse = m_uploadSession->GetLastResponse();
    }
}

StatusOrVal<ResumableUploadResponse> FileWriteStreambuf::Close()
{
    pubsync();
    CSA_LOG_INFO("{}()", __func__);
    return FlushFinal();
}

bool FileWriteStreambuf::IsOpen() const
{
    return static_cast<bool>(m_uploadSession) && !m_uploadSession->Done();
}

int FileWriteStreambuf::sync()
{
    auto result = Flush();
    if (!result.Ok())
    {
        return traits_type::eof();
    }
    return 0;
}

std::streamsize FileWriteStreambuf::xsputn(char const* s,
    std::streamsize count)
{
    if (!IsOpen())
    {
        return traits_type::eof();
    }

    std::streamsize bytesCopied = 0;
    while (bytesCopied != count)
    {
        std::streamsize remainingBufferSize = epptr() - pptr();
        std::streamsize bytesToCopy =
            std::min(count - bytesCopied, remainingBufferSize);
        std::copy(s, s + bytesToCopy, pptr());
        pbump(static_cast<int>(bytesToCopy));
        bytesCopied += bytesToCopy;
        s += bytesToCopy;
        m_lastResponse = Flush();
        // Upload failures are irrecoverable because the internal buffer is opaque
        // to the caller, so there is no way to know what byte range to specify
        // next. Replace it with a SessionError so next_expected_byte and
        // resumable_session_id can still be retrieved.
        if (!m_lastResponse.Ok())
        {
            m_uploadSession = std::unique_ptr<ResumableUploadSession>(
                new ResumableUploadSessionError(m_lastResponse.GetStatus(),
                    GetNextExpectedByte(),
                    GetResumableSessionId()));
            return traits_type::eof();
        }
    }
    return count;
}

FileWriteStreambuf::int_type FileWriteStreambuf::overflow(int_type ch)
{
    if (!IsOpen())
    {
        return traits_type::eof();
    }
    if (traits_type::eq_int_type(ch, traits_type::eof()))
    {
        // For ch == EOF this function must do nothing and return any value != EOF.
        return 0;
    }
    // If the buffer is full flush it immediately.
    auto result = Flush();
    if (!result.Ok())
    {
        return traits_type::eof();
    }
    // Make sure there is now room in the buffer for the char.
    if (pptr() == epptr())
    {
        return traits_type::eof();
    }
    // Push the character into the current buffer.
    *pptr() = traits_type::to_char_type(ch);
    pbump(1);
    return ch;
}

StatusOrVal<ResumableUploadResponse> FileWriteStreambuf::FlushFinal()
{
    if (!IsOpen())
    {
        return m_lastResponse;
    }
    // Shorten the buffer to the actual used size.
    auto actualSize = static_cast<std::size_t>(pptr() - pbase());
    std::size_t uploadSize = m_uploadSession->GetNextExpectedByte() + actualSize;

    std::string toUpload(pbase(), actualSize);
    m_lastResponse = m_uploadSession->UploadFinalChunk(toUpload, uploadSize);
    if (!m_lastResponse)
    {
        // This was an unrecoverable error, time to store status and signal an
        // error.
        return m_lastResponse;
    }
    // Reset the iostream put area with valid pointers, but empty.
    m_currentIosBuffer.resize(1);
    auto pbeg = m_currentIosBuffer.data();
    setp(pbeg, pbeg);

    m_uploadSession.reset();

    return m_lastResponse;
}

StatusOrVal<ResumableUploadResponse> FileWriteStreambuf::Flush()
{
    if (!IsOpen())
    {
        return m_lastResponse;
    }

    auto actualSize = static_cast<std::size_t>(pptr() - pbase());
    if (actualSize < m_maxBufferSize)
    {
        return m_lastResponse;
    }

    auto chunkCount = actualSize / m_uploadSession->GetFileChunkSizeQuantum();
    auto chunkSize = chunkCount * m_uploadSession->GetFileChunkSizeQuantum();
    // CSA upload returns an updated range header that sets the next expected
    // byte. Check to make sure it remains consistent with the bytes stored in the
    // buffer.
    auto expectedNextByte = m_uploadSession->GetNextExpectedByte() + chunkSize;

    StatusOrVal<ResumableUploadResponse> result;
    std::string toSend(pbase(), chunkSize);
    m_lastResponse = m_uploadSession->UploadChunk(toSend);
    if (!m_lastResponse)
    {
        return m_lastResponse;
    }
    auto actualNextByte = m_uploadSession->GetNextExpectedByte();
    auto bytesUploaded = static_cast<int64_t>(chunkSize);
    if (actualNextByte < expectedNextByte)
    {
        bytesUploaded -= expectedNextByte - actualNextByte;
        if (bytesUploaded < 0)
        {
            std::ostringstream errorMessage;
            errorMessage << "Could not continue upload stream. CSA requested byte "
                << actualNextByte << " which has already been uploaded.";
            return Status(StatusCode::Aborted, errorMessage.str());
        }
    }
    else if (actualNextByte > expectedNextByte)
    {
        std::ostringstream errorMessage;
        errorMessage << "Could not continue upload stream. "
            << "CSA requested unexpected byte. (expected: "
            << expectedNextByte << ", actual: " << actualNextByte
            << ")";
        return Status(StatusCode::Aborted, errorMessage.str());
    }

    std::copy(pbase() + bytesUploaded, epptr(), pbase());
    setp(pbase(), epptr());
    pbump(static_cast<int>(actualSize - bytesUploaded));
    return m_lastResponse;
}

} // namespace internal
} // namespace csa
