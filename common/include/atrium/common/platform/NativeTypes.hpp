// 窗口句柄等平台无关的 POD 定义。
// 规范依据：README §2.2.1（跨模块只传 POD）、§7.3（跨边界不传 STL 所有权）。
// 参考来源：Projects/NexusRenderer 的 Shared/Platform/NativeTypes.hpp（搬运，见 README §2.2）。
#pragma once

namespace atrium::common
{

// 原生窗口句柄：Windows 为 HWND，macOS 为 NSView*。
// 只作为不透明句柄传递，接收方自行按平台转换，禁止在本类型上附加生命周期语义。
using NativeWindowHandle = void *;

} // namespace atrium::common
