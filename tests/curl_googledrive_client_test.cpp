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

#include "cloudstorageapi/internal/clients/curl_googledrive_client.h"
#include "cloudstorageapi/auth/credential_factory.h"
#include "cloudstorageapi/auth/credentials.h"
#include "cloudstorageapi/auth/google_oauth2_credentials.h"
#include "cloudstorageapi/internal/curl_request_builder.h"
#include "cloudstorageapi/internal/utils.h"
#include "util/scoped_environment.h"
#include "util/status_matchers.h"
#include <gmock/gmock.h>
#include <memory>
#include <utility>
#include <vector>

namespace csa {
namespace internal {

using ::csa::auth::Credentials;
using ::csa::testing::util::StatusIs;
using ::testing::HasSubstr;
using ::testing::Not;

StatusCode const StatusErrorCode = StatusCode::Unavailable;
std::string const StatusErrorMsg = "FailingCredentials doing its job, failing";

// We create a credential class that always fails to fetch an access token; this
// allows us to check that CurlGoogleDriveClient methods fail early when their setup steps
// (which include adding the authorization header) return a failure Status.
// Note that if the methods performing the setup steps were not templated, we
// could simply mock those out instead via MOCK_METHOD<N> macros.
class FailingCredentials : public Credentials
{
public:
    StatusOrVal<std::string> AuthorizationHeader() override { return Status(StatusErrorCode, StatusErrorMsg); }
};

class CurlClientTest : public ::testing::Test, public ::testing::WithParamInterface<std::string>
{
protected:
    CurlClientTest() = default;

    void SetUp() override
    {
        std::string const errorType = GetParam();
        if (errorType == "credentials-failure")
        {
            m_client = CurlGoogleDriveClient::Create(
                Options{}.Set<Oauth2CredentialsOption>(std::make_shared<FailingCredentials>()));
            // We know exactly what error to expect, so setup the assertions to be
            // very strict.
            m_checkStatus = [](Status const& actual) {
                EXPECT_THAT(actual, StatusIs(StatusErrorCode, HasSubstr(StatusErrorMsg)));
            };
        }
        else if (errorType == "libcurl-failure")
        {
            m_client = CurlGoogleDriveClient::Create(Options{}.Set<Oauth2CredentialsOption>(
                csa::auth::CredentialFactory::CreateAnonymousCredentials(EProvider::GoogleDrive))
                                                     /*.Set<RestEndpointOption>("http://localhost:1")*/);
            // We do not know what libcurl will return. Some kind of error, but varies
            // by version of libcurl. Just make sure it is an error and the CURL
            // details are included in the error message.
            m_checkStatus = [](Status const& actual) {
                EXPECT_THAT(actual, StatusIs(Not(StatusCode::Ok), HasSubstr("CURL error")));
            };
        }
        else
        {
            FAIL() << "Invalid test parameter value: " << errorType
                   << ", expected either credentials-failure or libcurl-failure";
        }
    }

    void CheckStatus(Status const& actual) { m_checkStatus(actual); }

    void TearDown() override { m_client.reset(); }

