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

#include "cloudstorageapi/internal/retry_resumable_upload_session.h"
#include <sstream>
#include <thread>

namespace csa {
namespace internal {

namespace {
StatusOrVal<ResumableUploadResponse> ReturnError(Status&& lastStatus, RetryPolicy const& retryPolicy,
                                                 char const* error_message)
{
    std::ostringstream os;
    if (retryPolicy.IsExhausted())
    {
        os << "Retry policy exhausted in " << error_message << ": " << lastStatus.Message();
    }
    else
    {
        os << "Permanent error in " << error_message << ": " << lastStatus.Message();
    }
    return Status(lastStatus.Code(), std::move(os).str());
}
}  // namespace

StatusOrVal<ResumableUploadResponse> RetryResumableUploadSession::UploadChunk(ConstBufferSequence const& buffers)
{
    return UploadGenericChunk(__func__, buffers,
                              [this](ConstBufferSequence const& b) { return m_session->UploadChunk(b); });
}

StatusOrVal<ResumableUploadResponse> RetryResumableUploadSession::UploadFinalChunk(ConstBufferSequence const& buffers,
                                                                                   std::uint64_t uploadSize)
{
    return UploadGenericChunk(__func__, buffers, [this, uploadSize](ConstBufferSequence const& b) {
        return m_session->UploadFinalChunk(b, uploadSize);
    });
}

StatusOrVal<ResumableUploadResponse> RetryResumableUploadSession::UploadGenericChunk(char const* caller,
                                                                                     ConstBufferSequence buffers,
                                                                                     UploadChunkFunction const& upload)
{
    std::uint64_t nextByte = m_session->GetNextExpectedByte();
    Status lastStatus(StatusCode::DeadlineExceeded, "Retry policy exhausted before first attempt was made.");
    // On occasion, we might need to retry uploading only a part of the buffer.
    // The current APIs require us to copy the buffer in such a scenario. We can
    // and want to avoid the copy in the common case, so we use this variable to
    // either reference the copy or the original buffer.

    auto retryPolicy = m_retryPolicyPrototype->Clone();
    auto backoffPolicy = m_backoffPolicyPrototype->Clone();
    while (!retryPolicy->IsExhausted())
    {
        std::uint64_t newNextByte = m_session->GetNextExpectedByte();
        if (newNextByte < nextByte)
        {
            std::stringstream os;
            os << caller << ": server previously confirmed " << nextByte
               << " bytes as committed, but the current response only reports " << newNextByte << " bytes as committed."
               << " This is most likely a bug in the CSA client library, possibly"
               << " related to parsing the server response."
               << " Please report it at"
               << " https://github.com/awkv/cloudstorageapi/issues/new"
               << "    Include as much information as you can including this message";
            auto const& lastResponse = m_session->GetLastResponse();
            if (!lastResponse)
            {
                os << ", lastResponse.status=" << lastResponse.GetStatus();
            }
            else
            {
                os << ", lastResponse.value=" << lastResponse.Value();
            }
            return Status(StatusCode::Internal, os.str());
        }
        if (newNextByte > nextByte)
        {
            PopFrontBytes(buffers, static_cast<std::size_t>(newNextByte - nextByte));
            nextByte = newNextByte;
        }
        auto result = upload(buffers);
        if (result.Ok())
        {
            if (result->m_uploadState == ResumableUploadResponse::UploadState::Done)
            {
                // The upload was completed. This can happen even if
                // `is_final_chunk == false`, for example, if the application includes
                // the X-Upload-Content-Length` header, which allows the server to
                // detect a completed upload "early".
                return result;
            }
            auto currentNextExpectedByte = GetNextExpectedByte();
            auto const totalBytes = TotalBytes(buffers);
            if (currentNextExpectedByte - nextByte == totalBytes)
            {
                // Otherwise, return only if there were no failures and it wasn't a
                // short write.
                return result;
            }
            std::stringstream os;
            os << "Short write. Previous nextByte=" << nextByte << ", current nextByte=" << currentNextExpectedByte
               << ", intended to write=" << totalBytes << ", wrote=" << currentNextExpectedByte - nextByte;
            lastStatus = Status(StatusCode::Unavailable, os.str());
            // Don't reset the session on a short write nor wait according to the
            // backoff policy - we did get a response from the server after all.
            continue;
        }
        lastStatus = std::move(result).GetStatus();
        if (!retryPolicy->OnFailure(lastStatus))
        {
            return ReturnError(std::move(lastStatus), *retryPolicy, __func__);
        }
        auto delay = backoffPolicy->OnCompletion();
        std::this_thread::sleep_for(delay);

        result = ResetSession(*retryPolicy, *backoffPolicy, std::move(lastStatus));
        if (!result.Ok())
        {
            return result;
        }
        lastStatus = Status();
    }
    std::ostringstream os;
    os << "Retry policy exhausted in " << caller << ": " << lastStatus.Message();
    return Status(lastStatus.Code(), std::move(os).str());
}

StatusOrVal<ResumableUploadResponse> RetryResumableUploadSession::ResetSession(csa::RetryPolicy& retryPolicy,
                                                                               csa::BackoffPolicy& backoffPolicy,
                                                                               Status lastStatus)
{
    while (!retryPolicy.IsExhausted())
    {
        auto result = m_session->ResetSession();
        if (result.Ok())
        {
            return result;
        }
        lastStatus = std::move(result).GetStatus();
        if (!retryPolicy.OnFailure(lastStatus))
        {
            return ReturnError(std::move(lastStatus), retryPolicy, __func__);
        }
        auto delay = backoffPolicy.OnCompletion();
        std::this_thread::sleep_for(delay);
    }
    std::ostringstream os;
    os << "Retry policy exhausted in " << __func__ << ": " << lastStatus.Message();
    return Status(lastStatus.Code(), std::move(os).str());
}

StatusOrVal<ResumableUploadResponse> RetryResumableUploadSession::ResetSession()
{
    Status lastStatus(StatusCode::DeadlineExceeded, "Retry policy exhausted before first attempt was made.");
    auto retryPolicy = m_retryPolicyPrototype->Clone();
    auto backoffPolicy = m_backoffPolicyPrototype->Clone();
    return ResetSession(*retryPolicy, *backoffPolicy, std::move(lastStatus));
}

std::uint64_t RetryResumableUploadSession::GetNextExpectedByte() const { return m_session->GetNextExpectedByte(); }

std::string const& RetryResumableUploadSession::GetSessionId() const { return m_session->GetSessionId(); }

std::size_t RetryResumableUploadSession::GetFileChunkSizeQuantum() const
{
    return m_session->GetFileChunkSizeQuantum();
}

bool RetryResumableUploadSession::Done() const { return m_session->Done(); }

StatusOrVal<ResumableUploadResponse> const& RetryResumableUploadSession::GetLastResponse() const
{
    return m_session->GetLastResponse();
}

}  // namespace internal
}  // namespace csa
