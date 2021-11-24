// Copyright 2021 Andrew Karasyov
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

#include "cloudstorageapi/internal/clients/google_utils.h"
#include <sstream>
#include <utility>

namespace csa {
namespace internal {

std::string GoogleUtils::GetRangeHeader(UploadChunkRequest const& request)
{
    std::ostringstream os;
    os << "Content-Range: bytes ";
    auto const size = request.GetPayloadSize();
    if (size == 0)
    {
        // This typically happens when the sender realizes too late that the
        // previous chunk was really the last chunk (e.g. the file is exactly a
        // multiple of the quantum, reading the last chunk from a file, or sending
        // it as part of a stream that does not detect the EOF), the formatting of
        // the range is special in this case.
        os << "*";
    }
    else
    {
        os << request.GetRangeBegin() << "-" << request.GetRangeEnd();
    }

    if (!request.IsLastChunk())
    {
        os << "/*";
    }
    else
    {
        os << "/" << request.GetSourceSize();
    }

    return std::move(os).str();
}

std::string GoogleUtils::GetRangeHeader(ReadFileRangeRequest const& request)
{
    if (request.HasOption<ReadRange>() && request.HasOption<ReadFromOffset>())
    {
        auto range = request.GetOption<ReadRange>().Value();
        auto offset = request.GetOption<ReadFromOffset>().Value();
        auto begin = (std::max)(range.m_begin, offset);
        return "Range: bytes=" + std::to_string(begin) + "-" + std::to_string(range.m_end - 1);
    }
    if (request.HasOption<ReadRange>())
    {
        auto range = request.GetOption<ReadRange>().Value();
        return "Range: bytes=" + std::to_string(range.m_begin) + "-" + std::to_string(range.m_end - 1);
    }
    if (request.HasOption<ReadFromOffset>())
    {
        auto offset = request.GetOption<ReadFromOffset>().Value();
        if (offset != 0)
        {
            return "Range: bytes=" + std::to_string(offset) + "-";
        }
    }
    if (request.HasOption<ReadLast>())
    {
        auto last = request.GetOption<ReadLast>().Value();
        return "Range: bytes=-" + std::to_string(last);
    }
    return "";
}

}  // namespace internal
}  // namespace csa