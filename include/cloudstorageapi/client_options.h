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
#include "cloudstorageapi/providers.h"
#include "cloudstorageapi/status_or_val.h"
#include <chrono>
#include <memory>

namespace csa {
/**
 * Describes the configuration for a `cloudstorageapi::CloudStorageClient` object.
 *
 * By default, several environment variables are read to configure the client.
 */
class ClientOptions
{
public:
    explicit ClientOptions(EProvider provider, std::shared_ptr<auth::Credentials> credentials);

    /**
    * Creates a `ClientOptions` with Default Credentials (if any).
    *
    * If default credentials could not be loaded, this returns a
    * `Status` with failure details.
    */
    static StatusOrVal<ClientOptions> CreateDefaultClientOptions(EProvider provider);

    EProvider GetProvider() const { return m_provider; }

    std::shared_ptr<auth::Credentials> GetCredentials() const
    {
        return m_credentials;
    }

    ClientOptions& SetCredentials(std::shared_ptr<auth::Credentials> credentials)
    {
        m_credentials = std::move(credentials);
        return *this;
    }

    bool GetEnableHttpTracing() const { return m_enableHttpTracing; }
    ClientOptions& SetEnableHttpTracing(bool enable)
    {
        m_enableHttpTracing = enable;
        return *this;
    }

    bool GetEnableRawClientTracing() const { return m_enableRawClientTracing; }
    ClientOptions& SetEnableRawClientTracing(bool enable)
    {
        m_enableRawClientTracing = enable;
        return *this;
    }

    std::size_t GetConnectionPoolSize() const { return m_connectionPoolSize; }
    ClientOptions& SetConnectionPoolSize(std::size_t size)
    {
        m_connectionPoolSize = size;
        return *this;
    }

    std::size_t GetDownloadBufferSize() const { return m_downloadBufferSize; }
    ClientOptions& SetDownloadBufferSize(std::size_t size);

    std::size_t GetUploadBufferSize() const { return m_uploadBufferSize; }
    ClientOptions& SetUploadBufferSize(std::size_t size);

    std::string const& GetUserAgentPrefix() const { return m_userAgentPrefix; }
    ClientOptions& AddUserAgentPrefx(std::string const& v)
    {
        std::string prefix = v;
        if (!m_userAgentPrefix.empty()) {
            prefix += '/';
            prefix += m_userAgentPrefix;
        }
        m_userAgentPrefix = std::move(prefix);
        return *this;
    }

    std::size_t GetMaximumSimpleUploadSize() const
    {
        return m_maximumSimpleUploadSize;
    }
    ClientOptions& SetMaximumSimpleUploadSize(std::size_t v)
    {
        m_maximumSimpleUploadSize = v;
        return *this;
    }

    /**
    * If true and using OpenSSL 1.0.2 the library configures the OpenSSL
    * callbacks for locking.
    */
    bool GetEnableSslLockingCallbacks() const
    {
        return m_enableSslLockingCallbacks;
    }
    ClientOptions& SetEnableSslLockingCallbacks(bool v)
    {
        m_enableSslLockingCallbacks = v;
        return *this;
    }

    bool GetEnableSigpipeHandler() const { return m_enableSigpipeHandler; }
    ClientOptions& SetEnableSigpipeHandler(bool v)
    {
        m_enableSigpipeHandler = v;
        return *this;
    }

    std::size_t GetMaximumSocketRecvSize() const
    {
        return m_maximumSocketRecvSize;
    }
    ClientOptions& SetMaximumSocketRecvSize(std::size_t v)
    {
        m_maximumSocketRecvSize = v;
        return *this;
    }

    std::size_t GetMaximumSocketSendSize() const
    {
        return m_maximumSocketSendSize;
    }
    ClientOptions& SetMaximumSocketSendSize(std::size_t v)
    {
        m_maximumSocketSendSize = v;
        return *this;
    }

    /**
    * A download is "stalled" if it receives no data. If this state
    * remains for more than given timeout then the download is aborted.
    *
    * The default value is 2 minutes. Can be disabled by setting the value to 0.
    */
    std::chrono::seconds GetDownloadStallTimeout() const
    {
        return m_downloadStallTimeout;
    }
    ClientOptions& SetDownloadStallTimeout(std::chrono::seconds v)
    {
        m_downloadStallTimeout = v;
        return *this;
    }

 private:

     EProvider m_provider;
     std::shared_ptr<auth::Credentials> m_credentials;
     bool m_enableHttpTracing;
     bool m_enableRawClientTracing;
     std::size_t m_connectionPoolSize;
     std::size_t m_downloadBufferSize;
     std::size_t m_uploadBufferSize;
     std::string m_userAgentPrefix;
     std::size_t m_maximumSimpleUploadSize;
     bool m_enableSslLockingCallbacks = true;
     bool m_enableSigpipeHandler = true;
     std::size_t m_maximumSocketRecvSize = 0;
     std::size_t m_maximumSocketSendSize = 0;
     std::chrono::seconds m_downloadStallTimeout;
};

}  // namespace csa
