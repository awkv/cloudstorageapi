// Copyright 2021 Andrew Karasyov
//
// Copyright 2018 Google LLC
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

#include "csa_example_helper.h"
#include "cloudstorageapi/cloud_storage_client.h"

namespace csa {
namespace example {

std::string SerializeTimePoint(const time_point& time, const std::string& format)
{
    std::time_t tt = std::chrono::system_clock::to_time_t(time);
    std::tm tm = *std::gmtime(&tt);  // GMT (UTC)
    std::stringstream ss;
    ss << std::put_time(&tm, format.c_str());
    return ss.str();
}

std::string MakeRandomFolderName(csa::internal::DefaultPRNG gen, std::string const& prefix)
{
    // The total length of this bucket name must be <= 63 characters,
    static std::size_t const MaxBucketNameLength = 63;
    auto const date = SerializeTimePoint(std::chrono::system_clock::now(), "%Y-%m-%d_%H:%M:%S");
    auto const full = prefix + '-' + date + '_';
    std::size_t const max_random_characters = MaxBucketNameLength - full.size();
    return full +
           csa::internal::Sample(gen, static_cast<int>(max_random_characters), "abcdefghijklmnopqrstuvwxyz0123456789");
}

std::string MakeRandomFolderName(csa::internal::DefaultPRNG gen)
{
    return MakeRandomFolderName(gen, "csa-testing-examples");
}

std::string MakeRandomObjectName(csa::internal::DefaultPRNG& gen)
{
    // 128 characters seems long enough to avoid collisions.
    auto constexpr ObjectNameLength = 128;
    return csa::internal::Sample(gen, ObjectNameLength,
                                 "abcdefghijklmnopqrstuvwxyz"
                                 "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                 "0123456789");
}

std::string MakeRandomObjectName(csa::internal::DefaultPRNG& gen, std::string const& prefix)
{
    return prefix + MakeRandomObjectName(gen);
}

std::string MakeRandomFilename(csa::internal::DefaultPRNG& gen)
{
    auto constexpr MaxBasenameLength = 28;
    std::string const prefix = "f-";
    return prefix +
           csa::internal::Sample(gen, static_cast<int>(MaxBasenameLength - prefix.size()),
                                 "abcdefghijklmnopqrstuvwxyz0123456789") +
           ".txt";
}
Status RemoveFolderAndContents(csa::CloudStorageClient* client, std::string const& folderId)
{
    // List all the objects, and then delete each.
    for (auto o : client->ListFolder(folderId))
    {
        if (!o)
            return std::move(o).GetStatus();
        std::string cloudId =
            std::visit([](auto&& item) -> std::string { return item.GetCloudId(); }, std::move(o).Value());
        auto status = client->Delete(cloudId);
        if (!status.Ok())
            return status;
    }
    return client->Delete(folderId);
}

std::string GetObjectId(csa::CloudStorageClient* client, std::string const& parentId, std::string const& name,
                        bool folder)
{
    // List all objects and look for given name
    std::string folderId;
    for (auto o : client->ListFolder(parentId))
    {
        if (!o)
            return {};
        struct ObjMatcher
        {
            std::string const& m_name;
            std::string& m_folderId;
            bool folder;
            void operator()(FolderMetadata&& m)
            {
                if (folder && m.GetName() == m_name)
                    m_folderId = m.GetCloudId();
            }
            void operator()(FileMetadata&& m)
            {
                if (!folder && m.GetName() == m_name)
                    m_folderId = m.GetCloudId();
            }
        };
        std::visit(ObjMatcher{name, folderId, folder}, std::move(o).Value());
    }

    return folderId;
}

}  // namespace example
}  // namespace csa
