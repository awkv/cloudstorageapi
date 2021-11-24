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

#include "cloudstorageapi/internal/clients/google_metadata_parser.h"
#include "cloudstorageapi/internal/json_utils.h"
#include "cloudstorageapi/internal/rfc3339_time.h"

namespace csa {
namespace internal {
namespace {
/**
 * Sets a string field in @p json when @p value is not empty.
 *
 * This simplifies the implementation of ToJsonString() because we repeat this
 * check for many attributes.
 */
void SetIfNotEmpty(nlohmann::json& json, char const* key, std::string const& value)
{
    if (value.empty())
    {
        return;
    }
    json[key] = value;
}

void SetIfDifferent(nlohmann::json& json, char const* key, std::string const& originalValue,
                    std::string const& updatedValue)
{
    if (originalValue != updatedValue)
        json[key] = updatedValue;
}
}  // namespace

StatusOrVal<FileMetadata> GoogleMetadataParser::ParseFileMetadata(nlohmann::json const& json)
{
    if (!json.is_object())
    {
        return Status(StatusCode::InvalidArgument, __func__);
    }

    FileMetadata fileMetadata{};
    auto status = ParseCommonMetadata(fileMetadata, json);
    if (!status.Ok())
        return status;

    fileMetadata.SetMimeTypeOpt(json.value("mimeType", ""));
    if (json.contains("capabilities"))
    {
        auto downloadable = JsonUtils::ParseBool(json["capabilities"], "canDownload");
        if (!downloadable)
            return std::move(downloadable).GetStatus();
        fileMetadata.SetDownloadable(*downloadable);
    }

    return fileMetadata;
}

StatusOrVal<FileMetadata> GoogleMetadataParser::ParseFileMetadata(std::string const& payload)
{
    auto json = nlohmann::json::parse(payload, nullptr, false);
    if (json.is_discarded())
    {
        return Status(StatusCode::InvalidArgument,
                      "Invalid file metadata object. "
                      "Failed to parse json.");
    }
    return ParseFileMetadata(json);
}

StatusOrVal<FolderMetadata> GoogleMetadataParser::ParseFolderMetadata(nlohmann::json const& json)
{
    if (!json.is_object())
    {
        return Status(StatusCode::InvalidArgument, __func__);
    }

    FolderMetadata folderMetadata{};
    auto status = ParseCommonMetadata(folderMetadata, json);
    if (!status.Ok())
        return status;

    if (json.contains("capabilities"))
    {
        auto canAddChildren = JsonUtils::ParseBool(json["capabilities"], "canAddChildren");
        if (!canAddChildren)
            return std::move(canAddChildren).GetStatus();
        folderMetadata.SetCanCreateFolders(*canAddChildren);
        folderMetadata.SetCanUploadFile(*canAddChildren);
    }

    return folderMetadata;
}

StatusOrVal<FolderMetadata> GoogleMetadataParser::ParseFolderMetadata(std::string const& payload)
{
    auto json = nlohmann::json::parse(payload, nullptr, false);
    if (json.is_discarded())
    {
        return Status(StatusCode::InvalidArgument,
                      "Invalid folder metadata object. "
                      "Failed to parse json.");
    }
    return ParseFolderMetadata(json);
}

StatusOrVal<nlohmann::json> GoogleMetadataParser::ComposeFileMetadata(FileMetadata const& meta)
{
    nlohmann::json jmeta({});
    jmeta["kind"] = "drive#file";
    auto status = ComposeCommonMetadata(jmeta, meta);
    if (!status.Ok())
        return status;
    if (meta.GetMimeTypeOpt())
        jmeta["mimeType"] = meta.GetMimeTypeOpt().value();

    return jmeta;
}

StatusOrVal<nlohmann::json> GoogleMetadataParser::ComposeFolderMetadata(FolderMetadata const& meta)
{
    nlohmann::json jmeta({});
    jmeta["kind"] = "drive#file";
    auto status = ComposeCommonMetadata(jmeta, meta);
    if (!status.Ok())
        return status;
    jmeta["mimeType"] = "application/vnd.google-apps.folder";
    return jmeta;
}

StatusOrVal<nlohmann::json> GoogleMetadataParser::PatchFileMetadata(FileMetadata const& original,
                                                                    FileMetadata const& updated)
{
    nlohmann::json jmeta({});
    auto status = PatchCommonMetadata(jmeta, original, updated);
    if (!status.Ok())
        return status;
    if (original.GetMimeTypeOpt() != updated.GetMimeTypeOpt())
        if (updated.GetMimeTypeOpt())
            jmeta["mimeType"] = updated.GetMimeTypeOpt().value();
    return jmeta;
}

StatusOrVal<nlohmann::json> GoogleMetadataParser::PatchFolderMetadata(FolderMetadata const& original,
                                                                      FolderMetadata const& updated)
{
    nlohmann::json jmeta({});
    auto status = PatchCommonMetadata(jmeta, original, updated);
    if (!status.Ok())
        return status;
    return jmeta;
}

Status GoogleMetadataParser::ParseCommonMetadata(CommonMetadata& result, nlohmann::json const& json)
{
    if (!json.is_object())
    {
        return Status(StatusCode::InvalidArgument, __func__);
    }
    result.SetCloudId(json.value("id", ""));

    result.SetName(json.value("name", ""));

    if (json.count("parents") > 0)
    {
        if (json["parents"].size() > 0)
            result.SetParentId(json["parents"].front());
    }
    auto size = JsonUtils::ParseLong(json, "size");
    if (!size)
        return std::move(size).GetStatus();
    result.SetSize(*size);
    auto modifiedTime = JsonUtils::ParseRFC3339Timestamp(json, "modifiedTime");
    if (!modifiedTime)
        return std::move(modifiedTime).GetStatus();
    result.SetChangeTime(*modifiedTime);
    result.SetModifyTime(*modifiedTime);
    result.SetAccessTime(*modifiedTime);

    return Status();
}

Status GoogleMetadataParser::ComposeCommonMetadata(nlohmann::json& result, CommonMetadata const& meta)
{
    SetIfNotEmpty(result, "id", meta.GetCloudId());
    SetIfNotEmpty(result, "name", meta.GetName());
    if (!meta.GetParentId().empty())
        result["parents"] = nlohmann::json::array({meta.GetParentId()});
    // ? result["createdTime"] = FormatRfc3339Time(meta.GetChangeTime());
    const auto epochTimePoint = std::chrono::time_point<std::chrono::system_clock>{};
    if (meta.GetChangeTime() > epochTimePoint)
        result["modifiedTime"] = FormatRfc3339(meta.GetChangeTime());

    return Status();
}

Status GoogleMetadataParser::PatchCommonMetadata(nlohmann::json& result, CommonMetadata const& original,
                                                 CommonMetadata const& updated)
{
    // Set only relevant fields: https://developers.google.com/drive/api/v3/reference/files/update
    if (original.GetModifyTime() != updated.GetModifyTime())
        result["modifiedTime"] = FormatRfc3339(updated.GetModifyTime());
    SetIfDifferent(result, "name", original.GetName(), updated.GetName());

    return Status();
}

}  // namespace internal
}  // namespace csa
