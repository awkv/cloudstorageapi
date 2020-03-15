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
#include "cloudstorageapi/internal/file_requests.h"
#include "cloudstorageapi/internal/utils.h"
#include "cloudstorageapi/internal/log.h"
#include <algorithm>
#include <sstream>

namespace csa {
namespace internal {

FileReadStreambuf::FileReadStreambuf(
    ReadFileRangeRequest const& request,
    std::unique_ptr<ObjectReadSource> source)
    : m_source(std::move(source))
{
}

FileReadStreambuf::FileReadStreambuf(ReadFileRangeRequest const& request,
    Status status)
    : m_source(new ObjectReadErrorSource(status))
{
    m_status = std::move(status);
}

bool FileReadStreambuf::IsOpen() const
{
    return m_source->IsOpen();
}

void FileReadStreambuf::Close()
{
    auto response = m_source->Close();
    if (!response.Ok())
    {
        ReportError(std::move(response).GetStatus());
    }
}

StatusOrVal<FileReadStreambuf::int_type> FileReadStreambuf::Peek()
{
    if (!IsOpen())
    {
        // The stream is closed, reading from a closed stream can happen if there is
        // no object to read from, or the object is empty. In that case just setup
        // an empty (but valid) region and verify the checksums.
        SetEmptyRegion();
        return traits_type::eof();
    }

    m_currentIosBuffer.resize(128 * 1024);
    std::size_t n = m_currentIosBuffer.size();
    StatusOrVal<ReadSourceResult> readResult =
        m_source->Read(m_currentIosBuffer.data(), n);
    if (!readResult.Ok())
    {
        return std::move(readResult).GetStatus();
    }
    // assert(n <= m_currentIosBuffer.size())
    m_currentIosBuffer.resize(readResult->m_bytesReceived);

    for (auto const& kv : readResult->m_response.m_headers)
    {
        m_headers.emplace(kv.first, kv.second);
    }
    if (readResult->m_response.m_statusCode >= 300)
    {
        return AsStatus(readResult->m_response);
    }

    if (!m_currentIosBuffer.empty())
    {
        char* data = m_currentIosBuffer.data();
        setg(data, data, data + m_currentIosBuffer.size());
        return traits_type::to_int_type(*data);
    }

    // This is an actual EOF, there is no more data to download, create an
    // empty (but valid) region:
    SetEmptyRegion();
    return traits_type::eof();
}

FileReadStreambuf::int_type FileReadStreambuf::underflow()
{
    auto nextChar = Peek();
    if (!nextChar) {
        return ReportError(nextChar.GetStatus());
    }

    return *nextChar;
}

std::streamsize FileReadStreambuf::xsgetn(char* s, std::streamsize count)
{
    CSA_LOG_INFO("{}(): count={}, in_avail={}, status={}", __func__, count,
         in_avail(), m_status);
    // This function optimizes stream.read(), the data is copied directly from the
    // data source (typically libcurl) into a buffer provided by the application.
    std::streamsize offset = 0;
    if (!m_status.Ok())
    {
        return 0;
    }

    // Maybe the internal get area is enough to satisfy this request, no need to
    // read more in that case:
    auto fromInternal = (std::min)(count, in_avail());
    std::memcpy(s, gptr(), static_cast<std::size_t>(fromInternal));
    gbump(static_cast<int>(fromInternal));
    offset += fromInternal;
    if (offset >= count)
    {
        CSA_LOG_INFO("{}(): count={}, in_avail={}, offset={}", __func__, count,
            in_avail(), offset);
        ReportError(Status());
        return offset;
    }

    StatusOrVal<ReadSourceResult> readResult =
        m_source->Read(s + offset, static_cast<std::size_t>(count - offset));
    // If there was an error set the internal state, but we still return the
    // number of bytes.
    if (!readResult)
    {
        CSA_LOG_INFO("{}(): count={}, in_avail={}, offset={}, status={}", __func__ , count,
            in_avail(), offset, readResult.GetStatus());
        ReportError(std::move(readResult).GetStatus());
        return offset;
    }
    CSA_LOG_INFO("{}(): count={}, in_avail={}, offset={}, readResult->bytes_received={}",
        __func__, count, in_avail(), offset, readResult->m_bytesReceived);

    offset += readResult->m_bytesReceived;

    for (auto const& kv : readResult->m_response.m_headers)
    {
        m_headers.emplace(kv.first, kv.second);
    }
    if (readResult->m_response.m_statusCode >= 300)
    {
        ReportError(AsStatus(readResult->m_response));
        return 0;
    }

    ReportError(Status());
    return offset;
}

FileReadStreambuf::int_type FileReadStreambuf::ReportError(Status status)
{
    // The only way to report errors from a std::basic_streambuf<> (which this
    // class derives from) is to throw exceptions:
    //   https://stackoverflow.com/questions/50716688/how-to-set-the-badbit-of-a-stream-by-a-customized-streambuf
    // but we need to be able to report errors when the application has disabled
    // exceptions via `-fno-exceptions` or a similar option. In that case we set
    // `m_status`, and report the error as an EOF. This is obviously not ideal,
    // but it is the best we can do when the application disables the standard
    // mechanism to signal errors.
    if (status.Ok())
    {
        return traits_type::eof();
    }
    m_status = std::move(status);

    // ? google::cloud::internal::ThrowStatus(m_status);

    return traits_type::eof();
}

void FileReadStreambuf::SetEmptyRegion()
{
    m_currentIosBuffer.clear();
    m_currentIosBuffer.push_back('\0');
    char* data = &m_currentIosBuffer[0];
    setg(data, data + 1, data + 1);
}

////////////////////////////////////////////////////////////////////////////////

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
