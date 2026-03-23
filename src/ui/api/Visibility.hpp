/**
 * ************************************************************************
 *
 * @file Visibility.hpp
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2026-01-30
 * @version 0.1
 * @brief 可见性和样式快捷操作 API

  提供简化的接口函数，方便用户快速设置UI实体的可见性、透明度、背景色、边框等属性。
  这些函数封装了对相关组件的操作，提升开发效率。
 *
 * ************************************************************************
 * @copyright Copyright (c) 2026 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */
#pragma once

#include <entt/entt.hpp>
#include "../common/Types.hpp"
#include "Chains.hpp" // Changed: Include Chains.hpp for DSL

namespace ui::visibility
{
/**
 * @brief 设置可见性
 * @param entity 实体ID
 * @param visible 是否可见
 */
void SetVisible(::entt::entity entity, bool visible);
void Show(::entt::entity entity);
void Hide(::entt::entity entity);
/**
 * @brief 设置透明度
 * @param entity 实体ID
 * @param alpha 透明度值（0.0 - 1.0）
 */
void SetAlpha(::entt::entity entity, float alpha);
void SetBackgroundColor(::entt::entity entity, const Color& color);
void SetBorderRadius(::entt::entity entity, float radius);
void SetBorderColor(::entt::entity entity, const Color& color);
void SetBorderThickness(::entt::entity entity, float thickness);

} // namespace ui::visibility

namespace ui::actions
{
namespace visibility
{
inline constexpr EntityAction<&ui::visibility::SetVisible> SET_VISIBLE_ACTION{};
inline constexpr EntityAction<&ui::visibility::Show> SHOW_ACTION{};
inline constexpr EntityAction<&ui::visibility::Hide> HIDE_ACTION{};
inline constexpr EntityAction<&ui::visibility::SetAlpha> SET_ALPHA_ACTION{};
inline constexpr EntityAction<&ui::visibility::SetBackgroundColor> SET_BACKGROUND_COLOR_ACTION{};
inline constexpr EntityAction<&ui::visibility::SetBorderRadius> SET_BORDER_RADIUS_ACTION{};
inline constexpr EntityAction<&ui::visibility::SetBorderColor> SET_BORDER_COLOR_ACTION{};
inline constexpr EntityAction<&ui::visibility::SetBorderThickness> SET_BORDER_THICKNESS_ACTION{};
} // namespace visibility
} // namespace ui::actions

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
} // namespace ui::chains
