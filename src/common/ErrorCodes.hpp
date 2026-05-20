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

#include <format>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>

namespace ui
{

/// @brief 项目统一错误码枚举。每个段位预留 100 号方便扩展。
enum class ui_errc : int
{
    // 0 保留：成功（不应被使用，std::error_code{} 默认即 0 / null category）

    // 1xx：参数与上下文
    invalid_entity        = 1,
    invalid_argument      = 2,
    registry_unavailable  = 3,
    not_implemented       = 4,

    // 2xx：层级与生命周期
    hierarchy_cycle       = 100,
    hierarchy_detached    = 101,
    entity_already_exists = 102,

    // 3xx：资源加载
    asset_not_found       = 200,
    asset_load_failed     = 201,
    asset_decode_failed   = 202,
    asset_upload_failed   = 203,
    atlas_full            = 204,
    glyph_render_failed   = 205,
    file_open_failed      = 206,

    // 4xx：GPU / 渲染
    device_unavailable    = 300,
    pipeline_unavailable  = 301,
    shader_compile_failed = 302,
    swapchain_unavailable = 303,
    backend_unavailable   = 304,
    window_claim_failed   = 305,
    buffer_map_failed     = 306,

    // 5xx：主题与样式
    theme_not_found       = 400,
    theme_type_mismatch   = 401,

    // 6xx：脚本
    script_parse_error    = 500,
    script_runtime_error  = 501,

    // 9xx：兜底
    unknown               = 900,
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

/// @brief ADL 入口：将 ui_errc 转为 std::error_code。
[[nodiscard]] std::error_code make_error_code(ui_errc e) noexcept;

/// @brief 返回枚举对应的字符串名称（用于日志/格式化，独立于 message()）。
[[nodiscard]] std::string_view ToStringView(ui_errc e) noexcept;

} // namespace ui

namespace std
{
template <>
struct is_error_code_enum<ui::ui_errc> : true_type
{
};
} // namespace std

/// @brief std::formatter<ui_errc> 特化：直接输出枚举名（如 "asset_decode_failed"）。
template <>
struct std::formatter<ui::ui_errc> : std::formatter<std::string_view>
{
    template <typename FormatContext>
    auto format(ui::ui_errc e, FormatContext& ctx) const
    {
        return std::formatter<std::string_view>::format(ui::ToStringView(e), ctx);
    }
};
