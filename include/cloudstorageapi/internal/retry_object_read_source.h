// Copyright 2021 Andrew Karasyov
//
// Copyright 2019 Google LLC
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

#include "cloudstorageapi/internal/object_read_source.h"
#include "cloudstorageapi/internal/retry_client.h"

namespace csa {
namespace internal {

enum OffsetDirection
{
    FromBeginning,
    FromEnd
};

/**
 * A data source for ObjectReadStreambuf.
 *
 * This object represents an open download stream. It is an abstract class
 * because (a) we do not want to expose CURL types in the public headers, and
 * (b) we want to break the functionality for retry vs. simple downloads in
 * different classes.
 */
class RetryObjectReadSource : public ObjectReadSource
{
public:
    RetryObjectReadSource(std::shared_ptr<RetryClient> client, ReadFileRangeRequest request,
                          std::unique_ptr<ObjectReadSource> child, std::unique_ptr<csa::RetryPolicy> retryPolicy,
                          std::unique_ptr<csa::BackoffPolicy> backoffPolicy);

    bool IsOpen() const override { return m_child && m_child->IsOpen(); }
    StatusOrVal<HttpResponse> Close() override { return m_child->Close(); }
    StatusOrVal<ReadSourceResult> Read(char* buf, std::size_t n) override;

private:
    std::shared_ptr<RetryClient> m_client;
    ReadFileRangeRequest m_request;
    std::unique_ptr<ObjectReadSource> m_child;
    std::unique_ptr<csa::RetryPolicy const> m_retryPolicyPrototype;
    std::unique_ptr<csa::BackoffPolicy const> m_backoffPolicyPrototype;
    OffsetDirection m_offsetDirection;
    std::int64_t m_currentOffset;
};

}  // namespace internal
}  // namespace csa
