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

#include "cloudstorageapi/internal/file_write_streambuf.h"
#include <map>
#include <memory>
#include <ostream>
#include <string>

namespace csa {

/// Represents the headers returned in a streaming upload or download operation.
using HeadersMap = std::multimap<std::string, std::string>;

/**
 * Defines a `std::basic_ostream<char>` to write to a cloud file.
 *
 * This class is used to upload files to a cloud. It can handle files of any
 * size, but keep the following considerations in mind:
 *
 * * This API is designed for applications that need to stream the object
 *   payload. If you have the payload as one large buffer consider using
 *   `csa::Client::InsertFile()`, it is simpler and faster in most cases.
 * * This API can be used to perform unformatted I/O, as well as formatted I/O
 *   using the familiar `operator<<` APIs. Note that formatted I/O typically
 *   implies some form of buffering and data copying. For best performance,
 *   consider using the [.write()][cpp-reference-write] member function.
 * * A cloud expects to receive data in multiples of the *upload quantum* (e.g. grive 256KiB).
 *   Sending a buffer that is not a multiple of this quantum terminates the
 *   upload. This constraints the implementation of buffered and unbuffered I/O
 *   as described below.
 *
 * @par Unformatted I/O
 * On a `.write()` call this class attempts to send the data immediately, this
 * this the unbuffered API after all. If any previously buffered data and the
 * data provided in the `.write()` call are larger than an upload quantum the
 * class sends data immediately. Any data in excess of a multiple of the upload
 * quantum are buffered for the next upload.
 *
 * These examples may clarify how this works (for google drive):
 *   -# Consider a fresh `FileWriteStream` that receives a `.write()` call
 *      with 257 KiB of data. The first 256 KiB are immediately sent and the
 *      remaining 1 KiB is buffered for a future upload.
 *   -# If the same stream receives another `.write()` call with 256 KiB then it
 *      will send the buffered 1 KiB of data and the first 255 KiB from the new
 *      buffer. The last 1 KiB is buffered for a future upload.
 *   -# Consider a fresh `FileWriteStream` that receives a `.write()` call
 *      with 4 MiB of data. This data is sent immediately, and no data is
 *      buffered.
 *   -# Consider a stream with a 256 KiB buffer from previous buffered I/O (see
 *      below to understand how this might happen). If this stream receives a
 *      `.write()` call with 1024 KiB then both the 256 KiB and the 1024 KiB of
 *      data are uploaded immediately.
 *
 * @par Formatted I/O
 * When performing formatted I/O, typically used via `operator<<`, this class
 * will buffer data based on the`UploadBufferSizeOption` setting.
 * Note that this setting is expressed in bytes, but it is always rounded (up)
 * to an upload quantum.
 *
 * @par Recommendations
 * For best performance uploading data we recommend using *exclusively* the
 * unbuffered I/O API. Furthermore, we recommend that applications use data in
 * multiples of the upload quantum in all calls to `.write()`. Larger buffers
 * result in better performance. Note that for google drive
 * [empirical results][github-issue-2657] show that these improvements tapper
 * off around 32MiB or so.
 *
 * @par Suspending Uploads
 * Note that, as it is customary in C++, the destructor of this class finalizes
 * the upload. If you want to prevent the class from finalizing an upload, use
 * the `Suspend()` function.
 *
 */
class FileWriteStream : public std::basic_ostream<char>
{
public:
    /**
     * Creates a stream not associated with any buffer.
     *
     * Attempts to use this stream will result in failures.
     */
    FileWriteStream();

    /**
     * Creates a stream associated with the give request.
     *
     * Reading from the stream will result in http requests to get more data
     * from the cloud file.
     *
     * @param buf an initialized FileWriteStreambuf to upload the data.
     */
    explicit FileWriteStream(std::unique_ptr<internal::FileWriteStreambuf> buf);

    FileWriteStream(FileWriteStream&& rhs) noexcept;

    FileWriteStream& operator=(FileWriteStream&& rhs) noexcept
    {
        FileWriteStream tmp(std::move(rhs));
        swap(tmp);
        return *this;
    }

