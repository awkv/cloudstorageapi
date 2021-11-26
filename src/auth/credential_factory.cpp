// Copyright 2019 Andrew Karasyov
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

#include "cloudstorageapi/auth/credential_factory.h"
#include "cloudstorageapi/auth/anonymous_credentials.h"
#include "cloudstorageapi/auth/default_credentials_file.h"
#include "cloudstorageapi/auth/error_credentials.h"
#include "cloudstorageapi/auth/google_oauth2_credentials.h"
#include <nlohmann/json.hpp>
#include <fstream>

namespace csa {
namespace auth {

std::shared_ptr<Credentials> CredentialFactory::CreateDefaultCredentials(EProvider provider)
{
    // Check if CSA_CREDENTIALS environment variable is set.
    auto path = DefaultCredentialsFilePathFromEnvVarOrEmpty();
    if (path.empty())
    {
        // Check if well known credentials file exists.
        path = DefaultCredentialsFilePathFromWellKnownPathOrEmpty();
        if (path.empty())
            return std::make_shared<ErrorCredentials>(
                Status(StatusCode::InvalidArgument, "No default credentials file exist."));
    }

    // Now load credentials from the file.
    return CreateAuthorizedUserCredentialsFromJsonFilePath(provider, path);
}

std::shared_ptr<Credentials> CredentialFactory::CreateAnonymousCredentials(EProvider provider)
{
    return std::make_shared<AnonymousCredentials>();
}

std::shared_ptr<Credentials> CredentialFactory::CreateAuthorizedUserCredentialsFromJsonFilePath(EProvider provider,
                                                                                                std::string const& path)
{
    std::ifstream ifs(path);
    if (!ifs.good())
    {
        // Failed to read a file because it either doesn't exist, no permissions
        // or any other reason.
        return std::make_shared<ErrorCredentials>(Status(StatusCode::Unknown, "Cannot open credentials file " + path));
    }

    std::string contents(std::istreambuf_iterator<char>{ifs}, {});
    return CreateAuthorizedUserCredentialsFromJsonContents(provider, contents, path);
}

std::shared_ptr<Credentials> CredentialFactory::CreateAuthorizedUserCredentialsFromJsonContents(
    EProvider provider, std::string const& contents, std::string const& sourceFile)
{
    std::shared_ptr<Credentials> creds = nullptr;
    switch (provider)
    {
    case EProvider::GoogleDrive:
    {
        auto info = GoogleAuthHandler::ParseOAuth2Credentials(contents, sourceFile);
        if (!info)
            creds = std::make_shared<ErrorCredentials>(info.GetStatus());
        else
            creds = std::make_shared<GoogleOAuth2Credentials>(*info);
    }
    break;

    default:
        assert(0 && "Failed to create credentials: unexpected provider.");
        creds = std::make_shared<ErrorCredentials>(
            Status(StatusCode::InvalidArgument,
                   "Unsupported provider when reading Default Credentials file from " + sourceFile + "."));
    }

    return creds;
}

}  // namespace auth
}  // namespace csa
