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

#include "cloudstorageapi/internal/const_buffer.h"
#include "cloudstorageapi/internal/curl_handle.h"
#include "cloudstorageapi/internal/curl_handle_factory.h"
#include "cloudstorageapi/internal/http_response.h"

namespace csa {
namespace internal {

extern "C" size_t CurlRequestOnWriteData(char*, size_t, size_t, void*);
extern "C" size_t CurlRequestOnHeaderData(char*, size_t, size_t, void*);

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
    CurlRequest() = default;

    ~CurlRequest()
    {
        if (m_factory)
        {
            m_factory->CleanupHandle(std::move(m_handle));
        }
    }

    CurlRequest(CurlRequest&&) = default;
    CurlRequest& operator=(CurlRequest&& rhs) = default;

    /**
     * Makes the prepared request.
     * This function can be called multiple times on the same request.
     * @return The response HTTP error code and the response payload.
     */
    StatusOrVal<HttpResponse> MakeRequest(std::string const& payload);

    /** Overloaded function */
    StatusOrVal<HttpResponse> MakeUploadRequest(ConstBufferSequence payload);

private:
    StatusOrVal<HttpResponse> MakeRequestImpl();

    friend class CurlRequestBuilder;
    friend size_t CurlRequestOnWriteData(char*, size_t, size_t, void*);
    friend size_t CurlRequestOnHeaderData(char*, size_t, size_t, void*);

    std::size_t OnWriteData(char* contents, std::size_t size, std::size_t nmemb);
    std::size_t OnHeaderData(char* contents, std::size_t size, std::size_t nitems);

    std::string m_url;
    CurlHeaders m_headers = CurlHeaders(nullptr, &curl_slist_free_all);
    std::string m_userAgent;
    std::string m_httpVersion;
    std::string m_responsePayload;
    CurlReceivedHeaders m_receivedHeaders;
    bool m_loggingEnabled = false;
    CurlHandle::SocketOptions m_socketOptions;
    CurlHandle m_handle;
    std::shared_ptr<CurlHandleFactory> m_factory;
};

}  // namespace internal
}  // namespace csa
