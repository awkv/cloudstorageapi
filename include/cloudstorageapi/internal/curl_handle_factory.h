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

#include "cloudstorageapi/internal/curl_handle.h"
#include "cloudstorageapi/internal/curl_wrappers.h"
#include <mutex>
#include <optional>
#include <vector>

namespace csa {
namespace internal {
/**
 * Implements the Factory Pattern for CURL handles (and multi-handles).
 */
class CurlHandleFactory
{
public:
    virtual ~CurlHandleFactory() = default;

    virtual CurlPtr CreateHandle() = 0;
    virtual void CleanupHandle(CurlHandle&&) = 0;

    virtual CurlMulti CreateMultiHandle() = 0;
    virtual void CleanupMultiHandle(CurlMulti&&) = 0;

    virtual std::string LastClientIpAddress() const = 0;

protected:
    // Only virtual for testing purposes.
    virtual void SetCurlStringOption(CURL* handle, CURLoption option_tag, char const* value);

    static CURL* GetHandle(CurlHandle& h) { return h.m_handle.get(); }
    static void ResetHandle(CurlHandle& h) { h.m_handle.reset(); }
    static void ReleaseHandle(CurlHandle& h) { (void)h.m_handle.release(); }
};

std::shared_ptr<CurlHandleFactory> GetDefaultCurlHandleFactory(Options const& options);
std::shared_ptr<CurlHandleFactory> GetDefaultCurlHandleFactory();

/**
 * Implements the default CurlHandleFactory.
 *
 * This implementation of the CurlHandleFactory does not save handles, it
 * creates a new handle on each call to `CreateHandle()` and releases the
 * handle on `CleanupHandle()`.
 */
class DefaultCurlHandleFactory : public CurlHandleFactory
{
public:
    DefaultCurlHandleFactory() = default;
    explicit DefaultCurlHandleFactory(Options const& o);

    CurlPtr CreateHandle() override;
    void CleanupHandle(CurlHandle&&) override;

    CurlMulti CreateMultiHandle() override;
    void CleanupMultiHandle(CurlMulti&&) override;

    std::string LastClientIpAddress() const override
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        return m_lastClientIpAddress;
    }

private:
    void SetCurlOptions(CURL* handle);

    mutable std::mutex m_mutex;
    std::string m_lastClientIpAddress;
    std::optional<std::string> m_cainfo;
    std::optional<std::string> m_capath;
};

/**
 * Implements a CurlHandleFactory that pools handles.
 *
 * This implementation keeps up to N handles in memory, they are only released
 * when the factory is destructed.
 */
class PooledCurlHandleFactory : public CurlHandleFactory
{
public:
    explicit PooledCurlHandleFactory(std::size_t maximum_size, Options const& o);
    explicit PooledCurlHandleFactory(std::size_t maximum_size) : PooledCurlHandleFactory(maximum_size, {}) {}

    ~PooledCurlHandleFactory() override;

    CurlPtr CreateHandle() override;
    void CleanupHandle(CurlHandle&&) override;

    CurlMulti CreateMultiHandle() override;
    void CleanupMultiHandle(CurlMulti&&) override;

    std::string LastClientIpAddress() const override
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        return m_lastClientIpAddress;
    }

    // Test only
    std::size_t CurrentHandleCount() const
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        return m_handles.size();
    }
    // Test only
    std::size_t CurrentMultiHandleCount() const
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        return m_multiHandles.size();
    }

private:
    void SetCurlOptions(CURL* handle);

    std::size_t m_maximumSize;
    mutable std::mutex m_mutex;
    std::vector<CURL*> m_handles;
    std::vector<CURLM*> m_multiHandles;
    std::string m_lastClientIpAddress;
    std::optional<std::string> m_cainfo;
    std::optional<std::string> m_capath;
};

}  // namespace internal
}  // namespace csa
