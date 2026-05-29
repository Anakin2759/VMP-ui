/**
 * ************************************************************************
 *
 * @file Result.hpp
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2026-05-19
 * @version 0.1
 * @brief 项目级 Result<T> 别名（std::expected<T, std::error_code>）
 *
 * 设计依据：docs/architecture/result-type-design.md §2。
 * 本头文件仅声明类型别名 + 极少量工厂函数，不依赖具体错误码定义；
 * 具体错误码请见 ErrorCodes.hpp。
 *
 * ************************************************************************
 * @copyright Copyright (c) 2026 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */
#pragma once

#include <expected>
#include <system_error>
#include <type_traits>
#include <utility>

namespace ui
{

/// @brief 主别名：T 可为 void。
template <typename T>
using Result = std::expected<T, std::error_code>;

/// @brief 显式错误包装，调用点更醒目。
[[nodiscard]] inline std::unexpected<std::error_code> MakeError(std::error_code errorCode) noexcept
{
    return std::unexpected<std::error_code>{errorCode};
}

/// @brief 任何 is_error_code_enum 特化过的错误枚举均可直接传入。
template <typename E>
    requires std::is_error_code_enum_v<E>
[[nodiscard]] inline std::unexpected<std::error_code> MakeError(E errorValue) noexcept
{
    return std::unexpected<std::error_code>{MakeErrorCode(errorValue)};
}

/// @brief 成功值显式构造。
template <typename T>
[[nodiscard]] inline Result<std::remove_cvref_t<T>> Ok(T&& value)
{
    return Result<std::remove_cvref_t<T>>{std::forward<T>(value)};
}

/// @brief Result<void> 的成功值显式构造。
[[nodiscard]] inline Result<void> Ok() noexcept
{
    return Result<void>{};
}

} // namespace ui
