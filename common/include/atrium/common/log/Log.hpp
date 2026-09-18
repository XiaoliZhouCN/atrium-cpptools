// 日志门面。
// 规范依据：README §5.6（log 上报）、§5.4.1（禁止用 stdout 充当协议通道）。
// 设计约束：本模块不持有跨帧业务状态；sink 由宿主或工具自身注入。
#pragma once

#include "atrium/common/text/Utf8.hpp"

#include <cstdint>
#include <source_location>

namespace atrium::common
{

enum class LogLevel : std::uint8_t
{
    Trace = 0,
    Debug,
    Info,
    Warn,
    Error,
    Fatal,
};

// 日志接收端。实现方负责线程安全与落盘策略。
class ILogSink
{
public:
    virtual ~ILogSink() = default;

    virtual void Write(LogLevel level, Utf8View message,
                       const std::source_location &location) noexcept = 0;
};

// 注入 sink；返回被替换的旧 sink（可能为空）。
// 说明：实现必须在多线程下安全，且不依赖 sink 的生命周期长于调用方。
ILogSink *SetLogSink(ILogSink *sink) noexcept;

// 取当前 sink（可能为空；为空时实现走最小默认输出）。
ILogSink *GetLogSink() noexcept;

void Log(LogLevel level, Utf8View message,
         const std::source_location &location = std::source_location::current()) noexcept;

} // namespace atrium::common
