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

#include "cloudstorageapi/internal/curl_resumable_upload_session.h"

namespace csa {
namespace internal {

StatusOrVal<ResumableUploadResponse> CurlResumableUploadSession::UploadChunk(ConstBufferSequence const& buffers)
{
    UploadChunkRequest request(m_sessionId, m_nextExpected, buffers);
    auto result = m_client->UploadChunk(request);
    Update(result, TotalBytes(buffers));
    return result;
}

StatusOrVal<ResumableUploadResponse> CurlResumableUploadSession::UploadFinalChunk(ConstBufferSequence const& buffers,
                                                                                  std::uint64_t upload_size)
{
    UploadChunkRequest request(m_sessionId, m_nextExpected, buffers, upload_size);
    request.SetOption(GetCustomHeader());
    auto result = m_client->UploadChunk(request);
    Update(result, TotalBytes(buffers));
    return result;
}

StatusOrVal<ResumableUploadResponse> CurlResumableUploadSession::ResetSession()
{
    QueryResumableUploadRequest request(m_sessionId);
    auto result = m_client->QueryResumableUpload(request);
    Update(result, 0);
    return result;
}

std::uint64_t CurlResumableUploadSession::GetNextExpectedByte() const { return m_nextExpected; }

void CurlResumableUploadSession::Update(StatusOrVal<ResumableUploadResponse> const& result, std::size_t chunkSize)
{
    m_lastResponse = result;
    if (!result.Ok())
    {
        return;
    }
    m_done = result->m_uploadState == ResumableUploadResponse::UploadState::Done;
    if (m_done)
    {
        // Sometimes (e.g. when the user sets the X-Upload-Content-Length header)
        // the upload completes but does *not* include a `last_committed_byte`
        // value. In this case we update the next expected byte using the chunk
        // size, as we know the upload was successful.
        m_nextExpected += chunkSize;
    }
    else if (result->m_lastCommittedByte != 0)
    {
        m_nextExpected = result->m_lastCommittedByte + 1;
    }
    else
    {
        // Nothing has been committed on the server side yet, keep resending.
        m_nextExpected = 0;
    }
    if (m_sessionId.empty() && !result->m_uploadSessionUrl.empty())
    {
        m_sessionId = result->m_uploadSessionUrl;
    }
}

}  // namespace internal
}  // namespace csa
