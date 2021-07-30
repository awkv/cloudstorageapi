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

#include "cloudstorageapi/internal/curl_client_base.h"
#include "cloudstorageapi/internal/curl_handle.h"
#include "cloudstorageapi/internal/curl_request_builder.h"
#include "cloudstorageapi/terminate_handler.h"
#include <sstream>

namespace csa {
namespace internal {
namespace {

extern "C" void CurlShareLockCallback(CURL*, curl_lock_data data, curl_lock_access, void* userptr)
{
    auto* client = reinterpret_cast<CurlClientBase*>(userptr);
    client->LockShared(data);
}

extern "C" void CurlShareUnlockCallback(CURL*, curl_lock_data data, void* userptr)
{
    auto* client = reinterpret_cast<CurlClientBase*>(userptr);
    client->UnlockShared(data);
}

std::shared_ptr<CurlHandleFactory> CreateHandleFactory(ClientOptions const& options)
{
    if (options.GetConnectionPoolSize() == 0)
    {
        return std::make_shared<DefaultCurlHandleFactory>();
    }
    return std::make_shared<PooledCurlHandleFactory>(options.GetConnectionPoolSize());
}

std::string UrlEscapeString(std::string const& value)
{
    CurlHandle handle;
    return std::string(handle.MakeEscapedString(value).get());
}

}  // namespace

CurlClientBase::CurlClientBase(ClientOptions options)
    : m_options(std::move(options)),
      m_share(curl_share_init(), &curl_share_cleanup),
      m_generator(csa::internal::MakeDefaultPRNG()),
      m_storageFactory(CreateHandleFactory(m_options)),
      m_uploadFactory(CreateHandleFactory(m_options))
{
    curl_share_setopt(m_share.get(), CURLSHOPT_LOCKFUNC, CurlShareLockCallback);
    curl_share_setopt(m_share.get(), CURLSHOPT_UNLOCKFUNC, CurlShareUnlockCallback);
    curl_share_setopt(m_share.get(), CURLSHOPT_USERDATA, this);
    curl_share_setopt(m_share.get(), CURLSHOPT_SHARE, CURL_LOCK_DATA_CONNECT);
    curl_share_setopt(m_share.get(), CURLSHOPT_SHARE, CURL_LOCK_DATA_SSL_SESSION);
    curl_share_setopt(m_share.get(), CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS);
#if LIBCURL_VERSION_NUM >= 0x076100
    curl_share_setopt(m_share.get(), CURLSHOPT_SHARE, CURL_LOCK_DATA_PSL);
#endif  // LIBCURL_VERSION_NUM

    CurlInitializeOnce(options);
}

StatusOrVal<std::string> CurlClientBase::AuthorizationHeader(std::shared_ptr<auth::Credentials> const& credentials)
{
    return credentials->AuthorizationHeader();
}

void CurlClientBase::LockShared(curl_lock_data data)
{
    switch (data)
    {
    case CURL_LOCK_DATA_SHARE:
        m_muShare.lock();
        return;
    case CURL_LOCK_DATA_DNS:
        m_muDns.lock();
        return;
    case CURL_LOCK_DATA_SSL_SESSION:
        m_muSslSession.lock();
        return;
    case CURL_LOCK_DATA_CONNECT:
        m_muConnect.lock();
        return;
#if LIBCURL_VERSION_NUM >= 0x076100
    case CURL_LOCK_DATA_PSL:
        m_muPsl.lock();
        return;
#endif  // LIBCURL_VERSION_NUM
    default:
        // We use a default because different versions of libcurl have different
        // values in the `curl_lock_data` enum.
        break;
    }
    std::ostringstream os;
    os << __func__ << "() - invalid or unknown data argument=" << data;
    csa::Terminate(os.str().c_str());
}

void CurlClientBase::UnlockShared(curl_lock_data data)
{
    switch (data)
    {
    case CURL_LOCK_DATA_SHARE:
        m_muShare.unlock();
        return;
    case CURL_LOCK_DATA_DNS:
        m_muDns.unlock();
        return;
    case CURL_LOCK_DATA_SSL_SESSION:
        m_muSslSession.unlock();
        return;
    case CURL_LOCK_DATA_CONNECT:
        m_muConnect.unlock();
        return;
#if LIBCURL_VERSION_NUM >= 0x076100
    case CURL_LOCK_DATA_PSL:
        m_muPsl.unlock();
        return;
#endif  // LIBCURL_VERSION_NUM
    default:
        // We use a default because different versions of libcurl have different
        // values in the `curl_lock_data` enum.
        break;
    }
    std::ostringstream os;
    os << __func__ << "() - invalid or unknown data argument=" << data;
    csa::Terminate(os.str().c_str());
}

Status CurlClientBase::SetupBuilderCommon(CurlRequestBuilder& builder, char const* method)
{
    auto authHeader = AuthorizationHeader(m_options.GetCredentials());
    if (!authHeader.Ok())
    {
        return std::move(authHeader).GetStatus();
    }
    builder.SetMethod(method).ApplyClientOptions(m_options).SetCurlShare(m_share.get()).AddHeader(authHeader.Value());
    return Status();
}

}  // namespace internal
}  // namespace csa
