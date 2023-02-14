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

#include "cloudstorageapi/internal/random.h"
#include "cloudstorageapi/status.h"
#include <exception>
#include <chrono>
#include <string>

namespace csa {

class CloudStorageClient;

namespace example {
class NeedUsage : public std::runtime_error
{
public:
    NeedUsage(const char* const msg) : std::runtime_error(msg) {}
    NeedUsage(std::string const& msg) : std::runtime_error(msg.c_str()) {}
};

using time_point = std::chrono::system_clock::time_point;
std::string SerializeTimePoint(const time_point& time, const std::string& format);

std::string MakeRandomFolderName(csa::internal::DefaultPRNG gen, std::string const& prefix);

std::string MakeRandomFolderName(csa::internal::DefaultPRNG gen);

std::string MakeRandomObjectName(csa::internal::DefaultPRNG& gen);

std::string MakeRandomObjectName(csa::internal::DefaultPRNG& gen, std::string const& prefix);

std::string MakeRandomFilename(csa::internal::DefaultPRNG& gen);

Status RemoveFolderAndContents(csa::CloudStorageClient* client, std::string const& folderId);

std::string GetObjectId(csa::CloudStorageClient* client, std::string const& parentId, std::string const& name,
                        bool folder);

}  // namespace example
}  // namespace csa