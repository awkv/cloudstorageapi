// Copyright 2020 Andrew Karasyov
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

#include "cloudstorageapi/internal/resumable_upload_session.h"
#include "cloudstorageapi/internal/file_requests.h"
#include "cloudstorageapi/internal/utils.h"
#include <cassert>
#include <iostream>
#include <sstream>

namespace csa {
namespace internal {
namespace {
std::string ToStr(ResumableUploadResponse::UploadState const& uploadState)
{
    switch (uploadState)
    {
    case ResumableUploadResponse::UploadState::InProgress:
        return "InProgress";
    case ResumableUploadResponse::UploadState::Done:
        return "Done";
    }

    assert(false);
    return "unknown";
}
}  // namespace

bool operator==(ResumableUploadResponse const& lhs, ResumableUploadResponse const& rhs)
{
    return lhs.m_uploadSessionUrl == rhs.m_uploadSessionUrl && lhs.m_lastCommittedByte == rhs.m_lastCommittedByte &&
           lhs.m_payload == rhs.m_payload && lhs.m_uploadState == rhs.m_uploadState;
}

bool operator!=(ResumableUploadResponse const& lhs, ResumableUploadResponse const& rhs) { return !(lhs == rhs); }

std::ostream& operator<<(std::ostream& os, ResumableUploadResponse const& r)
{
    os << "ResumableUploadResponse={upload_session_url=" << r.m_uploadSessionUrl
       << ", last_committed_byte=" << r.m_lastCommittedByte << ", payload=";
    if (r.m_payload.has_value())
    {
        os << *r.m_payload;
    }
    else
    {
        os << "{}";
    }
    return os << ", upload_state=" << ToStr(r.m_uploadState) << ", annotations=" << r.m_annotations << "}";
}
}  // namespace internal
}  // namespace csa
