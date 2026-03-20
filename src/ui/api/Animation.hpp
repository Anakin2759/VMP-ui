/**
 * ************************************************************************
 *
 * @file animation.hpp
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2026-01-27
 * @version 0.1
 * @brief 动画API封装
  - 提供启动和停止位置与透明度动画的接口
  - 基于ECS组件实现动画状态管理
  - 简化动画控制逻辑，便于UI元素动画效果实现
 *
 * ************************************************************************
 * @copyright Copyright (c) 2026 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */
#pragma once

#include <optional>

#include <entt/entt.hpp>
#include "Utils.hpp"

namespace ui::animation
{
struct TweenOptions
{
    float duration = 200.0F;
    policies::Easing easing = policies::Easing::EASE_OUT_QUAD;
    policies::Play mode = policies::Play::ONCE;
    bool autoCleanup = true;
};

void StartPositionAnimation(::entt::entity entity,
                            const Vec2& startPos,
                            const Vec2& endPos,
                            const TweenOptions& options = {});
void StartAlphaAnimation(::entt::entity entity, float startAlpha, float endAlpha, const TweenOptions& options = {});
void StartScaleAnimation(::entt::entity entity,
                         const Vec2& startScale,
                         const Vec2& endScale,
                         const TweenOptions& options = {});
void StartRenderOffsetAnimation(::entt::entity entity,
                                const Vec2& startOffset,
                                const Vec2& endOffset,
                                const TweenOptions& options = {});
void StartColorAnimation(::entt::entity entity,
                         const Color& startColor,
                         const Color& endColor,
                         const TweenOptions& options = {});
void StartTransformAnimation(::entt::entity entity,
                             const std::optional<Vec2>& targetScale,
                             const std::optional<Vec2>& targetOffset,
                             const TweenOptions& options = {},
                             const Vec2& defaultScale = {1.0F, 1.0F},
                             const Vec2& defaultOffset = {0.0F, 0.0F});
void StopAnimation(::entt::entity entity);

} // namespace ui::animation
