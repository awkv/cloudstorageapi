// Copyright 2021 Andrew Karasyov
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

#include "cloudstorageapi/internal/resumable_upload_session.h"
#include "cloudstorageapi/retry_policy.h"
#include <functional>
#include <memory>
#include <string>

namespace csa {
namespace internal {
/**
 * Decorates a `ResumableUploadSession` to retry operations that fail.
 *
 * Note that to retry some operations the session may need to query the current
 * upload status.
 */
class RetryResumableUploadSession : public ResumableUploadSession
{
public:
    explicit RetryResumableUploadSession(std::unique_ptr<ResumableUploadSession> session,
                                         std::unique_ptr<csa::RetryPolicy> retryPolicy,
                                         std::unique_ptr<csa::BackoffPolicy> backoffPolicy)
        : m_session(std::move(session)),
          m_retryPolicyPrototype(std::move(retryPolicy)),
          m_backoffPolicyPrototype(std::move(backoffPolicy))
    {
    }

    StatusOrVal<ResumableUploadResponse> UploadChunk(ConstBufferSequence const& buffers) override;
    StatusOrVal<ResumableUploadResponse> UploadFinalChunk(ConstBufferSequence const& buffers,
                                                          std::uint64_t uploadSize) override;
    StatusOrVal<ResumableUploadResponse> ResetSession() override;
    std::uint64_t GetNextExpectedByte() const override;
    std::string const& GetSessionId() const override;
    std::size_t GetFileChunkSizeQuantum() const override;
    bool Done() const override;
    StatusOrVal<ResumableUploadResponse> const& GetLastResponse() const override;

private:
    using UploadChunkFunction = std::function<StatusOrVal<ResumableUploadResponse>(ConstBufferSequence const&)>;
    // Retry either UploadChunk or either UploadFinalChunk. Note that we need a
    // copy of the buffers because on some retries they need to be modified.
    StatusOrVal<ResumableUploadResponse> UploadGenericChunk(char const* caller, ConstBufferSequence buffers,
                                                            UploadChunkFunction const& upload);

    // Reset the current session using previously cloned policies.
    StatusOrVal<ResumableUploadResponse> ResetSession(csa::RetryPolicy& retryPolicy, csa::BackoffPolicy& backoffPolicy,
                                                      Status lastStatus);

    std::unique_ptr<ResumableUploadSession> m_session;
    std::unique_ptr<csa::RetryPolicy const> m_retryPolicyPrototype;
    std::unique_ptr<csa::BackoffPolicy const> m_backoffPolicyPrototype;
};

}  // namespace internal
}  // namespace csa
