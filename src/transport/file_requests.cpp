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

#include "cloudstorageapi/internal/file_requests.h"
#include "cloudstorageapi/file_metadata.h"
#include "cloudstorageapi/internal/utils.h"
#include <nlohmann/json.hpp>
#include <sstream>

namespace csa {
namespace internal {

std::ostream& operator<<(std::ostream& os, GetFileMetadataRequest const& r)
{
    os << "GetFileMetadataRequest={file_id=" << r.GetObjectId();
    r.DumpOptions(os, ", ");
    return os << "}";
}

std::ostream& operator<<(std::ostream& os, PatchFileMetadataRequest const& r)
{
    os << "PatchFileMetadataRequest={file_id=" << r.GetObjectId() << ", original_metadata=" << r.GetOriginalMetadata()
       << ", updated_metadata=" << r.GetUpdatedMetadata();
    r.DumpOptions(os, ", ");
    return os << "}";
}

std::ostream& operator<<(std::ostream& os, InsertFileRequest const& r)
{
    os << "InsertFileRequest={folder_id=" << r.GetFolderId() << ", name=" << r.GetName();
    r.DumpOptions(os, ", ");
    std::size_t constexpr MaxDumpSize = 1024;

    if (r.GetContent().size() > MaxDumpSize)
    {
        os << ", content[0..1024]=\n" << BinaryDataAsDebugString(r.GetContent().data(), MaxDumpSize);
    }
    else
    {
        os << ", content=\n" << BinaryDataAsDebugString(r.GetContent().data(), r.GetContent().size());
    }
    return os << "}";
}

std::ostream& operator<<(std::ostream& os, DeleteRequest const& r)
{
    os << "DeleteRequest={file_id=" << r.GetObjectId();
    r.DumpOptions(os, ", ");
    return os << "}";
}

std::ostream& operator<<(std::ostream& os, ResumableUploadRequest const& r)
{
    os << "ResumableUploadRequest={parent_folder_id=" << r.GetFolderId() << ", object_name=" << r.GetFileName();
    r.DumpOptions(os, ", ");
    return os << "}";
}

std::ostream& operator<<(std::ostream& os, DeleteResumableUploadRequest const& r)
{
    os << "DeleteResumableUploadRequest={upload_session_url=" << r.GetUploadSessionUrl();
    r.DumpOptions(os, ", ");
    return os << "}";
}

std::ostream& operator<<(std::ostream& os, UploadChunkRequest const& r)
{
    os << "UploadChunkRequest={file_id="
       << ", session_url=" << r.GetUploadSessionUrl() << ", range=(" << r.GetRangeBegin() << " : " << r.GetRangeEnd()
       << ")"
       << ", source_size=" << r.GetSourceSize() << ", last_chunk=" << (r.IsLastChunk() ? "true" : "false");

    r.DumpOptions(os, ", ");
    os << ", payload={";
    auto constexpr MaxOutputBytes = 128;
    char const* sep = "";
    for (auto const& b : r.GetPayload())
    {
        os << sep << "{" << BinaryDataAsDebugString(b.data(), b.size(), MaxOutputBytes) << "}";
        sep = ", ";
    }
    return os << "}}";
}

std::ostream& operator<<(std::ostream& os, QueryResumableUploadRequest const& r)
{
    os << "QueryResumableUploadRequest={upload_session_url=" << r.GetUploadSessionUrl();
    r.DumpOptions(os, ", ");
    return os << "}";
}

bool ReadFileRangeRequest::RequiresNoCache() const
{
    if (HasOption<ReadRange>())
    {
        return true;
    }
    if (HasOption<ReadFromOffset>() && GetOption<ReadFromOffset>().Value() != 0)
    {
        return true;
    }
    return HasOption<ReadLast>();
}

bool ReadFileRangeRequest::RequiresRangeHeader() const { return RequiresNoCache(); }

std::int64_t ReadFileRangeRequest::GetStartingByte() const
{
    std::int64_t result = 0;
    if (HasOption<ReadRange>())
    {
        result = (std::max)(result, GetOption<ReadRange>().Value().m_begin);
    }
    if (HasOption<ReadFromOffset>())
    {
        result = (std::max)(result, GetOption<ReadFromOffset>().Value());
    }
    if (HasOption<ReadLast>())
    {
        // The value of `StartingByte()` is unknown if `ReadLast` is set
        result = -1;
    }
    return result;
}

std::ostream& operator<<(std::ostream& os, ReadFileRangeRequest const& r)
{
    os << "ReadFileRangeRequest={file_id" << r.GetObjectId();
    r.DumpOptions(os, ", ");
    return os << "}";
}

std::ostream& operator<<(std::ostream& os, CopyFileRequest const& r)
{
    os << "CopyFileRequest={file_id" << r.GetObjectId() << ", destination_parent_id = " << r.GetDestinationParentId()
       << ", destination_file_name = " << r.GetDestinationFileName();
    r.DumpOptions(os, ", ");
    return os << "}";
}

}  // namespace internal
}  // namespace csa
