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

#include "cloudstorageapi/internal/complex_option.h"
#include "cloudstorageapi/internal/curl_download_request.h"
#include "cloudstorageapi/internal/curl_handle_factory.h"
#include "cloudstorageapi/internal/curl_request.h"
#include "cloudstorageapi/well_known_headers.h"
#include "cloudstorageapi/well_known_parameters.h"

namespace csa {
namespace internal {
/**
 * Implements the Builder pattern for CurlRequest.
 */
class CurlRequestBuilder
{
public:
    using RequestType = CurlRequest;

    explicit CurlRequestBuilder(std::string base_url, std::shared_ptr<CurlHandleFactory> factory);

    /**
     * Creates a http request with the given payload.
     *
     * This function invalidates the builder. The application should not use this
     * builder once this function is called.
     */
    CurlRequest BuildRequest();

    /**
     * Creates a non-blocking http request.
     *
     * This function invalidates the builder. The application should not use this
     * builder once this function is called.
     */
    CurlDownloadRequest BuildDownloadRequest(std::string payload);

    // Adds one of the well-known parameters as a query parameter
    template <typename P>
    CurlRequestBuilder& AddOption(WellKnownParameter<P, std::string> const& p)
    {
        if (p.HasValue())
        {
            AddQueryParameter(p.ParameterName(), p.value());
        }
        return *this;
    }

    // Adds one of the well-known parameters as a query parameter
    template <typename P>
    CurlRequestBuilder& AddOption(WellKnownParameter<P, std::int64_t> const& p)
    {
        if (p.HasValue())
        {
            AddQueryParameter(p.ParameterName(), std::to_string(p.value()));
        }
        return *this;
    }

    /// Adds one of the well-known parameters as a query parameter
    template <typename P>
    CurlRequestBuilder& AddOption(WellKnownParameter<P, bool> const& p)
    {
        if (!p.HasValue())
        {
            return *this;
        }
        AddQueryParameter(p.ParameterName(), p.value() ? "true" : "false");
        return *this;
    }

    // Adds one of the well-known headers to the request.
    template <typename P>
    CurlRequestBuilder& AddOption(WellKnownHeader<P, std::string> const& p)
    {
        if (p.HasValue())
        {
            AddHeader(std::string(p.header_name()) + ": " + p.value());
        }
        return *this;
    }

    // Adds a custom header to the request.
    CurlRequestBuilder& AddOption(CustomHeader const& p)
    {
        if (p.HasValue())
        {
            AddHeader(p.CustomHeaderName() + ": " + p.value());
        }
        return *this;
    }

    /**
     * Ignore complex options, these are managed explicitly in the requests that
     * use them.
     */
    template <typename Option, typename T>
    CurlRequestBuilder& AddOption(ComplexOption<Option, T> const&)
    {
        // Ignore other options.
        return *this;
    }

    // Adds request headers.
    CurlRequestBuilder& AddHeader(std::string const& header);

    // Adds a parameter for a request.
    CurlRequestBuilder& AddQueryParameter(std::string const& key, std::string const& value);

    // Changes the http method used for this request.
    CurlRequestBuilder& SetMethod(std::string const& method);

    // Copy interesting configuration parameters from the client options.
    CurlRequestBuilder& ApplyClientOptions(ClientOptions const& options);

    // Sets the CURLSH* handle to share resources.
    CurlRequestBuilder& SetCurlShare(CURLSH* share);

    // Gets the user-agent suffix.
    std::string UserAgentSuffix() const;

    // URL-escapes a string.
    CurlString MakeEscapedString(std::string const& s) { return m_handle.MakeEscapedString(s); }

    // Get the last local IP address from the factory.
    std::string LastClientIpAddress() const { return m_factory->LastClientIpAddress(); }

private:
    void ValidateBuilderState(char const* where) const;

    std::shared_ptr<CurlHandleFactory> m_factory;

    CurlHandle m_handle;
    CurlHeaders m_headers;

    std::string m_url;
    char const* m_queryParameterSeparator;

    std::string m_userAgentPrefix;
    CurlHandle::SocketOptions m_socketOptions;
    std::chrono::seconds m_downloadStallTimeout;
};

}  // namespace internal
}  // namespace csa
