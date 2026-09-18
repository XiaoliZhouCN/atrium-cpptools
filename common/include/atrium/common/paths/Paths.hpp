// 路径解析。
// 规范依据：README §5.9（工具不得自行猜测工作区路径）；
//           搬运自 NexusRenderer 红线 11（禁止裸相对路径依赖进程 CWD）与 PathResolver。
#pragma once

#include "atrium/common/error/Error.hpp"
#include "atrium/common/error/Outcome.hpp"
#include "atrium/common/text/Utf8.hpp"

namespace atrium::common
{

// 当前可执行文件所在目录（含文件名）。
[[nodiscard]] Outcome<Utf8> GetExecutablePath();

// 当前可执行文件所在目录。
[[nodiscard]] Outcome<Utf8> GetExecutableDirectory();

// 相对可执行文件目录解析资源路径。
// 约定：仅用于"随程序分发的只读资源"（图标、内置模板等）。
// 用户数据与内容仓库路径必须由调用方显式传入，禁止在此处做隐式探测。
[[nodiscard]] Outcome<Utf8> ResolveResourcePath(const Utf8 &relativePath);

// 读取环境变量；未设置时返回 NotFound（禁止静默返回空字符串）。
//
// ⚠ 命名注意：本函数【不能】叫做 GetEnvironmentVariable。
//   <windows.h> 把该名字定义为宏（GetEnvironmentVariableW/A），
//   任何同名函数都会被预处理器改写并导致链接期找不到符号。
//   同类需回避的 Windows 宏名还有：GetCurrentDirectory、GetTempPath、
//   CreateFile、DeleteFile、FindFirstFile 等（见下方 kWindowsMacroHazardNote）。
[[nodiscard]] Outcome<Utf8> GetEnvironmentValue(const Utf8 &name);

// 编译期守卫：若有人把本函数改回 Windows 宏名，这里会立刻失败。
// 保留本注释块是为了让 grep "GetEnvironmentVariable" 能命中此处并读到原因。
//
// kWindowsMacroHazardNote: see docs/BUILD.md「已知问题 → Windows 宏名冲突」。

} // namespace atrium::common
