// Copyright 2019 Andrew Karasyov
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

#include "cloudstorageapi/cloud_storage_client.h"
#include "cloudstorageapi/internal/clients/curl_client_factory.h"

namespace csa {
static_assert(std::is_copy_constructible<csa::CloudStorageClient>::value,
    "csa::CloudStorageClient must be constructible");
static_assert(std::is_copy_assignable<csa::CloudStorageClient>::value,
    "csa::CloudStorageClient must be assignable");

StatusOrVal<CloudStorageClient> CloudStorageClient::CreateDefaultClient(EProvider provider)
{
    auto opts = ClientOptions::CreateDefaultClientOptions(provider);
    if (!opts)
    {
        return StatusOrVal<CloudStorageClient>(opts.GetStatus());
    }
    return StatusOrVal<CloudStorageClient>(CloudStorageClient(*opts));
}

std::shared_ptr<internal::RawClient> CloudStorageClient::CreateDefaultRawClient(ClientOptions options)
{
    return internal::CurlClientFactory::CreateClient(std::move(options));
}

}; // namespace csa
