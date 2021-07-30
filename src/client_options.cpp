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

#include "cloudstorageapi/client_options.h"
#include "cloudstorageapi/auth/credential_factory.h"
#include "cloudstorageapi/auth/credentials.h"
#include "cloudstorageapi/status_or_val.h"
#include <thread>

namespace csa {

namespace {
StatusOrVal<std::shared_ptr<auth::Credentials>> StorageDefaultCredentials(EProvider provider)
{
    return auth::CredentialFactory::CreateDefaultCredentials(provider);
}

std::size_t DefaultConnectionPoolSize()
{
    std::size_t nthreads = std::thread::hardware_concurrency();
    constexpr auto singleThreadPoolSize = 4;
    return (nthreads == 0) ? singleThreadPoolSize : singleThreadPoolSize * nthreads;
}

constexpr auto DefaultUploadBufferSize = 8 * 1024 * 1024;

constexpr auto DefaultDownloadBufferSize = 3 * 1024 * 1024 / 2;

constexpr auto DefaultMaximumSimpleUploadSize = 20 * 1024 * 1024L;

constexpr auto DefaultDownloadStallTimeout = 120;

}  // namespace

StatusOrVal<ClientOptions> ClientOptions::CreateDefaultClientOptions(EProvider provider)
{
    auto creds = StorageDefaultCredentials(provider);
    if (!creds)
        return creds.GetStatus();
    else
        return ClientOptions(provider, *creds);
}

ClientOptions::ClientOptions(EProvider provider, std::shared_ptr<auth::Credentials> credentials)
    : m_provider(provider),
      m_credentials(std::move(credentials)),
      m_enableHttpTracing(false),
      m_enableRawClientTracing(false),
      m_connectionPoolSize(DefaultConnectionPoolSize()),
      m_downloadBufferSize(DefaultDownloadBufferSize),
      m_uploadBufferSize(DefaultUploadBufferSize),
      m_maximumSimpleUploadSize(DefaultMaximumSimpleUploadSize),
      m_downloadStallTimeout(DefaultDownloadStallTimeout)
{
}

ClientOptions& ClientOptions::SetDownloadBufferSize(std::size_t size)
{
    m_downloadBufferSize = (size == 0) ? DefaultDownloadBufferSize : size;

    return *this;
}

ClientOptions& ClientOptions::SetUploadBufferSize(std::size_t size)
{
    m_uploadBufferSize = (size == 0) ? DefaultUploadBufferSize : size;

    return *this;
}

}  // namespace csa
