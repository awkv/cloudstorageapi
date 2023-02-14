// Copyright 2019 Andrew Karasyov
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

#include "cloudstorageapi/auth/credentials.h"
#include "cloudstorageapi/providers.h"
#include "cloudstorageapi/status_or_val.h"
#include <memory>

namespace csa {
namespace auth {

class CredentialFactory
{
public:
    CredentialFactory() = delete;

    /**
     * Produces a Credentials type based on the runtime environment.
     *
     * If the CSA_CREDENTIALS environment variable is set, the JSON
     * file it points to will be loaded and used to create a credential of the
     * specified type.
     *
     */
    static std::shared_ptr<Credentials> CreateDefaultCredentials(EProvider provider);

    //@{
    /**
     * @name Functions to manually create specific credential types.
     */

    // Creates an AnonymousCredentials.
    static std::shared_ptr<Credentials> CreateAnonymousCredentials(EProvider provider);

    /**
     * Creates an AuthorizedUserCredentials from a JSON file at the specified path.
     */
    static std::shared_ptr<Credentials> CreateAuthorizedUserCredentialsFromJsonFilePath(EProvider provider,
                                                                                        std::string const& path);

    /**
     * Creates an AuthorizedUserCredentials from a JSON string.
     *
     * @note It is strongly preferred to instead use service account credentials
     * with Cloud Storage client libraries.
     */
    static std::shared_ptr<Credentials> CreateAuthorizedUserCredentialsFromJsonContents(EProvider provider,
                                                                                        std::string const& contents,
                                                                                        std::string const& sourceFile);
};

}  // namespace auth
}  // namespace csa
