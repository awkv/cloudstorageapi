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

#include "cloudstorageapi/internal/retry_client.h"
#include "cloudstorageapi/client_options.h"
#include "cloudstorageapi/internal/raw_client_wrapper_utils.h"
#include "cloudstorageapi/internal/retry_object_read_source.h"
#include "cloudstorageapi/internal/retry_policy_internal.h"
#include "cloudstorageapi/internal/retry_resumable_upload_session.h"
#include <sstream>
#include <thread>

namespace csa {
namespace internal {
namespace {

/**
 * Calls a client operation with retries borrowing the RPC policies.
 *
 * @tparam MemberFunction the signature of the member function.
 * @param client the storage::Client object to make the call through.
 * @param retryPolicy the policy controlling what failures are retryable, and
 *     for how long we can retry
 * @param backoffPolicy the policy controlling how long to wait before
 *     retrying.
 * @param function the pointer to the member function to call.
 * @param request an initialized request parameter for the call.
 * @param errorMessage include this message in any exception or error log.
 * @return the result from making the call;
 * @throw std::exception with a description of the last error.
 */
template <typename F>
std::invoke_result<F>::type MakeCall(csa::RetryPolicy& retryPolicy, csa::BackoffPolicy& backoffPolicy, F function,
                                     char const* errorMessage)
{
    Status lastStatus(StatusCode::DeadlineExceeded, "Retry policy exhausted before first attempt was made.");
    auto error = [&lastStatus](std::string const& msg) { return Status(lastStatus.Code(), msg); };

    while (!retryPolicy.IsExhausted())
    {
        auto result = std::invoke(function);
        if (result.Ok())
        {
            return result;
        }
        lastStatus = std::move(result).GetStatus();
        if (!retryPolicy.OnFailure(lastStatus))
        {
            if (internal::StatusTraits::IsPermanentFailure(lastStatus))
            {
                // The last error cannot be retried, but it is not because the retry
                // policy is exhausted, we call these "permanent errors", and they
                // get a special message.
                std::ostringstream os;
                os << "Permanent error in " << errorMessage << ": " << lastStatus.Message();
                return error(std::move(os).str());
            }
            // Exit the loop immediately instead of sleeping before trying again.
            break;
        }
        auto delay = backoffPolicy.OnCompletion();
        std::this_thread::sleep_for(delay);
    }
    std::ostringstream os;
    os << "Retry policy exhausted in " << errorMessage << ": " << lastStatus.Message();
    return error(std::move(os).str());
}

template <typename MemberFunction>
typename Signature<MemberFunction>::ReturnType MakeCall(csa::RetryPolicy& retryPolicy,
                                                        csa::BackoffPolicy& backoffPolicy, RawClient& client,
                                                        MemberFunction function,
                                                        typename Signature<MemberFunction>::RequestType const& request,
                                                        char const* errorMessage)
{
    static_assert(std::is_invocable_r_v<typename Signature<MemberFunction>::ReturnType, MemberFunction, RawClient&,
                                        typename Signature<MemberFunction>::RequestType const&>);

    return MakeCall(
        retryPolicy, backoffPolicy, [&client, &function, &request]() { return (client.*function)(request); },
        errorMessage);
}

template <typename MemberFunction>
static typename Signature<MemberFunction>::ReturnType MakeCallNoRequest(csa::RetryPolicy& retryPolicy,
                                                                        csa::BackoffPolicy& backoffPolicy,
                                                                        csa::internal::RawClient& client,
                                                                        MemberFunction function,
                                                                        char const* errorMessage)
{
    static_assert(std::is_invocable_r_v<typename Signature<MemberFunction>::ReturnType, MemberFunction, RawClient&>);

    return MakeCall(
        retryPolicy, backoffPolicy, [&client, &function]() { return (client.*function)(); }, errorMessage);
}

}  // namespace

RetryClient::RetryClient(std::shared_ptr<RawClient> client, Options const& options)
    : m_client(std::move(client)),
      m_retryPolicyPrototype(options.Get<RetryPolicyOption>()->Clone()),
      m_backoffPolicyPrototype(options.Get<BackoffPolicyOption>()->Clone())
{
}

Options const& RetryClient::GetOptions() const { return m_client->GetOptions(); }

std::string RetryClient::GetProviderName() const { return m_client->GetProviderName(); }

StatusOrVal<UserInfo> RetryClient::GetUserInfo()
{
    auto retry_policy = m_retryPolicyPrototype->Clone();
    auto backoff_policy = m_backoffPolicyPrototype->Clone();
    return MakeCallNoRequest(*retry_policy, *backoff_policy, *m_client, &RawClient::GetUserInfo, __func__);
}

std::size_t RetryClient::GetFileChunkQuantum() const { return m_client->GetFileChunkQuantum(); }

StatusOrVal<EmptyResponse> RetryClient::Delete(DeleteRequest const& request)
{
    auto retry_policy = m_retryPolicyPrototype->Clone();
    auto backoff_policy = m_backoffPolicyPrototype->Clone();
    return MakeCall(*retry_policy, *backoff_policy, *m_client, &RawClient::Delete, request, __func__);
}

StatusOrVal<ListFolderResponse> RetryClient::ListFolder(ListFolderRequest const& request)
{
    auto retry_policy = m_retryPolicyPrototype->Clone();
    auto backoff_policy = m_backoffPolicyPrototype->Clone();
    return MakeCall(*retry_policy, *backoff_policy, *m_client, &RawClient::ListFolder, request, __func__);
}

StatusOrVal<FolderMetadata> RetryClient::GetFolderMetadata(GetFolderMetadataRequest const& request)
{
    auto retry_policy = m_retryPolicyPrototype->Clone();
    auto backoff_policy = m_backoffPolicyPrototype->Clone();
    return MakeCall(*retry_policy, *backoff_policy, *m_client, &RawClient::GetFolderMetadata, request, __func__);
}

StatusOrVal<FolderMetadata> RetryClient::CreateFolder(CreateFolderRequest const& request)
{
    auto retry_policy = m_retryPolicyPrototype->Clone();
    auto backoff_policy = m_backoffPolicyPrototype->Clone();
    return MakeCall(*retry_policy, *backoff_policy, *m_client, &RawClient::CreateFolder, request, __func__);
}

StatusOrVal<FolderMetadata> RetryClient::RenameFolder(RenameRequest const& request)
{
    auto retry_policy = m_retryPolicyPrototype->Clone();
    auto backoff_policy = m_backoffPolicyPrototype->Clone();
    return MakeCall(*retry_policy, *backoff_policy, *m_client, &RawClient::RenameFolder, request, __func__);
}

StatusOrVal<FolderMetadata> RetryClient::PatchFolderMetadata(PatchFolderMetadataRequest const& request)
{
    auto retry_policy = m_retryPolicyPrototype->Clone();
    auto backoff_policy = m_backoffPolicyPrototype->Clone();
    return MakeCall(*retry_policy, *backoff_policy, *m_client, &RawClient::PatchFolderMetadata, request, __func__);
}

StatusOrVal<FileMetadata> RetryClient::GetFileMetadata(GetFileMetadataRequest const& request)
{
    auto retry_policy = m_retryPolicyPrototype->Clone();
    auto backoff_policy = m_backoffPolicyPrototype->Clone();
    return MakeCall(*retry_policy, *backoff_policy, *m_client, &RawClient::GetFileMetadata, request, __func__);
}

StatusOrVal<FileMetadata> RetryClient::PatchFileMetadata(PatchFileMetadataRequest const& request)
{
    auto retry_policy = m_retryPolicyPrototype->Clone();
    auto backoff_policy = m_backoffPolicyPrototype->Clone();
    return MakeCall(*retry_policy, *backoff_policy, *m_client, &RawClient::PatchFileMetadata, request, __func__);
}

StatusOrVal<FileMetadata> RetryClient::RenameFile(RenameRequest const& request)
{
    auto retry_policy = m_retryPolicyPrototype->Clone();
    auto backoff_policy = m_backoffPolicyPrototype->Clone();
    return MakeCall(*retry_policy, *backoff_policy, *m_client, &RawClient::RenameFile, request, __func__);
}

StatusOrVal<FileMetadata> RetryClient::InsertFile(InsertFileRequest const& request)
{
    auto retry_policy = m_retryPolicyPrototype->Clone();
    auto backoff_policy = m_backoffPolicyPrototype->Clone();
    return MakeCall(*retry_policy, *backoff_policy, *m_client, &RawClient::InsertFile, request, __func__);
}

StatusOrVal<std::unique_ptr<ObjectReadSource>> RetryClient::ReadFileNotWrapped(ReadFileRangeRequest const& request,
                                                                               csa::RetryPolicy& retry_policy,
                                                                               csa::BackoffPolicy& backoff_policy)
{
    return MakeCall(retry_policy, backoff_policy, *m_client, &RawClient::ReadFile, request, __func__);
}

StatusOrVal<std::unique_ptr<ObjectReadSource>> RetryClient::ReadFile(ReadFileRangeRequest const& request)
{
    auto retry_policy = m_retryPolicyPrototype->Clone();
    auto backoff_policy = m_backoffPolicyPrototype->Clone();
    auto child = ReadFileNotWrapped(request, *retry_policy, *backoff_policy);
    if (!child)
    {
        return child;
    }
    auto self = shared_from_this();
    return std::unique_ptr<ObjectReadSource>(new RetryObjectReadSource(
        self, request, *std::move(child), std::move(retry_policy), std::move(backoff_policy)));
}

StatusOrVal<std::unique_ptr<ResumableUploadSession>> RetryClient::CreateResumableSession(
    ResumableUploadRequest const& request)
{
    auto retry_policy = m_retryPolicyPrototype->Clone();
    auto backoff_policy = m_backoffPolicyPrototype->Clone();
    auto result =
        MakeCall(*retry_policy, *backoff_policy, *m_client, &RawClient::CreateResumableSession, request, __func__);
    if (!result.Ok())
    {
        return result;
    }

    return std::unique_ptr<ResumableUploadSession>(std::make_unique<RetryResumableUploadSession>(
        std::move(result).Value(), std::move(retry_policy), std::move(backoff_policy)));
}

StatusOrVal<std::unique_ptr<ResumableUploadSession>> RetryClient::RestoreResumableSession(std::string const& sessionId)
{
    auto retry_policy = m_retryPolicyPrototype->Clone();
    auto backoff_policy = m_backoffPolicyPrototype->Clone();
    return MakeCall(*retry_policy, *backoff_policy, *m_client, &RawClient::RestoreResumableSession, sessionId,
                    __func__);
}

StatusOrVal<EmptyResponse> RetryClient::DeleteResumableUpload(DeleteResumableUploadRequest const& request)
{
    auto retry_policy = m_retryPolicyPrototype->Clone();
    auto backoff_policy = m_backoffPolicyPrototype->Clone();
    return MakeCall(*retry_policy, *backoff_policy, *m_client, &RawClient::DeleteResumableUpload, request, __func__);
}

StatusOrVal<FileMetadata> RetryClient::CopyFileObject(CopyFileRequest const& request)
{
    auto retry_policy = m_retryPolicyPrototype->Clone();
    auto backoff_policy = m_backoffPolicyPrototype->Clone();
    return MakeCall(*retry_policy, *backoff_policy, *m_client, &RawClient::CopyFileObject, request, __func__);
}

StatusOrVal<StorageQuota> RetryClient::GetQuota()
{
    auto retry_policy = m_retryPolicyPrototype->Clone();
    auto backoff_policy = m_backoffPolicyPrototype->Clone();
    return MakeCallNoRequest(*retry_policy, *backoff_policy, *m_client, &RawClient::GetQuota, __func__);
}

}  // namespace internal
}  // namespace csa
