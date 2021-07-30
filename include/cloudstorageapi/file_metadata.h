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

#include "common_metadata.h"
#include "internal/complex_option.h"
#include <chrono>
#include <optional>
#include <string>

namespace csa {

class FileMetadata : public CommonMetadata
{
public:
    std::optional<std::string> GetMimeTypeOpt() const { return m_mimetype; }
    void SetMimeTypeOpt(std::optional<std::string> mimetype) { m_mimetype = mimetype; }
    bool IsDownloadable() const { return m_downloadable; }
    void SetDownloadable(bool downloadable) { m_downloadable = downloadable; }

    friend bool operator==(FileMetadata const& lhs, FileMetadata const& rhs);
    friend bool operator!=(FileMetadata const& lhs, FileMetadata const& rhs) { return !(lhs == rhs); }

private:
    friend std::ostream& operator<<(std::ostream& os, FileMetadata const& rhs);

    std::optional<std::string> m_mimetype;
    bool m_downloadable = false;
};

std::ostream& operator<<(std::ostream& os, FileMetadata const& rhs);

struct WithFileMetadata : public internal::ComplexOption<WithFileMetadata, FileMetadata>
{
    using ComplexOption<WithFileMetadata, FileMetadata>::ComplexOption;
    static char const* name() { return "object-metadata"; }
};

using FileMetadataSharedPtr = std::shared_ptr<FileMetadata>;
}  // namespace csa
