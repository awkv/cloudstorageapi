// Copyright 2019 Andrew Karasyov
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
#include "cloudstorageapi/download_options.h"
#include "cloudstorageapi/file_metadata.h"
#include "cloudstorageapi/internal/const_buffer.h"
#include "cloudstorageapi/internal/generic_object_requests.h"
#include "cloudstorageapi/upload_options.h"
#include "cloudstorageapi/well_known_headers.h"

namespace csa {
namespace internal {

/**
 * Represents a request to the file metadata.
 */
class GetFileMetadataRequest : public GenericObjectRequest<GetFileMetadataRequest>
{
public:
    using GenericObjectRequest::GenericObjectRequest;
};

std::ostream& operator<<(std::ostream& os, GetFileMetadataRequest const& r);

/**
 * Represents a request to the `Files: update` API.
 */
class PatchFileMetadataRequest : public GenericObjectRequest<PatchFileMetadataRequest>
{
public:
    PatchFileMetadataRequest() = default;
    explicit PatchFileMetadataRequest(std::string fileId, FileMetadata original, FileMetadata updated)
        : GenericObjectRequest(std::move(fileId)),
          m_OriginalMeta(std::move(original)),
          m_UpdatedMeta(std::move(updated))
    {
    }

    FileMetadata const& GetOriginalMetadata() const { return m_OriginalMeta; }
    FileMetadata const& GetUpdatedMetadata() const { return m_UpdatedMeta; }

private:
    FileMetadata m_OriginalMeta;
    FileMetadata m_UpdatedMeta;
};

std::ostream& operator<<(std::ostream& os, PatchFileMetadataRequest const& r);

class InsertFileRequest : public GenericObjectRequest<InsertFileRequest, ContentEncoding, ContentType, UploadFromOffset,
                                                      UploadLimit, WithFileMetadata>
{
public:
    InsertFileRequest(std::string folderId, std::string name, std::string content)
        : m_folderId(std::move(folderId)), m_name(std::move(name)), m_content(std::move(content))
    {
    }

    std::string GetFolderId() const { return m_folderId; }
    std::string GetName() const { return m_name; }
    std::string const& GetContent() const { return m_content; }
    InsertFileRequest& SetContent(std::string&& cnt)
    {
        m_content = std::move(cnt);
        return *this;
    }

private:
    std::string m_folderId;
    std::string m_name;
    std::string m_content;
};

std::ostream& operator<<(std::ostream& os, InsertFileRequest const& r);

class DeleteRequest : public GenericObjectRequest<DeleteRequest>
{
public:
    using GenericObjectRequest::GenericObjectRequest;
};

std::ostream& operator<<(std::ostream& os, DeleteRequest const& r);

/**
 * Represents a request to start a resumable upload in `Files: insert`.
 *
 * This request type is used to start resumable uploads. A resumable upload is
 * started with a `File: insert` request with the `uploadType=resumable`
 * query parameter. The payload for the initial request includes the (optional)
 * object metadata. The response includes a URL to send requests that upload
 * the media.
 */
class ResumableUploadRequest
    : public GenericObjectRequest<ResumableUploadRequest, ContentEncoding, ContentType, UseResumableUploadSession,
                                  UploadFromOffset, UploadLimit, WithFileMetadata, UploadContentLength, AutoFinalize>
{
public:
    ResumableUploadRequest() = default;

    ResumableUploadRequest(std::string folderId, std::string fileName)
        : GenericObjectRequest(std::move(folderId)), m_name(std::move(fileName))
    {
    }

    std::string GetFolderId() const { return GetObjectId(); }
    std::string GetFileName() const { return m_name; }

private:
    std::string m_name;
};

std::ostream& operator<<(std::ostream& os, ResumableUploadRequest const& r);

/**
 * A request to cancel a resumable upload.
 */
class DeleteResumableUploadRequest : public GenericRequest<DeleteResumableUploadRequest>
{
public:
    DeleteResumableUploadRequest() = default;
    explicit DeleteResumableUploadRequest(std::string uploadSessionUrl)
        : m_uploadSessionUrl(std::move(uploadSessionUrl))
    {
    }

