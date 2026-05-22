/**
 * @file Interaction.hpp
 * @brief 输入交互组件（点击、悬停、拖拽、滚动等）
 *
 * 包含：ScrollArea / ScrollBarInteractionState /
 *       Clickable / Hoverable / Pressable / Draggable / Checkable
 */
#pragma once

#include "../Types.hpp"
#include "../Policies.hpp"
#include <functional>

namespace ui::components
{

template <typename... Args>
using on_event = std::move_only_function<void(Args...)>;

/**
 * @brief 滚动区域组件
 */
struct ScrollArea
{
    using is_component_tag = void;
    static constexpr float DEFAULT_SCROLL_SPEED = 10.0F;
    static constexpr float SCROLLBAR_TRACK_WIDTH = 12.0F;
    static constexpr float SCROLLBAR_TRACK_PADDING = 2.0F;
    static constexpr float SCROLLBAR_THUMB_WIDTH = 10.0F;
    static constexpr float SCROLLBAR_THUMB_MIN_SIZE = 20.0F;
    static constexpr float SCROLLBAR_THUMB_INSET = 2.0F;
    Vec2 scrollOffset{0.0F, 0.0F};                        // 当前滚动位置
    Vec2 contentSize{0.0F, 0.0F};                         // 内容区域大小
    float scrollSpeed{DEFAULT_SCROLL_SPEED};              // 滚动速度
    policies::Scroll scroll = policies::Scroll::VERTICAL; // 滚动方向
    policies::ScrollBar scrollBar = policies::ScrollBar::DRAGGABLE;
    policies::ScrollAnchor anchor = policies::ScrollAnchor::TOP; // 滚动锚定策略
};

/**
 * @brief 滚动条运行时交互状态（独立于纯数据组件 ScrollArea）
 *
 * 当滚动条进入交互状态时由 StateSystem 动态 Emplace，
 * 交互结束后保留（StateSystem 负责更新）。
 * ScrollBarRenderer 通过 TryGet 读取此组件，缺失时使用默认样式。
 */
struct ScrollBarInteractionState
{
    using is_component_tag = void;
    bool scrollbarHovered{false}; // 滚动条滑块是否悬停
    bool scrollbarPressed{false}; // 滚动条滑块是否按下
    bool trackHovered{false};     // 滚动条轨道是否悬停
};

/**
 * @brief 可点击组件
 */
struct Clickable
{
    using is_component_tag = void;
    on_event<> onClick;
    policies::Feature enabled = policies::Feature::ENABLED;
};

/**
 * @brief 可拖拽组件
 * 标记实体可被鼠标拖拽移动
 */
struct Draggable
{
    using is_component_tag = void;
    policies::Feature enabled = policies::Feature::ENABLED;
    bool lockX = false; // 锁定X轴
    bool lockY = false; // 锁定Y轴
    bool dragging = false;

    // 拖拽开始/结束的回调
    on_event<> onDragStart;
    on_event<> onDragEnd;
    on_event<Vec2> onDragMove; // 参数为 delta
};

/**
 * @brief 可悬浮组件
 */
struct Hoverable
{
    using is_component_tag = void;
    on_event<> onHover;
    on_event<> onUnhover;
    policies::Feature enabled = policies::Feature::ENABLED;
};

/**
 * @brief 可按压组件 - 用于处理长按、拖动等场景
 */
struct Pressable
{
    using is_component_tag = void;
    on_event<> onPress;   // 鼠标按下时触发
    on_event<> onRelease; // 鼠标松开时触发
    policies::Feature enabled = policies::Feature::ENABLED;
};

/**
 * @brief 可选中组件
 */
struct Checkable
{
    using is_component_tag = void;
    policies::CheckState checked = policies::CheckState::UNCHECKED;
};

} // namespace ui::components
