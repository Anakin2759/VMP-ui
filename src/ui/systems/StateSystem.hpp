/**
 * ************************************************************************
 *
 * @file StateSystem.h
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2025-12-11 (Updated)
 * @version 0.2
 * @brief 状态系统 - 处理UI状态生命周期和窗口状态同步
 *
 * 职责：
 * 1. 管理全局UI状态（焦点/激活/悬停实体）
 * 2. 同步窗口状态到ECS组件（如Resizable、Frameless等）
 * 3. 处理状态变更事件（Hover / Active / Focus）
 * 4. 处理窗口生命周期事件（关闭/移动/大小变化）
 *
 * 注意：Focus 和 Active 同时只有一个组件被操作，Hover 需要考虑父组件
 *
 * ************************************************************************
 * @copyright Copyright (c) 2025 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */

#pragma once
#include <entt/entt.hpp>
#include <cmath>
#include <algorithm>
#include <unordered_set>
#include <SDL3/SDL.h>
#include "api/Utils.hpp"
#include "../singleton/Registry.hpp"
#include "../singleton/Dispatcher.hpp"
#include "../common/Components.hpp"
#include "../common/Tags.hpp"
#include "../common/Policies.hpp"
#include "../interface/Isystem.hpp"
#include "../common/Events.hpp"
#include "HitTestSystem.hpp"
#include "../common/GlobalContext.hpp"
#include "../singleton/Logger.hpp"

namespace ui::systems
{

// GlobalState now lives in ui::globalcontext::GlobalState (see GlobalContext.hpp)

class StateSystem : public ui::interface::EnableRegister<StateSystem>
{
public:
    // 滚动条命中类型
    enum class ScrollbarHitType : std::uint8_t
    {
        NONE,  // 未命中
        THUMB, // 命中滑块
        TRACK  // 命中轨道
    };

    void registerHandlersImpl()
    {
        // 窗口事件
        Dispatcher::Sink<events::CloseWindow>().connect<&StateSystem::onCloseWindow>(*this);
        Dispatcher::Sink<events::WindowPixelSizeChanged>().connect<&StateSystem::onWindowPixelSizeChanged>(*this);
        Dispatcher::Sink<events::WindowMoved>().connect<&StateSystem::onWindowMoved>(*this);

        // 交互事件
        Dispatcher::Sink<events::HoverEvent>().connect<&StateSystem::onHoverEvent>(*this);
        Dispatcher::Sink<events::UnhoverEvent>().connect<&StateSystem::onUnhoverEvent>(*this);
        Dispatcher::Sink<events::MousePressEvent>().connect<&StateSystem::onMousePressEvent>(*this);
        Dispatcher::Sink<events::MouseReleaseEvent>().connect<&StateSystem::onMouseReleaseEvent>(*this);

        // 命中测试后的输入事件（由 HitTestSystem 发送）
        Dispatcher::Sink<events::HitPointerMove>().connect<&StateSystem::onHitPointerMove>(*this);
        Dispatcher::Sink<events::HitPointerButton>().connect<&StateSystem::onHitPointerButton>(*this);
        Dispatcher::Sink<events::HitPointerWheel>().connect<&StateSystem::onHitPointerWheel>(*this);

        // 帧结束时应用状态更新
        Dispatcher::Sink<events::EndFrame>().connect<&StateSystem::onEndFrame>(*this);
    }

    void unregisterHandlersImpl()
    {
        Dispatcher::Sink<events::CloseWindow>().disconnect<&StateSystem::onCloseWindow>(*this);
        Dispatcher::Sink<events::WindowPixelSizeChanged>().disconnect<&StateSystem::onWindowPixelSizeChanged>(*this);
        Dispatcher::Sink<events::WindowMoved>().disconnect<&StateSystem::onWindowMoved>(*this);
        Dispatcher::Sink<events::HoverEvent>().disconnect<&StateSystem::onHoverEvent>(*this);
        Dispatcher::Sink<events::UnhoverEvent>().disconnect<&StateSystem::onUnhoverEvent>(*this);
        Dispatcher::Sink<events::MousePressEvent>().disconnect<&StateSystem::onMousePressEvent>(*this);
        Dispatcher::Sink<events::MouseReleaseEvent>().disconnect<&StateSystem::onMouseReleaseEvent>(*this);

        Dispatcher::Sink<events::HitPointerMove>().disconnect<&StateSystem::onHitPointerMove>(*this);
        Dispatcher::Sink<events::HitPointerButton>().disconnect<&StateSystem::onHitPointerButton>(*this);
        Dispatcher::Sink<events::HitPointerWheel>().disconnect<&StateSystem::onHitPointerWheel>(*this);
        Dispatcher::Sink<events::EndFrame>().disconnect<&StateSystem::onEndFrame>(*this);
    }

private:
    // ===================================================================
    // Pointer / Focus 辅助判定
    // ===================================================================

    static bool isWritableTextEdit(entt::entity entity)
    {
        if (!Registry::Valid(entity) || !Registry::AnyOf<components::TextEditTag>(entity))
        {
            return false;
        }

        const auto* edit = Registry::TryGet<components::TextEdit>(entity);
        return edit == nullptr || !policies::HasFlag(edit->inputMode, policies::TextFlag::ReadOnly);
    }

