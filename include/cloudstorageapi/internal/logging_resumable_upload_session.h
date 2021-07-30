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

#include "cloudstorageapi/internal/resumable_upload_session.h"
#include <memory>

namespace csa {
namespace internal {
/**
 * A decorator for `ResumableUploadSession` that logs each operation.
 */
class LoggingResumableUploadSession : public ResumableUploadSession
{
public:
    explicit LoggingResumableUploadSession(std::unique_ptr<ResumableUploadSession> session)
        : m_session(std::move(session))
    {
    }

    StatusOrVal<ResumableUploadResponse> UploadChunk(std::string const& buffer) override;
    StatusOrVal<ResumableUploadResponse> UploadFinalChunk(std::string const& buffer, std::uint64_t uploadSize) override;
    StatusOrVal<ResumableUploadResponse> ResetSession() override;
    std::uint64_t GetNextExpectedByte() const override;
    std::string const& GetSessionId() const override;
    std::size_t GetFileChunkSizeQuantum() const override;
    StatusOrVal<ResumableUploadResponse> const& GetLastResponse() const override;
    bool Done() const override;

private:
    std::unique_ptr<ResumableUploadSession> m_session;
};

}  // namespace internal
}  // namespace csa
