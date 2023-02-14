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

#include "cloudstorageapi/auth/default_credentials_file.h"
#include "cloudstorageapi/internal/utils.h"

namespace csa {
namespace auth {
namespace {

std::string WellKnownCredentialsFilePathSuffix()
{
#ifdef _WIN32
    return "/csa/default_credentials.json";
#else
    return "/.config/csa/default_credentials.json";
#endif
}

}  // namespace

std::string DefaultCredentialsFilePathFromEnvVarOrEmpty()
{
    auto overrideValue = csa::internal::GetEnv(DefaultCredentialsEnvVar());
    if (overrideValue.has_value())
    {
        return *overrideValue;
    }
    return "";
}

std::string DefaultCredentialsFilePathFromWellKnownPathOrEmpty()
{
    // Allow mocking out this value for testing.
    auto overridePath = csa::internal::GetEnv(DefaultTestCredentialsEnvVar());
    if (overridePath.has_value())
    {
        return *overridePath;
    }

    // Search well known path.
    auto pathRoot = csa::internal::GetEnv(HomeEnvVar());
    if (pathRoot.has_value())
    {
        return *pathRoot + WellKnownCredentialsFilePathSuffix();
    }
    return "";
}

}  // namespace auth
}  // namespace csa