    static bool shouldEmitPressForEntity(entt::entity entity)
    {
        return Registry::Valid(entity) &&
               (Registry::AnyOf<components::Pressable>(entity) || Registry::AnyOf<components::Clickable>(entity) ||
                Registry::AnyOf<components::TextEditTag>(entity));
    }

    static entt::entity findScrollTargetFromHit(entt::entity hitEntity)
    {
        entt::entity current = hitEntity;
        while (current != entt::null && Registry::Valid(current))
        {
            if (Registry::AnyOf<components::ScrollArea>(current))
            {
                return current;
            }

            if (const auto* hierarchy = Registry::TryGet<components::Hierarchy>(current))
            {
                current = hierarchy->parent;
            }
            else
            {
                current = entt::null;
            }
        }

        return entt::null;
    }

    static entt::entity findScrollTargetAtPosition(const Vec2& pointerPosition)
    {
        auto view =
            Registry::View<components::ScrollArea, components::Size, components::VisibleTag, components::Position>();
        for (auto entity : view)
        {
            const auto& size = view.get<components::Size>(entity);
            Vec2 absPos = HitTestSystem::getAbsolutePosition(entity);
            if (HitTestSystem::isPointInRect(pointerPosition, absPos, size.size))
            {
                return entity;
            }
        }

        return entt::null;
    }

    static void applyScrollWheelDelta(entt::entity target, const Vec2& scrollDelta)
    {
        auto& scroll = Registry::Get<components::ScrollArea>(target);
        const auto& size = Registry::Get<components::Size>(target);

        float step = 30.0F;
        float delta = -scrollDelta.y() * step;
        scroll.scrollOffset.y() += delta;

        float viewportHeight = size.size.y();
        if (const auto* padding = Registry::TryGet<components::Padding>(target))
        {
            viewportHeight -= (padding->values.x() + padding->values.z());
        }
        viewportHeight = std::max(0.0F, viewportHeight);

        float maxScroll = std::max(0.0F, scroll.contentSize.y() - viewportHeight);
        scroll.scrollOffset.y() = std::clamp(scroll.scrollOffset.y(), 0.0F, maxScroll);

        ui::utils::MarkLayoutAndVisualChanged(target);
    }

    void queueHoveredEntity(globalcontext::StateContext& state, entt::entity entity)
    {
        if (state.hoveredEntity != entt::null && Registry::Valid(state.hoveredEntity))
        {
            m_pendingHoverRemove.insert(state.hoveredEntity);
        }

        state.hoveredEntity = entity;
        if (Registry::Valid(entity))
        {
            m_pendingHoverAdd.insert(entity);
            m_pendingHoverRemove.erase(entity);
        }
    }

    void queueHoverClear(globalcontext::StateContext& state, entt::entity entity)
    {
        if (Registry::Valid(entity))
        {
            m_pendingHoverRemove.insert(entity);
            m_pendingHoverAdd.erase(entity);
        }

        if (state.hoveredEntity == entity)
        {
            state.hoveredEntity = entt::null;
        }
    }

    void queueActiveEntity(globalcontext::StateContext& state, entt::entity entity)
    {
        state.activeDragMoved = false;

        if (state.activeEntity != entt::null && Registry::Valid(state.activeEntity))
        {
            m_pendingActiveRemove.insert(state.activeEntity);
        }

        state.activeEntity = entity;
        if (Registry::Valid(entity))
        {
            m_pendingActiveAdd.insert(entity);
            m_pendingActiveRemove.erase(entity);
        }
    }

    void queueActiveClear(globalcontext::StateContext& state, entt::entity entity)
    {
        if (Registry::Valid(entity))
        {
            m_pendingActiveRemove.insert(entity);
            m_pendingActiveAdd.erase(entity);
        }

        if (state.activeEntity == entity)
        {
            state.activeEntity = entt::null;
        }

        state.activeDragMoved = false;
    }

    // ===================================================================
    // PointerState: Hover / Active 标签延迟状态机
    // ===================================================================

    /**
     * @brief 处理悬停事件 - 更新悬停状态（延迟应用）
     */
    void onHoverEvent(const events::HoverEvent& event)
    {
        auto& state = Registry::ctx().get<globalcontext::StateContext>();
        queueHoveredEntity(state, event.entity);
    }

    /**
     * @brief 处理取消悬停事件（延迟应用）
     */
    void onUnhoverEvent(const events::UnhoverEvent& event)
    {
        auto& state = Registry::ctx().get<globalcontext::StateContext>();
        queueHoverClear(state, event.entity);
    }

    /**
     * @brief 处理鼠标按下事件 - 设置激活状态（延迟应用）
     */
    void onMousePressEvent(const events::MousePressEvent& event)
    {
        auto& state = Registry::ctx().get<globalcontext::StateContext>();
        queueActiveEntity(state, event.entity);
    }

