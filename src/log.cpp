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

namespace csa {
namespace internal {

namespace {
LogRecord ConvertLogMsg(spdlog::details::log_msg const& msg)
{
    LogRecord res;
    res.m_file = msg.source.filename;
    res.m_functionName = msg.source.funcname;
    res.m_lineNo = msg.source.line;
    res.m_message = std::string(msg.payload.begin(), msg.payload.end());
    res.m_timestamp = msg.time;
    switch (msg.level)
    {
    case spdlog::level::level_enum::trace:
        res.m_logLevel = ELogLevel::Trace;
        break;
    case spdlog::level::level_enum::debug:
        res.m_logLevel = ELogLevel::Debug;
        break;
    case spdlog::level::level_enum::info:
        res.m_logLevel = ELogLevel::Info;
        break;
    case spdlog::level::level_enum::warn:
        res.m_logLevel = ELogLevel::Warning;
        break;
    case spdlog::level::level_enum::err:
        res.m_logLevel = ELogLevel::Error;
        break;
    default:
        assert(0 && "Unexpected spdlog log level");
        res.m_logLevel = ELogLevel::Error;
    }
    return res;
}
}  // namespace

namespace detail {

void SpdSinkProxy::SetSink(std::weak_ptr<SinkBase> sink) { m_Sink = std::move(sink); }

bool SpdSinkProxy::IsExpired() const { return m_Sink.expired(); }

void SpdSinkProxy::sink_it_(const spdlog::details::log_msg& msg)
{
    if (auto sinkObj = m_Sink.lock())
    {
        sinkObj->SinkRecord(ConvertLogMsg(msg));
    }
}

void SpdSinkProxy::flush_()
{
    if (auto sinkObj = m_Sink.lock())
        sinkObj->Flush();
}

}  // namespace detail

SinkBase::SinkBase() : m_SpdSinkProxy(std::make_shared<detail::SpdSinkProxy>()) {}

long Logger::AddSink(std::shared_ptr<SinkBase> sink)
{
    sink->m_SpdSinkProxy->SetSink(sink);
    std::unique_lock<std::mutex>(m_mu);
    long id = ++m_nextId;
    m_sinks.emplace(id, sink);
    m_spdLogger->sinks().push_back(sink->m_SpdSinkProxy);
    return id;
}

void Logger::RemoveSink(long id)
{
    std::unique_lock<std::mutex> lk(m_mu);
    auto it = m_sinks.find(id);
    if (m_sinks.end() != it)
        m_sinks.erase(it);

    ClearSpdlogSinks();
}

void Logger::ClearSinks()
{
    std::unique_lock<std::mutex> lk(m_mu);
    m_sinks.clear();
    ClearSpdlogSinks();
}

std::size_t Logger::GetSinkCount() const
{
    std::unique_lock<std::mutex> lk(m_mu);
    return m_sinks.size();
}

void Logger::Flush() { m_spdLogger->flush(); }

void Logger::ClearSpdlogSinks()
{
    auto& sinks = m_spdLogger->sinks();
    sinks.erase(std::remove_if(sinks.begin(), sinks.end(),
                               [](auto const& sink) {
                                   // Check if it is SinkBase
                                   SinkBase* csaSink = dynamic_cast<SinkBase*>(sink.get());
                                   return csaSink && csaSink->m_SpdSinkProxy->IsExpired();
                               }),
                sinks.end());
}

}  // namespace internal
}  // namespace csa