    std::string const& GetUploadSessionUrl() const { return m_uploadSessionUrl; }

private:
    std::string m_uploadSessionUrl;
};

std::ostream& operator<<(std::ostream& os, DeleteResumableUploadRequest const& r);

/**
 * A request to send one chunk in an upload session.
 */
class UploadChunkRequest : public GenericRequest<UploadChunkRequest>
{
public:
    UploadChunkRequest() = default;
    UploadChunkRequest(std::string uploadSessionUrl, std::uint64_t rangeBegin, ConstBufferSequence payload)
        : GenericRequest(),
          m_uploadSessionUrl(std::move(uploadSessionUrl)),
          m_rangeBegin(rangeBegin),
          m_payload(std::move(payload)),
          m_sourceSize(0),
          m_lastChunk(false)
    {
    }

    UploadChunkRequest(std::string uploadSessionUrl, std::uint64_t rangeBegin, ConstBufferSequence payload,
                       std::uint64_t sourceSize)
        : GenericRequest(),
          m_uploadSessionUrl(std::move(uploadSessionUrl)),
          m_rangeBegin(rangeBegin),
          m_payload(std::move(payload)),
          m_sourceSize(sourceSize),
          m_lastChunk(true)
    {
    }

    std::string const& GetUploadSessionUrl() const { return m_uploadSessionUrl; }
    std::uint64_t GetRangeBegin() const { return m_rangeBegin; }
    std::uint64_t GetRangeEnd() const { return m_rangeBegin + GetPayloadSize() - 1; }
    std::uint64_t GetSourceSize() const { return m_sourceSize; }
    std::size_t GetPayloadSize() const { return TotalBytes(m_payload); }
    ConstBufferSequence const& GetPayload() const { return m_payload; }
    bool IsLastChunk() const { return m_lastChunk; }

private:
    std::string m_uploadSessionUrl;
    std::uint64_t m_rangeBegin = 0;
    ConstBufferSequence m_payload;
    std::uint64_t m_sourceSize = 0;
    bool m_lastChunk = false;
};

std::ostream& operator<<(std::ostream& os, UploadChunkRequest const& r);

/**
 * A request to query the status of a resumable upload.
 */
class QueryResumableUploadRequest : public GenericRequest<QueryResumableUploadRequest>
{
public:
    QueryResumableUploadRequest() = default;
    explicit QueryResumableUploadRequest(std::string uploadSessionUrl)
        : GenericRequest(), m_uploadSessionUrl(std::move(uploadSessionUrl))
    {
    }

    std::string const& GetUploadSessionUrl() const { return m_uploadSessionUrl; }

private:
    std::string m_uploadSessionUrl;
};

std::ostream& operator<<(std::ostream& os, QueryResumableUploadRequest const& r);

/**
 * Represents a request to the `Objects: get` API with `alt=media`.
 */
class ReadFileRangeRequest : public GenericObjectRequest<ReadFileRangeRequest, ReadFromOffset, ReadRange, ReadLast>
{
public:
    using GenericObjectRequest::GenericObjectRequest;

    bool RequiresNoCache() const;
    bool RequiresRangeHeader() const;
    std::int64_t GetStartingByte() const;
};

std::ostream& operator<<(std::ostream& os, ReadFileRangeRequest const& r);

/**
 * Represents a request to the `Files: copy` API.
 */
class CopyFileRequest : public GenericObjectRequest<CopyFileRequest, WithFileMetadata>
{
public:
    using GenericObjectRequest::GenericObjectRequest;
    CopyFileRequest(std::string fileId, std::string newParentId, std::string newFileName)
        : GenericObjectRequest(std::move(fileId)),
          m_destinationParentId(std::move(newParentId)),
          m_destinationFileName(std::move(newFileName))
    {
    }

    std::string const& GetDestinationParentId() const { return m_destinationParentId; }
    std::string const& GetDestinationFileName() const { return m_destinationFileName; }

private:
    std::string m_destinationParentId;
    std::string m_destinationFileName;
};

std::ostream& operator<<(std::ostream& os, CopyFileRequest const& r);

}  // namespace internal
}  // namespace csa