    void swap(FileWriteStream& rhs)
    {
        basic_ostream<char>::swap(rhs);
        std::swap(m_buf, rhs.m_buf);
        rhs.set_rdbuf(rhs.m_buf.get());
        set_rdbuf(m_buf.get());
        std::swap(m_metadata, rhs.m_metadata);
        std::swap(m_headers, rhs.m_headers);
        std::swap(m_payload, rhs.m_payload);
    }

    FileWriteStream(FileWriteStream const&) = delete;
    FileWriteStream& operator=(FileWriteStream const&) = delete;

    /// Closes the stream (if necessary).
    ~FileWriteStream() override;

    /**
     * Return true if the stream is open to write more data.
     *
     * @note
     * write streams can be "born closed" when created using a previously
     * finalized upload session. Applications that restore a previous session
     * should check the state, for example:
     *
     * @code
     * auto stream = client.WriteFile(...,
     *     csa::RestoreResumableUploadSession(sessionId));
     * if (!stream.IsOpen() && stream.GetMetadata().Ok()) {
     *   std::cout << "Yay! The upload was finalized previously.\n";
     *   return;
     * }
     * @endcode
     */
    bool IsOpen() const { return m_buf != nullptr && m_buf->IsOpen(); }

    /**
     * Close the stream, finalizing the upload.
     *
     * Closing a stream completes an upload and creates the uploaded object. On
     * failure it sets the `badbit` of the stream.
     *
     * The metadata of the uploaded object, or a detailed error status, is
     * accessible via the `GetMetadata()` member function. Note that the metadata may
     * be empty if the application creates a stream with the `Fields("")`
     * parameter, applications cannot assume that all fields in the metadata are
     * filled on success.
     *
     * @throws may  throw `std::ios_base::failure`.
     */
    void Close();

    //@{
    /**
     * Access the upload results.
     *
     * Note that calling these member functions before `Close()` is undefined
     * behavior.
     */
    StatusOrVal<FileMetadata> const& GetMetadata() const& { return m_metadata; }
    StatusOrVal<FileMetadata>&& GetMetadata() && { return std::move(m_metadata); }

    /// The headers returned by the service, for debugging only.
    HeadersMap const& GetHeaders() const { return m_headers; }

    /// The returned payload as a raw string, for debugging only.
    std::string const& GetPayload() const { return m_payload; }
    //@}

    /**
     * Returns the resumable upload session id for this upload.
     *
     * Note that this is an empty string for uploads that do not use resumable
     * upload session ids. `Client::WriteFile()` enables resumable uploads based
     * on the options set by the application.
     *
     * Furthermore, this value might change during an upload.
     */
    std::string const& GetResumableSessionId() const { return m_buf->GetResumableSessionId(); }

    /**
     * Returns the next expected byte.
     *
     * For non-resumable uploads this is always zero. Applications that use
     * resumable uploads can use this value to resend any data not committed in
     * the cloud.
     */
    std::uint64_t GetNextExpectedByte() const { return m_buf->GetNextExpectedByte(); }

    /**
     * Suspends an upload.
     *
     * This is a destructive operation. Using this object after calling this
     * function results in undefined behavior. Applications should copy any
     * necessary state (such as the value `resumable_session_id()`) before calling
     * this function.
     */
    void Suspend() &&;

    /**
     * Returns the status of partial errors.
     *
     * Application may write multiple times before closing the stream, this
     * function gives the capability to find out status even before stream
     * closure.
     *
     * This function is different than `metadata()` as calling `metadata()`
     * before Close() is undefined.
     */
    Status GetLastStatus() const { return m_buf->GetLastStatus(); }

private:
    /**
     * Closes the underlying object write stream.
     */
    void CloseBuf();

    std::unique_ptr<internal::FileWriteStreambuf> m_buf;
    StatusOrVal<FileMetadata> m_metadata;
    std::multimap<std::string, std::string> m_headers;
    std::string m_payload;
};

}  // namespace csa
