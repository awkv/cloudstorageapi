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

#include "cloudstorageapi/internal/curl_download_request.h"
#include "cloudstorageapi/internal/curl_wrappers.h"
#include "cloudstorageapi/internal/log.h"
#include <curl/multi.h>
#include <algorithm>
#include <cstring>
#include <sstream>
#include <thread>

namespace csa {
namespace internal {

extern "C" std::size_t CurlDownloadRequestWrite(char* ptr, size_t size, size_t nmemb, void* userdata)
{
    auto* request = reinterpret_cast<CurlDownloadRequest*>(userdata);
    return request->WriteCallback(ptr, size, nmemb);
}

extern "C" std::size_t CurlDownloadRequestHeader(char* contents, std::size_t size, std::size_t nitems, void* userdata)
{
    auto* request = reinterpret_cast<CurlDownloadRequest*>(userdata);
    return request->HeaderCallback(contents, size, nitems);
}

// Note that TRACE-level messages are disabled by default, even in
// CMAKE_BUILD_TYPE=Debug builds. The level of detail created by the
// TRACE_STATE() macro is only needed by the library developers when
// troubleshooting this class.
#define TRACE_STATE()                                                                                             \
    CSA_LOG_TRACE(                                                                                                \
        "{}(), m_bufferSize={}, m_bufferOffset={}, m_spill.size()={}, m_spillOffset={}, closing={}, closed={}, "  \
        "paused={}, in_multi={}",                                                                                 \
        __func__, m_bufferSize, m_bufferOffset, m_spill.size(), m_spillOffset, m_closing, m_curlClosed, m_paused, \
        m_inMulti)

CurlDownloadRequest::CurlDownloadRequest(CurlHeaders headers, CurlHandle handle, CurlMulti multi)
    : m_headers(std::move(headers)), m_downloadStallTimeout(0), m_multi(std::move(multi)), m_spill(CURL_MAX_WRITE_SIZE)
{
}

CurlDownloadRequest::~CurlDownloadRequest()
{
    CleanupHandles();
    if (m_factory)
    {
        m_factory->CleanupHandle(std::move(m_handle));
        m_factory->CleanupMultiHandle(std::move(m_multi));
    }
}

StatusOrVal<HttpResponse> CurlDownloadRequest::Close()
{
    if (m_curlClosed)
        return HttpResponse{m_httpCode, {}, m_receivedHeaders};

    TRACE_STATE();
    // Set the the m_closing flag to trigger a return 0 from the next read
    // callback, see the comments in the header file for more details.
    m_closing = true;
    TRACE_STATE();

    CleanupHandles();

    if (!m_curlClosed)
    {
        // Ignore errors. Except in some really unfortunate cases [*] we are closing
        // the download early. That is done [**] by having the write callback return
        // 0, which always results in libcurl returning `CURLE_WRITE_ERROR`.
        //
        // [*]: the only other case would be the case where a download completes
        //   and the handle is paused because just the right number of bytes
        //   arrived to satisfy the last `Read()` request. In that case ignoring the
        //   errors seems sensible too, the download completed, what is the problem?
        // [**]: this is the recommended practice to shutdown a download early. See
        //   the comments in the header file and elsewhere in this file.
        (void)m_handle.EasyPerform();
        m_curlClosed = true;
        TRACE_STATE();
        OnTransferDone();
    }
    return HttpResponse{m_httpCode, std::string{}, m_receivedHeaders};
}

StatusOrVal<ReadSourceResult> CurlDownloadRequest::Read(char* buf, std::size_t n)
{
    m_buffer = buf;
    m_bufferOffset = 0;
    m_bufferSize = n;
    if (n == 0)
    {
        return Status(StatusCode::InvalidArgument, "Empty buffer for Read()");
    }

    // Before calling `Wait()` copy any data from the spill buffer into the
    // application buffer. It is possible that `Wait()` will never call
    // `WriteCallback()`, for example, because the Read() or Peek() closed the
    // connection, but if there is any data left in the spill buffer we need
    // to return it.
    DrainSpillBuffer();
    if (m_curlClosed)
    {
        return ReadSourceResult{m_bufferOffset, HttpResponse{m_httpCode, std::string{}, std::move(m_receivedHeaders)}};
    }

    m_handle.SetOption(CURLOPT_WRITEFUNCTION, &CurlDownloadRequestWrite);
    m_handle.SetOption(CURLOPT_WRITEDATA, this);
    m_handle.SetOption(CURLOPT_HEADERFUNCTION, &CurlDownloadRequestHeader);
    m_handle.SetOption(CURLOPT_HEADERDATA, this);

    m_handle.FlushDebug(__func__);
    TRACE_STATE();

    if (!m_curlClosed && m_paused)
    {
        m_paused = false;
        auto status = m_handle.EasyPause(CURLPAUSE_RECV_CONT);
        TRACE_STATE();
        CSA_LOG_TRACE(", status={}", status);
        if (!status.Ok())
            return status;
    }

    auto status = Wait([this] { return m_curlClosed || m_paused || m_bufferOffset >= m_bufferSize; });
    if (!status.Ok())
    {
        return status;
    }
    TRACE_STATE();
    CSA_LOG_TRACE(", status={}", status);
    if (!status.Ok())
        return OnTransferError(std::move(status));
    auto bytes_read = m_bufferOffset;
    m_buffer = nullptr;
    m_bufferOffset = 0;
    m_bufferSize = 0;
    if (m_curlClosed)
    {
        OnTransferDone();
        HttpResponse response{m_httpCode, std::string{}, std::move(m_receivedHeaders)};
        status = csa::internal::AsStatus(response);
        TRACE_STATE();
        CSA_LOG_TRACE(", status={}, http code={}", status, response.m_statusCode);

        if (!status.Ok())
            return status;
        return ReadSourceResult{bytes_read, std::move(response)};
    }
    TRACE_STATE();
    CSA_LOG_TRACE(", code=100");
    m_receivedHeaders.emplace(":curl-peer", m_handle.GetPeer());
    return ReadSourceResult{bytes_read, HttpResponse{100, {}, std::move(m_receivedHeaders)}};
}

void CurlDownloadRequest::CleanupHandles()
{
    if (!m_multi != !m_handle.m_handle)
    {
        CSA_LOG_ERROR("handles are inconsistent, multi={}, handle.handle={}", m_multi.get(), m_handle.m_handle.get());
    }
    if (m_curlClosed || !m_multi)
        return;

    if (m_paused)
    {
        m_paused = false;
        (void)m_handle.EasyPause(CURLPAUSE_RECV_CONT);
        TRACE_STATE();
    }

    // Now remove the handle from the CURLM* interface and wait for the response.
    if (m_inMulti)
    {
        (void)curl_multi_remove_handle(m_multi.get(), m_handle.m_handle.get());
        m_inMulti = false;
        TRACE_STATE();
    }
}

void CurlDownloadRequest::SetOptions()
{
    // We get better performance using a slightly larger buffer (128KiB) than the
    // default buffer size set by libcurl (16KiB)
    auto constexpr DefaultBufferSize = 128 * 1024L;

    m_handle.SetOption(CURLOPT_URL, m_url.c_str());
    m_handle.SetOption(CURLOPT_HTTPHEADER, m_headers.get());
    m_handle.SetOption(CURLOPT_USERAGENT, m_userAgent.c_str());
    m_handle.SetOption(CURLOPT_NOSIGNAL, 1L);
    m_handle.SetOption(CURLOPT_NOPROGRESS, 1L);
    m_handle.SetOption(CURLOPT_BUFFERSIZE, DefaultBufferSize);
    if (!m_payload.empty())
    {
        m_handle.SetOption(CURLOPT_POSTFIELDSIZE, m_payload.length());
        m_handle.SetOption(CURLOPT_POSTFIELDS, m_payload.c_str());
    }
    m_handle.EnableLogging(m_loggingEnabled);
    m_handle.SetSocketCallback(m_socketOptions);
    m_handle.SetOptionUnchecked(CURLOPT_HTTP_VERSION, VersionToCurlCode(m_httpVersion));
    if (m_downloadStallTimeout.count() != 0)
    {
        // Timeout if the download receives less than 1 byte/second (i.e.
        // effectively no bytes) for `download_stall_timeout_` seconds.
        m_handle.SetOption(CURLOPT_LOW_SPEED_LIMIT, 1L);
        m_handle.SetOption(CURLOPT_LOW_SPEED_TIME,
                           // NOLINTNEXTLINE(google-runtime-int) - libcurl *requires* `long`
                           static_cast<long>(m_downloadStallTimeout.count()));
    }

    if (m_inMulti)
    {
        CSA_LOG_ERROR("m_inMulti should be false in `SetOptions`");
    }

    auto error = curl_multi_add_handle(m_multi.get(), m_handle.m_handle.get());
    if (error != CURLM_OK)
    {
        // This indicates that we are using the API incorrectly, the application
        // can not recover from these problems, raising an exception is the
        // "Right Thing"[tm] here.
        throw RuntimeStatusError(AsStatus(error, __func__));
    }
    m_inMulti = true;
}

void CurlDownloadRequest::OnTransferDone()
{
    // Retrieve the response code for a closed stream. Note the use of
    // `.Value()`, this is equivalent to: assert(m_httpCode.Ok());
    // The only way the previous call can fail indicates a bug in our code (or
    // corrupted memory), the documentation for CURLINFO_RESPONSE_CODE:
    //   https://curl.haxx.se/libcurl/c/CURLINFO_RESPONSE_CODE.html
    // says:
    //   Returns CURLE_OK if the option is supported, and CURLE_UNKNOWN_OPTION
    //   if not.
    // if the option is not supported then we cannot use HTTP at all in libcurl
    // and the whole library would be unusable.
    m_httpCode = m_handle.GetResponseCode().Value();

    // Capture the peer (the HTTP server), used for troubleshooting.
    m_receivedHeaders.emplace(":curl-peer", m_handle.GetPeer());
    TRACE_STATE();

    // Release the handles back to the factory as soon as possible, so they can be
    // reused for any other requests.
    if (m_factory)
    {
        m_factory->CleanupHandle(std::move(m_handle));
        m_factory->CleanupMultiHandle(std::move(m_multi));
    }
}

Status CurlDownloadRequest::OnTransferError(Status status)
{
    // When there is a transfer error the handle is suspect. It could be pointing
    // to an invalid host, a host that is slow and trickling data, or otherwise in
    // a bad state. Release the handle, but do not return it to the pool.
    CleanupHandles();
    auto handle = std::move(m_handle);
    if (m_factory)
    {
        // While the handle is suspect, there is probably nothing wrong with the
        // CURLM* handle, that just represents a local resource, such as data
        // structures for `epoll(7)` or `select(2)`
        m_factory->CleanupMultiHandle(std::move(m_multi));
    }
    return status;
}

void CurlDownloadRequest::DrainSpillBuffer()
{
    std::size_t free = m_bufferSize - m_bufferOffset;
    auto copy_count = (std::min)(free, m_spillOffset);
    std::copy(m_spill.data(), m_spill.data() + copy_count, m_buffer + m_bufferOffset);
    m_bufferOffset += copy_count;
    std::memmove(m_spill.data(), m_spill.data() + copy_count, m_spill.size() - copy_count);
    m_spillOffset -= copy_count;
}

std::size_t CurlDownloadRequest::WriteCallback(void* ptr, std::size_t size, std::size_t nmemb)
{
    m_handle.FlushDebug(__func__);
    TRACE_STATE();
    CSA_LOG_TRACE(", n={}", size * nmemb);

    // This transfer is closing, just return zero, that will make libcurl finish
    // any pending work, and will return the m_handle pointer from
    // curl_multi_info_read() in PerformWork(). That is the point where
    // `m_curlClosed` is set.
    if (m_closing)
    {
        TRACE_STATE();
        CSA_LOG_TRACE("closing");
        return 0;
    }
    if (m_bufferOffset >= m_bufferSize)
    {
        TRACE_STATE();
        CSA_LOG_TRACE(" *** PAUSING HANDLE ***");
        m_paused = true;
        return CURL_WRITEFUNC_PAUSE;
    }

    // Use the spill buffer first, if there is any...
    DrainSpillBuffer();
    std::size_t free = m_bufferSize - m_bufferOffset;
    if (free == 0)
    {
        TRACE_STATE();
        CSA_LOG_TRACE(" *** PAUSING HANDLE ***");
        m_paused = true;
        return CURL_WRITEFUNC_PAUSE;
    }
    TRACE_STATE();
    CSA_LOG_TRACE(", n={}, free={}", size * nmemb, free);

    // Copy the full contents of `ptr` into the application buffer.
    if (size * nmemb < free)
    {
        std::memcpy(m_buffer + m_bufferOffset, ptr, size * nmemb);
        m_bufferOffset += size * nmemb;
        TRACE_STATE();
        CSA_LOG_TRACE(", n={}", size * nmemb);
        return size * nmemb;
    }
    // Copy as much as possible from `ptr` into the application buffer.
    std::memcpy(m_buffer + m_bufferOffset, ptr, free);
    m_bufferOffset += free;
    m_spillOffset = size * nmemb - free;
    // The rest goes into the spill buffer.
    std::memcpy(m_spill.data(), static_cast<char*>(ptr) + free, m_spillOffset);
    TRACE_STATE();
    CSA_LOG_TRACE(", n={}, free={}", size * nmemb, free);
    return size * nmemb;
}

std::size_t CurlDownloadRequest::HeaderCallback(char* contents, std::size_t size, std::size_t nitems)
{
    return CurlAppendHeaderData(m_receivedHeaders, contents, size * nitems);
}

template <typename Predicate>
Status CurlDownloadRequest::Wait(Predicate predicate)
{
    int repeats = 0;
    // We can assert that the current thread is the leader, because the
    // predicate is satisfied, and the condition variable exited. Therefore,
    // this thread must run the I/O event loop.
    while (!predicate())
    {
        m_handle.FlushDebug(__func__);
        TRACE_STATE();
        CSA_LOG_TRACE(", repeats={}", repeats);
        auto running_handles = PerformWork();
        if (!running_handles.Ok())
        {
            return std::move(running_handles).GetStatus();
        }
        // Only wait if there are CURL handles with pending work *and* the
        // predicate is not satisfied. Note that if the predicate is ill-defined
        // it might continue to be unsatisfied even though the handles have
        // completed their work.
        if (*running_handles == 0 || predicate())
        {
            break;
        }
        auto status = WaitForHandles(repeats);
        if (!status.Ok())
        {
            return status;
        }
    }
    return Status();
}

StatusOrVal<int> CurlDownloadRequest::PerformWork()
{
    TRACE_STATE();
    if (!m_inMulti)
    {
        return 0;
    }

    // Block while there is work to do, apparently newer versions of libcurl do
    // not need this loop and curl_multi_perform() blocks until there is no more
    // work, but is it pretty harmless to keep here.
    int running_handles = 0;
    CURLMcode result;
    do
    {
        result = curl_multi_perform(m_multi.get(), &running_handles);
    } while (result == CURLM_CALL_MULTI_PERFORM);

    // Throw an exception if the result is unexpected, otherwise return.
    auto status = AsStatus(result, __func__);
    if (!status.Ok())
    {
        TRACE_STATE();
        CSA_LOG_TRACE(", status={}", status);
        return status;
    }
    if (running_handles == 0)
    {
        // The only way we get here is if the handle "completed", and therefore the
        // transfer either failed or was successful. Pull all the messages out of
        // the info queue until we get the message about our handle.
        int remaining;
        while (auto* msg = curl_multi_info_read(m_multi.get(), &remaining))
        {
            if (msg->easy_handle != m_handle.m_handle.get())
            {
                // Return an error if this is the wrong handle. This should never
                // happen, if it does we are using the libcurl API incorrectly. But it
                // is better to give a meaningful error message in this case.
                std::ostringstream os;
                os << __func__ << " unknown handle returned by curl_multi_info_read()"
                   << ", msg.msg=[" << msg->msg << "]"
                   << ", result=[" << msg->data.result << "]=" << curl_easy_strerror(msg->data.result);
                return Status(StatusCode::Unknown, std::move(os).str());
            }
            status = CurlHandle::AsStatus(msg->data.result, __func__);
            TRACE_STATE();
            CSA_LOG_TRACE(", status={}, remaining={}, running_handles={}", status, remaining, running_handles);
            // Whatever the status is, the transfer is done, we need to remove it
            // from the CURLM* interface.
            m_curlClosed = true;
            Status multi_remove_status;
            if (m_inMulti)
            {
                // In the extremely unlikely case that removing the handle from CURLM*
                // was an error, return that as a status.
                multi_remove_status =
                    AsStatus(curl_multi_remove_handle(m_multi.get(), m_handle.m_handle.get()), __func__);
                m_inMulti = false;
            }

            TRACE_STATE();
            CSA_LOG_TRACE(", status={}, remaining={}, running_handles={}, multi_remove_status={}", status, remaining,
                          running_handles, multi_remove_status);

            // Ignore errors when closing the handle. They are expected because
            // libcurl may have received a block of data, but the WriteCallback()
            // (see above) tells libcurl that it cannot receive more data.
            if (m_closing)
            {
                continue;
            }
            if (!status.Ok())
            {
                return status;
            }
            if (!multi_remove_status.Ok())
            {
                return multi_remove_status;
            }
        }
    }
    TRACE_STATE();
    CSA_LOG_TRACE(", running_handles={}", running_handles);
    return running_handles;
}

Status CurlDownloadRequest::WaitForHandles(int& repeats)
{
    int const timeout_ms = 1;
    std::chrono::milliseconds const timeout(timeout_ms);
    int numfds = 0;
    CURLMcode result = curl_multi_wait(m_multi.get(), nullptr, 0, timeout_ms, &numfds);
    TRACE_STATE();
    CSA_LOG_TRACE(", numfds={}, result={}, repeats={}", numfds, result, repeats);
    Status status = AsStatus(result, __func__);
    if (!status.Ok())
    {
        return status;
    }
    // The documentation for curl_multi_wait() recommends sleeping if it returns
    // numfds == 0 more than once in a row :shrug:
    //    https://curl.haxx.se/libcurl/c/curl_multi_wait.html
    if (numfds == 0)
    {
        if (++repeats > 1)
        {
            std::this_thread::sleep_for(timeout);
        }
    }
    else
    {
        repeats = 0;
    }
    return status;
}

Status CurlDownloadRequest::AsStatus(CURLMcode result, char const* where)
{
    if (result == CURLM_OK)
    {
        return Status();
    }
    std::ostringstream os;
    os << where << "(): unexpected error code in curl_multi_*, [" << result << "]=" << curl_multi_strerror(result);
    return Status(StatusCode::Unknown, std::move(os).str());
}

}  // namespace internal
}  // namespace csa
