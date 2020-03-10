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

#include "cloudstorageapi/status_or_val.h"
#include "cloudstorageapi/internal/http_response.h"
#include "cloudstorageapi/file_metadata.h"
#include <cstdint>
#include <iosfwd>
#include <optional>
#include <string>

namespace csa {
namespace internal {

struct ResumableUploadResponse;

/**
 * Defines the interface for a resumable upload session.
 */
class ResumableUploadSession
{
public:
    virtual ~ResumableUploadSession() = default;

    /**
     * Uploads a chunk and returns the resulting response.
     *
     * @param buffer the chunk to upload.
     * @return The result of uploading the chunk.
     */
    virtual StatusOrVal<ResumableUploadResponse> UploadChunk(
        std::string const& buffer) = 0;

    /**
     * Uploads the final chunk in a stream, committing all previous data.
     *
     * @param buffer the chunk to upload.
     * @param upload_size the total size of the upload, use `0` if the size is not
     *   known.
     * @return The final result of the upload, including the object metadata.
     */
    virtual StatusOrVal<ResumableUploadResponse> UploadFinalChunk(
        std::string const& buffer, std::uint64_t uploadSize) = 0;

    /// Resets the session by querying its current state.
    virtual StatusOrVal<ResumableUploadResponse> ResetSession() = 0;

    /**
     * Returns the next expected byte in the server.
     *
     * Users of this class should check this value in case a previous
     * UploadChunk() has partially failed and the application (or the component
     * using this class) needs to re-send a chunk.
     */
    virtual std::uint64_t GetNextExpectedByte() const = 0;

    /**
     * Returns the current upload session id.
     *
     * Note that the session id might change during an upload.
     */
    virtual std::string const& GetSessionId() const = 0;

    /**
     * Returns the chunk size quantum
     */
    virtual std::size_t GetFileChunkSizeQuantum() const = 0;

    /// Returns whether the upload session has completed.
    virtual bool Done() const = 0;

    /// Returns the last upload response encountered during the upload.
    virtual StatusOrVal<ResumableUploadResponse> const& GetLastResponse() const = 0;
};

struct ResumableUploadResponse
{
    enum class UploadState { InProgress, Done };

    std::string m_uploadSessionUrl;
    std::uint64_t m_lastCommittedByte;
    std::optional<FileMetadata> m_payload;
    UploadState m_uploadState;
    std::string m_annotations;
};

bool operator==(ResumableUploadResponse const& lhs,
    ResumableUploadResponse const& rhs);
bool operator!=(ResumableUploadResponse const& lhs,
    ResumableUploadResponse const& rhs);

std::ostream& operator<<(std::ostream& os, ResumableUploadResponse const& r);

/**
 * A resumable upload session that always returns an error.
 *
 * When an unrecoverable error is detected (or the policies to recover from an
 * error are exhausted), we create an object of this type to represent a session
 * that will never succeed. This is cleaner than returning a nullptr and then
 * checking for null in each call.
 */
class ResumableUploadSessionError : public ResumableUploadSession
{
public:
    explicit ResumableUploadSessionError(Status status)
        : m_lastResponse(std::move(status)) {}

    ResumableUploadSessionError(Status status, std::uint64_t nextExpectedByte,
        std::string id)
        : m_lastResponse(std::move(status)),
        m_nextExpectedByte(nextExpectedByte),
        m_id(id) {}

    ~ResumableUploadSessionError() override = default;

    StatusOrVal<ResumableUploadResponse> UploadChunk(std::string const&) override
    {
        return m_lastResponse;
    }

    StatusOrVal<ResumableUploadResponse> UploadFinalChunk(std::string const&,
        std::uint64_t) override
    {
        return m_lastResponse;
    }

    StatusOrVal<ResumableUploadResponse> ResetSession() override
    {
        return m_lastResponse;
    }

    std::uint64_t GetNextExpectedByte() const override
    {
        return m_nextExpectedByte;
    }

    std::string const& GetSessionId() const override { return m_id; }

    std::size_t GetFileChunkSizeQuantum() const override
    {
        return 0;
    }

    bool Done() const override { return true; }

    StatusOrVal<ResumableUploadResponse> const& GetLastResponse() const override
    {
        return m_lastResponse;
    }

private:
    StatusOrVal<ResumableUploadResponse> m_lastResponse;
    std::uint64_t m_nextExpectedByte = 0;
    std::string m_id;
};

}  // namespace internal
}  // namespace csa
