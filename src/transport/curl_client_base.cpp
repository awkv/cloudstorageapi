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

#include "cloudstorageapi/internal/curl_client_base.h"
#include "cloudstorageapi/internal/curl_handle.h"
#include "cloudstorageapi/internal/curl_request_builder.h"
#include "cloudstorageapi/terminate_handler.h"
#include <sstream>

namespace csa {
namespace internal {
namespace {

std::shared_ptr<CurlHandleFactory> CreateHandleFactory(Options const& options)
{
    auto const poolSize = options.Get<csa::ConnectionPoolSizeOption>();
    if (poolSize == 0)
    {
        return std::make_shared<DefaultCurlHandleFactory>();
    }
    return std::make_shared<PooledCurlHandleFactory>(poolSize);
}

std::string UrlEscapeString(std::string const& value)
{
    CurlHandle handle;
    return std::string(handle.MakeEscapedString(value).get());
}

}  // namespace

CurlClientBase::CurlClientBase(Options options)
    : m_options(std::move(options)),
      m_generator(csa::internal::MakeDefaultPRNG()),
      m_storageFactory(CreateHandleFactory(m_options)),
      m_uploadFactory(CreateHandleFactory(m_options))
{
    CurlInitializeOnce(options);
}

Status CurlClientBase::SetupBuilderCommon(CurlRequestBuilder& builder, char const* method)
{
    auto authHeader = m_options.Get<Oauth2CredentialsOption>()->AuthorizationHeader();
    if (!authHeader.Ok())
    {
        return std::move(authHeader).GetStatus();
    }
    builder.SetMethod(method).ApplyClientOptions(m_options).AddHeader(authHeader.Value());
    return Status();
}

}  // namespace internal
}  // namespace csa
