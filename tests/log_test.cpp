// Copyright 2020 Andrew Karasyov
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

#include "cloudstorageapi/internal/log.h"
#include "spdlog/fmt/ostr.h"  // must be included to enable operator<< for user defined types to work for logging.
#include <gmock/gmock.h>

namespace {
/// A class to count calls to IOStream operator.
struct IOStreamCounter
{
    mutable int m_count;
};

std::ostream& operator<<(std::ostream& os, IOStreamCounter const& rhs)
{
    ++rhs.m_count;
    return os;
}

}  // namespace

// Need to declare outside of any namespace
template <>
struct fmt::formatter<IOStreamCounter> : ostream_formatter
{
};

namespace csa {
namespace internal {
namespace {

using ::testing::_;
using ::testing::ExitedWithCode;
using ::testing::HasSubstr;
using ::testing::Invoke;

TEST(LogLevel, CompileTimeLevel)
{
#ifndef _NDEBUG
    EXPECT_TRUE(CSA_LOG_ACTIVE_LOG_LEVEL == CSA_LOG_LEVEL_TRACE);
#else
    EXPECT_TRUE(CSA_LOG_ACTIVE_LOG_LEVEL == CSA_LOG_LEVEL_INFO);
#endif
}

class MockLogSink : public SinkBase
{
public:
    MOCK_METHOD1(SinkRecord, void(LogRecord const&));
    MOCK_METHOD0(Flush, void());
};

TEST(LoggerSinkTest, SinkAddRemove)
{
    // Expected default sink is there.
    size_t default_sinks_count = GetLogger()->GetSinkCount();
    EXPECT_TRUE(default_sinks_count == 0);
    long id = GetLogger()->AddSink(std::make_shared<MockLogSink>());
    EXPECT_TRUE(default_sinks_count + 1 == GetLogger()->GetSinkCount());
    GetLogger()->RemoveSink(id);
    EXPECT_TRUE(default_sinks_count == GetLogger()->GetSinkCount());
}

TEST(LoggerSinkTest, ClearSink)
{
    size_t default_sinks_count = GetLogger()->GetSinkCount();
    GetLogger()->AddSink(std::make_shared<MockLogSink>());
    GetLogger()->AddSink(std::make_shared<MockLogSink>());
    EXPECT_TRUE(GetLogger()->GetSinkCount() >= 2);
    GetLogger()->ClearSinks();
    EXPECT_TRUE(GetLogger()->GetSinkCount() == default_sinks_count);
}

TEST(LoggerSinkTest, MultiSinkMessage)
{
    auto mockSink1 = std::make_shared<MockLogSink>();
    auto mockSink2 = std::make_shared<MockLogSink>();
    EXPECT_CALL(*mockSink1, SinkRecord(_)).WillOnce(Invoke([](LogRecord const& lr) {
        EXPECT_EQ(true, std::string::npos != lr.m_message.find("test message"));
    }));
    GetLogger()->AddSink(mockSink1);

    EXPECT_CALL(*mockSink2, SinkRecord(_)).WillOnce(Invoke([](LogRecord const& lr) {
        EXPECT_EQ(true, std::string::npos != lr.m_message.find("test message"));
    }));
    GetLogger()->AddSink(mockSink2);

    CSA_LOG_ERROR("test message");

    GetLogger()->ClearSinks();
}

TEST(LoggerSinkTest, LogCheckCounter)
{
    IOStreamCounter counter{0};
    auto mockSink = std::make_shared<MockLogSink>();
    EXPECT_CALL(*mockSink, SinkRecord(_)).Times(2);
    GetLogger()->AddSink(mockSink);
    CSA_LOG_ERROR("count is {}", counter);
    CSA_LOG_WARNING("count is {}", counter);
    EXPECT_EQ(2, counter.m_count);
    GetLogger()->ClearSinks();
}

// Add more test like:
//   test log level,
//   test log level in runtime,
//   test disabled log levels.

}  // namespace
}  // namespace internal
}  // namespace csa
