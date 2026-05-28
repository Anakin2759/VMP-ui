/**
 * @file Visual.hpp
 * @brief 纯视觉渲染组件（不影响布局，只影响渲染外观）
 *
 * 包含：Scale / RenderOffset / Alpha / Background / Border / Shadow / Title / Targetable
 */
#pragma once

#include <optional>

#include "../Types.hpp"
#include "../Policies.hpp"

namespace ui::components
{

/**
 * @brief 缩放组件（渲染时生效，不影响布局）
 */
struct Scale
{
    using is_component_tag = void;
    Vec2 value{1.0F, 1.0F};
};

/**
 * @brief 渲染偏移组件（渲染时生效，不影响布局）
 * 用于实现按下下沉、悬浮上浮等视觉效果
 */
struct RenderOffset
{
    using is_component_tag = void;
    Vec2 value{0.0F, 0.0F};
};

/**
 * @brief 透明度组件（叠加）
 */
struct Alpha
{
    using is_component_tag = void;
    float value = 1.0F;
};

/**
 * @brief 背景绘制组件
 */
struct Background
{
    using is_component_tag = void;
    Color color{0.0F, 0.0F, 0.0F, 0.0F};
    Vec4 borderRadius{0.0F, 0.0F, 0.0F, 0.0F}; // 圆角半径 (x:TopLeft, y:TopRight, z:BottomRight, w:BottomLeft)
    policies::Feature enabled = policies::Feature::DISABLED; // 是否启用背景绘制
};

/**
 * @brief 边框组件
 */
struct Border
{
    using is_component_tag = void;
    Color color{1.0F, 1.0F, 1.0F, 1.0F};
    float thickness = 1.0F;
    Vec4 borderRadius{0.0F, 0.0F, 0.0F, 0.0F}; // 圆角半径
    policies::Feature enabled = policies::Feature::DISABLED;
};

/**
 * @brief 阴影组件
 */
struct Shadow
{
    using is_component_tag = void;
    float softness{};                    // 阴影柔和度
    Vec2 offset{0.0F, 0.0F};             // 阴影偏移 (x, y)
    Color color{0.0F, 0.0F, 0.0F, 1.0F}; // 阴影颜色
    policies::Feature enabled = policies::Feature::DISABLED;
};

/**
 * @brief 标题组件 (通常用于 Window/Dialog)
 */
struct Title
{
    using is_component_tag = void;
    std::string text;
};

/**
 * @brief 可选中/目标化组件
 */
struct Targetable
{
    using is_component_tag = void;
    int priority = 0;
    policies::Feature selectable = policies::Feature::DISABLED;
};

struct ThemeStyleState
{
    using is_component_tag = void;
    std::optional<Color> managedBackgroundColor;
    std::optional<Vec4> managedBackgroundRadius;
    std::optional<Color> managedBorderColor;
    std::optional<Vec4> managedBorderRadius;
    std::optional<Color> managedTextColor;
    std::optional<Color> managedIndicatorColor;
    std::optional<Color> managedAuxiliaryColor;
};

} // namespace ui::components
