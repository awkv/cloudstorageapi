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

#include <chrono>
#include <cinttypes>
#include <string>
#include <vector>

#include "common_metadata.h"

namespace csa {

class FolderMetadata : public CommonMetadata
{
public:
    bool GetCanCreateFolders() const { return m_CanCreateFolders; }
    void SetCanCreateFolders(bool canCreateFolder) { m_CanCreateFolders = canCreateFolder; }
    bool GetCanUploadFile() const { return m_CanUploadFile; }
    void SetCanUploadFile(bool canUploadFile) { m_CanUploadFile = canUploadFile; }

    friend bool operator==(FolderMetadata const& lhs, FolderMetadata const& rhs);
    friend bool operator!=(FolderMetadata const& lhs, FolderMetadata const& rhs)
    {
        return !(lhs == rhs);
    }

private:

    friend std::ostream& operator<<(std::ostream& os, FolderMetadata const& rhs);

    bool m_CanCreateFolders;
    bool m_CanUploadFile;
};

std::ostream& operator<<(std::ostream& os, FolderMetadata const& rhs);

using FolderMetadataSharedPtr = std::shared_ptr<FolderMetadata>;
} // namespace csa
