// Copyright 2023 Andrew Karasyov
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

// This file is expected to be included from log.h (spdlog based logger).

// spdlog uses fmtlib` which provides std::ostream support including formatting of user-defined types.
// However, in order to make a type formattable via std::ostream
// you should provide a formatter specialization inherited from ostream_formatter.
// @see https://fmt.dev/latest/api.html?highlight=ostream#std-ostream-support

#include "cloudstorageapi/common_metadata.h"
#include "cloudstorageapi/file_metadata.h"
#include "cloudstorageapi/folder_metadata.h"
#include "cloudstorageapi/internal/empty_response.h"
#include "cloudstorageapi/internal/file_requests.h"
#include "cloudstorageapi/internal/folder_requests.h"
#include "cloudstorageapi/internal/http_empty_response.h"
#include "cloudstorageapi/internal/http_response.h"
#include "cloudstorageapi/internal/resumable_upload_session.h"
#include "cloudstorageapi/status_or_val.h"
#include "cloudstorageapi/storage_quota.h"
#include "cloudstorageapi/user_info.h"

template <>
struct fmt::formatter<csa::Status> : ostream_formatter
{
};
template <>
struct fmt::formatter<csa::StatusCode> : ostream_formatter
{
};
template <>
struct fmt::formatter<csa::UserInfo> : ostream_formatter
{
};
template <>
struct fmt::formatter<csa::StorageQuota> : ostream_formatter
{
};
template <>
struct fmt::formatter<csa::CommonMetadata> : ostream_formatter
{
};
template <>
struct fmt::formatter<csa::ReadRangeData> : ostream_formatter
{
};
template <>
struct fmt::formatter<csa::FileMetadata> : ostream_formatter
{
};
template <>
struct fmt::formatter<csa::FolderMetadata> : ostream_formatter
{
};
template <>
struct fmt::formatter<csa::internal::ResumableUploadResponse> : ostream_formatter
{
};
template <>
struct fmt::formatter<csa::internal::EmptyResponse> : ostream_formatter
{
};
template <>
struct fmt::formatter<csa::internal::GetFileMetadataRequest> : ostream_formatter
{
};
template <>
struct fmt::formatter<csa::internal::PatchFileMetadataRequest> : ostream_formatter
{
};
template <>
struct fmt::formatter<csa::internal::InsertFileRequest> : ostream_formatter
{
};
template <>
struct fmt::formatter<csa::internal::DeleteRequest> : ostream_formatter
{
};
template <>
struct fmt::formatter<csa::internal::ResumableUploadRequest> : ostream_formatter
{
};
template <>
struct fmt::formatter<csa::internal::DeleteResumableUploadRequest> : ostream_formatter
{
};
template <>
struct fmt::formatter<csa::internal::UploadChunkRequest> : ostream_formatter
{
};
template <>
struct fmt::formatter<csa::internal::QueryResumableUploadRequest> : ostream_formatter
{
};
template <>
struct fmt::formatter<csa::internal::ReadFileRangeRequest> : ostream_formatter
{
};
template <>
struct fmt::formatter<csa::internal::CopyFileRequest> : ostream_formatter
{
};
template <>
struct fmt::formatter<csa::internal::ListFolderRequest> : ostream_formatter
{
};
template <>
struct fmt::formatter<csa::internal::ListFolderResponse> : ostream_formatter
{
};
template <>
struct fmt::formatter<csa::internal::GetFolderMetadataRequest> : ostream_formatter
{
};
template <>
struct fmt::formatter<csa::internal::CreateFolderRequest> : ostream_formatter
{
};
template <>
struct fmt::formatter<csa::internal::PatchFolderMetadataRequest> : ostream_formatter
{
};
template <>
struct fmt::formatter<csa::internal::RenameRequest> : ostream_formatter
{
};
template <>
struct fmt::formatter<csa::internal::HttpEmptyResponse> : ostream_formatter
{
};
template <>
struct fmt::formatter<csa::internal::HttpResponse> : ostream_formatter
{
};
