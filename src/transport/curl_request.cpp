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

#include "cloudstorageapi/internal/curl_request.h"
#include <iostream>

namespace csa {
namespace internal {

extern "C" size_t CurlRequestOnWriteData(char* ptr, size_t size, size_t nmemb, void* userdata)
{
    auto* request = reinterpret_cast<CurlRequest*>(userdata);
    return request->OnWriteData(ptr, size, nmemb);
}

extern "C" size_t CurlRequestOnHeaderData(char* contents, size_t size, size_t nitems, void* userdata)
{
    auto* request = reinterpret_cast<CurlRequest*>(userdata);
    return request->OnHeaderData(contents, size, nitems);
}

class WriteVector
{
public:
    explicit WriteVector(ConstBufferSequence w) : m_writev(std::move(w)) {}

    bool empty() const { return m_writev.empty(); }

    std::size_t OnRead(char* ptr, std::size_t size, std::size_t nitems)
    {
        std::size_t offset = 0;
        auto capacity = size * nitems;
        while (capacity > 0 && !m_writev.empty())
        {
            auto& f = m_writev.front();
            auto n = (std::min)(capacity, m_writev.front().size());
            std::copy(f.data(), f.data() + n, ptr + offset);
            offset += n;
            capacity -= n;
            PopFrontBytes(m_writev, n);
        }
        return offset;
    }

private:
    ConstBufferSequence m_writev;
};

extern "C" std::size_t CurlRequestOnReadData(char* ptr, std::size_t size, std::size_t nitems, void* userdata)
{
    auto* v = reinterpret_cast<WriteVector*>(userdata);
    return v->OnRead(ptr, size, nitems);
}

StatusOrVal<HttpResponse> CurlRequest::MakeRequest(std::string const& payload)
{
    m_handle.SetOption(CURLOPT_UPLOAD, 0L);
    if (!payload.empty())
    {
        m_handle.SetOption(CURLOPT_POSTFIELDSIZE, payload.length());
        m_handle.SetOption(CURLOPT_POSTFIELDS, payload.c_str());
    }
    return MakeRequestImpl();
}

StatusOrVal<HttpResponse> CurlRequest::MakeUploadRequest(ConstBufferSequence payload)
{
    m_handle.SetOption(CURLOPT_UPLOAD, 0L);
    if (payload.empty())
        return MakeRequestImpl();
    if (payload.size() == 1)
    {
        m_handle.SetOption(CURLOPT_POSTFIELDSIZE, payload[0].size());
        m_handle.SetOption(CURLOPT_POSTFIELDS, payload[0].data());
        return MakeRequestImpl();
    }

    WriteVector writev{std::move(payload)};
    m_handle.SetOption(CURLOPT_READFUNCTION, &CurlRequestOnReadData);
    m_handle.SetOption(CURLOPT_READDATA, &writev);
    m_handle.SetOption(CURLOPT_UPLOAD, 1L);
    return MakeRequestImpl();
}

StatusOrVal<HttpResponse> CurlRequest::MakeRequestImpl()
{
    // We get better performance using a slightly larger buffer (128KiB) than the
    // default buffer size set by libcurl (16KiB)
    auto constexpr DefaultBufferSize = 128 * 1024L;

    m_responsePayload.clear();
    m_handle.SetOption(CURLOPT_BUFFERSIZE, DefaultBufferSize);
    m_handle.SetOption(CURLOPT_URL, m_url.c_str());
    m_handle.SetOption(CURLOPT_HTTPHEADER, m_headers.get());
    m_handle.SetOption(CURLOPT_USERAGENT, m_userAgent.c_str());
    m_handle.SetOption(CURLOPT_NOSIGNAL, 1);
    m_handle.SetOption(CURLOPT_TCP_KEEPALIVE, 1L);
    m_handle.EnableLogging(m_loggingEnabled);
    m_handle.SetSocketCallback(m_socketOptions);
    m_handle.SetOptionUnchecked(CURLOPT_HTTP_VERSION, VersionToCurlCode(m_httpVersion));
    m_handle.SetOption(CURLOPT_WRITEFUNCTION, &CurlRequestOnWriteData);
    m_handle.SetOption(CURLOPT_WRITEDATA, this);
    m_handle.SetOption(CURLOPT_HEADERFUNCTION, &CurlRequestOnHeaderData);
    m_handle.SetOption(CURLOPT_HEADERDATA, this);
    auto status = m_handle.EasyPerform();
    if (!status.Ok())
        return status;

    if (m_loggingEnabled)
        m_handle.FlushDebug(__func__);
    auto code = m_handle.GetResponseCode();
    if (!code.Ok())
        return std::move(code).GetStatus();
    return HttpResponse{code.Value(), std::move(m_responsePayload), std::move(m_receivedHeaders)};
}

std::size_t CurlRequest::OnWriteData(char* contents, std::size_t size, std::size_t nmemb)
{
    m_responsePayload.append(contents, size * nmemb);
    return size * nmemb;
}

std::size_t CurlRequest::OnHeaderData(char* contents, std::size_t size, std::size_t nitems)
{
    return CurlAppendHeaderData(m_receivedHeaders, contents, size * nitems);
}

}  // namespace internal
}  // namespace csa
