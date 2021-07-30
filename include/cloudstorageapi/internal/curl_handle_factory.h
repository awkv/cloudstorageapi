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

#include "cloudstorageapi/internal/curl_wrappers.h"
#include <mutex>
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
    virtual void CleanupHandle(CurlPtr&&) = 0;

    virtual CurlMulti CreateMultiHandle() = 0;
    virtual void CleanupMultiHandle(CurlMulti&&) = 0;

    virtual std::string LastClientIpAddress() const = 0;
};

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

    CurlPtr CreateHandle() override;
    void CleanupHandle(CurlPtr&&) override;

    CurlMulti CreateMultiHandle() override;
    void CleanupMultiHandle(CurlMulti&&) override;

    std::string LastClientIpAddress() const override
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        return m_lastClientIpAddress;
    }

private:
    mutable std::mutex m_mutex;
    std::string m_lastClientIpAddress;
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
    explicit PooledCurlHandleFactory(std::size_t maximum_size);
    ~PooledCurlHandleFactory() override;

    CurlPtr CreateHandle() override;
    void CleanupHandle(CurlPtr&&) override;

    CurlMulti CreateMultiHandle() override;
    void CleanupMultiHandle(CurlMulti&&) override;

    std::string LastClientIpAddress() const override
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        return m_lastClientIpAddress;
    }

private:
    std::size_t m_maximumSize;
    mutable std::mutex m_mutex;
    std::vector<CURL*> m_handles;
    std::vector<CURLM*> m_multiHandles;
    std::string m_lastClientIpAddress;
};

}  // namespace internal
}  // namespace csa
