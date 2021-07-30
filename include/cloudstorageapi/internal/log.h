// Copyright 2019 Andrew Karasyov
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

/**
 * This interface abstract out any underlaying log implementation.
 * Currently stdlog is used https://github.com/gabime/spdlog.
 * But could be replaced later at any time.
 *
 * Note: spdlog doesn't support operator<<() to chain output like
 * LOG() << a << b << c;
 * There is a good reason for this https://github.com/gabime/spdlog/pull/236
 *
 * Keep logging simple now. Try to use it only for development.
 * TODO: This configuration overwrites user defined spdlog configuration.
 *       Thus it is not suitable for releasing.
 *       Fix it if logging will go to release version.
 */

#define CSA_LOG_LEVEL_TRACE 0
#define CSA_LOG_LEVEL_DEBUG 1
#define CSA_LOG_LEVEL_INFO 2
#define CSA_LOG_LEVEL_WARN 3
#define CSA_LOG_LEVEL_ERROR 4
#define CSA_LOG_LEVEL_OFF 6

/**
 * Make it possible to define compile time logging level like
 * -DCSA_LOG_ACTIVE_LEVEL=CSA_LOG_LEVEL_TRACE
 */
#ifndef CSA_LOG_ACTIVE_LOG_LEVEL
#ifndef _NDEBUG
#define CSA_LOG_ACTIVE_LOG_LEVEL CSA_LOG_LEVEL_TRACE  // in debug switch on full logging
#else
#define CSA_LOG_ACTIVE_LOG_LEVEL CSA_LOG_LEVEL_INFO  // in release keep INFO logging level
#endif                                               // ##ifndef _NDEBUG
#endif                                               // #ifndef CSA_LOG_ACTIVE_LOG_LEVEL

// Need to define this to enable corresponding logging level
#define SPDLOG_ACTIVE_LEVEL CSA_LOG_ACTIVE_LOG_LEVEL

#include "spdlog/fmt/ostr.h"  // must be included to enable operator<< for user defined types to work for logging.
#include "spdlog/sinks/base_sink.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/spdlog.h"
#include <map>

namespace csa {
namespace internal {

class SinkBase;
struct LogRecord;

namespace detail {
class SpdSinkProxy : public spdlog::sinks::base_sink<std::mutex>
{
public:
    void SetSink(std::weak_ptr<SinkBase> sink);
    bool IsExpired() const;

protected:
    void sink_it_(const spdlog::details::log_msg& msg) override;

    void flush_();

private:
    std::weak_ptr<SinkBase> m_Sink;
};
}  // namespace detail

enum class ELogLevel : int
{
    Trace,
    Debug,
    Info,
    Warning,
    Error,
    Off
};

struct LogRecord
{
    ELogLevel m_logLevel = ELogLevel::Off;
    std::string m_file;
    std::string m_functionName;
    int m_lineNo = 0;
    std::chrono::system_clock::time_point m_timestamp;
    std::string m_message;
};

class SinkBase
{
public:
    SinkBase();

    virtual void SinkRecord(LogRecord const& logRec) = 0;
    virtual void Flush() = 0;

private:
    friend class Logger;
    std::shared_ptr<detail::SpdSinkProxy> m_SpdSinkProxy;
};

class Logger
{
public:
    static std::shared_ptr<Logger> Instance()
    {
        static auto instance = std::shared_ptr<Logger>(new Logger());
        return instance;
    }

    ~Logger()
    {
        // TODO: it's not a correct place to call spdlog::shotdown() because it's too late. Find correct one.
        spdlog::shutdown();
    }

    long AddSink(std::shared_ptr<SinkBase> sink);
    void RemoveSink(long id);
    void ClearSinks();
    std::size_t GetSinkCount() const;
    void Flush();

    template <typename... Args>
    void Log(std::string file, std::string function, int lineNo, ELogLevel logLevel, std::string fmt,
             const Args&... args)
    {
        if (ELogLevel::Off == logLevel)
            return;

        spdlog::level::level_enum level{spdlog::level::level_enum::err};
        switch (logLevel)
        {
        case ELogLevel::Trace:
            level = spdlog::level::level_enum::trace;
            break;
        case ELogLevel::Debug:
            level = spdlog::level::level_enum::debug;
            break;
        case ELogLevel::Info:
            level = spdlog::level::level_enum::info;
            break;
        case ELogLevel::Warning:
            level = spdlog::level::level_enum::warn;
            break;
        case ELogLevel::Error:
            level = spdlog::level::level_enum::err;
            break;

        default:
            assert(0 && "Unexpected log level.");
        }

        m_spdLogger->log(spdlog::source_loc{file.c_str(), lineNo, function.c_str()}, level, fmt, args...);
    }

private:
    mutable std::mutex m_mu;
    long m_nextId = 0;
    std::map<long, std::shared_ptr<SinkBase>> m_sinks;
    std::shared_ptr<spdlog::logger> m_spdLogger;

    Logger()
    {
        m_spdLogger = spdlog::rotating_logger_mt("csa_file_logger", "csa_file_logger.log", 1024 * 1024 * 5, 10);
    }

    void ClearSpdlogSinks();
};

// Get logger
inline std::shared_ptr<Logger> GetLogger() { return Logger::Instance(); }

#define CSA_LOG_TRACE(...) \
    csa::internal::GetLogger()->Log(__FILE__, __func__, __LINE__, csa::internal::ELogLevel::Trace, __VA_ARGS__)
#define CSA_LOG_DEBUG(...) \
    csa::internal::GetLogger()->Log(__FILE__, __func__, __LINE__, csa::internal::ELogLevel::Debug, __VA_ARGS__)
#define CSA_LOG_INFO(...) \
    csa::internal::GetLogger()->Log(__FILE__, __func__, __LINE__, csa::internal::ELogLevel::Info, __VA_ARGS__)
#define CSA_LOG_WARNING(...) \
    csa::internal::GetLogger()->Log(__FILE__, __func__, __LINE__, csa::internal::ELogLevel::Warning, __VA_ARGS__)
#define CSA_LOG_ERROR(...) \
    csa::internal::GetLogger()->Log(__FILE__, __func__, __LINE__, csa::internal::ELogLevel::Error, __VA_ARGS__)

}  // namespace internal
}  // namespace csa
