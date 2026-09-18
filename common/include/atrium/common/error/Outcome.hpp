// Outcome<T> 的定义与实现（header-only）。
// 依据 README §7.2 第 3 条：失败必须非静默上报，禁止"返回空值 + 静默成功"。
#pragma once

#include "atrium/common/error/Error.hpp"

#include <cstdlib>
#include <utility>

namespace atrium::common
{

namespace detail
{
// 违反 Outcome 契约属于程序缺陷，立即终止而不是返回默认值掩盖错误。
[[noreturn]] inline void OutcomeContractViolation(const Error &error)
{
    (void)error;
    std::abort();
}
} // namespace detail

template <typename T> inline Outcome<T>::Outcome(T value) : m_value(std::move(value)) {}

template <typename T> inline Outcome<T>::Outcome(Error error) : m_error(std::move(error)) {}

template <typename T> inline bool Outcome<T>::IsOk() const noexcept
{
    return m_error.IsOk();
}

template <typename T> inline Outcome<T>::operator bool() const noexcept
{
    return IsOk();
}

template <typename T> inline const Error &Outcome<T>::GetError() const noexcept
{
    return m_error;
}

template <typename T> inline const T &Outcome<T>::Value() const
{
    if (!IsOk())
    {
        detail::OutcomeContractViolation(m_error);
    }
    return m_value;
}

template <typename T> inline T &Outcome<T>::Value()
{
    if (!IsOk())
    {
        detail::OutcomeContractViolation(m_error);
    }
    return m_value;
}

// 无返回值的 Outcome 用法：Outcome<void>。
template <> class Outcome<void>
{
public:
    Outcome() = default;
    Outcome(Error error) : m_error(std::move(error)) {}

    [[nodiscard]] bool IsOk() const noexcept { return m_error.IsOk(); }
    [[nodiscard]] explicit operator bool() const noexcept { return IsOk(); }
    [[nodiscard]] const Error &GetError() const noexcept { return m_error; }

private:
    Error m_error{};
};

} // namespace atrium::common
