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

#pragma once

#include "cloudstorageapi/auth/credentials.h"
#include "cloudstorageapi/internal/curl_handle_factory.h"
#include "cloudstorageapi/internal/random.h"
#include "cloudstorageapi/internal/raw_client.h"
#include <mutex>

namespace csa {
namespace internal {
class CurlRequestBuilder;
class UploadChunkRequest;
class QueryResumableUploadRequest;
struct ResumableUploadResponse;

/**
 * Implements the low-level RPCs to Cloud Storage using libcurl.
 */
class CurlClientBase : public RawClient
{
public:
    CurlClientBase(CurlClientBase const& rhs) = delete;
    CurlClientBase(CurlClientBase&& rhs) = delete;
    CurlClientBase& operator=(CurlClientBase const& rhs) = delete;
    CurlClientBase& operator=(CurlClientBase&& rhs) = delete;

    using LockFunction = std::function<void(CURL*, curl_lock_data, curl_lock_access)>;
    using UnlockFunction = std::function<void(CURL*, curl_lock_data)>;

    ClientOptions const& GetClientOptions() const override { return m_options; }

    StatusOrVal<std::string> AuthorizationHeader(std::shared_ptr<auth::Credentials> const&);

    void LockShared(curl_lock_data data);
    void UnlockShared(curl_lock_data data);

    //@{
    // @name Implement the CurlResumableSession operations.
    // Note that these member functions are not inherited from RawClient, they are
    // called only by `CurlResumableUploadSession`, because the retry loop for
    // them is very different from the standard retry loop. Also note that these
    // are virtual functions only because we need to override them in the unit
    // tests.
    virtual StatusOrVal<ResumableUploadResponse> UploadChunk(UploadChunkRequest const&) = 0;
    virtual StatusOrVal<ResumableUploadResponse> QueryResumableUpload(QueryResumableUploadRequest const&) = 0;
    //@}

protected:
    // The constructor is protected because the class must always be created
    // as a shared_ptr<>.
    explicit CurlClientBase(ClientOptions options);

    /// Setup the configuration parameters that do not depend on the request.
    Status SetupBuilderCommon(CurlRequestBuilder& builder, char const* method);

    /// Applies the common configuration parameters to @p builder.
    template <typename Request>
    Status SetupBuilder(CurlRequestBuilder& builder, Request const& request, char const* method);

    ClientOptions m_options;

    // These mutexes are used to protect different portions of `share_`.
    std::mutex m_muShare;
    std::mutex m_muDns;
    std::mutex m_muSslSession;
    std::mutex m_muConnect;
    std::mutex m_muPsl;
    CurlShare m_share;

    std::mutex m_muRNG;
    DefaultPRNG m_generator;  // GUARDED_BY(m_muRNG);

    // The factories must be listed *after* the CurlShare. libcurl keeps a
    // usage count on each CURLSH* handle, which is only released once the CURL*
    // handle is *closed*. So we want the order of destruction to be (1)
    // factories, as that will delete all the CURL* handles, and then (2) CURLSH*.
    // To guarantee this order just list the members in the opposite order.
    std::shared_ptr<CurlHandleFactory> m_storageFactory;
    std::shared_ptr<CurlHandleFactory> m_uploadFactory;
};

template <typename Request>
inline Status CurlClientBase::SetupBuilder(CurlRequestBuilder& builder, Request const& request, char const* method)
{
    auto status = SetupBuilderCommon(builder, method);
    if (!status.Ok())
    {
        return status;
    }
    request.AddOptionsToHttpRequest(builder);
    return Status();
}

}  // namespace internal
}  // namespace csa