    /**
     * @brief 处理鼠标释放事件 - 清除激活状态（延迟应用）
     */
    void onMouseReleaseEvent(const events::MouseReleaseEvent& event)
    {
        auto& state = Registry::ctx().get<globalcontext::StateContext>();
        queueActiveClear(state, event.entity);
    }

    // ===================================================================
    // PointerState: 命中后的指针输入状态机
    // ===================================================================

    void onHitPointerMove(const events::HitPointerMove& event)
    {
        auto& state = Registry::ctx().get<globalcontext::StateContext>();
        state.latestMousePosition = event.raw.position;
        state.latestMouseDelta = event.raw.delta;

        if (!state.activeDragMoved && state.activeEntity != entt::null && Registry::Valid(state.activeEntity) &&
            Registry::TryGet<components::Draggable>(state.activeEntity) != nullptr &&
            event.raw.delta != Vec2{0.0F, 0.0F})
        {
            state.activeDragMoved = true;
        }

        if (m_isDraggingSlider)
        {
            handleSliderDrag(event);
            return;
        }

        // 处理滚动条拖拽
        if (state.isDraggingScrollbar && Registry::Valid(state.dragScrollEntity))
        {
            handleScrollbarDrag(event, state);
            return;
        }

        // 更新滚动条悬停状态
        updateScrollbarHoverStates(event);

        handleHoverUpdate(event, state);
    }

    void onHitPointerButton(const events::HitPointerButton& event)
    {
        if (event.raw.button != SDL_BUTTON_LEFT) return;

        auto& state = Registry::ctx().get<globalcontext::StateContext>();
        state.latestMousePosition = event.raw.position;

        if (event.raw.pressed)
        {
            // 检查滚动条点击（优先于内容交互）
            if (tryHandleScrollbarPress(event, state))
            {
                return; // 消费事件，不处理后续点击
            }
            if (tryHandleSliderPress(event))
            {
                return;
            }
            handleEntityPress(event);
            return;
        }

        handleEntityRelease(event, state);
    }

    void onHitPointerWheel(const events::HitPointerWheel& event)
    {
        auto& state = Registry::ctx().get<globalcontext::StateContext>();
        state.latestScrollDelta = event.raw.delta;

        entt::entity target = findScrollTargetFromHit(event.hitEntity);

        if (target == entt::null)
        {
            target = findScrollTargetAtPosition(event.raw.position);
        }

        if (target != entt::null)
        {
            applyScrollWheelDelta(target, event.raw.delta);
        }
    }

    /**
     * @brief 设置焦点到指定实体（用于输入框等需要焦点的组件）
     * @note Focus 状态立即应用，因为涉及 SDL 输入法状态同步
     */
    static void setFocus(entt::entity entity, SDL_Window* sdlWindow = nullptr)
    {
        auto& state = Registry::ctx().get<globalcontext::StateContext>();

        // 移除旧焦点实体的标签（Focus 需要立即应用）
        if (state.focusedEntity != entt::null && Registry::Valid(state.focusedEntity))
        {
            ui::utils::MarkVisualChanged(state.focusedEntity);
            Registry::Remove<components::FocusedTag>(state.focusedEntity);
        }

        // 设置新焦点实体
        state.focusedEntity = entity;
        if (entity != entt::null && Registry::Valid(entity))
        {
            Registry::EmplaceOrReplace<components::FocusedTag>(entity);
            ui::utils::MarkVisualChanged(entity);

            // 如果是输入框，启动文本输入并重置光标闪烁状态（立即可见）
            if (Registry::AnyOf<components::TextEditTag>(entity))
            {
                Registry::EmplaceOrReplace<components::Caret>(entity);
                auto& caret = Registry::Get<components::Caret>(entity);
                caret.visible = true;
                caret.elapsedTime = 0.0F;

                if (sdlWindow != nullptr)
                {
                    SDL_StartTextInput(sdlWindow);
                }
            }
        }
    }

    /**
     * @brief 清除当前焦点
     */
    static void clearFocus(SDL_Window* sdlWindow = nullptr)
    {
        auto& state = Registry::ctx().get<globalcontext::StateContext>();

        if (state.focusedEntity != entt::null && Registry::Valid(state.focusedEntity))
        {
            ui::utils::MarkVisualChanged(state.focusedEntity);
            Registry::Remove<components::FocusedTag>(state.focusedEntity);
            state.focusedEntity = entt::null;
        }

        if (sdlWindow != nullptr)
        {
            SDL_StopTextInput(sdlWindow);
        }
    }

    // ===================================================================
    // WindowSyncState: 窗口生命周期与 SDL 同步
    // ===================================================================

    void onCloseWindow(const events::CloseWindow& event)
    {
        // 调用递归销毁
        if (Registry::Valid(event.entity))
        {
            destroyWidget(event.entity);
        }

        if (Registry::View<components::Window>().empty())
        {
            // 没有窗口实体了，发出退出请求
            Dispatcher::Trigger<events::QuitRequested>();
        }
    }

