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
#include "cloudstorageapi/internal/algorithm.h"
#include "cloudstorageapi/internal/log.h"
#include "cloudstorageapi/internal/utils.h"
#include "cloudstorageapi/status_or_val.h"
#include <set>
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

constexpr auto DefaultMaximumRetryPeriod = std::chrono::minutes(15);

constexpr auto DefaultInitialBackoffDelay = std::chrono::seconds(1);

constexpr auto DefaultMaximumBackoffDelay = std::chrono::minutes(5);

constexpr auto DefaultBackoffScaling = 2.0;

}  // namespace

namespace internal {
Options CreateDefaultOptions(std::shared_ptr<auth::Credentials> credentials, Options opts)
{
    auto o =
        Options{}
            .Set<Oauth2CredentialsOption>(std::move(credentials))
            .Set<ConnectionPoolSizeOption>(DefaultConnectionPoolSize())
            .Set<DownloadBufferSizeOption>(DefaultDownloadBufferSize)
            .Set<UploadBufferSizeOption>(DefaultUploadBufferSize)
            .Set<MaximumSimpleUploadSizeOption>(DefaultMaximumSimpleUploadSize)
            .Set<EnableCurlSslLockingOption>(true)
            .Set<EnableCurlSigpipeHandlerOption>(true)
            .Set<MaximumCurlSocketRecvSizeOption>(0)
            .Set<MaximumCurlSocketSendSizeOption>(0)
            .Set<DownloadStallTimeoutOption>(std::chrono::seconds(DefaultDownloadStallTimeout))
            .Set<RetryPolicyOption>(csa::LimitedTimeRetryPolicy(DefaultMaximumRetryPeriod).Clone())
            .Set<BackoffPolicyOption>(csa::ExponentialBackoffPolicy(DefaultInitialBackoffDelay,
                                                                    DefaultMaximumBackoffDelay, DefaultBackoffScaling)
                                          .Clone());

    auto tracing = csa::internal::GetEnv("CSA_ENABLE_TRACING");
    if (tracing.has_value())
    {
        auto const enabled = csa::internal::StrSplit(*tracing, ',');
        if (enabled.end() != std::find(enabled.begin(), enabled.end(), "http"))
        {
            CSA_LOG_INFO("Enabling logging for http");
            o.Lookup<TracingComponentsOption>().insert("http");
        }
        if (enabled.end() != std::find(enabled.begin(), enabled.end(), "raw-client"))
        {
            CSA_LOG_INFO("Enabling logging for RawClient functions");
            o.Lookup<TracingComponentsOption>().insert("raw-client");
        }
    }

    return o;
}

Options CreateDefaultOptionsWithCredentials(Options opts)
{
    if (opts.Has<Oauth2CredentialsOption>())
    {
        auto credentials = opts.Get<Oauth2CredentialsOption>();
        return internal::CreateDefaultOptions(std::move(credentials), std::move(opts));
    }
    return internal::CreateDefaultOptions(
        csa::auth::CredentialFactory::CreateDefaultCredentials(opts.Get<ProviderOption>()), std::move(opts));
}
}  // namespace internal
}  // namespace csa
