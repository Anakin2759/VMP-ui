/**
 * ************************************************************************
 *
 * @file ErrorCodes.hpp
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2026-05-19
 * @version 0.1
 * @brief 项目统一错误码 ui_errc + std::error_category 声明
 *
 * 设计依据：docs/architecture/result-type-design.md §3。
 * 单例定义见 ErrorCodes.cpp（跨 TU ODR 唯一）。
 *
 * ************************************************************************
 * @copyright Copyright (c) 2026 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */
#pragma once

#include <cstdint>
#include <format>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>

namespace ui
{

/// @brief 项目统一错误码枚举。每个段位预留 100 号方便扩展。
enum class UiErrc : std::uint16_t
{
    // 0 保留：成功（不应被使用，std::error_code{} 默认即 0 / null category）

    // 1xx：参数与上下文
    INVALID_ENTITY = 1,
    INVALID_ARGUMENT = 2,
    REGISTRY_UNAVAILABLE = 3,
    NOT_IMPLEMENTED = 4,

    // 2xx：层级与生命周期
    HIERARCHY_CYCLE = 100,
    HIERARCHY_DETACHED = 101,
    ENTITY_ALREADY_EXISTS = 102,

    // 3xx：资源加载
    ASSET_NOT_FOUND = 200,
    ASSET_LOAD_FAILED = 201,
    ASSET_DECODE_FAILED = 202,
    ASSET_UPLOAD_FAILED = 203,
    ATLAS_FULL = 204,
    GLYPH_RENDER_FAILED = 205,
    FILE_OPEN_FAILED = 206,

    // 4xx：GPU / 渲染
    DEVICE_UNAVAILABLE = 300,
    PIPELINE_UNAVAILABLE = 301,
    SHADER_COMPILE_FAILED = 302,
    SWAPCHAIN_UNAVAILABLE = 303,
    BACKEND_UNAVAILABLE = 304,
    WINDOW_CLAIM_FAILED = 305,
    BUFFER_MAP_FAILED = 306,

    // 5xx：主题与样式
    THEME_NOT_FOUND = 400,
    THEME_TYPE_MISMATCH = 401,

    // 6xx：脚本
    SCRIPT_PARSE_ERROR = 500,
    SCRIPT_RUNTIME_ERROR = 501,

    // 9xx：兜底
    UNKNOWN = 900,
};

/// @brief 统一 UI 错误分类。name() 固定返回 "ui"。
class UiErrorCategory final : public std::error_category
{
public:
    [[nodiscard]] const char* name() const noexcept override;
    [[nodiscard]] std::string message(int condition) const override;
};

/// @brief 进程级单例；地址唯一，跨 TU 比较稳定。
[[nodiscard]] const std::error_category& GetUiErrorCategory() noexcept;

/// @brief ADL 入口：将 UiErrc 转为 std::error_code。
[[nodiscard]] std::error_code MakeErrorCode(UiErrc errorCode) noexcept;

/// @brief 返回枚举对应的字符串名称（用于日志/格式化，独立于 message()）。
[[nodiscard]] std::string_view ToStringView(UiErrc errorCode) noexcept;

} // namespace ui

namespace std
{
template <>
struct is_error_code_enum<ui::UiErrc> : true_type
{
};
} // namespace std

/// @brief std::formatter<UiErrc> 特化：直接输出枚举名（如 "asset_decode_failed"）。
template <>
struct std::formatter<ui::UiErrc> : std::formatter<std::string_view>
{
    template <typename FormatContext>
    auto format(ui::UiErrc errorCode, FormatContext& ctx) const
    {
        return std::formatter<std::string_view>::format(ui::ToStringView(errorCode), ctx);
    }
};