    /**
     * @brief 处理窗口尺寸变化事件
     */
    void onWindowPixelSizeChanged(const events::WindowPixelSizeChanged& event)
    {
        auto view = Registry::View<components::Window, components::Size>();

        for (auto entity : view)
        {
            if (view.get<components::Window>(entity).windowID == event.windowID)
            {
                auto& size = view.get<components::Size>(entity);
                size.size.x() = static_cast<float>(event.width);
                size.size.y() = static_cast<float>(event.height);
                ui::utils::MarkLayoutAndVisualChanged(entity);
                break;
            }
        }
    }

    /**
     * @brief 处理窗口位置变化事件
     */
    void onWindowMoved(const events::WindowMoved& event)
    {
        auto view = Registry::View<components::Window, components::Position>();

        for (auto entity : view)
        {
            if (view.get<components::Window>(entity).windowID == event.windowID)
            {
                auto& pos = view.get<components::Position>(entity);
                pos.value.x() = static_cast<float>(event.x);
                pos.value.y() = static_cast<float>(event.y);
                break;
            }
        }
    }

    // ===================================================================
    // ScrollInteractionState: ScrollArea / Slider 交互
    // ===================================================================

    /**
     * @brief 更新所有可见 ScrollArea 的滚动条悬停状态
     */
    void updateScrollbarHoverStates(const events::HitPointerMove& event)
    {
        auto& state = Registry::ctx().get<globalcontext::StateContext>();
        auto view = Registry::View<components::ScrollArea, components::Size, components::VisibleTag>();

        for (auto entity : view)
        {
            auto& scrollArea = view.get<components::ScrollArea>(entity);

            // 记录旧的悬停状态（用于后面判断是否需要重绘）
            const bool wasHovered = scrollArea.scrollbarHovered || scrollArea.trackHovered;

            // 如果正在拖拽此滚动条，跳过悬停状态更新（保持按下状态）
            if (state.isDraggingScrollbar && state.dragScrollEntity == entity)
            {
                continue;
            }

            // 重置状态
            scrollArea.scrollbarHovered = false;
            scrollArea.trackHovered = false;

            // 检查是否应该显示滚动条
            if (policies::HasFlag(scrollArea.scrollBar, policies::ScrollBar::NoVisibility))
            {
                continue;
            }

            const auto& sizeComp = view.get<components::Size>(entity);
            Vec2 pos = HitTestSystem::getAbsolutePosition(entity);
            Vec2 size = sizeComp.size;

            float viewportHeight = size.y();
            if (const auto* padding = Registry::TryGet<components::Padding>(entity))
            {
                viewportHeight = std::max(0.0F, size.y() - padding->values.x() - padding->values.z());
            }

            bool hasVerticalScroll =
                (scrollArea.scroll == policies::Scroll::Vertical || scrollArea.scroll == policies::Scroll::Both);

            if (!hasVerticalScroll || scrollArea.contentSize.y() <= viewportHeight)
            {
                continue;
            }

            // 计算滚动条几何信息
            float trackHeight = size.y();
            float visibleRatio = viewportHeight / scrollArea.contentSize.y();
            float thumbSize = std::max(20.0F, trackHeight * visibleRatio);
            float maxScroll = std::max(0.0F, scrollArea.contentSize.y() - viewportHeight);
            float scrollRatio =
                maxScroll > 0.0F ? std::clamp(scrollArea.scrollOffset.y() / maxScroll, 0.0F, 1.0F) : 0.0F;
            float thumbPos = (trackHeight - thumbSize) * scrollRatio;
            thumbPos = std::clamp(thumbPos, 0.0F, trackHeight - thumbSize);

            float trackWidth = 12.0F;
            float trackPadding = 2.0F;
            float barWidth = 10.0F;

            // 检查轨道悬停
            float trackX = pos.x() + size.x() - trackWidth - trackPadding;
            float trackY = pos.y();
            if (event.raw.position.x() >= trackX && event.raw.position.x() <= trackX + trackWidth &&
                event.raw.position.y() >= trackY && event.raw.position.y() <= trackY + trackHeight)
            {
                scrollArea.trackHovered = true;

                // 检查滑块悬停
                float barX = pos.x() + size.x() - barWidth - trackPadding - 1.0F;
                float barY = pos.y() + thumbPos + 2.0F;
                float barH = thumbSize - 4.0F;

                if (event.raw.position.x() >= barX && event.raw.position.x() <= barX + barWidth &&
                    event.raw.position.y() >= barY && event.raw.position.y() <= barY + barH)
                {
                    scrollArea.scrollbarHovered = true;
                }
            }

            // 如果状态改变，标记需要重绘
            if (wasHovered != (scrollArea.scrollbarHovered || scrollArea.trackHovered))
            {
                ui::utils::MarkVisualChanged(entity);
            }
        }
    }

