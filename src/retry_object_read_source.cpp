// Copyright 2021 Andrew Karasyov
//
// Copyright 2019 Google LLC
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

#include "cloudstorageapi/internal/retry_object_read_source.h"
#include "cloudstorageapi/internal/log.h"
#include <thread>

namespace csa {
namespace internal {

std::uint64_t InitialOffset(OffsetDirection const& offsetDirection, ReadFileRangeRequest const& request)
{
    if (offsetDirection == FromEnd)
    {
        return request.GetOption<ReadLast>().Value();
    }
    return request.GetStartingByte();
}

RetryObjectReadSource::RetryObjectReadSource(std::shared_ptr<RetryClient> client, ReadFileRangeRequest request,
                                             std::unique_ptr<ObjectReadSource> child,
                                             std::unique_ptr<csa::RetryPolicy> retryPolicy,
                                             std::unique_ptr<csa::BackoffPolicy> backoffPolicy)
    : m_client(std::move(client)),
      m_request(std::move(request)),
      m_child(std::move(child)),
      m_retryPolicyPrototype(std::move(retryPolicy)),
      m_backoffPolicyPrototype(std::move(backoffPolicy)),
      m_offsetDirection(m_request.HasOption<ReadLast>() ? FromEnd : FromBeginning),
      m_currentOffset(InitialOffset(m_offsetDirection, m_request))
{
}

StatusOrVal<ReadSourceResult> RetryObjectReadSource::Read(char* buf, std::size_t n)
{
    CSA_LOG_INFO("{}() current_offset={}", __func__, m_currentOffset);
    if (!m_child)
    {
        return Status(StatusCode::FailedPrecondition, "Stream is not open");
    }
    // Refactor code to handle a successful read so we can return early.
    auto handle_result = [this](StatusOrVal<ReadSourceResult> const& r) {
        if (!r)
        {
            return false;
        }
        if (m_offsetDirection == FromEnd)
        {
            m_currentOffset -= r->m_bytesReceived;
        }
        else
        {
            m_currentOffset += r->m_bytesReceived;
        }
        return true;
    };
    // Read some data, if successful return immediately, saving some allocations.
    auto result = m_child->Read(buf, n);
    if (handle_result(result))
    {
        return result;
    }

    // Start a new retry loop to get the data.
    auto backoff_policy = m_backoffPolicyPrototype->Clone();
    auto retry_policy = m_retryPolicyPrototype->Clone();
    int counter = 0;
    for (; !result && retry_policy->OnFailure(result.GetStatus());
         std::this_thread::sleep_for(backoff_policy->OnCompletion()), result = m_child->Read(buf, n))
    {
        // A Read() request failed, most likely that means the connection failed or
        // stalled. The current child might no longer be usable, so we will try to
        // create a new one and replace it. Should that fail, the retry policy would
        // already be exhausted, so we should fail this operation too.
        m_child.reset();

        if (m_offsetDirection == FromEnd)
        {
            m_request.SetOption(ReadLast(m_currentOffset));
        }
        else
        {
            m_request.SetOption(ReadFromOffset(m_currentOffset));
        }
        auto new_child = m_client->ReadFileNotWrapped(m_request, *retry_policy, *backoff_policy);
        if (!new_child)
        {
            // We've exhausted the retry policy while trying to create the child, so
            // return right away.
            return new_child.GetStatus();
        }
        m_child = std::move(*new_child);
    }
    if (handle_result(result))
    {
        return result;
    }
    // We have exhausted the retry policy, return an error.
    auto status = std::move(result).GetStatus();
    std::stringstream os;
    if (internal::StatusTraits::IsPermanentFailure(status))
    {
        os << "Permanent error in Read(): " << status.Message();
    }
    else
    {
        os << "Retry policy exhausted in Read(): " << status.Message();
    }
    return Status(status.Code(), std::move(os).str());
}

}  // namespace internal
}  // namespace csa
