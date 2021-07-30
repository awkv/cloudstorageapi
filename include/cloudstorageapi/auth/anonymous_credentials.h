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

#include "cloudstorageapi/auth/credentials.h"
#include <string>

namespace csa {
namespace auth {

/**
 * A `Credentials` type representing "anonymous".
 *
 * This is only useful in two cases: (a) in testing, where you want to access
 * a test bench without having to worry about authentication or SSL setup, and
 * (b) when accessing publicly readable resources (e.g. a Google Cloud Storage
 * object that is readable by the "allUsers" entity), which requires no
 * authentication or authorization.
 */
class AnonymousCredentials : public Credentials
{
public:
    AnonymousCredentials() = default;

    /**
     * While other Credentials subclasses return a string containing an
     * Authorization HTTP header from this method, this class always returns an
     * empty string as its value.
     */
    StatusOrVal<std::string> AuthorizationHeader() override { return std::string{}; }
};

}  // namespace auth
}  // namespace csa