    void handleScrollbarDrag(const events::HitPointerMove& event, globalcontext::StateContext& state)
    {
        float deltaPix = state.isVerticalDrag ? (event.raw.position.y() - state.dragStartMousePos.y())
                                              : (event.raw.position.x() - state.dragStartMousePos.x());

        auto& scroll = Registry::Get<components::ScrollArea>(state.dragScrollEntity);
        const auto& sizeComp = Registry::Get<components::Size>(state.dragScrollEntity);
        float viewportSize = state.isVerticalDrag ? sizeComp.size.y() : sizeComp.size.x();

        if (const auto* padding = Registry::TryGet<components::Padding>(state.dragScrollEntity))
        {
            viewportSize -= state.isVerticalDrag ? (padding->values.x() + padding->values.z())
                                                 : (padding->values.w() + padding->values.y());
        }

        float contentSize = state.isVerticalDrag ? scroll.contentSize.y() : scroll.contentSize.x();
        float maxScroll = std::max(0.0F, contentSize - viewportSize);
        float trackScrollableArea = state.dragTrackLength - state.dragThumbSize;

        if (trackScrollableArea > 0 && maxScroll > 0)
        {
            float ratio = deltaPix / trackScrollableArea;
            float offsetDelta = ratio * maxScroll;

            float& currentOffset = state.isVerticalDrag ? scroll.scrollOffset.y() : scroll.scrollOffset.x();
            float startOffset =
                state.isVerticalDrag ? state.dragStartScrollOffset.y() : state.dragStartScrollOffset.x();

            currentOffset = std::clamp(startOffset + offsetDelta, 0.0F, maxScroll);

            ui::utils::MarkLayoutAndVisualChanged(state.dragScrollEntity);
        }
    }

    void handleHoverUpdate(const events::HitPointerMove& event, const globalcontext::StateContext& state)
    {
        if (event.hitEntity != state.hoveredEntity)
        {
            if (state.hoveredEntity != entt::null && Registry::Valid(state.hoveredEntity))
            {
                Dispatcher::Enqueue<events::UnhoverEvent>(events::UnhoverEvent{state.hoveredEntity});
            }
            if (event.hitEntity != entt::null)
            {
                Dispatcher::Enqueue<events::HoverEvent>(events::HoverEvent{event.hitEntity});
            }
        }
    }

    /**
     * @brief 尝试处理滚动条按下事件
     * @param event 点击事件
     * @param state 状态上下文
     * @return true 如果处理了滚动条按下事件
     * @return false 如果未处理滚动条按下事件
     */
    bool tryHandleScrollbarPress(const events::HitPointerButton& event, globalcontext::StateContext& state)
    {
        entt::entity scrollEntity = entt::null;
        bool isVertical = true;
        bool clickedOnThumb = false;
        entt::entity current = event.hitEntity;

        // 向上遍历层级查找 ScrollArea
        while (current != entt::null && Registry::Valid(current))
        {
            if (Registry::AnyOf<components::ScrollArea>(current))
            {
                auto hitType = checkScrollbarHit(current, event.raw.position, isVertical);
                if (hitType != ScrollbarHitType::NONE)
                {
                    scrollEntity = current;
                    clickedOnThumb = (hitType == ScrollbarHitType::THUMB);
                    break;
                }
            }
            const auto* hierarchy = Registry::TryGet<components::Hierarchy>(current);
            current = hierarchy != nullptr ? hierarchy->parent : entt::null;
        }

        if (scrollEntity != entt::null)
        {
            auto& scroll = Registry::Get<components::ScrollArea>(scrollEntity);

            if (clickedOnThumb)
            {
                // 点击滑块 - 开始拖拽
                scroll.scrollbarPressed = true;
                state.isDraggingScrollbar = true;
                state.dragScrollEntity = scrollEntity;
                state.dragStartMousePos = event.raw.position;
                state.isVerticalDrag = isVertical;
                state.dragStartScrollOffset = scroll.scrollOffset;

                calculateScrollbarGeometry(scrollEntity, isVertical, state.dragTrackLength, state.dragThumbSize);
            }
            else
            {
                // 点击轨道 - 跳转到点击位置
                handleTrackClick(scrollEntity, event.raw.position, isVertical);
            }

            ui::utils::MarkVisualChanged(scrollEntity);
            return true;
        }
        return false;
    }

