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
#include "Utils.hpp"
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

namespace ui::chains
{
inline auto Visible(bool v)
{
    return Call<ui::visibility::SetVisible>(v);
}
inline auto Show()
{
    return Call<ui::visibility::Show>();
}
inline auto Hide()
{
    return Call<ui::visibility::Hide>();
}
inline auto Alpha(float v)
{
    return Call<ui::visibility::SetAlpha>(v);
}
inline auto BackgroundColor(const Color& c)
{
    return Call<ui::visibility::SetBackgroundColor>(c);
}
inline auto BorderRadius(float r)
{
    return Call<ui::visibility::SetBorderRadius>(r);
}
inline auto BorderColor(const Color& c)
{
    return Call<ui::visibility::SetBorderColor>(c);
}
inline auto BorderThickness(float t)
{
    return Call<ui::visibility::SetBorderThickness>(t);
}
} // namespace ui::chains
