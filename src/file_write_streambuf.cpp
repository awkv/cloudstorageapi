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

#include "cloudstorageapi/internal/file_write_streambuf.h"
#include "cloudstorageapi/internal/http_response.h"
#include "cloudstorageapi/internal/object_read_source.h"
#include "cloudstorageapi/internal/utils.h"

namespace csa {
namespace internal {

FileWriteStreambuf::FileWriteStreambuf(std::unique_ptr<ResumableUploadSession> uploadSession, std::size_t maxBufferSize,
                                       AutoFinalizeConfig autoFinalize)
    : m_uploadSession(std::move(uploadSession)),
      m_maxBufferSize(RoundUpToQuantum(maxBufferSize, m_uploadSession->GetFileChunkSizeQuantum())),
      m_autoFinalize(autoFinalize),
      m_lastResponse(ResumableUploadResponse{{}, 0, {}, ResumableUploadResponse::UploadState::InProgress, {}})
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

void FileWriteStreambuf::AutoFlushFinal()
{
    if (m_autoFinalize != AutoFinalizeConfig::Enabled)
        return;
    Close();
}

StatusOrVal<ResumableUploadResponse> FileWriteStreambuf::Close()
{
    FlushFinal();
    return m_lastResponse;
}

bool FileWriteStreambuf::IsOpen() const { return static_cast<bool>(m_uploadSession) && !m_uploadSession->Done(); }

int FileWriteStreambuf::sync()
{
    Flush();
    return !m_lastResponse ? traits_type::eof() : 0;
}

std::streamsize FileWriteStreambuf::xsputn(char const* s, std::streamsize count)
{
    if (!IsOpen())
    {
        return traits_type::eof();
    }

    auto const actualSize = PutAreaSize();
    if (count + actualSize >= m_maxBufferSize)
    {
        if (actualSize == 0)
        {
            FlushRoundChunk({ConstBuffer(s, static_cast<std::size_t>(count))});
        }
        else
        {
            FlushRoundChunk({
                ConstBuffer(pbase(), actualSize),
                ConstBuffer(s, static_cast<std::size_t>(count)),
            });
        }
        if (!m_lastResponse)
            return traits_type::eof();
    }
    else
    {
        std::copy(s, s + count, pptr());
        pbump(static_cast<int>(count));
    }
    return count;
}

FileWriteStreambuf::int_type FileWriteStreambuf::overflow(int_type ch)
{
    // For ch == EOF this function must do nothing and return any value != EOF.
    if (traits_type::eq_int_type(ch, traits_type::eof()))
        return 0;

    if (!IsOpen())
        return traits_type::eof();

    auto actualSize = PutAreaSize();
    if (actualSize >= m_maxBufferSize)
        Flush();
    *pptr() = traits_type::to_char_type(ch);
    pbump(1);
    return m_lastResponse ? ch : traits_type::eof();
}

void FileWriteStreambuf::FlushFinal()
{
    if (!IsOpen())
        return;

    // Calculate the portion of the buffer that needs to be uploaded, if any.
    auto const actualSize = PutAreaSize();
    auto const uploadSize = m_uploadSession->GetNextExpectedByte() + actualSize;

    // After this point the session will be closed, and no more calls to the hash
    // function are possible.
    m_lastResponse = m_uploadSession->UploadFinalChunk({ConstBuffer(pbase(), actualSize)}, uploadSize);

    // Reset the iostream put area with valid pointers, but empty.
    m_currentIosBuffer.resize(1);
    auto* pbeg = m_currentIosBuffer.data();
    setp(pbeg, pbeg);

    // Close the stream
    m_uploadSession.reset();
}

void FileWriteStreambuf::Flush()
{
    if (!IsOpen())
        return;
    auto actualSize = PutAreaSize();
    if (actualSize < m_uploadSession->GetFileChunkSizeQuantum())
        return;

    ConstBufferSequence payload{ConstBuffer(pbase(), actualSize)};
    FlushRoundChunk(payload);
}

void FileWriteStreambuf::FlushRoundChunk(ConstBufferSequence buffers)
{
    auto actualSize = TotalBytes(buffers);
    auto chunkCount = actualSize / m_uploadSession->GetFileChunkSizeQuantum();
    auto roundedSize = chunkCount * m_uploadSession->GetFileChunkSizeQuantum();

    // Trim the buffers to the rounded chunk we will actually upload.
    auto payload = buffers;
    while (actualSize > roundedSize && !payload.empty())
    {
        const auto& lastSpan = payload.back();
        auto const n = (std::min)(actualSize - roundedSize, lastSpan.size());
        payload.back() = lastSpan.subspan(0, lastSpan.size() - n);
        actualSize -= n;
        if (payload.back().empty())
            payload.pop_back();
    }

    // CSA upload returns an updated range header that sets the next expected
    // byte. Check to make sure it remains consistent with the bytes stored in the
    // buffer.
    auto firstBufferedByte = m_uploadSession->GetNextExpectedByte();
    auto expectedNextByte = m_uploadSession->GetNextExpectedByte() + actualSize;
    m_lastResponse = m_uploadSession->UploadChunk(payload);

    if (m_lastResponse)
    {
        // Reset the internal buffer and copy any trailing bytes from `buffers` to
        // it.
        auto* pbeg = m_currentIosBuffer.data();
        setp(pbeg, pbeg + m_currentIosBuffer.size());
        PopFrontBytes(buffers, roundedSize);
        for (auto const& b : buffers)
        {
            std::copy(b.begin(), b.end(), pptr());
            pbump(static_cast<int>(b.size()));
        }

        // We cannot use the last committed byte in `m_lastResponse` because when
        // using X-Upload-Content-Length in e.g. gdrive returns 0 when the upload completed
        // even if no "final chunk" is sent.  The resumable upload classes know how
        // to deal with this mess, so let's not duplicate that code here.
        auto actualNextByte = m_uploadSession->GetNextExpectedByte();
        if (actualNextByte < expectedNextByte && actualNextByte < firstBufferedByte)
        {
            std::ostringstream errorMessage;
            errorMessage << "Could not continue upload stream. CSA requested byte " << actualNextByte
                         << " which has already been uploaded.";
            m_lastResponse = Status(StatusCode::Aborted, errorMessage.str());
        }
        else if (actualNextByte > expectedNextByte)
        {
            std::ostringstream errorMessage;
            errorMessage << "Could not continue upload stream. "
                         << "CSA requested unexpected byte. (expected: " << expectedNextByte
                         << ", actual: " << actualNextByte << ")";
            m_lastResponse = Status(StatusCode::Aborted, errorMessage.str());
        }
    }

    // Upload failures are irrecoverable because the internal buffer is opaque
    // to the caller, so there is no way to know what byte range to specify
    // next.  Replace it with a SessionError so nextExpectedByte and
    // resumable_session_id can still be retrieved.
    if (!m_lastResponse)
    {
        m_uploadSession = std::make_unique<ResumableUploadSessionError>(m_lastResponse.GetStatus(),
                                                                        GetNextExpectedByte(), GetResumableSessionId());
    }
}

}  // namespace internal
}  // namespace csa
