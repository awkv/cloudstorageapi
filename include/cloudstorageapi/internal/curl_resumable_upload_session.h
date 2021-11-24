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

#pragma once

#include "cloudstorageapi/internal/curl_client_base.h"
#include "cloudstorageapi/internal/resumable_upload_session.h"

namespace csa {
namespace internal {
/**
 * Implement a ResumableUploadSession that delegates to a CurlClient.
 */
class CurlResumableUploadSession : public ResumableUploadSession
{
public:
    explicit CurlResumableUploadSession(std::shared_ptr<CurlClientBase> client, std::string sessionId,
                                        CustomHeader customHeader = CustomHeader())
        : m_client(std::move(client)), m_sessionId(std::move(sessionId)), m_customHeader(std::move(customHeader))
    {
    }

    StatusOrVal<ResumableUploadResponse> UploadChunk(ConstBufferSequence const& buffers) override;

    StatusOrVal<ResumableUploadResponse> UploadFinalChunk(ConstBufferSequence const& buffers,
                                                          std::uint64_t upload_size) override;

    StatusOrVal<ResumableUploadResponse> ResetSession() override;

    std::uint64_t GetNextExpectedByte() const override;

    std::string const& GetSessionId() const override { return m_sessionId; }

    CustomHeader const& GetCustomHeader() const { return m_customHeader; }

    std::size_t GetFileChunkSizeQuantum() const { return m_client->GetFileChunkQuantum(); }

    bool Done() const override { return m_done; }

    StatusOrVal<ResumableUploadResponse> const& GetLastResponse() const override { return m_lastResponse; }

private:
    void Update(StatusOrVal<ResumableUploadResponse> const& result, std::size_t chunkSize);

    std::shared_ptr<CurlClientBase> m_client;
    std::string m_sessionId;
    CustomHeader m_customHeader;
    std::uint64_t m_nextExpected = 0;
    bool m_done = false;
    StatusOrVal<ResumableUploadResponse> m_lastResponse;
};

}  // namespace internal
}  // namespace csa
