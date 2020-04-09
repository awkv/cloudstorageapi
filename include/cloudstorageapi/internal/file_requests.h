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

#include "cloudstorageapi/download_options.h"
#include "cloudstorageapi/file_metadata.h"
#include "cloudstorageapi/internal/generic_object_request.h"
#include "cloudstorageapi/upload_options.h"
#include "cloudstorageapi/well_known_headers.h"

namespace csa {
namespace internal {

/**
 * Represents a request to the file metadata.
 */
class GetFileMetadataRequest
    : public GenericObjectRequest<GetFileMetadataRequest>
{
public:
    using GenericObjectRequest::GenericObjectRequest;
};

std::ostream& operator<<(std::ostream& os, GetFileMetadataRequest const& r);

/**
 * Represents a request to the `Files: update` API.
 */
class PatchFileMetadataRequest
    : public GenericObjectRequest<PatchFileMetadataRequest>
{
public:
    PatchFileMetadataRequest() = default;
    explicit PatchFileMetadataRequest(std::string fileId, FileMetadata original,
        FileMetadata updated)
        : GenericObjectRequest(std::move(fileId)),
        m_OriginalMeta(std::move(original)),
        m_UpdatedMeta(std::move(updated))
    {}

    FileMetadata const& GetOriginalMetadata() const { return m_OriginalMeta; }
    FileMetadata const& GetUpdatedMetadata() const { return m_UpdatedMeta; }

private:
    FileMetadata m_OriginalMeta;
    FileMetadata m_UpdatedMeta;
};

std::ostream& operator<<(std::ostream& os, PatchFileMetadataRequest const& r);

/**
 * Represents a request to rename/move file.
 */
class RenameFileRequest
    : public GenericObjectRequest<RenameFileRequest>
{
public:
    RenameFileRequest(std::string id, std::string newName, std::string parentId, std::string newParentId)
        : GenericObjectRequest<RenameFileRequest>(id),
        m_newName(std::move(newName)),
        m_parentId(std::move(parentId)),
        m_newParentId(std::move(newParentId))
    {}

    std::string GetParentId() const { return m_parentId; }
    std::string GetNewParentId() const { return m_newParentId; }
    std::string GetNewName() const { return m_newName; }

private:
    std::string m_newName;
    std::string m_parentId;
    std::string m_newParentId;
};

std::ostream& operator<<(std::ostream& os, RenameFileRequest const& r);

class InsertFileRequest : public GenericObjectRequest<InsertFileRequest, ContentType, WithFileMetadata>
{
public:
    InsertFileRequest(std::string folderId, std::string name, std::string content)
        : m_folderId(std::move(folderId)),
        m_name(std::move(name)),
        m_content(std::move(content))
    {}

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
    : public GenericObjectRequest<ResumableUploadRequest, ContentEncoding,
    ContentType, UseResumableUploadSession, WithFileMetadata>
{
public:
    ResumableUploadRequest() = default;

    ResumableUploadRequest(std::string folderId, std::string fileName)
        : GenericObjectRequest(),
        m_folderId(std::move(folderId)),
        m_name(std::move(fileName))
    {}

    std::string GetFolderId() const { return m_folderId; }
    std::string GetFileName() const { return m_name; }

private:
    std::string m_folderId;
    std::string m_name;
};

std::ostream& operator<<(std::ostream& os, ResumableUploadRequest const& r);

/**
 * A request to send one chunk in an upload session.
 */
class UploadChunkRequest : public GenericRequest<UploadChunkRequest>
{
public:
    UploadChunkRequest() = default;
    UploadChunkRequest(std::string uploadSessionUrl, std::uint64_t rangeBegin,
        std::string payload)
        : GenericRequest(),
        m_uploadSessionUrl(std::move(uploadSessionUrl)),
        m_rangeBegin(rangeBegin),
        m_payload(std::move(payload)),
        m_sourceSize(0),
        m_lastChunk(false)
    {}

    UploadChunkRequest(std::string uploadSessionUrl, std::uint64_t rangeBegin,
        std::string payload, std::uint64_t sourceSize)
        : GenericRequest(),
        m_uploadSessionUrl(std::move(uploadSessionUrl)),
        m_rangeBegin(rangeBegin),
        m_payload(std::move(payload)),
        m_sourceSize(sourceSize),
        m_lastChunk(true)
    {}

    std::string const& GetUploadSessionUrl() const { return m_uploadSessionUrl; }
    std::uint64_t GetRangeBegin() const { return m_rangeBegin; }
    std::uint64_t GetRangeEnd() const { return m_rangeBegin + m_payload.size() - 1; }
    std::uint64_t GetSourceSize() const { return m_sourceSize; }
    std::string const& GetPayload() const { return m_payload; }
    bool IsLastChunk() const { return m_lastChunk; }

private:
    std::string m_uploadSessionUrl;
    std::uint64_t m_rangeBegin = 0;
    std::string m_payload;
    std::uint64_t m_sourceSize = 0;
    bool m_lastChunk = false;
};

std::ostream& operator<<(std::ostream& os, UploadChunkRequest const& r);

/**
 * A request to query the status of a resumable upload.
 */
class QueryResumableUploadRequest
    : public GenericRequest<QueryResumableUploadRequest>
{
public:
    QueryResumableUploadRequest() = default;
    explicit QueryResumableUploadRequest(std::string uploadSessionUrl)
        : GenericRequest(), m_uploadSessionUrl(std::move(uploadSessionUrl)) {}

    std::string const& GetUploadSessionUrl() const { return m_uploadSessionUrl; }

private:
    std::string m_uploadSessionUrl;
};

std::ostream& operator<<(std::ostream& os,
    QueryResumableUploadRequest const& r);

/**
 * Represents a request to the `Objects: get` API with `alt=media`.
 */
class ReadFileRangeRequest
    : public GenericObjectRequest<
    ReadFileRangeRequest,
    Generation, ReadFromOffset, ReadRange, ReadLast>
{
public:
    using GenericObjectRequest::GenericObjectRequest;

    bool RequiresRangeHeader() const;
    std::int64_t GetStartingByte() const;
};

std::ostream& operator<<(std::ostream& os, ReadFileRangeRequest const& r);

/**
 * Represents a request to the `Files: copy` API.
 */
class CopyFileRequest
    : public GenericObjectRequest<
    CopyFileRequest,
    Generation, WithFileMetadata>
{
public:
    using GenericObjectRequest::GenericObjectRequest;
    CopyFileRequest(std::string fileId,
        std::string newParentId,
        std::string newFileName)
        : GenericObjectRequest(std::move(fileId)),
        m_newParentId(std::move(newParentId)),
        m_newFileName(std::move(newFileName))
    {}

    std::string const& GetNewParentId() const { return m_newParentId; }
    std::string const& GetNewFileName() const { return m_newFileName; }

private:
    std::string m_newParentId;
    std::string m_newFileName;
};

std::ostream& operator<<(std::ostream& os, CopyFileRequest const& r);

}  // namespace internal
}  // namespace csa