    void handleEntityPress(const events::HitPointerButton& event)
    {
        if (event.hitEntity != entt::null)
        {
            if (isWritableTextEdit(event.hitEntity))
            {
                if (SDL_Window* sdlWindow = SDL_GetWindowFromID(event.raw.windowID))
                {
                    setFocus(event.hitEntity, sdlWindow);
                }
            }

            if (shouldEmitPressForEntity(event.hitEntity))
            {
                Dispatcher::Trigger<events::MousePressEvent>(events::MousePressEvent{event.hitEntity});
            }
        }
    }
    /**
     * @brief 处理实体释放事件
     * @param event 点击事件
     * @param state 状态上下文
     */
    void handleEntityRelease(const events::HitPointerButton& event, globalcontext::StateContext& state)
    {
        // 先保存当前激活实体，点击逻辑需要在释放清理之前完成
        const entt::entity releasedEntity = state.activeEntity;
        const bool suppressClick = state.activeDragMoved;

        if (m_isDraggingSlider)
        {
            stopSliderDrag();
        }

        // 如果正在拖拽滚动条，停止拖拽并清除按下状态
        if (state.isDraggingScrollbar)
        {
            if (Registry::Valid(state.dragScrollEntity))
            {
                auto& scrollArea = Registry::Get<components::ScrollArea>(state.dragScrollEntity);
                scrollArea.scrollbarPressed = false;
                ui::utils::MarkVisualChanged(state.dragScrollEntity);
            }

            state.isDraggingScrollbar = false;
            state.dragScrollEntity = entt::null;
            // 不 return，允许释放 activeEntity（如果有）
        }

        if (!suppressClick && releasedEntity != entt::null && releasedEntity == event.hitEntity)
        {
            if (Registry::TryGet<components::Clickable>(releasedEntity) != nullptr)
            {
                Dispatcher::Trigger<events::ClickEvent>(events::ClickEvent{releasedEntity});
            }

            if (SDL_Window* sdlWindow = SDL_GetWindowFromID(event.raw.windowID))
            {
                if (isWritableTextEdit(releasedEntity))
                {
                    setFocus(releasedEntity, sdlWindow);
                    return;
                }
                clearFocus(sdlWindow);
            }
        }
        else
        {
            // 兜底：如果没有 activeEntity 但命中了可点击实体，也触发点击
            if (!suppressClick && event.hitEntity != entt::null &&
                Registry::TryGet<components::Clickable>(event.hitEntity) != nullptr)
            {
                Logger::debug("StateSystem: Click Event (fallback) on entity {}",
                              static_cast<uint32_t>(event.hitEntity));
                Dispatcher::Trigger<events::ClickEvent>(events::ClickEvent{event.hitEntity});
            }

            if (SDL_Window* sdlWindow = SDL_GetWindowFromID(event.raw.windowID))
            {
                clearFocus(sdlWindow);
            }
        }

        // 最后再触发释放，避免提前清空 activeEntity 影响点击逻辑
        if (releasedEntity != entt::null)
        {
            Dispatcher::Trigger<events::MouseReleaseEvent>(events::MouseReleaseEvent{releasedEntity});
        }
    }

    bool tryHandleSliderPress(const events::HitPointerButton& event)
    {
        if (event.hitEntity == entt::null || !Registry::Valid(event.hitEntity))
        {
            return false;
        }

        if (Registry::TryGet<components::SliderInfo>(event.hitEntity) == nullptr)
        {
            return false;
        }

        m_isDraggingSlider = true;
        m_dragSliderEntity = event.hitEntity;
        updateSliderValueFromPointer(event.hitEntity, event.raw.position);
        return true;
    }

    void handleSliderDrag(const events::HitPointerMove& event)
    {
        if (!Registry::Valid(m_dragSliderEntity) ||
            Registry::TryGet<components::SliderInfo>(m_dragSliderEntity) == nullptr)
        {
            stopSliderDrag();
            return;
        }

        updateSliderValueFromPointer(m_dragSliderEntity, event.raw.position);
    }

    void stopSliderDrag()
    {
        m_isDraggingSlider = false;
        m_dragSliderEntity = entt::null;
    }

    void updateSliderValueFromPointer(entt::entity entity, const Vec2& mousePos)
    {
        auto* slider = Registry::TryGet<components::SliderInfo>(entity);
        const auto* sizeComp = Registry::TryGet<components::Size>(entity);
        if (slider == nullptr || sizeComp == nullptr)
        {
            return;
        }

        Vec2 absPos = HitTestSystem::getAbsolutePosition(entity);
        const Vec2 size = sizeComp->size;

        float ratio = 0.0F;
        if (slider->vertical == policies::Orientation::Vertical)
        {
            if (size.y() <= 0.0F) return;
            ratio = (absPos.y() + size.y() - mousePos.y()) / size.y();
        }
        else
        {
            if (size.x() <= 0.0F) return;
            ratio = (mousePos.x() - absPos.x()) / size.x();
        }

        ratio = std::clamp(ratio, 0.0F, 1.0F);
        const float range = slider->maxValue - slider->minValue;
        float newValue = slider->minValue + (range * ratio);

        if (slider->step > 0.0F)
        {
            const float stepIndex = std::round((newValue - slider->minValue) / slider->step);
            newValue = slider->minValue + (stepIndex * slider->step);
        }

        newValue = std::clamp(newValue, slider->minValue, slider->maxValue);
        if (std::abs(slider->currentValue - newValue) < 0.0001F)
        {
            return;
        }

        slider->currentValue = newValue;
        if (slider->onValueChanged)
        {
            slider->onValueChanged(slider->currentValue);
        }

        ui::utils::MarkVisualChanged(entity);
    }

