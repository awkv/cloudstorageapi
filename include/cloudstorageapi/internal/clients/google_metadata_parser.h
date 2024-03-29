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

#include "cloudstorageapi/file_metadata.h"
#include "cloudstorageapi/folder_metadata.h"
#include "cloudstorageapi/status_or_val.h"
#include <nlohmann/json.hpp>

namespace csa {
namespace internal {

class GoogleMetadataParser
{
public:
    GoogleMetadataParser() = delete;

    static StatusOrVal<CommonMetadataSharedPtr> ParseMetadata(nlohmann::json const& json);
    static StatusOrVal<CommonMetadataSharedPtr> ParseMetadata(std::string const& payload);
    static StatusOrVal<FileMetadata> ParseFileMetadata(nlohmann::json const& json);
    static StatusOrVal<FileMetadata> ParseFileMetadata(std::string const& payload);
    static StatusOrVal<FolderMetadata> ParseFolderMetadata(nlohmann::json const& json);
    static StatusOrVal<FolderMetadata> ParseFolderMetadata(std::string const& payload);

    static StatusOrVal<nlohmann::json> ComposeFileMetadata(FileMetadata const& meta);
    static StatusOrVal<nlohmann::json> ComposeFolderMetadata(FolderMetadata const& meta);

    static StatusOrVal<nlohmann::json> PatchFileMetadata(FileMetadata const& original, FileMetadata const& updated);
    static StatusOrVal<nlohmann::json> PatchFolderMetadata(FolderMetadata const& original,
                                                           FolderMetadata const& updated);

private:
    static Status ParseCommonMetadata(CommonMetadata& result, nlohmann::json const& json);
    static Status ComposeCommonMetadata(nlohmann::json& result, CommonMetadata const& meta);
    static Status PatchCommonMetadata(nlohmann::json& result, CommonMetadata const& original,
                                      CommonMetadata const& updated);
};
}  // namespace internal
}  // namespace csa
