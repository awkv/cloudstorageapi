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

#include "cloudstorageapi/auth/credential_factory.h"
#include "cloudstorageapi/cloud_storage_client.h"
#include "cloudstorageapi/internal/raw_client.h"
#include "cloudstorageapi/internal/resumable_upload_session.h"
#include <gmock/gmock.h>
#include <string>

namespace csa {
namespace internal {

class MockClient : public csa::internal::RawClient
{
public:
    MockClient(EProvider provider)
        : m_options(Options{}.Set<ProviderOption>(provider).Set<Oauth2CredentialsOption>(
              csa::auth::CredentialFactory::CreateAnonymousCredentials(provider)))
    {
        EXPECT_CALL(*this, GetOptions()).WillRepeatedly(::testing::ReturnRef(m_options));
    }

    MOCK_METHOD(Options const&, GetOptions, (), (const, override));
    MOCK_METHOD(std::string, GetProviderName, (), (const, override));
    MOCK_METHOD(StatusOrVal<UserInfo>, GetUserInfo, (), (override));
    MOCK_METHOD(std::size_t, GetFileChunkQuantum, (), (const, override));

    MOCK_METHOD(StatusOrVal<EmptyResponse>, Delete, (DeleteRequest const&), (override));

    MOCK_METHOD(StatusOrVal<ListFolderResponse>, ListFolder, (ListFolderRequest const&), (override));
    MOCK_METHOD(StatusOrVal<FolderMetadata>, GetFolderMetadata, (GetFolderMetadataRequest const&), (override));
    MOCK_METHOD(StatusOrVal<FolderMetadata>, CreateFolder, (CreateFolderRequest const&), (override));
    MOCK_METHOD(StatusOrVal<FolderMetadata>, RenameFolder, (RenameRequest const&), (override));
    MOCK_METHOD(StatusOrVal<FolderMetadata>, PatchFolderMetadata, (PatchFolderMetadataRequest const&), (override));

    MOCK_METHOD(StatusOrVal<FileMetadata>, GetFileMetadata, (GetFileMetadataRequest const&), (override));
    MOCK_METHOD(StatusOrVal<FileMetadata>, PatchFileMetadata, (PatchFileMetadataRequest const&), (override));
    MOCK_METHOD(StatusOrVal<FileMetadata>, RenameFile, (RenameRequest const&), (override));
    MOCK_METHOD(StatusOrVal<FileMetadata>, InsertFile, (InsertFileRequest const&), (override));
    MOCK_METHOD(StatusOrVal<std::unique_ptr<ObjectReadSource>>, ReadFile, (ReadFileRangeRequest const&), (override));
    MOCK_METHOD(StatusOrVal<std::unique_ptr<ResumableUploadSession>>, CreateResumableSession,
                (ResumableUploadRequest const&), (override));
    MOCK_METHOD(StatusOrVal<std::unique_ptr<ResumableUploadSession>>, RestoreResumableSession, (std::string const&),
                (override));
    MOCK_METHOD(StatusOrVal<internal::EmptyResponse>, DeleteResumableUpload,
                (internal::DeleteResumableUploadRequest const&), (override));
    MOCK_METHOD(StatusOrVal<FileMetadata>, CopyFileObject, (CopyFileRequest const&), (override));

    MOCK_METHOD(StatusOrVal<StorageQuota>, GetQuota, (), (override));

private:
    Options m_options;
};

}  // namespace internal
}  // namespace csa
