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

#include "cloudstorageapi/internal/curl_request_builder.h"

namespace csa {
namespace internal {

CurlRequestBuilder::CurlRequestBuilder(
    std::string base_url, std::shared_ptr<CurlHandleFactory> factory)
    : m_factory(std::move(factory)),
      m_handle(m_factory->CreateHandle()),
      m_headers(nullptr, &curl_slist_free_all),
      m_url(std::move(base_url)),
      m_queryParameterSeparator("?"),
      m_downloadStallTimeout(0)
{
}

CurlRequest CurlRequestBuilder::BuildRequest()
{
    ValidateBuilderState(__func__);
    CurlRequest request;
    request.m_url = std::move(m_url);
    request.m_headers = std::move(m_headers);
    request.m_userAgent = m_userAgentPrefix + UserAgentSuffix();
    request.m_handle = std::move(m_handle);
    request.m_factory = std::move(m_factory);
    request.m_socketOptions = m_socketOptions;
    request.ResetOptions();
    return request;
}

CurlDownloadRequest CurlRequestBuilder::BuildDownloadRequest(
    std::string payload)
{
    ValidateBuilderState(__func__);
    CurlDownloadRequest request;
    request.m_url = std::move(m_url);
    request.m_headers = std::move(m_headers);
    request.m_userAgent = m_userAgentPrefix + UserAgentSuffix();
    request.m_payload = std::move(payload);
    request.m_handle = std::move(m_handle);
    request.m_multi = m_factory->CreateMultiHandle();
    request.m_factory = m_factory;
    request.m_socketOptions = m_socketOptions;
    request.m_downloadStallTimeout = m_downloadStallTimeout;
    request.SetOptions();
    return request;
}

CurlRequestBuilder& CurlRequestBuilder::ApplyClientOptions(
    ClientOptions const& options)
{
    ValidateBuilderState(__func__);
    m_socketOptions.m_recvBufferSize = options.GetMaximumSocketRecvSize();
    m_socketOptions.m_sendBufferSize = options.GetMaximumSocketSendSize();
    m_userAgentPrefix = options.GetUserAgentPrefix() + m_userAgentPrefix;
    m_downloadStallTimeout = options.GetDownloadStallTimeout();
    return *this;
}

CurlRequestBuilder& CurlRequestBuilder::AddHeader(std::string const& header)
{
    ValidateBuilderState(__func__);
    auto new_header = curl_slist_append(m_headers.get(), header.c_str());
    (void)m_headers.release();
    m_headers.reset(new_header);
    return *this;
}

CurlRequestBuilder& CurlRequestBuilder::AddQueryParameter(
    std::string const& key, std::string const& value)
{
    ValidateBuilderState(__func__);
    std::string parameter = m_queryParameterSeparator;
    parameter += m_handle.MakeEscapedString(key).get();
    parameter += "=";
    parameter += m_handle.MakeEscapedString(value).get();
    m_queryParameterSeparator = "&";
    m_url.append(parameter);
    return *this;
}

CurlRequestBuilder& CurlRequestBuilder::SetMethod(std::string const& method)
{
    ValidateBuilderState(__func__);
    m_handle.SetOption(CURLOPT_CUSTOMREQUEST, method.c_str());
    return *this;
}

CurlRequestBuilder& CurlRequestBuilder::SetCurlShare(CURLSH* share)
{
    m_handle.SetOption(CURLOPT_SHARE, share);
    return *this;
}

std::string CurlRequestBuilder::UserAgentSuffix() const
{
    ValidateBuilderState(__func__);
    // Pre-compute and cache the user agent string:
    static std::string const UserAgentSuffix = [] {
        std::string agent = std::string("csa/") + /*storage::version_string() +*/ "1.0.0 ";
        agent += curl_version();
        return agent;
    }();
    return UserAgentSuffix;
}

void CurlRequestBuilder::ValidateBuilderState(char const* where) const
{
    if (m_handle.m_handle.get() == nullptr)
    {
        std::string msg = "Attempt to use invalidated CurlRequest in ";
        msg += where;
        throw std::runtime_error(msg.c_str());
    }
}
}  // namespace internal
}  // namespace csa
