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

#include "cloudstorageapi/internal/file_streambuf.h"
#include <ios>
#include <iostream>
#include <string>

namespace csa {

/**
 * Defines a `std::basic_ostream<char>` to write to a cloud file.
 */
class FileWriteStream : public std::basic_ostream<char>
{
public:
    /**
     * Creates a stream not associated with any buffer.
     *
     * Attempts to use this stream will result in failures.
     */
    FileWriteStream() : std::basic_ostream<char>(nullptr), m_buf() {}

    /**
     * Creates a stream associated with the give request.
     *
     * Reading from the stream will result in http requests to get more data
     * from the cloud file.
     *
     * @param buf an initialized FileWriteStreambuf to upload the data.
     */
    explicit FileWriteStream(
        std::unique_ptr<internal::FileWriteStreambuf> buf);

    FileWriteStream(FileWriteStream&& rhs) noexcept
        : FileWriteStream(std::move(rhs.m_buf))
    {
        m_metadata = std::move(rhs.m_metadata);
        m_headers = std::move(rhs.m_headers);
        m_payload = std::move(rhs.m_payload);
        // We cannot use set_rdbuf() because older versions of libstdc++ do not
        // implement this function. Unfortunately `move()` resets `rdbuf()`, and
        // `rdbuf()` resets the state, so we have to manually copy the rest of
        // the state.
        setstate(rhs.rdstate());
        copyfmt(rhs);
        rhs.rdbuf(nullptr);
    }

    FileWriteStream& operator=(FileWriteStream&& rhs) noexcept
    {
        m_buf = std::move(rhs.m_buf);
        m_metadata = std::move(rhs.m_metadata);
        m_headers = std::move(rhs.m_headers);
        m_payload = std::move(rhs.m_payload);
        // Use rdbuf() (instead of set_rdbuf()) because older versions of libstdc++
        // do not implement this function. Unfortunately `rdbuf()` resets the state,
        // and `move()` resets `rdbuf()`, so we have to manually copy the rest of
        // the state.
        rdbuf(m_buf.get());
        setstate(rhs.rdstate());
        copyfmt(rhs);
        rhs.rdbuf(nullptr);
        return *this;
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
    StatusOrVal<FileMetadata>&& GetMetadata()&& { return std::move(m_metadata); }

    /// The headers returned by the service, for debugging only.
    std::multimap<std::string, std::string> const& GetHeaders() const
    {
        return m_headers;
    }

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
    std::string const& GetResumableSessionId() const
    {
        return m_buf->GetResumableSessionId();
    }

    /**
     * Returns the next expected byte.
     *
     * For non-resumable uploads this is always zero. Applications that use
     * resumable uploads can use this value to resend any data not committed in
     * the cloud.
     */
    std::uint64_t GetNextExpectedByte() const
    {
        return m_buf->GetNextExpectedByte();
    }

    /**
     * Suspends an upload.
     *
     * This is a destructive operation. Using this object after calling this
     * function results in undefined behavior. Applications should copy any
     * necessary state (such as the value `resumable_session_id()`) before calling
     * this function.
     */
    void Suspend()&&;

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

} // namespace csa
