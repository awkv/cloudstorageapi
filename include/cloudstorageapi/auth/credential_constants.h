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

#include <chrono>

namespace csa {
namespace auth {

/**
 * Supported signing algorithms used in JWT auth flows.
 */
enum class JwtSigningAlgorithms { RS256 };

/// The max lifetime in seconds of an access token.
constexpr std::chrono::seconds OAuthAccessTokenLifetime()
{
  return std::chrono::seconds(3600);
}

/**
 * Returns the slack to consider when checking if an access token is expired.
 *
 * This time should be subtracted from a token's expiration time when checking
 * if it is expired. This prevents race conditions where, for example, one might
 * check expiration time one second before the expiration, see that the token is
 * still valid, then attempt to use it two seconds later and receive an
 * error.
 */
constexpr std::chrono::seconds OAuthAccessTokenExpirationSlack()
{
  return std::chrono::seconds(500);
}

}  // namespace auth
}  // namespace csa