    /**
     * @brief 检查点是否在滚动条内，并返回命中类型
     * @param entity 实体
     * @param mousePos 鼠标位置
     * @param outIsVertical 输出是否为垂直滚动条
     * @return ScrollbarHitType 命中类型（None/Thumb/Track）
     */
    ScrollbarHitType checkScrollbarHit(entt::entity entity, const Vec2& mousePos, bool& outIsVertical)
    {
        const auto* scrollArea = Registry::TryGet<components::ScrollArea>(entity);
        if (scrollArea == nullptr || policies::HasFlag(scrollArea->scrollBar, policies::ScrollBar::NoVisibility))
        {
            return ScrollbarHitType::NONE;
        }
        if (!policies::HasFlag(scrollArea->scrollBar, policies::ScrollBar::Draggable)) return ScrollbarHitType::NONE;

        const auto* sizeComp = Registry::TryGet<components::Size>(entity);
        if (sizeComp == nullptr) return ScrollbarHitType::NONE;

        Vec2 pos = HitTestSystem::getAbsolutePosition(entity);
        Vec2 size = sizeComp->size;

        float viewportWidth = size.x();
        float viewportHeight = size.y();
        if (const auto* padding = Registry::TryGet<components::Padding>(entity))
        {
            viewportHeight = std::max(0.0F, size.y() - padding->values.x() - padding->values.z());
            viewportWidth = std::max(0.0F, size.x() - padding->values.w() - padding->values.y());
        }

        float trackWidth = 12.0F;
        float trackPadding = 2.0F;
        float barWidth = 10.0F;

        // 检查垂直滚动条
        bool hasVertical =
            (scrollArea->scroll == policies::Scroll::Vertical || scrollArea->scroll == policies::Scroll::Both);
        if (hasVertical && scrollArea->contentSize.y() > viewportHeight)
        {
            float trackHeight = size.y();
            float visibleRatio = viewportHeight / scrollArea->contentSize.y();
            float thumbSize = std::max(20.0F, trackHeight * visibleRatio);
            float maxScroll = std::max(0.0F, scrollArea->contentSize.y() - viewportHeight);
            float scrollRatio =
                maxScroll > 0.0F ? std::clamp(scrollArea->scrollOffset.y() / maxScroll, 0.0F, 1.0F) : 0.0F;
            float thumbPos = (trackHeight - thumbSize) * scrollRatio;
            thumbPos = std::clamp(thumbPos, 0.0F, trackHeight - thumbSize);

            // 先检查轨道区域
            float trackX = pos.x() + size.x() - trackWidth - trackPadding;
            float trackY = pos.y();

            if (mousePos.x() >= trackX && mousePos.x() <= trackX + trackWidth && mousePos.y() >= trackY &&
                mousePos.y() <= trackY + trackHeight)
            {
                outIsVertical = true;

                // 再检查是否命中滑块
                float barX = pos.x() + size.x() - barWidth - trackPadding - 1.0F;
                float barY = pos.y() + thumbPos + 2.0F;
                float barH = thumbSize - 4.0F;

                if (mousePos.x() >= barX && mousePos.x() <= barX + barWidth && mousePos.y() >= barY &&
                    mousePos.y() <= barY + barH)
                {
                    return ScrollbarHitType::THUMB;
                }

                // 命中轨道但不是滑块
                return ScrollbarHitType::TRACK;
            }
        }

        // TODO: 水平滚动条支持可在此添加

        return ScrollbarHitType::NONE;
    }

    void calculateScrollbarGeometry(entt::entity entity, bool isVertical, float& outTrackLen, float& outThumbSize)
    {
        const auto& scrollArea = Registry::Get<components::ScrollArea>(entity);
        const auto& sizeComp = Registry::Get<components::Size>(entity);

        float viewportSize = isVertical ? sizeComp.size.y() : sizeComp.size.x();
        if (const auto* padding = Registry::TryGet<components::Padding>(entity))
        {
            viewportSize -=
                isVertical ? (padding->values.x() + padding->values.z()) : (padding->values.w() + padding->values.y());
        }

        float contentSize = isVertical ? scrollArea.contentSize.y() : scrollArea.contentSize.x();

        outTrackLen = isVertical ? sizeComp.size.y() : sizeComp.size.x();
        float visibleRatio = std::max(0.001F, viewportSize / std::max(1.0F, contentSize));
        outThumbSize = std::max(20.0F, outTrackLen * visibleRatio);
    }

