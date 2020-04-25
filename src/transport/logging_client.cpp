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

#include "cloudstorageapi/internal/log.h"
#include "cloudstorageapi/internal/logging_client.h"
#include "cloudstorageapi/internal/logging_resumable_upload_session.h"
#include "cloudstorageapi/internal/raw_client_wrapper_utils.h"

#include <sstream>

namespace csa {
namespace internal {

namespace {
/**
 * Logs the input and results of each `RawClient` operation.
 *
 * @tparam MemberFunction the signature of the member function.
 * @param client the storage::RawClient object to make the call through.
 * @param function the pointer to the member function to call.
 * @param request an initialized request parameter for the call.
 * @param error_message include this message in any exception or error log.
 * @return the result from making the call;
 */
template <typename MemberFunction>
static typename Signature<MemberFunction>::ReturnType MakeCall(
    RawClient& client, MemberFunction function,
    typename Signature<MemberFunction>::RequestType const& request,
    char const* context)
{
    CSA_LOG_INFO("{}() {}", context, request);

    auto response = (client.*function)(request);
    if (response.Ok())
    {
        CSA_LOG_INFO("{}() payload={{{}}}", context, response.Value());
    }
    else
    {
        CSA_LOG_INFO("{}() status={{{}}}", context, response.GetStatus());
    }
    return response;
}

/**
 * Calls a `RawClient` operation logging only the input.
 *
 * This is useful when the result is not something you can easily log, such as
 * a pointer of some kind.
 *
 * @tparam MemberFunction the signature of the member function.
 * @param client the storage::RawClient object to make the call through.
 * @param function the pointer to the member function to call.
 * @param request an initialized request parameter for the call.
 * @param error_message include this message in any exception or error log.
 * @return the result from making the call;
 */
template <typename MemberFunction>
static typename Signature<MemberFunction>::ReturnType MakeCallNoResponseLogging(
    csa::internal::RawClient& client,
    MemberFunction function,
    typename Signature<MemberFunction>::RequestType const& request,
    char const* context)
{
    CSA_LOG_INFO("{}() {}", context, request);
    return (client.*function)(request);
}

template <typename MemberFunction>
static typename Signature<MemberFunction>::ReturnType MakeCallNoRequest(
    csa::internal::RawClient& client,
    MemberFunction function,
    char const* context)
{
    CSA_LOG_INFO("{}()", context);

    auto response = (client.*function)();
    if (response.Ok())
    {
        CSA_LOG_INFO("{}() payload={{{}}}", context, response.Value());
    }
    else
    {
        CSA_LOG_INFO("{}() status={{{}}}", context, response.GetStatus());
    }
    return response;
}

}  // namespace

LoggingClient::LoggingClient(std::shared_ptr<RawClient> client)
    : m_client(std::move(client))
{
}

ClientOptions const& LoggingClient::GetClientOptions() const
{
    return m_client->GetClientOptions();
}

std::string LoggingClient::GetProviderName() const
{
    return m_client->GetProviderName();
}

StatusOrVal<UserInfo> LoggingClient::GetUserInfo()
{
    return MakeCallNoRequest(*m_client, &RawClient::GetUserInfo, __func__);
}

std::size_t LoggingClient::GetFileChunkQuantum() const
{
    return m_client->GetFileChunkQuantum();
}

StatusOrVal<EmptyResponse> LoggingClient::Delete(DeleteRequest const& request)
{
    return MakeCall(*m_client, &RawClient::Delete, request, __func__);
}

StatusOrVal<ListFolderResponse> LoggingClient::ListFolder(ListFolderRequest const& request)
{
    return MakeCall(*m_client, &RawClient::ListFolder, request, __func__);
}

StatusOrVal<FolderMetadata> LoggingClient::GetFolderMetadata(GetFolderMetadataRequest const& request)
{
    return MakeCall(*m_client, &RawClient::GetFolderMetadata, request, __func__);
}

StatusOrVal<FolderMetadata> LoggingClient::CreateFolder(CreateFolderRequest const& request)
{
    return MakeCall(*m_client, &RawClient::CreateFolder, request, __func__);
}

StatusOrVal<FolderMetadata> LoggingClient::RenameFolder(RenameRequest const& request)
{
    return MakeCall(*m_client, &RawClient::RenameFolder, request, __func__);
}

StatusOrVal<FolderMetadata> LoggingClient::PatchFolderMetadata(PatchFolderMetadataRequest const& request)
{
    return MakeCall(*m_client, &RawClient::PatchFolderMetadata, request, __func__);
}

StatusOrVal<FileMetadata> LoggingClient::GetFileMetadata(GetFileMetadataRequest const& request)
{
    return MakeCall(*m_client, &RawClient::GetFileMetadata, request, __func__);
}

StatusOrVal<FileMetadata> LoggingClient::PatchFileMetadata(PatchFileMetadataRequest const& request)
{
    return MakeCall(*m_client, &RawClient::PatchFileMetadata, request, __func__);
}

StatusOrVal<FileMetadata> LoggingClient::RenameFile(RenameRequest const& request)
{
    return MakeCall(*m_client, &RawClient::RenameFile, request, __func__);
}

StatusOrVal<FileMetadata> LoggingClient::InsertFile(InsertFileRequest const& request)
{
    return MakeCall(*m_client, &RawClient::InsertFile, request, __func__);
}

StatusOrVal<std::unique_ptr<ObjectReadSource>> LoggingClient::ReadFile(
    ReadFileRangeRequest const& request)
{
    return MakeCallNoResponseLogging(*m_client, &RawClient::ReadFile, request,
        __func__);
}

StatusOrVal<std::unique_ptr<ResumableUploadSession>>
LoggingClient::CreateResumableSession(ResumableUploadRequest const& request)
{
    auto result = MakeCallNoResponseLogging(
        *m_client, &RawClient::CreateResumableSession, request, __func__);
    if (!result.Ok())
    {
        CSA_LOG_INFO("{}() >> status={{{}}}", __func__, result.GetStatus());
        return std::move(result).GetStatus();
    }

    return std::unique_ptr<ResumableUploadSession>(
        std::make_unique<LoggingResumableUploadSession>(
            std::move(result).Value()));
}

StatusOrVal<std::unique_ptr<ResumableUploadSession>>
LoggingClient::RestoreResumableSession(std::string const& sessionId)
{
    return MakeCallNoResponseLogging(
        *m_client, &RawClient::RestoreResumableSession, sessionId, __func__);
}

StatusOrVal<FileMetadata> 
LoggingClient::CopyFileObject(CopyFileRequest const& request)
{
    return MakeCall(*m_client, &RawClient::CopyFileObject, request, __func__);
}

StatusOrVal<StorageQuota> LoggingClient::GetQuota()
{
    return MakeCallNoRequest(*m_client, &RawClient::GetQuota, __func__);
}

} // namespace internal
} // namespace csa
