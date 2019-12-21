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

#include "cloudstorageapi/client_options.h"
#include "cloudstorageapi/internal/clients/curl_googledrive_client.h"
#include <cassert>

namespace csa {
namespace internal {

class CurlClientFactory
{
public:
    CurlClientFactory() = delete;

    static std::shared_ptr<CurlClientBase> CreateClient(ClientOptions options)
    {
        switch (options.GetProvider())
        {
        case EProvider::GoogleDrive:
            return CurlGoogleDriveClient::Create(std::move(options));
        }

        assert(0&&"Unexpected provider.");
        return nullptr;
    }
};
} // namespace internal
} // namespace csa