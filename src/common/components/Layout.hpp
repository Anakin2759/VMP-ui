/**
 * @file Layout.hpp
 * @brief 布局与几何结构组件
 *
 * 包含：Size / Position / CanvasSize / Margin / Padding /
 *       Hierarchy / ZOrderIndex / LayoutInfo / Spacer /
 *       Arrow / LineInfo / VerticalScrollbarGeometry
 */
#pragma once

#include "../Types.hpp"
#include "../Policies.hpp"
#include <cfloat>
#include <vector>
#include <entt/entt.hpp>

namespace ui::components
{

/**
 * @brief 尺寸约束组件
 */
struct Size
{
    using is_component_tag = void;
    Vec2 size{0.0F, 0.0F};
    Vec2 minSize{0.0F, 0.0F};
    Vec2 maxSize{FLT_MAX, FLT_MAX};
    policies::Size sizePolicy = policies::Size::Auto; // 宽度策略

    float percentage = 1.0F; // 百分比值 (当策略为 Percentage 时使用，0.0-1.0)
};

/**
 * @brief UI 元素的相对位置组件
 */
struct Position
{
    using is_component_tag = void;
    Vec2 value{0.0F, 0.0F};
    policies::Position positionPolicy = policies::Position::Fixed;
};

/**
 * @brief UI画布/屏幕尺寸
 *
 * 该组件推荐存放在 registry.ctx() 中，供 LayoutSystem 等逻辑系统读取。
 */
struct CanvasSize
{
    using is_component_tag = void;
    Vec2 value{0.0F, 0.0F};
};

/**
 * @brief 边距组件 (外边距)
 */
struct Margin
{
    using is_component_tag = void;
    Vec4 values{0.0F, 0.0F, 0.0F, 0.0F}; // Top, Right, Bottom, Left
};

/**
 * @brief 内边距组件
 * 定义元素内容与其边框之间的空间。
 */
struct Padding
{
    using is_component_tag = void;
    Vec4 values{0.0F, 0.0F, 0.0F, 0.0F}; // Top, Right, Bottom, Left
};

/**
 * @brief 层级关系组件 - Parent-Children 结构
 */
struct Hierarchy
{
    using is_component_tag = void;
    entt::entity parent = entt::null;
    std::vector<entt::entity> children; // 存储所有子节点
};

/**
 * @brief Z轴顺序组件 - 控制元素的前后显示顺序
 */
struct ZOrderIndex
{
    using is_component_tag = void;
    int value = 0; // Z 顺序值，值越大越靠前
};

/**
 * @brief 布局容器配置组件
 */
struct LayoutInfo
{
    using is_component_tag = void;
    static constexpr float DEFAULT_SPACING = 5.0F;
    policies::LayoutDirection direction = policies::LayoutDirection::HORIZONTAL; // 布局方向
    policies::Alignment alignment = policies::Alignment::CENTER;                 // 元素对齐方式
    float spacing = DEFAULT_SPACING;                                             // 元素间距
};

/**
 * @brief 间隔器配置组件
 */
struct Spacer
{
    using is_component_tag = void;
    uint8_t stretchFactor = 1; // 默认拉伸因子
};

/**
 * @brief 箭头组件
 */
struct Arrow
{
    using is_component_tag = void;
    static constexpr float DEFAULT_THICKNESS = 2.0F;
    static constexpr float DEFAULT_ARROW_SIZE = 10.0F;
    Vec2 startPoint{0.0F, 0.0F};
    Vec2 endPoint{100.0F, 100.0F};
    Color color{1.0F, 1.0F, 1.0F, 1.0F};
    float thickness = DEFAULT_THICKNESS;
    float arrowSize = DEFAULT_ARROW_SIZE;
};

/**
 * @brief 线条组件
 */
struct LineInfo
{
    using is_component_tag = void;
    static constexpr float DEFAULT_THICKNESS = 2.0F;
    Vec2 startPoint{0.0F, 0.0F};
    Vec2 endPoint{100.0F, 0.0F};
    Vec4 color{1.0F, 1.0F, 1.0F, 1.0F};
    float thickness = DEFAULT_THICKNESS;
};

/**
 * @brief 纵向滚动条几何缓存（由 LayoutSystem 写入，Renderer 只读）
 */
struct VerticalScrollbarGeometry
{
    using is_component_tag = void;
    bool visible = false;
    Rect containerRect;
    Rect viewportRect;
    Rect trackRect;
    Rect thumbRect;
    float viewportHeight = 0.0F;
    float contentHeight = 0.0F;
    float trackHeight = 0.0F;
    float thumbHeight = 0.0F;
    float maxScroll = 0.0F;
};

} // namespace ui::components
