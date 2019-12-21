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

#pragma once

#include "cloudstorageapi/status.h"
#include "cloudstorageapi/status_or_val.h"

namespace csa {
namespace auth {
/**
 * Common interface for OAuth 2.0 and password-based credentials used to access cloud services.
 */
class Credentials
{
public:
    virtual ~Credentials() = default;

    /**
     * Attempts to obtain a value for the Authorization HTTP header.
     *
     * If unable to obtain a value for the Authorization header, which could
     * happen for `Credentials` that need to be periodically refreshed, the
     * underyling `Status` will indicate failure details from the refresh HTTP
     * request. Otherwise, the returned value will contain the Authorization
     * header to be used in HTTP requests.
     */
    virtual StatusOrVal<std::string> AuthorizationHeader() = 0;

    // Return the account's email associated with these credentials, if any.
    virtual std::string AccountEmail() const { return std::string{}; }

    // TODO: needed ?
    // Return the account's key_id associated with these credentials, if any.
    //virtual std::string KeyId() const { return std::string{}; }
};

}  // namespace auth
}  // namespace csa
