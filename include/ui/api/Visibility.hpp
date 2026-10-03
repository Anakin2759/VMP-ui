/**
 * @file Visibility.hpp
 * @brief 可见性和基础视觉样式快捷操作 API。
 */
#pragma once

#include "ui/Color.hpp"
#include "ui/api/Chains.hpp"
#include "ui/api/Entity.hpp"

namespace ui::visibility
{
void SetVisible(ui::entity entity, bool visible); // 设置实体的可见性
void Show(ui::entity entity);
void Hide(ui::entity entity);
void SetAlpha(ui::entity entity, float alpha);
void SetBackgroundColor(ui::entity entity, const Color& color);
void SetBorderRadius(ui::entity entity, float radius);
void SetBorderColor(ui::entity entity, const Color& color);
void SetBorderThickness(ui::entity entity, float thickness);
}  // namespace ui::visibility


/**
 * @brief 这个命名空间提供了可见性和基础视觉样式的链式操作接口。
 * 这些操作可以直接应用于 UI 实体，允许开发者以更直观的方式设置 UI 元素的可见性和视觉样式。
 */
namespace ui::actions::visibility
{
inline constexpr EntityAction<&ui::visibility::SetVisible> SET_VISIBLE_ACTION{}; // 设置可见性
inline constexpr EntityAction<&ui::visibility::Show> SHOW_ACTION{}; // 显示实体
inline constexpr EntityAction<&ui::visibility::Hide> HIDE_ACTION{};
inline constexpr EntityAction<&ui::visibility::SetAlpha> SET_ALPHA_ACTION{};
inline constexpr EntityAction<&ui::visibility::SetBackgroundColor> SET_BACKGROUND_COLOR_ACTION{};
inline constexpr EntityAction<&ui::visibility::SetBorderRadius> SET_BORDER_RADIUS_ACTION{};
inline constexpr EntityAction<&ui::visibility::SetBorderColor> SET_BORDER_COLOR_ACTION{};
inline constexpr EntityAction<&ui::visibility::SetBorderThickness> SET_BORDER_THICKNESS_ACTION{};
}  // namespace ui::actions::visibility 

namespace ui::chains
{
inline auto Visible(bool visible)
{
    return ui::actions::visibility::SET_VISIBLE_ACTION.bind(visible);
}
inline auto Show()
{
    return ui::actions::visibility::SHOW_ACTION.bind();
}
inline auto Hide()
{
    return ui::actions::visibility::HIDE_ACTION.bind();
}
inline auto Alpha(float alpha)
{
    return ui::actions::visibility::SET_ALPHA_ACTION.bind(alpha);
}
inline auto BackgroundColor(const Color& color)
{
    return ui::actions::visibility::SET_BACKGROUND_COLOR_ACTION.bind(color);
}
inline auto BorderRadius(float radius)
{
    return ui::actions::visibility::SET_BORDER_RADIUS_ACTION.bind(radius);
}
inline auto BorderColor(const Color& color)
{
    return ui::actions::visibility::SET_BORDER_COLOR_ACTION.bind(color);
}
inline auto BorderThickness(float thickness)
{
    return ui::actions::visibility::SET_BORDER_THICKNESS_ACTION.bind(thickness);
}
}  // namespace ui::chains