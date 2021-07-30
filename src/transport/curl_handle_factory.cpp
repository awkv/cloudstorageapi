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

std::shared_ptr<CurlHandleFactory> GetDefaultCurlHandleFactory()
{
    std::call_once(defaultCurlHandleFactoryInitialized,
                   [] { defaultCurlHandleFactory = std::make_shared<DefaultCurlHandleFactory>(); });
    return defaultCurlHandleFactory;
}

CurlPtr DefaultCurlHandleFactory::CreateHandle() { return CurlPtr(curl_easy_init(), &curl_easy_cleanup); }

void DefaultCurlHandleFactory::CleanupHandle(CurlPtr&& h)
{
    char* ip;
    auto res = curl_easy_getinfo(h.get(), CURLINFO_LOCAL_IP, &ip);
    if (res == CURLE_OK && ip != nullptr)
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        m_lastClientIpAddress = ip;
    }

    h.reset();
}

CurlMulti DefaultCurlHandleFactory::CreateMultiHandle() { return CurlMulti(curl_multi_init(), &curl_multi_cleanup); }

void DefaultCurlHandleFactory::CleanupMultiHandle(CurlMulti&& m) { m.reset(); }

PooledCurlHandleFactory::PooledCurlHandleFactory(std::size_t maximum_size) : m_maximumSize(maximum_size)
{
    m_handles.reserve(maximum_size);
    m_multiHandles.reserve(maximum_size);
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
        return CurlPtr(handle, &curl_easy_cleanup);
    }
    return CurlPtr(curl_easy_init(), &curl_easy_cleanup);
}

void PooledCurlHandleFactory::CleanupHandle(CurlPtr&& h)
{
    std::unique_lock<std::mutex> lk(m_mutex);
    char* ip;
    auto res = curl_easy_getinfo(h.get(), CURLINFO_LOCAL_IP, &ip);
    if (res == CURLE_OK && ip != nullptr)
    {
        m_lastClientIpAddress = ip;
    }
    if (m_handles.size() >= m_maximumSize)
    {
        CURL* tmp = m_handles.front();
        m_handles.erase(m_handles.begin());
        curl_easy_cleanup(tmp);
    }
    m_handles.push_back(h.get());
    // The handles_ vector now has ownership, so release it.
    (void)h.release();
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
    std::unique_lock<std::mutex> lk(m_mutex);
    if (m_multiHandles.size() >= m_maximumSize)
    {
        CURLM* tmp = m_multiHandles.front();
        m_multiHandles.erase(m_multiHandles.begin());
        curl_multi_cleanup(tmp);
    }
    m_multiHandles.push_back(m.get());
    // The multi_handles_ vector now has ownership, so release it.
    (void)m.release();
}

}  // namespace internal
}  // namespace csa
