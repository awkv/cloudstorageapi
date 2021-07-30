// Copyright 2020 Andrew Karasyov
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

#include "cloudstorageapi/internal/logging_resumable_upload_session.h"
#include "cloudstorageapi/internal/log.h"
#include <ios>

namespace csa {
namespace internal {

StatusOrVal<ResumableUploadResponse> LoggingResumableUploadSession::UploadChunk(std::string const& buffer)
{
    CSA_LOG_INFO("{}() << {{buffer.size={{{}}}", __func__, buffer.size());
    auto response = m_session->UploadChunk(buffer);
    if (response.Ok())
    {
        CSA_LOG_INFO("{}() >> payload={{{}}}", __func__, response.Value());
    }
    else
    {
        CSA_LOG_INFO("{}() >> status={{{}}}", __func__, response.GetStatus());
    }
    return response;
}

StatusOrVal<ResumableUploadResponse> LoggingResumableUploadSession::UploadFinalChunk(std::string const& buffer,
                                                                                     std::uint64_t uploadSize)
{
    CSA_LOG_INFO("{}() << upload_size={}, buffer.size={}", __func__, uploadSize, buffer.size());
    auto response = m_session->UploadFinalChunk(buffer, uploadSize);
    if (response.Ok())
    {
        CSA_LOG_INFO("{}() >> payload={{{}}}", __func__, response.Value());
    }
    else
    {
        CSA_LOG_INFO("{}() >> status={{{}}}", __func__, response.GetStatus());
    }
    return response;
}

StatusOrVal<ResumableUploadResponse> LoggingResumableUploadSession::ResetSession()
{
    CSA_LOG_INFO("{}() << {{}}", __func__);
    auto response = m_session->ResetSession();
    if (response.Ok())
    {
        CSA_LOG_INFO("{}() >> payload={{{}}}", __func__, response.Value());
    }
    else
    {
        CSA_LOG_INFO("{}() >> status={{{}}}", __func__, response.GetStatus());
    }
    return response;
}

std::uint64_t LoggingResumableUploadSession::GetNextExpectedByte() const
{
    CSA_LOG_INFO("{}() << {{}}", __func__);
    auto response = m_session->GetNextExpectedByte();
    CSA_LOG_INFO("{}() >> {}", __func__, response);
    return response;
}

std::string const& LoggingResumableUploadSession::GetSessionId() const
{
    CSA_LOG_INFO("{}() << {{}}", __func__);
    auto const& response = m_session->GetSessionId();
    CSA_LOG_INFO("{}() >> {}", __func__, response);
    return response;
}

std::size_t LoggingResumableUploadSession::GetFileChunkSizeQuantum() const
{
    return m_session->GetFileChunkSizeQuantum();
}

StatusOrVal<ResumableUploadResponse> const& LoggingResumableUploadSession::GetLastResponse() const
{
    CSA_LOG_INFO("{}() << {{}}", __func__);
    auto const& response = m_session->GetLastResponse();
    if (response.Ok())
    {
        CSA_LOG_INFO("{}() >> payload={{{}}}", __func__, response.Value());
    }
    else
    {
        CSA_LOG_INFO("{}() >> status={{{}}}", __func__, response.GetStatus());
    }
    return response;
}

bool LoggingResumableUploadSession::Done() const { return m_session->Done(); }

}  // namespace internal
}  // namespace csa
