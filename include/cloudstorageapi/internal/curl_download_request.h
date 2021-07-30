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

#include "cloudstorageapi/internal/curl_request.h"
#include "cloudstorageapi/internal/http_response.h"
#include "cloudstorageapi/internal/object_read_source.h"

namespace csa {
namespace internal {
/**
 * Makes streaming download requests using libcurl.
 *
 * This class manages the resources and workflow to make requests where the
 * payload is streamed, and the total size is not known. Under the hood this
 * uses chunked transfer encoding.
 *
 * @see `CurlRequest` for simpler transfers where the size of the payload is
 *     known and relatively small.
 */
class CurlDownloadRequest : public ObjectReadSource
{
public:
    explicit CurlDownloadRequest();

    ~CurlDownloadRequest() override
    {
        if (!m_factory)
        {
            return;
        }
        m_factory->CleanupHandle(std::move(m_handle.m_handle));
        m_factory->CleanupMultiHandle(std::move(m_multi));
    }

    CurlDownloadRequest(CurlDownloadRequest&& rhs) noexcept(false)
        : m_url(std::move(rhs.m_url)),
          m_headers(std::move(rhs.m_headers)),
          m_payload(std::move(rhs.m_payload)),
          m_userAgent(std::move(rhs.m_userAgent)),
          m_socketOptions(rhs.m_socketOptions),
          m_downloadStallTimeout(rhs.m_downloadStallTimeout),
          m_handle(std::move(rhs.m_handle)),
          m_multi(std::move(rhs.m_multi)),
          m_factory(std::move(rhs.m_factory)),
          m_closing(rhs.m_closing),
          m_curlClosed(rhs.m_curlClosed),
          m_inMulti(rhs.m_inMulti),
          m_paused(rhs.m_paused),
          m_buffer(rhs.m_buffer),
          m_bufferSize(rhs.m_bufferSize),
          m_bufferOffset(rhs.m_bufferOffset),
          m_spill(std::move(rhs.m_spill)),
          m_spillOffset(rhs.m_spillOffset)
    {
        ResetOptions();
    }

    CurlDownloadRequest& operator=(CurlDownloadRequest&& rhs) noexcept
    {
        m_url = std::move(rhs.m_url);
        m_headers = std::move(rhs.m_headers);
        m_payload = std::move(rhs.m_payload);
        m_userAgent = std::move(rhs.m_userAgent);
        m_socketOptions = rhs.m_socketOptions;
        m_downloadStallTimeout = rhs.m_downloadStallTimeout;
        m_handle = std::move(rhs.m_handle);
        m_multi = std::move(rhs.m_multi);
        m_factory = std::move(rhs.m_factory);
        m_closing = rhs.m_closing;
        m_curlClosed = rhs.m_curlClosed;
        m_inMulti = rhs.m_inMulti;
        m_paused = rhs.m_paused;
        m_buffer = rhs.m_buffer;
        m_bufferSize = rhs.m_bufferSize;
        m_bufferOffset = rhs.m_bufferOffset;
        m_spill = std::move(rhs.m_spill);
        m_spillOffset = rhs.m_spillOffset;
        ResetOptions();
        return *this;
    }

    bool IsOpen() const override { return !(m_curlClosed && m_spillOffset == 0); }
    StatusOrVal<HttpResponse> Close() override;

    /**
     * Waits for additional data or the end of the transfer.
     *
     * This operation blocks until `initial_buffer_size` data has been received or
     * the transfer is completed.
     *
     * @param buffer the location to return the new data. Note that the contents
     *     of this parameter are completely replaced with the new data.
     * @returns 100-Continue if the transfer is not yet completed.
     */
    StatusOrVal<ReadSourceResult> Read(char* buf, std::size_t n) override;

private:
    friend class CurlRequestBuilder;
    // Set the underlying CurlHandle options on a new CurlDownloadRequest.
    void SetOptions();

    // Reset the underlying CurlHandle options after a move operation.
    void ResetOptions();

    // Copy any available data from the spill buffer to `m_buffer`
    void DrainSpillBuffer();

    // Called by libcurl to show that more data is available in the download.
    std::size_t WriteCallback(void* ptr, std::size_t size, std::size_t nmemb);

    // Wait until a condition is met.
    template <typename Predicate>
    Status Wait(Predicate predicate);

    // Use libcurl to perform at least part of the transfer.
    StatusOrVal<int> PerformWork();

    // Use libcurl to wait until the underlying data can perform work.
    Status WaitForHandles(int& repeats);

    // Simplify handling of errors in the curl_multi_* API.
    Status AsStatus(CURLMcode result, char const* where);

    std::string m_url;
    CurlHeaders m_headers;
    std::string m_payload;
    std::string m_userAgent;
    CurlReceivedHeaders m_receivedHeaders;
    CurlHandle::SocketOptions m_socketOptions;
    std::chrono::seconds m_downloadStallTimeout;
    CurlHandle m_handle;
    CurlMulti m_multi;
    std::shared_ptr<CurlHandleFactory> m_factory;

    // Explicitly closing the handle happens in two steps.
    // 1. First the application (or higher-level class), calls Close(). This class
    //    needs to notify libcurl that the transfer is terminated by returning 0
    //    from the callback.
    // 2. Once that callback returns 0, this class needs to wait until libcurl
    //    stops using the handle, which happens via PerformWork().
    //
    // Closing also happens automatically when the transfer completes successfully
    // or when the connection is dropped due to some error. In both cases
    // PerformWork() sets the m_curlClosed flags to true.
    //
    // The m_closing flag is set when we enter step 1.
    bool m_closing = false;
    // The m_curlClosed flag is set when we enter step 2, or when the transfer
    // completes.
    bool m_curlClosed = false;

    // Track whether `m_handle` has been added to `m_multi` or not. The exact
    // lifecycle for the handle depends on the libcurl version, and using this
    // flag makes the code less elegant, but less prone to bugs.
    bool m_inMulti = false;

    bool m_paused = false;

    char* m_buffer = nullptr;
    std::size_t m_bufferSize = 0;
    std::size_t m_bufferOffset = 0;

    // libcurl(1) will never pass a block larger than CURL_MAX_WRITE_SIZE to the
    // WriteCallback. However, the callback *must* save all the bytes, returning
    // less bytes read aborts the download (we do that on a Close(), but in
    // general we do not). The application may have requested less bytes in the
    // call to `Read()`, so we need a place to store the additional bytes.
    std::vector<char> m_spill;
    std::size_t m_spillOffset = 0;
};

}  // namespace internal
}  // namespace csa