    /**
     * @brief 处理点击轨道跳转
     * @param entity 滚动区域实体
     * @param mousePos 鼠标位置
     * @param isVertical 是否为垂直滚动条
     */
    void handleTrackClick(entt::entity entity, const Vec2& mousePos, bool isVertical)
    {
        auto& scrollArea = Registry::Get<components::ScrollArea>(entity);
        const auto& sizeComp = Registry::Get<components::Size>(entity);

        Vec2 pos = HitTestSystem::getAbsolutePosition(entity);
        Vec2 size = sizeComp.size;

        float viewportSize = isVertical ? size.y() : size.x();
        if (const auto* padding = Registry::TryGet<components::Padding>(entity))
        {
            viewportSize -=
                isVertical ? (padding->values.x() + padding->values.z()) : (padding->values.w() + padding->values.y());
        }

        float contentSize = isVertical ? scrollArea.contentSize.y() : scrollArea.contentSize.x();
        float maxScroll = std::max(0.0F, contentSize - viewportSize);

        if (maxScroll <= 0.0F) return;

        // 计算点击位置相对于轨道的比例
        float trackLen = isVertical ? size.y() : size.x();
        float clickPos = isVertical ? (mousePos.y() - pos.y()) : (mousePos.x() - pos.x());

        // 设置滚动位置（点击位置对应滑块中心）
        float& scrollOffset = isVertical ? scrollArea.scrollOffset.y() : scrollArea.scrollOffset.x();

        // 计算滑块大小
        float visibleRatio = viewportSize / contentSize;
        float thumbSize = std::max(20.0F, trackLen * visibleRatio);

        // 将点击位置映射到内容位置（考虑滑块会占用一定空间）
        float scrollableTrack = trackLen - thumbSize;
        float targetThumbPos = clickPos - (thumbSize * 0.5F); // 使滑块中心对齐点击位置
        targetThumbPos = std::clamp(targetThumbPos, 0.0F, scrollableTrack);

        float targetRatio = scrollableTrack > 0.0F ? (targetThumbPos / scrollableTrack) : 0.0F;
        scrollOffset = targetRatio * maxScroll;
        scrollOffset = std::clamp(scrollOffset, 0.0F, maxScroll);

        ui::utils::MarkLayoutAndVisualChanged(entity);
    }

    // ===================================================================
    // PointerState: 帧尾标签合并
    // ===================================================================

    /**
     * @brief 待处理的状态标签更新集合
     * @note 使用 unordered_set 自动去重，避免同一实体多次操作
     */
    std::unordered_set<entt::entity> m_pendingHoverAdd;
    std::unordered_set<entt::entity> m_pendingHoverRemove;
    std::unordered_set<entt::entity> m_pendingActiveAdd;
    std::unordered_set<entt::entity> m_pendingActiveRemove;
    bool m_isDraggingSlider = false;
    entt::entity m_dragSliderEntity = entt::null;

    /**
     * @brief 帧结束时批量应用状态更新
     * @note 通过合并同帧内的多次状态变化，减少无效的 Registry 操作
     */
    void onEndFrame()
    {
        // 1. 应用 Hover 状态更新
        for (entt::entity entity : m_pendingHoverRemove)
        {
            if (Registry::Valid(entity))
            {
                Registry::Remove<components::HoveredTag>(entity);
            }
        }
        for (entt::entity entity : m_pendingHoverAdd)
        {
            if (Registry::Valid(entity))
            {
                Registry::EmplaceOrReplace<components::HoveredTag>(entity);
            }
        }

        // 2. 应用 Active 状态更新
        for (entt::entity entity : m_pendingActiveRemove)
        {
            if (Registry::Valid(entity))
            {
                Registry::Remove<components::ActiveTag>(entity);
            }
        }
        for (entt::entity entity : m_pendingActiveAdd)
        {
            if (Registry::Valid(entity))
            {
                Registry::EmplaceOrReplace<components::ActiveTag>(entity);
            }
        }

        // 3. 清空待处理队列
        m_pendingHoverAdd.clear();
        m_pendingHoverRemove.clear();
        m_pendingActiveAdd.clear();
        m_pendingActiveRemove.clear();
    }

    // ===================================================================
    // 工具方法
    // ===================================================================

    static void destroyWidget(entt::entity entity)
    {
        if (!Registry::Valid(entity)) return;

        // 使用迭代代替递归，避免深层嵌套导致栈溢出
        std::vector<entt::entity> stack;
        std::vector<entt::entity> toDestroy;

        stack.push_back(entity);

        // 1. 深度优先遍历收集所有需要销毁的实体
        while (!stack.empty())
        {
            entt::entity current = stack.back();
            stack.pop_back();

            if (!Registry::Valid(current)) continue;

            // 将当前实体加入销毁列表
            toDestroy.push_back(current);

            // 将子节点加入栈（逆序添加以保持遍历顺序）
            if (auto* hierarchy = Registry::TryGet<components::Hierarchy>(current))
            {
                // 复制子节点列表，防止在遍历过程中引用失效
                auto children = hierarchy->children;
                for (auto it = children.rbegin(); it != children.rend(); ++it)
                {
                    stack.push_back(*it);
                }
            }
        }

        // 2. 逆序销毁：先销毁叶子节点，最后销毁根节点
        for (entt::entity entity : std::ranges::reverse_view(toDestroy))
        {
            if (!Registry::Valid(entity)) continue;

            // 如果是窗口，查找并销毁关联的 SDL_Window 资源
            if (auto* windowComp = Registry::TryGet<components::Window>(entity))
            {
                // 通过 WindowID 找回 SDL_Window 指针
                if (SDL_Window* sdlWindow = SDL_GetWindowFromID(windowComp->windowID))
                {
                    // 通知 RenderSystem 解绑上下文（尽管 RenderSystem 可能目前未执行动作，保留逻辑完整性）
                    Dispatcher::Trigger<events::WindowGraphicsContextUnsetEvent>(
                        events::WindowGraphicsContextUnsetEvent{entity});

                    SDL_DestroyWindow(sdlWindow);
                }
            }

            // 销毁实体本身
            Registry::Destroy(entity);
        }
    }
};

} // namespace ui::systems