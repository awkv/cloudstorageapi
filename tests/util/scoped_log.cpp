// Copyright 2021 Andrew Karasyov
//
// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "scoped_log.h"
#include <iterator>

namespace csa {
namespace testing {
namespace internal {

std::vector<std::string> ScopedLog::Sink::ExtractLines()
{
    std::vector<std::string> result;
    {
        std::lock_guard<std::mutex> lk(m_mu);
        result.swap(m_logLines);
    }
    return result;
}

void ScopedLog::Sink::SinkRecord(csa::internal::LogRecord const& logRec)
{
    // Break the record into lines. It is easier to analyze them as such.
    std::vector<std::string> result;
    std::istringstream input{logRec.m_message};
    for (std::string line; std::getline(input, line, '\n');)
    {
        result.emplace_back(std::move(line));
    }
    std::lock_guard<std::mutex> lk(m_mu);
    m_logLines.insert(m_logLines.end(), std::make_move_iterator(result.begin()), std::make_move_iterator(result.end()));
}

void ScopedLog::Sink::Flush() {}

}  // namespace internal
}  // namespace testing
}  // namespace csa
