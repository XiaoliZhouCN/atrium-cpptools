// 日志门面实现。
// 说明：日志是跨线程的，sink 指针用原子量保护；不持有业务状态（README 目录红线 D2）。
#include "atrium/common/log/Log.hpp"

#include <atomic>
#include <cstdio>

namespace atrium::common
{
namespace
{

std::atomic<ILogSink *> g_sink{nullptr};

const char *LevelName(LogLevel level) noexcept
{
    switch (level)
    {
    case LogLevel::Trace:
        return "TRACE";
    case LogLevel::Debug:
        return "DEBUG";
    case LogLevel::Info:
        return "INFO";
    case LogLevel::Warn:
        return "WARN";
    case LogLevel::Error:
        return "ERROR";
    case LogLevel::Fatal:
        return "FATAL";
    }
    return "UNKNOWN";
}

// 无 sink 时的最小输出。注意：stdout 归协议通道使用（README §5.4.1），日志只走 stderr。
void WriteToStderr(LogLevel level, Utf8View message) noexcept
{
    std::fprintf(stderr, "[%s] %.*s\n", LevelName(level), static_cast<int>(message.size()),
                 message.data());
}

} // namespace

ILogSink *SetLogSink(ILogSink *sink) noexcept
{
    return g_sink.exchange(sink, std::memory_order_acq_rel);
}

ILogSink *GetLogSink() noexcept
{
    return g_sink.load(std::memory_order_acquire);
}

void Log(LogLevel level, Utf8View message, const std::source_location &location) noexcept
{
    ILogSink *sink = GetLogSink();
    if (sink != nullptr)
    {
        sink->Write(level, message, location);
        return;
    }
    WriteToStderr(level, message);
}

} // namespace atrium::common
