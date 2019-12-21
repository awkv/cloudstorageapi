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

#include <string>

namespace csa {
namespace auth {

/**
 * Returns the Application Default %Credentials environment variable name.
 * It stores a file path which should be checked when loading default credentials.
 */
inline constexpr const char* DefaultCredentialsEnvVar()
{
    return "CSA_CREDENTIALS";
}

inline constexpr const char* DefaultTestCredentialsEnvVar()
{
    return "CSA_TEST_CREDENTIALS";
}

inline constexpr const char* HomeEnvVar()
{
#ifdef _WIN32
    return "APPDATA";
#else
    return "HOME";
#endif // WIN32
}

/**
 * Returns the path to the Default %Credentials file, if set.
 */
std::string DefaultCredentialsFilePathFromEnvVarOrEmpty();

/**
 * Returns the path to the Application Default %Credentials file, if set.
 */
std::string DefaultCredentialsFilePathFromWellKnownPathOrEmpty();
} // namespace auth
} // namespace csa
