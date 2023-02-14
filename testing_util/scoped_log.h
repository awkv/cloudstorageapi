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

#pragma once

#include "cloudstorageapi/internal/log.h"
#include <string>
#include <vector>

namespace csa {
namespace testing {
namespace internal {

/**
 * Captures log lines within the current scope.
 *
 * Captured lines are exposed via the `ScopedLog::ExtractLines` method.
 *
 * @par Example
 * @code
 * TEST(Foo, Bar) {
 *   ScopedLog log;
 *   ... call code that should log
 *   EXPECT_THAT(log.ExtractLines(), testing::Contains("foo"));
 * }
 * @endcode
 */

class ScopedLog
{
public:
    ScopedLog() : m_sink(std::make_shared<Sink>()), m_id(csa::internal::GetLogger()->AddSink(m_sink)) {}
    ~ScopedLog() { csa::internal::GetLogger()->RemoveSink(m_id); }

    std::vector<std::string> ExtractLines() { return m_sink->ExtractLines(); }

    class Sink : public csa::internal::SinkBase
    {
    public:
        std::vector<std::string> ExtractLines();
        void SinkRecord(csa::internal::LogRecord const& logRec) override;
        void Flush() override;

    private:
        std::mutex m_mu;
        std::vector<std::string> m_logLines;
    };

private:
    std::shared_ptr<Sink> m_sink;
    long m_id;
};

}  // namespace internal
}  // namespace testing
}  // namespace csa
