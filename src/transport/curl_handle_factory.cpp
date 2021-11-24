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

#include "cloudstorageapi/internal/curl_handle_factory.h"

namespace csa {
namespace internal {

std::once_flag defaultCurlHandleFactoryInitialized;
std::shared_ptr<CurlHandleFactory> defaultCurlHandleFactory;

void CurlHandleFactory::SetCurlStringOption(CURL* handle, CURLoption option_tag, char const* value)
{
    curl_easy_setopt(handle, option_tag, value);
}

std::shared_ptr<CurlHandleFactory> GetDefaultCurlHandleFactory()
{
    std::call_once(defaultCurlHandleFactoryInitialized,
                   [] { defaultCurlHandleFactory = std::make_shared<DefaultCurlHandleFactory>(); });
    return defaultCurlHandleFactory;
}

std::shared_ptr<CurlHandleFactory> GetDefaultCurlHandleFactory(Options const& options)
{
    if (!options.Get<CARootsFilePathOption>().empty())
    {
        return std::make_shared<DefaultCurlHandleFactory>(options);
    }
    return GetDefaultCurlHandleFactory();
}

DefaultCurlHandleFactory::DefaultCurlHandleFactory(Options const& o)
{
    if (o.Has<CARootsFilePathOption>())
        m_cainfo = o.Get<CARootsFilePathOption>();
    if (o.Has<CAPathOption>())
        m_capath = o.Get<CAPathOption>();
}

CurlPtr DefaultCurlHandleFactory::CreateHandle()
{
    CurlPtr curl(curl_easy_init(), &curl_easy_cleanup);
    SetCurlOptions(curl.get());
    return curl;
}

void DefaultCurlHandleFactory::CleanupHandle(CurlHandle&& h)
{
    if (GetHandle(h) == nullptr)
        return;
    char* ip;
    auto res = curl_easy_getinfo(GetHandle(h), CURLINFO_LOCAL_IP, &ip);
    if (res == CURLE_OK && ip != nullptr)
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        m_lastClientIpAddress = ip;
    }
    ResetHandle(h);
}

CurlMulti DefaultCurlHandleFactory::CreateMultiHandle() { return CurlMulti(curl_multi_init(), &curl_multi_cleanup); }

void DefaultCurlHandleFactory::CleanupMultiHandle(CurlMulti&& m) { m.reset(); }

void DefaultCurlHandleFactory::SetCurlOptions(CURL* handle)
{
    if (m_cainfo)
    {
        SetCurlStringOption(handle, CURLOPT_CAINFO, m_cainfo->c_str());
    }
    if (m_capath)
    {
        SetCurlStringOption(handle, CURLOPT_CAPATH, m_capath->c_str());
    }
}

PooledCurlHandleFactory::PooledCurlHandleFactory(std::size_t maximumSize, Options const& o) : m_maximumSize(maximumSize)
{
    if (o.Has<CARootsFilePathOption>())
        m_cainfo = o.Get<CARootsFilePathOption>();
    if (o.Has<CAPathOption>())
        m_capath = o.Get<CAPathOption>();
}

PooledCurlHandleFactory::~PooledCurlHandleFactory()
{
    for (auto* h : m_handles)
    {
        curl_easy_cleanup(h);
    }
    for (auto* m : m_multiHandles)
    {
        curl_multi_cleanup(m);
    }
}

CurlPtr PooledCurlHandleFactory::CreateHandle()
{
    std::unique_lock<std::mutex> lk(m_mutex);
    if (!m_handles.empty())
    {
        CURL* handle = m_handles.back();
        // Clear all the options in the handle so we do not leak its previous state.
        (void)curl_easy_reset(handle);
        m_handles.pop_back();
        CurlPtr curl(handle, &curl_easy_cleanup);
        SetCurlOptions(curl.get());
        return curl;
    }
    CurlPtr curl(curl_easy_init(), &curl_easy_cleanup);
    SetCurlOptions(curl.get());
    return curl;
}

void PooledCurlHandleFactory::CleanupHandle(CurlHandle&& h)
{
    if (GetHandle(h) == nullptr)
        return;
    std::unique_lock<std::mutex> lk(m_mutex);
    char* ip;
    auto res = curl_easy_getinfo(GetHandle(h), CURLINFO_LOCAL_IP, &ip);
    if (res == CURLE_OK && ip != nullptr)
    {
        m_lastClientIpAddress = ip;
    }
    while (m_handles.size() >= m_maximumSize)
    {
        CURL* tmp = m_handles.front();
        m_handles.erase(m_handles.begin());
        curl_easy_cleanup(tmp);
    }
    m_handles.push_back(GetHandle(h));
    // The handles_ vector now has ownership, so release it.
    ReleaseHandle(h);
}

CurlMulti PooledCurlHandleFactory::CreateMultiHandle()
{
    std::unique_lock<std::mutex> lk(m_mutex);
    if (!m_multiHandles.empty())
    {
        CURL* m = m_multiHandles.back();
        m_multiHandles.pop_back();
        return CurlMulti(m, &curl_multi_cleanup);
    }
    return CurlMulti(curl_multi_init(), &curl_multi_cleanup);
}

void PooledCurlHandleFactory::CleanupMultiHandle(CurlMulti&& m)
{
    if (!m)
        return;
    std::unique_lock<std::mutex> lk(m_mutex);
    while (m_multiHandles.size() >= m_maximumSize)
    {
        auto* tmp = m_multiHandles.front();
        m_multiHandles.erase(m_multiHandles.begin());
        curl_multi_cleanup(tmp);
    }
    m_multiHandles.push_back(m.get());
    // The multi_handles_ vector now has ownership, so release it.
    (void)m.release();
}

void PooledCurlHandleFactory::SetCurlOptions(CURL* handle)
{
    if (m_cainfo)
    {
        SetCurlStringOption(handle, CURLOPT_CAINFO, m_cainfo->c_str());
    }
    if (m_capath)
    {
        SetCurlStringOption(handle, CURLOPT_CAPATH, m_capath->c_str());
    }
}

}  // namespace internal
}  // namespace csa
