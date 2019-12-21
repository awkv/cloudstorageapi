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

#include "cloudstorageapi/status_or_val.h"
#include "cloudstorageapi/internal/file_requests.h"
#include "cloudstorageapi/internal/folder_requests.h"
#include "cloudstorageapi/internal/nljson.h"
#include "cloudstorageapi/internal/json_utils.h"

namespace csa {
namespace internal {

class GoogleResponseParser
{
public:
    static constexpr const char* ResponseKindFileList = "drive#fileList";
    static constexpr const char* ObjectKindFile = "drive#file";
    static constexpr const char* FolderMimetype = "application/vnd.google-apps.folder";

    GoogleResponseParser() = delete;

    // Base function templates. They overload on json and string. They have no body because
    // only specializations will be used.
    template<typename ResponseType>
    static StatusOrVal<ResponseType> ParseResponse(nl::json const& json); // no implementation
    template<typename ResponseType>
    static StatusOrVal<ResponseType> ParseResponse(std::string const& payload); // no implementation

    // Specializations for concrete response types
    template<>
    static StatusOrVal<ListFolderResponse> ParseResponse<ListFolderResponse>(nl::json const& json);
    template<>
    static StatusOrVal<ListFolderResponse> ParseResponse<ListFolderResponse>(std::string const& payload);
};

template<>
inline StatusOrVal<ListFolderResponse> GoogleResponseParser::ParseResponse<ListFolderResponse>(nl::json const& json)
{
    if (json.empty())
    {
        return Status(StatusCode::Internal, "Empty folder list response. Expected some generic fields are present.");
    }
    else if (json.value("kind", "") != ResponseKindFileList)
    {
        return Status(StatusCode::Internal, "Unexpected folder list response kind: " +
        json.value("kind", "") + ". Expected: " + ResponseKindFileList);
    }

    ListFolderResponse result{};
    result.m_nextPageToken = json.value("nextPageToken", "");

    if (json.count("files") > 0)
    {
        for (auto const& file : json["files"])
        {
            if (file.value("kind", "") == ObjectKindFile)
            {
                if (file.value("mimeType", "") == FolderMimetype)
                {
                    auto objectMetadata = GoogleMetadataParser::ParseFolderMetadata(file);
                    if (objectMetadata.Ok())
                    {
                        result.m_items.emplace_back(std::move(*objectMetadata));
                    }
                    else
                    {
                        return Status(StatusCode::InvalidArgument, "Invalid list folder request."
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
                        return Status(StatusCode::InvalidArgument, "Invalid list folder request."
                        "Failed to parse file metadata.");
                    }
                }
            }
        }
    }

    return result;
}

template<>
inline StatusOrVal<ListFolderResponse> GoogleResponseParser::ParseResponse<ListFolderResponse>(std::string const& payload)
{
    auto json = nl::json::parse(payload, nullptr, false);
    if (json.is_discarded())
    {
        return Status(StatusCode::InvalidArgument, "Invalid folder list response. "
            "Failed to parse json.");
    }

    return ParseResponse<ListFolderResponse>(json);
}

}  // namespace internal
}  // namespace csa

