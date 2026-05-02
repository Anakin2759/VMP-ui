/**
 * ************************************************************************
 *
 * @file GlobalContext.hpp
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2026-01-31
 * @version 0.1
 * @brief 全局渲染上下文组件定义
 *
 * ************************************************************************
 * @copyright Copyright (c) 2026 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */

#pragma once

#include "Types.hpp"
#include <entt/entt.hpp>

namespace ui::globalcontext
{
/**
 * @brief 全局获取帧间隔状态组件
 */
struct FrameContext
{
    using is_component_tag = void;
    uint32_t intervalMs = 0; // 时间间隔（毫秒）
    uint8_t frameSlot = 0;   // 当前帧变更槽位 0-1和1-0 是切换到下一帧
};

/**
 * @brief 定时器任务信息
 */
struct TimerTask
{
    uint32_t id;                          // 任务ID
    std::move_only_function<void()> func; // 任务函数
    uint32_t intervalMs;                  // 间隔时间（毫秒）
    uint32_t remainingMs;                 // 剩余时间（毫秒）
    bool singleShot;                      // 是否单次执行
    uint8_t frameSlot;                    // 帧槽位
    bool cancelled;                       // 是否已取消
};

/**
 * @brief 全局 UI 状态（从 StateSystem 提取）
 */
struct StateContext
{
    using is_component_tag = void;
    Vec2 latestMousePosition{0.0F, 0.0F};   // 全局最新鼠标位置
    Vec2 latestMouseDelta{0.0F, 0.0F};      // 全局最新鼠标移动增量
    Vec2 latestScrollDelta{0.0F, 0.0F};     // 全局最新滚轮滚动增量
    entt::entity focusedEntity{entt::null}; // 当前获得焦点的实体
    entt::entity activeEntity{entt::null};  // 当前处于活动状态的实体（鼠标按下）
    entt::entity hoveredEntity{entt::null}; // 当前悬停的实体
    bool activeDragMoved{false};            // 当前按压实体在本次手势中是否已经发生拖拽

    // 拖拽状态
    bool isDraggingScrollbar{false};
    entt::entity dragScrollEntity{entt::null};
    Vec2 dragStartMousePos{0.0F, 0.0F};
    Vec2 dragStartScrollOffset{0.0F, 0.0F};
    bool isVerticalDrag{true};
    float dragTrackLength{0.0F}; // 轨道可活动区域长度
    float dragThumbSize{0.0F};   // 滑块大小

    void syncLatestPointer(const Vec2& position, const Vec2& delta)
    {
        latestMousePosition = position;
        latestMouseDelta = delta;
    }

    void syncLatestScroll(const Vec2& delta)
    {
        latestScrollDelta = delta;
    }

    [[nodiscard]] bool hasScrollbarDrag() const
    {
        return isDraggingScrollbar && dragScrollEntity != entt::null;
    }

    void beginScrollbarDrag(
        entt::entity entity,
        const Vec2& mousePosition,
        const Vec2& scrollOffset,
        bool vertical,
        float trackLength,
        float thumbSize)
    {
        isDraggingScrollbar = true;
        dragScrollEntity = entity;
        dragStartMousePos = mousePosition;
        dragStartScrollOffset = scrollOffset;
        isVerticalDrag = vertical;
        dragTrackLength = trackLength;
        dragThumbSize = thumbSize;
    }

    void stopScrollbarDrag()
    {
        isDraggingScrollbar = false;
        dragScrollEntity = entt::null;
        dragStartMousePos = Vec2{0.0F, 0.0F};
        dragStartScrollOffset = Vec2{0.0F, 0.0F};
        isVerticalDrag = true;
        dragTrackLength = 0.0F;
        dragThumbSize = 0.0F;
    }

    /**
     * @brief 重置所有状态
     */
    void reset()
    {
        latestMousePosition = Vec2{0.0F, 0.0F};
        latestMouseDelta = Vec2{0.0F, 0.0F};
        latestScrollDelta = Vec2{0.0F, 0.0F};
        focusedEntity = entt::null;
        activeEntity = entt::null;
        hoveredEntity = entt::null;
        activeDragMoved = false;

        stopScrollbarDrag();
    }
};

} // namespace ui::globalcontext