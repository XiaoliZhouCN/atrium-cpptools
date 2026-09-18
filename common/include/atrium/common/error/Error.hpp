// 错误模型。
// 规范依据：README §7.2 第 3 条（失败必须非静默上报）、§5.8（退出码约定）。
// 设计约束：任何接口都不得用"返回空值 + 静默成功"表示失败。
#pragma once

#include <string>
#include <utility>

namespace atrium::common
{

// 退出码约定，与 README §5.8 一一对应。
enum class ExitCode : int
{
    Ok = 0,              // 正常退出（含响应 shutdown）
    RuntimeFailure = 1,  // 运行期失败
    PreconditionFailed = 2, // 启动前置条件不满足（缺依赖、路径不可用）
    IncompatibleProtocol = 3, // 协议版本或 manifest 不兼容
    SingletonConflict = 4,    // 单实例冲突
};

// 错误分类。新增分类必须同步 README，不允许实现侧私自扩展语义。
enum class ErrorKind
{
    None = 0,
    NotFound,         // 目标不存在
    InvalidArgument,  // 入参非法
    Unsupported,      // 当前实现不支持该能力（必须显式返回，禁止静默降级）
    IoFailure,        // 磁盘 / 设备 IO 失败
    DecodeFailure,    // 解码失败（媒体）
    CorruptedData,    // 数据损坏（格式不合法）
    Permission,       // 权限不足
    Busy,             // 资源被占用 / 忙
    Cancelled,        // 被调用方取消
    Internal,         // 内部错误（不应发生，出现即为缺陷）
};

struct Error
{
    ErrorKind Kind = ErrorKind::None;
    // 面向开发者的可读描述，UTF-8。
    std::string Message;
    // 平台原始错误码（如 HRESULT / GetLastError / errno），无则为 0。
    int PlatformCode = 0;

    [[nodiscard]] constexpr bool IsOk() const noexcept { return Kind == ErrorKind::None; }
};

inline Error Ok()
{
    return Error{};
}

inline Error MakeError(ErrorKind kind, std::string message, int platformCode = 0)
{
    return Error{kind, std::move(message), platformCode};
}

// 结果包装：失败必须携带 Error，调用方无法忽略。
template <typename T> class Outcome
{
public:
    Outcome(T value);
    Outcome(Error error);

    [[nodiscard]] bool IsOk() const noexcept;
    [[nodiscard]] explicit operator bool() const noexcept;

    [[nodiscard]] const Error &GetError() const noexcept;

    // 仅在 IsOk() 为真时可用；否则为缺陷，直接终止（不返回默认值掩盖错误）。
    [[nodiscard]] const T &Value() const;
    [[nodiscard]] T &Value();

private:
    T m_value{};
    Error m_error{};
};

} // namespace atrium::common
