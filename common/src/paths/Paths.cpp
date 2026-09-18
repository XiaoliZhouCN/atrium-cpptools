// 路径解析实现。
// 约定（README §5.9）：不做任何工作区隐式探测；失败一律返回 Error，不返回空字符串。
#include "atrium/common/paths/Paths.hpp"

#include <cstdlib>
#include <filesystem>
#include <vector>

#if defined(_WIN32)
#include <cstdlib> // _dupenv_s
#include <windows.h>
#endif

namespace atrium::common
{

Outcome<Utf8> GetExecutablePath()
{
#if defined(_WIN32)
    std::vector<wchar_t> buffer(MAX_PATH);
    for (;;)
    {
        const DWORD written =
            ::GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (written == 0)
        {
            return MakeError(ErrorKind::Internal, "GetModuleFileNameW failed",
                             static_cast<int>(::GetLastError()));
        }
        if (written < buffer.size())
        {
            break;
        }
        buffer.resize(buffer.size() * 2);
    }

    const std::filesystem::path path(buffer.data());
    return Utf8(path.string());
#else
    return MakeError(ErrorKind::Unsupported,
                     "GetExecutablePath is not implemented for this platform yet");
#endif
}

Outcome<Utf8> GetExecutableDirectory()
{
    auto executable = GetExecutablePath();
    if (!executable)
    {
        return executable.GetError();
    }

    std::error_code ec;
    const std::filesystem::path parent =
        std::filesystem::path(executable.Value()).parent_path();
    if (parent.empty())
    {
        return MakeError(ErrorKind::Internal, "executable path has no parent directory");
    }
    return Utf8(parent.string());
}

Outcome<Utf8> ResolveResourcePath(const Utf8 &relativePath)
{
    if (relativePath.empty())
    {
        return MakeError(ErrorKind::InvalidArgument, "relativePath is empty");
    }

    auto directory = GetExecutableDirectory();
    if (!directory)
    {
        return directory.GetError();
    }

    std::error_code ec;
    const std::filesystem::path candidate =
        std::filesystem::path(directory.Value()) / std::filesystem::path(relativePath);
    const std::filesystem::path normalized = candidate.lexically_normal();

    if (!std::filesystem::exists(normalized, ec) || ec)
    {
        return MakeError(ErrorKind::NotFound, "resource not found: " + normalized.string());
    }
    if (!std::filesystem::is_regular_file(normalized, ec) || ec)
    {
        return MakeError(ErrorKind::InvalidArgument,
                         "resource is not a regular file: " + normalized.string());
    }

    return Utf8(normalized.string());
}

Outcome<Utf8> GetEnvironmentValue(const Utf8 &name)
{
    if (name.empty())
    {
        return MakeError(ErrorKind::InvalidArgument, "name is empty");
    }

#if defined(_WIN32)
    // 使用 _dupenv_s 而非 getenv：避免 MSVC C4996，且避免在多线程下返回共享缓冲的指针。
    // 注意：环境变量值按当前进程 ANSI 代码页返回；非 ASCII 值可能失真。
    // 若后续需要可靠的非 ASCII 环境变量，必须改用 _wdupenv_s + UTF-8 转换。
    char *value = nullptr;
    size_t size = 0;
    const errno_t status = ::_dupenv_s(&value, &size, name.c_str());
    if (status != 0)
    {
        return MakeError(ErrorKind::Internal, "failed to query environment variable: " + name,
                         static_cast<int>(status));
    }
    if (value == nullptr)
    {
        return MakeError(ErrorKind::NotFound, "environment variable not set: " + name);
    }
    Utf8 result(value);
    std::free(value);
    return result;
#else
    const char *value = std::getenv(name.c_str());
    if (value == nullptr)
    {
        return MakeError(ErrorKind::NotFound, "environment variable not set: " + name);
    }
    return Utf8(value);
#endif
}

} // namespace atrium::common
