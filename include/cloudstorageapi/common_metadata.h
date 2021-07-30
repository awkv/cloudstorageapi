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
#include <memory>
#include <string>

namespace csa {

class CommonMetadata
{
public:
    std::string GetCloudId() const { return m_cloudId; }
    void SetCloudId(std::string cloudId) { m_cloudId = cloudId; }
    std::string GetName() const { return m_name; }
    void SetName(std::string name) { m_name = name; }
    std::string GetParentId() const { return m_parentId; }
    void SetParentId(std::string parentId) { m_parentId = parentId; }
    std::int64_t GetSize() const { return m_size; }
    void SetSize(std::int64_t size) { m_size = size; }
    // On linux systems:
    // atime (access time)
    // Access time shows the last time the data from a file or directory was accessed � read by one
    // of the Unix processes directly or through commands and scripts. Due to its definition,
    // atime attribute must be updated � meaning written to a disk � every time a Unix file is accessed,
    // even if it was just a read operation.
    // ctime (change time)
    // ctime shows when your file or directory got metadata changes � typically that's file ownership
    // (username and/or group) and access permissions. ctime will also get updated if the file contents got changed.
    // mtime (modify time)
    // Last modification time shows time of the  last change to file's contents. It does not change with
    // owner or permission changes, and is therefore used for tracking the actual changes to data of the file itself.
    //
    // NOTE: there's no file creation timestamp kept in most filesystems � meaning you can't run a command
    // like "show me all files created on certain date". This said, it's usually possible to deduce the same
    // from ctime and mtime (if they match � this probably means that's when the file was created).
    std::chrono::system_clock::time_point GetChangeTime() const { return m_ctime; }
    void SetChangeTime(std::chrono::system_clock::time_point ctime) { m_ctime = ctime; }
    std::chrono::system_clock::time_point GetModifyTime() const { return m_mtime; }
    void SetModifyTime(std::chrono::system_clock::time_point mtime) { m_mtime = mtime; }
    std::chrono::system_clock::time_point GetAccessTime() const { return m_atime; }
    void SetAccessTime(std::chrono::system_clock::time_point atime) { m_atime = atime; }

    friend bool operator==(CommonMetadata const& lhs, CommonMetadata const& rhs);
    friend bool operator!=(CommonMetadata const& lhs, CommonMetadata const& rhs) { return !(lhs == rhs); }

protected:
    std::string m_cloudId;
    std::string m_name;
    std::string m_parentId;
    std::int64_t m_size = 0;
    std::chrono::system_clock::time_point m_ctime;
    std::chrono::system_clock::time_point m_mtime;
    std::chrono::system_clock::time_point m_atime;

private:
    friend std::ostream& operator<<(std::ostream& os, const CommonMetadata& commonMetadata);
};

std::ostream& operator<<(std::ostream& os, const CommonMetadata& commonMetadata);

using CommonMetadataSharedPtr = std::shared_ptr<CommonMetadata>;
}  // namespace csa
