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

#include "cloudstorageapi/internal/clients/google_metadata_parser.h"
#include "cloudstorageapi/internal/file_requests.h"
#include "cloudstorageapi/internal/folder_requests.h"
#include "cloudstorageapi/internal/json_utils.h"
#include "cloudstorageapi/internal/resumable_upload_session.h"
#include "cloudstorageapi/status_or_val.h"
#include <nlohmann/json.hpp>
#include <sstream>

namespace csa {
namespace internal {

namespace detail {

// GCC limitation: see https://gcc.gnu.org/bugzilla/show_bug.cgi?id=85282
// GCC fail to compile full function template specialization in non namespace scope.
// So, declare them in this namespace.
// Base function templates. They overload on json and string. They have no body because
// only specializations will be used.
template <typename ResponseType>
StatusOrVal<ResponseType> ParseResponse(nlohmann::json const& json);  // no implementation
template <typename ResponseType>
StatusOrVal<ResponseType> ParseResponse(std::string const& payload);  // no implementation
template <typename ResponseType>
StatusOrVal<ResponseType> ParseResponse(HttpResponse response);  // no implementation

}  // namespace detail

class GoogleResponseParser
{
public:
    static constexpr const char* ResponseKindFileList = "drive#fileList";
    static constexpr const char* ObjectKindFile = "drive#file";
    static constexpr const char* FolderMimetype = "application/vnd.google-apps.folder";

    GoogleResponseParser() = delete;

    template <typename ResponseType>
    static StatusOrVal<ResponseType> ParseResponse(nlohmann::json const& json)
    {
        return detail::ParseResponse<ResponseType>(json);
    }

    template <typename ResponseType>
    static StatusOrVal<ResponseType> ParseResponse(std::string const& payload)
    {
        return detail::ParseResponse<ResponseType>(payload);
    }

    template <typename ResponseType>
    static StatusOrVal<ResponseType> ParseResponse(HttpResponse response)
    {
        return detail::ParseResponse<ResponseType>(response);
    }
};

namespace detail {

// Specializations for concrete response types

template <>
inline StatusOrVal<ListFolderResponse> ParseResponse<ListFolderResponse>(nlohmann::json const& json)
{
    if (json.empty())
    {
        return Status(StatusCode::Internal, "Empty folder list response. Expected some generic fields are present.");
    }
    else if (json.value("kind", "") != GoogleResponseParser::ResponseKindFileList)
    {
        return Status(StatusCode::Internal, "Unexpected folder list response kind: " + json.value("kind", "") +
                                                ". Expected: " + GoogleResponseParser::ResponseKindFileList);
    }

    ListFolderResponse result{};
    result.m_nextPageToken = json.value("nextPageToken", "");

    if (json.count("files") > 0)
    {
        for (auto const& file : json["files"])
        {
            if (file.value("kind", "") == GoogleResponseParser::ObjectKindFile)
            {
                if (file.value("mimeType", "") == GoogleResponseParser::FolderMimetype)
                {
                    auto objectMetadata = GoogleMetadataParser::ParseFolderMetadata(file);
                    if (objectMetadata.Ok())
                    {
                        result.m_items.emplace_back(std::move(*objectMetadata));
                    }
                    else
                    {
                        return Status(StatusCode::InvalidArgument,
                                      "Invalid list folder request."
                                      "Failed to parse folder metadata.");
                    }
                }
                else
                {
                    auto objectMetadata = GoogleMetadataParser::ParseFileMetadata(file);
                    if (objectMetadata.Ok())
                    {
                        result.m_items.emplace_back(std::move(*objectMetadata));
                    }
                    else
                    {
                        return Status(StatusCode::InvalidArgument,
                                      "Invalid list folder request."
                                      "Failed to parse file metadata.");
                    }
                }
            }
        }
    }

    return result;
}

template <>
inline StatusOrVal<ListFolderResponse> ParseResponse<ListFolderResponse>(std::string const& payload)
{
    auto json = nlohmann::json::parse(payload, nullptr, false);
    if (json.is_discarded())
    {
        return Status(StatusCode::InvalidArgument,
                      "Invalid folder list response. "
                      "Failed to parse json.");
    }

    return ParseResponse<ListFolderResponse>(json);
}

template <>
inline StatusOrVal<ResumableUploadResponse> ParseResponse<ResumableUploadResponse>(HttpResponse response)
{
    ResumableUploadResponse result;
    if (response.m_statusCode == HttpStatusCode::Ok || response.m_statusCode == HttpStatusCode::Created)
    {
        result.m_uploadState = ResumableUploadResponse::UploadState::Done;
    }
    else
    {
        result.m_uploadState = ResumableUploadResponse::UploadState::InProgress;
    }

    result.m_lastCommittedByte = 0;

    // The payload contains the object resource when the upload
    // is finished. In that case, we try to parse it.
    if (result.m_uploadState == ResumableUploadResponse::UploadState::Done && !response.m_payload.empty())
    {
        auto metadata = GoogleMetadataParser::ParseFileMetadata(response.m_payload);
        if (!metadata)
        {
            return std::move(metadata).GetStatus();
        }
        result.m_payload = *std::move(metadata);
    }

    if (response.m_headers.find("location") != response.m_headers.end())
    {
        result.m_uploadSessionUrl = response.m_headers.find("location")->second;
    }

    auto r = response.m_headers.find("range");
    if (r == response.m_headers.end())
    {
        std::ostringstream os;
        os << __func__ << "() missing range header in resumable upload response"
           << ", response=" << response;
        result.m_annotations = std::move(os).str();
        return result;
    }

    // We expect a `Range:` header in the format described here:
    //    https://developers.google.com/drive/api/v3/manage-uploads#resumable
    // that is the value should match `bytes=0-[0-9]+`:
    std::string const& range = r->second;

    if (range.rfind("bytes=0-", 0) != 0)
    {
        std::ostringstream os;
        os << __func__ << "() cannot parse range: header in resumable upload"
           << " response, header=" << range << ", response=" << response;
        result.m_annotations = std::move(os).str();
        return result;
    }
    char const* buffer = range.data() + 8;
    char* endptr;
    auto last = std::strtoll(buffer, &endptr, 10);
    if (buffer != endptr && *endptr == '\0' && 0 <= last)
    {
        result.m_lastCommittedByte = static_cast<std::uint64_t>(last);
    }
    else
    {
        std::ostringstream os;
        os << __func__ << "() cannot parse range: header in resumable upload"
           << " response, header=" << range << ", response=" << response;
        result.m_annotations = std::move(os).str();
    }

    return result;
}
}  // namespace detail

}  // namespace internal
}  // namespace csa
