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
#include "cloudstorageapi/internal/curl_handle_factory.h"
#include "cloudstorageapi/internal/http_response.h"

namespace csa {
namespace internal {
/**
 * Makes RPC-like requests using CURL.
 *
 * The cloudstorageapi library is using libcurl to make http requests,
 * this class manages the resources and workflow to make a simple RPC-like
 * request.
 */
class CurlRequest
{
public:
    CurlRequest();

    ~CurlRequest()
    {
        if (!m_factory)
        {
            return;
        }
        m_factory->CleanupHandle(std::move(m_handle.m_handle));
    }

    CurlRequest(CurlRequest&& rhs) noexcept(false)
        : m_url(std::move(rhs.m_url)),
          m_headers(std::move(rhs.m_headers)),
          m_userAgent(std::move(rhs.m_userAgent)),
          m_responsePayload(std::move(rhs.m_responsePayload)),
          m_receivedHeaders(std::move(rhs.m_receivedHeaders)),
          m_socketOptions(rhs.m_socketOptions),
          m_handle(std::move(rhs.m_handle)),
          m_factory(std::move(rhs.m_factory))
    {
        ResetOptions();
    }

    CurlRequest& operator=(CurlRequest&& rhs) noexcept(false)
    {
        m_url = std::move(rhs.m_url);
        m_headers = std::move(rhs.m_headers);
        m_userAgent = std::move(rhs.m_userAgent);
        m_responsePayload = std::move(rhs.m_responsePayload);
        m_receivedHeaders = std::move(rhs.m_receivedHeaders);
        m_socketOptions = rhs.m_socketOptions;
        m_handle = std::move(rhs.m_handle);
        m_factory = std::move(rhs.m_factory);

        ResetOptions();
        return *this;
    }

    /**
     * Makes the prepared request.
     * This function can be called multiple times on the same request.
     * @return The response HTTP error code and the response payload.
     * @throw std::runtime_error if the request cannot be made at all.
     */
    StatusOrVal<HttpResponse> MakeRequest(std::string const& payload);

private:
    friend class CurlRequestBuilder;
    void ResetOptions();

    std::string m_url;
    CurlHeaders m_headers;
    std::string m_userAgent;
    std::string m_responsePayload;
    CurlReceivedHeaders m_receivedHeaders;
    CurlHandle::SocketOptions m_socketOptions;
    CurlHandle m_handle;
    std::shared_ptr<CurlHandleFactory> m_factory;
};

}  // namespace internal
}  // namespace csa
