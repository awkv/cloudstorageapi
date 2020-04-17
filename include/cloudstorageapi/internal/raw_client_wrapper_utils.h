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

#pragma once

#include "cloudstorageapi/internal/raw_client.h"
#include <type_traits>

namespace csa {
namespace internal {
/**
 * Defines types to wrap `RawClient` function calls.
 *
 * We have a couple of classes that basically wrap every function in `RawClient`
 * with some additional behavior (`LoggingClient` logs every call, and
 * `RetryClient` retries every call).  Instead of hand-coding every wrapped
 * function we use a helper to wrap it, and in turn those helpers use the
 * meta-functions defined here.
 */


 /**
  * Metafunction to determine if @p F is a pointer to member function with the
  * expected signature for a `RawClient` member function.
  *
  * This is the generic case, where the type does not match the expected
  * signature and so member type aliases do not exist.
  *
  * @tparam F the type to check against the expected signature.
  */
template <typename F>
struct Signature {};

/**
 * Partial specialization for the above `Signature` metafunction.
 *
 * This is the case where the type actually matches the expected signature. The
 * class also extracts the request and response types used in the
 * implementation of `CallWithRetry()`.
 *
 * @tparam Request the RPC request type.
 * @tparam Response the RPC response type.
 */
template <typename Request, typename Response>
struct Signature<StatusOrVal<Response>(
    csa::internal::RawClient::*)(Request const&)>
{
    using RequestType = Request;
    using ReturnType = StatusOrVal<Response>;
};

template <typename Response>
struct Signature < StatusOrVal<Response>(
    csa::internal::RawClient::*)()>
{
    using ReturnType = StatusOrVal<Response>;
};

} // namespace internal
} // namespace csa
