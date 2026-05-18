/**
 * ************************************************************************
 *
 * @file layout.hpp
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2026-01-27
 * @version 0.1
 * @brief 布局API封装
  - 提供设置布局方向、间距和内边距的接口
  - 支持标记布局脏标志以触发重新布局
  - 基于ECS组件实现布局状态管理
  - 简化UI元素的布局控制逻辑
 *
 * ************************************************************************
 * @copyright Copyright (c) 2026 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */
#pragma once

#include <entt/entt.hpp>
#include "../common/Policies.hpp"
#include "Chains.hpp" // Changed: Include Chains.hpp for DSL

namespace ui::layout
{

void SetLayoutDirection(::entt::entity entity, policies::LayoutDirection direction);
void SetLayoutSpacing(::entt::entity entity, float spacing);
void SetPadding(::entt::entity entity, float left, float top, float right, float bottom);
void SetPadding(::entt::entity entity, float padding);
void CenterInParent(::entt::entity entity);

} // namespace ui::layout

namespace ui::actions
{
namespace layout
{
inline constexpr EntityAction<&ui::layout::SetLayoutDirection> SET_LAYOUT_DIRECTION_ACTION{};
inline constexpr EntityAction<&ui::layout::SetLayoutSpacing> SET_LAYOUT_SPACING_ACTION{};
inline constexpr EntityAction<static_cast<void (*)(::entt::entity, float, float, float, float)>(ui::layout::SetPadding)>
    SET_PADDING_EDGES_ACTION{};
inline constexpr EntityAction<static_cast<void (*)(::entt::entity, float)>(ui::layout::SetPadding)>
    SET_PADDING_ALL_ACTION{};
inline constexpr EntityAction<&ui::layout::CenterInParent> CENTER_IN_PARENT_ACTION{};
} // namespace layout
} // namespace ui::actions

namespace ui::chains
{
inline auto LayoutDirection(ui::policies::LayoutDirection direction)
{
    return ui::actions::layout::SET_LAYOUT_DIRECTION_ACTION.bind(direction);
}
inline auto Spacing(float spacing)
{
    return ui::actions::layout::SET_LAYOUT_SPACING_ACTION.bind(spacing);
}
inline auto Padding(float left, float top, float right, float bottom)
{
    return ui::actions::layout::SET_PADDING_EDGES_ACTION.bind(left, top, right, bottom);
}
inline auto Padding(float padding)
{
    return ui::actions::layout::SET_PADDING_ALL_ACTION.bind(padding);
}
inline auto Center()
{
    return ui::actions::layout::CENTER_IN_PARENT_ACTION.bind();
}
} // namespace ui::chains
