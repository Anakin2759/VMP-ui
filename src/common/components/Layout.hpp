/**
 * @file Layout.hpp
 * @brief 布局与几何结构组件
 *
 * 包含：Size / Position / CanvasSize / Margin / Padding /
 *       Hierarchy / ZOrderIndex / LayoutInfo / Spacer /
 *       Arrow / LineInfo
 */
#pragma once

#include "common/Types.hpp"
#include "ui/Policies.hpp"
#include "ui/Anchors.hpp"
#include <cfloat>
#include <vector>
#include <entt/entt.hpp>

namespace ui::components
{

/// 锚点节点退出 Flex 排列，约束由 LayoutSystem 的局部 solver 解析。
struct Anchors
{
    using is_component_tag = void;
    ui::AnchorSet value;
    std::uint64_t revision = 0;
};

/// 内容测量得到的隐式尺寸，与用户声明的 Size 分离保存。
struct ImplicitSize
{
    using is_component_tag = void;
    Vec2 value{0.0F, 0.0F};
    bool hasWidth = false;
    bool hasHeight = false;
};

/**
 * @brief 尺寸约束组件
 */
struct Size
{
    using is_component_tag = void;
    Vec2 size{0.0F, 0.0F};
    Vec2 declaredSize{0.0F, 0.0F};
    bool hasDeclaredWidth = false;
    bool hasDeclaredHeight = false;
    Vec2 minSize{0.0F, 0.0F};
    Vec2 maxSize{FLT_MAX, FLT_MAX};
    policies::Size sizePolicy = policies::Size::AUTO;  // 宽度策略

    float percentage = 1.0F;  // 百分比值 (当策略为 Percentage 时使用，0.0-1.0)
};

/**
 * @brief UI 元素的相对位置组件
 */
struct Position
{
    using is_component_tag = void;
    Vec2 value{0.0F, 0.0F};
    policies::Position positionPolicy = policies::Position::FIXED;
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
    Vec4 values{0.0F, 0.0F, 0.0F, 0.0F};  // Top, Right, Bottom, Left
};

/**
 * @brief 内边距组件
 * 定义元素内容与其边框之间的空间。
 */
struct Padding
{
    using is_component_tag = void;
    Vec4 values{0.0F, 0.0F, 0.0F, 0.0F};  // Top, Right, Bottom, Left
};

/**
 * @brief 层级关系组件 - Parent-Children 结构
 */
struct Hierarchy
{
    using is_component_tag = void;
    entt::entity parent = entt::null;
    std::vector<entt::entity> children;  // 存储所有子节点
};

/**
 * @brief Z轴顺序组件 - 控制元素的前后显示顺序
 */
struct ZOrderIndex
{
    using is_component_tag = void;
    int value = 0;  // Z 顺序值，值越大越靠前
};

/**
 * @brief 布局容器配置组件
 */
struct LayoutInfo
{
    using is_component_tag = void;
    static constexpr float DEFAULT_SPACING = 5.0F;
    policies::LayoutDirection direction = policies::LayoutDirection::HORIZONTAL;  // 布局方向
    policies::Alignment alignment = policies::Alignment::CENTER;                  // 元素对齐方式
    float spacing = DEFAULT_SPACING;                                              // 元素间距
};

/**
 * @brief 间隔器配置组件
 */
struct Spacer
{
    using is_component_tag = void;
    uint8_t stretchFactor = 1;  // 默认拉伸因子
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

}  // namespace ui::components
