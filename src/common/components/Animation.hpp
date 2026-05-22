/**
 * @file Animation.hpp
 * @brief 动画驱动组件
 *
 * 包含：AnimationTime / AnimationPosition / AnimationAlpha /
 *       AnimationScale / AnimationRenderOffset / AnimationColor /
 *       InteractiveAnimation
 */
#pragma once

#include "../Types.hpp"
#include "../Policies.hpp"
#include <optional>

namespace ui::components
{

/**
 * @brief 动画时间状态 (单位: 毫秒 ms)
 */
struct AnimationTime
{
    using is_component_tag = void;
    float duration = 200.0F; // 默认200毫秒
    float elapsed = 0.0F;
    policies::Easing easing = policies::Easing::LINEAR;
    policies::Play mode = policies::Play::ONCE;
    policies::AnimationState state = policies::AnimationState::PLAYING;
    bool autoCleanup = true;
};

/**
 * @brief 位置动画目标
 */
struct AnimationPosition
{
    using is_component_tag = void;
    Vec2 from;
    Vec2 to;
};

/**
 * @brief 透明度动画目标
 */
struct AnimationAlpha
{
    using is_component_tag = void;
    float from = 1.0F;
    float to = 0.0F;
};

/**
 * @brief 缩放动画目标
 */
struct AnimationScale
{
    using is_component_tag = void;
    Vec2 from{1.0F, 1.0F};
    Vec2 to{1.0F, 1.0F};
};

/**
 * @brief 渲染偏移动画目标
 */
struct AnimationRenderOffset
{
    using is_component_tag = void;
    Vec2 from{0.0F, 0.0F};
    Vec2 to{0.0F, 0.0F};
};

/**
 * @brief 颜色动画目标
 * 支持驱动常见可着色组件（如 Background、Text、Image、Icon 等）
 */
struct AnimationColor
{
    using is_component_tag = void;
    Color from;
    Color to;
};

/**
 * @brief 交互动效配置
 * 当发生交互时，自动触发 Tween 动画
 */
struct InteractiveAnimation
{
    using is_component_tag = void;

    // 悬停配置
    std::optional<Vec2> hoverScale;
    std::optional<Vec2> hoverOffset; // 悬停位移
    float hoverDuration = 200.0F;    // 毫秒

    // 按下配置
    std::optional<Vec2> pressScale;
    std::optional<Vec2> pressOffset; // 按下位移
    float pressDuration = 100.0F;    // 毫秒

    // 拖曳配置 (当组件支持拖拽时)
    std::optional<Vec2> dragScale;
    std::optional<Vec2> dragLiftOffset; // 拖起时的视觉偏移
    float dragDuration = 200.0F;

    // 原始状态记录（用于恢复）
    Vec2 normalScale{1.0F, 1.0F};
    Vec2 normalOffset{0.0F, 0.0F};
};

} // namespace ui::components
