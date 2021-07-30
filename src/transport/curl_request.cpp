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

CurlRequest::CurlRequest() : m_headers(nullptr, &curl_slist_free_all) {}

StatusOrVal<HttpResponse> CurlRequest::MakeRequest(std::string const& payload)
{
    if (!payload.empty())
    {
        m_handle.SetOption(CURLOPT_POSTFIELDSIZE, payload.length());
        m_handle.SetOption(CURLOPT_POSTFIELDS, payload.c_str());
    }
    auto status = m_handle.EasyPerform();
    if (!status.Ok())
    {
        return status;
    }
    m_handle.FlushDebug(__func__);
    auto code = m_handle.GetResponseCode();
    if (!code.Ok())
    {
        return std::move(code).GetStatus();
    }
    return HttpResponse{code.Value(), std::move(m_responsePayload), std::move(m_receivedHeaders)};
}

void CurlRequest::ResetOptions()
{
    m_handle.SetOption(CURLOPT_URL, m_url.c_str());
    m_handle.SetOption(CURLOPT_HTTPHEADER, m_headers.get());
    m_handle.SetOption(CURLOPT_USERAGENT, m_userAgent.c_str());
    m_handle.SetOption(CURLOPT_NOSIGNAL, 1);
    m_handle.SetWriterCallback([this](void* ptr, std::size_t size, std::size_t nmemb) {
        m_responsePayload.append(static_cast<char*>(ptr), size * nmemb);
        return size * nmemb;
    });
    m_handle.SetHeaderCallback([this](char* contents, std::size_t size, std::size_t nitems) {
        return CurlAppendHeaderData(m_receivedHeaders, static_cast<char const*>(contents), size * nitems);
    });
    m_handle.SetSocketCallback(m_socketOptions);
}

}  // namespace internal
}  // namespace csa
