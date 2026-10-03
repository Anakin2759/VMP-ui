/**
 * ************************************************************************
 *
 * @file Layout.hpp
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2026-01-27
 * @version 0.1
 * @brief 布局 API 封装
 *
 * ************************************************************************
 * @copyright Copyright (c) 2026 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */
#pragma once

#include "ui/Policies.hpp"
#include "ui/Anchors.hpp"
#include "ui/Result.hpp"
#include "ui/api/Chains.hpp"
#include "ui/api/Entity.hpp"

namespace ui::layout
{
/// 设置相对父节点锚点；Runtime 由 Application 内部上下文绑定，公共接口不暴露 Runtime。
[[nodiscard]] Result<void> SetAnchors(ui::entity entity, const AnchorSet& anchors);
[[nodiscard]] Result<void> SetAnchors(ui::entity entity, const ParentAnchors& anchors);
[[nodiscard]] Result<void> SetAnchor(ui::entity entity, AnchorLine line, AnchorTarget target,
                                      float offset = 0.0F);
[[nodiscard]] Result<void> SetAnchor(ui::entity entity, AnchorLine line, AnchorTarget target,
                                      AnchorLine targetLine, float offset = 0.0F);
[[nodiscard]] Result<void> ClearAnchor(ui::entity entity, AnchorLine line);
[[nodiscard]] Result<void> SetAnchorMargins(ui::entity entity, const AnchorMargins& margins);
[[nodiscard]] Result<void> SetAnchorMargin(ui::entity entity, AnchorLine line, float margin);
[[nodiscard]] Result<void> SetBaselineOffset(ui::entity entity, float offset);
[[nodiscard]] Result<void> SetAnchorFill(ui::entity entity, AnchorTarget target = AnchorTarget::Parent(),
                                          float margin = 0.0F);
[[nodiscard]] Result<void> SetAnchorCenterIn(ui::entity entity, AnchorTarget target = AnchorTarget::Parent(),
                                              float offsetX = 0.0F, float offsetY = 0.0F);
[[nodiscard]] Result<void> ClearAllAnchors(ui::entity entity);
/// 清除实体锚点并恢复普通布局行为。
[[nodiscard]] Result<void> ClearAnchors(ui::entity entity);
void SetLayoutDirection(ui::entity entity, policies::LayoutDirection direction);
void SetLayoutSpacing(ui::entity entity, float spacing);
void SetPadding(ui::entity entity, float left, float top, float right, float bottom);
void SetPadding(ui::entity entity, float padding);
void CenterInParent(ui::entity entity);
}  // namespace ui::layout

namespace ui::actions::layout
{
inline constexpr EntityAction<static_cast<void (*)(ui::entity, policies::LayoutDirection)>(
    &ui::layout::SetLayoutDirection)>
    SET_LAYOUT_DIRECTION_ACTION{};
inline constexpr EntityAction<static_cast<void (*)(ui::entity, float)>(&ui::layout::SetLayoutSpacing)>
    SET_LAYOUT_SPACING_ACTION{};
inline constexpr EntityAction<static_cast<void (*)(ui::entity, float, float, float, float)>(
    ui::layout::SetPadding)>
    SET_PADDING_EDGES_ACTION{};
inline constexpr EntityAction<static_cast<void (*)(ui::entity, float)>(ui::layout::SetPadding)>
    SET_PADDING_ALL_ACTION{};
inline constexpr EntityAction<&ui::layout::CenterInParent> CENTER_IN_PARENT_ACTION{};
}  // namespace ui::actions::layout

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

/// 将实体锚定到父节点中心；Runtime 由 Application 内部上下文绑定。
inline auto AnchorCenterIn(float offsetX = 0.0F, float offsetY = 0.0F)
{
    return Chain{[offsetX, offsetY](ui::entity entity) {
        (void)static_cast<bool>(ui::layout::SetAnchorCenterIn(entity, ui::AnchorTarget::Parent(), offsetX, offsetY));
    }};
}

/// 将实体的四条边锚定到父节点，并保留指定内边距。
inline auto AnchorFill(float margin = 0.0F)
{
    return Chain{[margin](ui::entity entity) {
        (void)static_cast<bool>(ui::layout::SetAnchorFill(entity, ui::AnchorTarget::Parent(), margin));
    }};
}

inline auto AnchorLeft(AnchorTarget target = AnchorTarget::Parent(), float offset = 0.0F)
{
    return Chain{[target, offset](ui::entity entity) {
        (void)static_cast<bool>(ui::layout::SetAnchor(entity, ui::AnchorLine::Left, target, offset));
    }};
}

inline auto AnchorRight(AnchorTarget target = AnchorTarget::Parent(), float offset = 0.0F)
{
    return Chain{[target, offset](ui::entity entity) {
        (void)static_cast<bool>(ui::layout::SetAnchor(entity, ui::AnchorLine::Right, target, offset));
    }};
}

inline auto AnchorTop(AnchorTarget target = AnchorTarget::Parent(), float offset = 0.0F)
{
    return Chain{[target, offset](ui::entity entity) {
        (void)static_cast<bool>(ui::layout::SetAnchor(entity, ui::AnchorLine::Top, target, offset));
    }};
}

inline auto AnchorBottom(AnchorTarget target = AnchorTarget::Parent(), float offset = 0.0F)
{
    return Chain{[target, offset](ui::entity entity) {
        (void)static_cast<bool>(ui::layout::SetAnchor(entity, ui::AnchorLine::Bottom, target, offset));
    }};
}

inline auto AnchorHorizontalCenter(AnchorTarget target = AnchorTarget::Parent(), float offset = 0.0F)
{
    return Chain{[target, offset](ui::entity entity) {
        (void)static_cast<bool>(ui::layout::SetAnchor(entity, ui::AnchorLine::HorizontalCenter, target, offset));
    }};
}

inline auto AnchorVerticalCenter(AnchorTarget target = AnchorTarget::Parent(), float offset = 0.0F)
{
    return Chain{[target, offset](ui::entity entity) {
        (void)static_cast<bool>(ui::layout::SetAnchor(entity, ui::AnchorLine::VerticalCenter, target, offset));
    }};
}

inline auto AnchorBaseline(AnchorTarget target = AnchorTarget::Parent(), float offset = 0.0F)
{
    return Chain{[target, offset](ui::entity entity) {
        (void)static_cast<bool>(ui::layout::SetAnchor(entity, ui::AnchorLine::Baseline, target, offset));
    }};
}

inline auto AnchorMargins(const ui::AnchorMargins& margins)
{
    return Chain{[margins](ui::entity entity) {
        (void)static_cast<bool>(ui::layout::SetAnchorMargins(entity, margins));
    }};
}

inline auto ClearAnchor()
{
    return Chain{[](ui::entity entity) {
        (void)static_cast<bool>(ui::layout::ClearAllAnchors(entity));
    }};
}
}  // namespace ui::chains