    std::shared_ptr<CurlGoogleDriveClient> m_client;
    std::function<void(Status const& status)> m_checkStatus;
};

TEST_P(CurlClientTest, GetUserInfo)
{
    auto actual = m_client->GetUserInfo().GetStatus();
    CheckStatus(actual);
}

TEST_P(CurlClientTest, UploadChunk)
{
    // Use an invalid port (0) to force a libcurl failure
    auto actual = m_client
                      ->UploadChunk(UploadChunkRequest{
                          "http://localhost:1/invalid-session-id", 0, {ConstBuffer{std::string{}}}, 0})
                      .GetStatus();
    CheckStatus(actual);
}

TEST_P(CurlClientTest, QueryResumableUpload)
{
    // Use http://localhost:1 to force a libcurl failure
    auto actual = m_client->QueryResumableUpload(QueryResumableUploadRequest("http://localhost:9/invalid-session-id"))
                      .GetStatus();
    CheckStatus(actual);
}

TEST_P(CurlClientTest, Delete)
{
    auto actual = m_client->Delete(DeleteRequest{"object_id"}).GetStatus();
    CheckStatus(actual);
}

TEST_P(CurlClientTest, ListFolder)
{
    auto actual = m_client->ListFolder(ListFolderRequest{"project_id"}).GetStatus();
    CheckStatus(actual);
}

TEST_P(CurlClientTest, GetFolderMetadata)
{
    auto actual = m_client->GetFolderMetadata(GetFolderMetadataRequest("fldr")).GetStatus();
    CheckStatus(actual);
}

TEST_P(CurlClientTest, CreateFolder)
{
    auto actual = m_client->CreateFolder(CreateFolderRequest("parent_fldr", "fldr")).GetStatus();
    CheckStatus(actual);
}

TEST_P(CurlClientTest, RenameFolder)
{
    auto actual = m_client->RenameFolder(RenameRequest("id", "NewName", "parent_id", "newParentId")).GetStatus();
    CheckStatus(actual);
}

TEST_P(CurlClientTest, PatchFolderMetadata)
{
    FolderMetadata origFolder;
    origFolder.SetName("fldr");
    FolderMetadata newFolder;
    newFolder.SetName("fldr");
    auto actual =
        m_client->PatchFolderMetadata(PatchFolderMetadataRequest("folderId", origFolder, newFolder)).GetStatus();
    CheckStatus(actual);
}

TEST_P(CurlClientTest, GetFileMetadata)
{
    auto actual = m_client->GetFileMetadata(GetFileMetadataRequest("id")).GetStatus();
    CheckStatus(actual);
}

TEST_P(CurlClientTest, PatchFileMetadata)
{
    FileMetadata origFile;
    origFile.SetName("fl");
    FileMetadata newFile;
    newFile.SetName("fl");
    auto actual = m_client->PatchFileMetadata(PatchFileMetadataRequest("bkt", origFile, newFile)).GetStatus();
    CheckStatus(actual);
}

TEST_P(CurlClientTest, RenameFile)
{
    auto actual = m_client->RenameFile(RenameRequest("id", "newName", "parentId", "newParentId")).GetStatus();
    CheckStatus(actual);
}

TEST_P(CurlClientTest, InsertFileSimple)
{
    auto actual = m_client->InsertFile(InsertFileRequest("", "", "contents")).GetStatus();
    CheckStatus(actual);
}

TEST_P(CurlClientTest, InsertFileMultipart)
{
    auto actual = m_client->InsertFile(InsertFileRequest("folder", "name", "contents")).GetStatus();
    CheckStatus(actual);
}

TEST_P(CurlClientTest, ReadFile)
{
    if (GetParam() == "libcurl-failure")
    {
        return;
    }
    auto actual = m_client->ReadFile(ReadFileRangeRequest("file")).GetStatus();
    CheckStatus(actual);
}

TEST_P(CurlClientTest, CreateResumableSession)
{
    auto actual = m_client->CreateResumableSession(ResumableUploadRequest("folderId", "fileName")).GetStatus();
    CheckStatus(actual);
}

TEST_P(CurlClientTest, RestoreResumableSession)
{
    auto actual = m_client->RestoreResumableSession("session-id").GetStatus();
    CheckStatus(actual);
}

TEST_P(CurlClientTest, DeleteResumableUpload)
{
    auto actual = m_client->DeleteResumableUpload(DeleteResumableUploadRequest("upload-session-url")).GetStatus();
    CheckStatus(actual);
}

TEST_P(CurlClientTest, CopyFileObject)
{
    auto actual = m_client->CopyFileObject(CopyFileRequest("fileId", "newParentId", "newFileName")).GetStatus();
    CheckStatus(actual);
}

TEST_P(CurlClientTest, GetQuota)
{
    auto actual = m_client->GetQuota().GetStatus();
    CheckStatus(actual);
}

INSTANTIATE_TEST_SUITE_P(CredentialsFailure, CurlClientTest, ::testing::Values("credentials-failure"));

// TODO: implement setting custom rest api endpoint in the client and uncomment these tests.
// INSTANTIATE_TEST_SUITE_P(LibCurlFailure, CurlClientTest, ::testing::Values("libcurl-failure"));

}  // namespace internal
}  // namespace csa
