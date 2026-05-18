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
#include "../core/RuntimeFacade.hpp"
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
    struct SliderStateHelpers
    {
        static bool tryHandlePress(StateSystem& system, const events::HitPointerButton& event)
        {
            if (event.hitEntity == entt::null || !Registry::Valid(event.hitEntity))
            {
                return false;
            }

            if (Registry::TryGet<components::SliderInfo>(event.hitEntity) == nullptr)
            {
                return false;
            }

            system.m_isDraggingSlider = true;
            system.m_dragSliderEntity = event.hitEntity;
            updateValueFromPointer(event.hitEntity, event.raw.position);
            return true;
        }

        static void handleDrag(StateSystem& system, const events::HitPointerMove& event)
        {
            if (!Registry::Valid(system.m_dragSliderEntity) ||
                Registry::TryGet<components::SliderInfo>(system.m_dragSliderEntity) == nullptr)
            {
                stopDrag(system);
                return;
            }

            updateValueFromPointer(system.m_dragSliderEntity, event.raw.position);
        }

        static void stopDrag(StateSystem& system)
        {
            system.m_isDraggingSlider = false;
            system.m_dragSliderEntity = entt::null;
        }

        static void updateValueFromPointer(entt::entity entity, const Vec2& mousePos)
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
    };

    struct ScrollbarStateHelpers
    {
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
            auto view = Registry::View<components::ScrollArea, components::Size, components::VisibleTag,
                                       components::Position>();
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
            const float delta = -scrollDelta.y() * 30.0F;
            scroll.scrollOffset.y() = std::clamp(scroll.scrollOffset.y() + delta,
                                                 0.0F,
                                                 ui::utils::GetScrollMaxOffset(target, true));

            ui::utils::MarkLayoutAndVisualChanged(target);
        }

        static void updateHoverStates(
            const events::HitPointerMove& event,
            const globalcontext::StateContext& state)
        {
            auto view = Registry::View<components::ScrollArea, components::Size, components::VisibleTag>();

            for (auto entity : view)
            {
                auto& scrollArea = view.get<components::ScrollArea>(entity);
                const bool wasHovered = scrollArea.scrollbarHovered || scrollArea.trackHovered;

                if (state.hasScrollbarDrag() && state.dragScrollEntity == entity)
                {
                    continue;
                }

                scrollArea.scrollbarHovered = false;
                scrollArea.trackHovered = false;

                if (policies::HasFlag(scrollArea.scrollBar, policies::ScrollBar::NoVisibility))
                {
                    continue;
                }

                const components::VerticalScrollbarGeometry geometry = ui::utils::GetVerticalScrollbarGeometry(entity);
                if (!geometry.visible)
                {
                    continue;
                }

                if (geometry.trackRect.contains(event.raw.position))
                {
                    scrollArea.trackHovered = true;

                    if (geometry.thumbRect.contains(event.raw.position))
                    {
                        scrollArea.scrollbarHovered = true;
                    }
                }

                if (wasHovered != (scrollArea.scrollbarHovered || scrollArea.trackHovered))
                {
                    ui::utils::MarkVisualChanged(entity);
                }
            }
        }

        static void handleDrag(const events::HitPointerMove& event, globalcontext::StateContext& state)
        {
            const float deltaPix = state.isVerticalDrag ? (event.raw.position.y() - state.dragStartMousePos.y())
                                                        : (event.raw.position.x() - state.dragStartMousePos.x());

            auto& scroll = Registry::Get<components::ScrollArea>(state.dragScrollEntity);
            const float maxScroll = ui::utils::GetScrollMaxOffset(state.dragScrollEntity, state.isVerticalDrag);
            const float trackScrollableArea = state.dragTrackLength - state.dragThumbSize;

            if (trackScrollableArea > 0.0F && maxScroll > 0.0F)
            {
                const float ratio = deltaPix / trackScrollableArea;
                const float offsetDelta = ratio * maxScroll;

                float& currentOffset = state.isVerticalDrag ? scroll.scrollOffset.y() : scroll.scrollOffset.x();
                const float startOffset =
                    state.isVerticalDrag ? state.dragStartScrollOffset.y() : state.dragStartScrollOffset.x();

                currentOffset = std::clamp(startOffset + offsetDelta, 0.0F, maxScroll);

                ui::utils::MarkLayoutAndVisualChanged(state.dragScrollEntity);
            }
        }

        static bool tryHandlePress(
            const events::HitPointerButton& event,
            globalcontext::StateContext& state)
        {
            entt::entity scrollEntity = entt::null;
            bool isVertical = true;
            bool clickedOnThumb = false;
            entt::entity current = event.hitEntity;

            while (current != entt::null && Registry::Valid(current))
            {
                if (Registry::AnyOf<components::ScrollArea>(current))
                {
                    const auto hitType = checkHit(current, event.raw.position, isVertical);
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

            if (scrollEntity == entt::null)
            {
                return false;
            }

            auto& scroll = Registry::Get<components::ScrollArea>(scrollEntity);
            if (clickedOnThumb)
            {
                scroll.scrollbarPressed = true;

                float trackLength = 0.0F;
                float thumbSize = 0.0F;
                calculateGeometry(scrollEntity, isVertical, trackLength, thumbSize);
                state.beginScrollbarDrag(
                    scrollEntity,
                    event.raw.position,
                    scroll.scrollOffset,
                    isVertical,
                    trackLength,
                    thumbSize);
            }
            else
            {
                handleTrackClick(scrollEntity, event.raw.position, isVertical);
            }

            ui::utils::MarkVisualChanged(scrollEntity);
            return true;
        }

        static ScrollbarHitType checkHit(entt::entity entity, const Vec2& mousePos, bool& outIsVertical)
        {
            const auto* scrollArea = Registry::TryGet<components::ScrollArea>(entity);
            if (scrollArea == nullptr || policies::HasFlag(scrollArea->scrollBar, policies::ScrollBar::NoVisibility))
            {
                return ScrollbarHitType::NONE;
            }
            if (!policies::HasFlag(scrollArea->scrollBar, policies::ScrollBar::Draggable))
            {
                return ScrollbarHitType::NONE;
            }

            const components::VerticalScrollbarGeometry geometry = ui::utils::GetVerticalScrollbarGeometry(entity);
            if (geometry.visible)
            {
                if (geometry.trackRect.contains(mousePos))
                {
                    outIsVertical = true;

                    if (geometry.thumbRect.contains(mousePos))
                    {
                        return ScrollbarHitType::THUMB;
                    }

                    return ScrollbarHitType::TRACK;
                }
            }
            return ScrollbarHitType::NONE;
        }

        static void calculateGeometry(entt::entity entity, bool isVertical, float& outTrackLen, float& outThumbSize)
        {
            outTrackLen = isVertical ? ui::utils::GetEntityRect(entity).height() : ui::utils::GetEntityRect(entity).width();
            const float viewportSize = ui::utils::GetScrollViewportLength(entity, isVertical);
            const float contentSize = std::max(1.0F, ui::utils::GetScrollContentLength(entity, isVertical));
            const float visibleRatio = std::max(0.001F, viewportSize / contentSize);
            outThumbSize = std::max(components::ScrollArea::SCROLLBAR_THUMB_MIN_SIZE, outTrackLen * visibleRatio);
        }

        static void handleTrackClick(entt::entity entity, const Vec2& mousePos, bool isVertical)
        {
            auto& scrollArea = Registry::Get<components::ScrollArea>(entity);
            const Rect entityRect = ui::utils::GetEntityRect(entity);
            const float viewportSize = ui::utils::GetScrollViewportLength(entity, isVertical);
            const float contentSize = ui::utils::GetScrollContentLength(entity, isVertical);
            const float maxScroll = std::max(0.0F, contentSize - viewportSize);
            if (maxScroll <= 0.0F)
            {
                return;
            }

            const float trackLen = isVertical ? entityRect.height() : entityRect.width();
            const float clickPos = isVertical ? (mousePos.y() - entityRect.y()) : (mousePos.x() - entityRect.x());
            float& scrollOffset = isVertical ? scrollArea.scrollOffset.y() : scrollArea.scrollOffset.x();

            const float visibleRatio = viewportSize / contentSize;
            const float thumbSize =
                std::max(components::ScrollArea::SCROLLBAR_THUMB_MIN_SIZE, trackLen * visibleRatio);
            const float scrollableTrack = trackLen - thumbSize;
            float targetThumbPos = clickPos - (thumbSize * 0.5F);
            targetThumbPos = std::clamp(targetThumbPos, 0.0F, scrollableTrack);

            const float targetRatio = scrollableTrack > 0.0F ? (targetThumbPos / scrollableTrack) : 0.0F;
            scrollOffset = std::clamp(targetRatio * maxScroll, 0.0F, maxScroll);

            ui::utils::MarkLayoutAndVisualChanged(entity);
        }
    };

    struct PointerStateHelpers
    {
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
                   (Registry::AnyOf<components::Pressable>(entity) ||
                    Registry::AnyOf<components::Clickable>(entity) ||
                    Registry::AnyOf<components::TextEditTag>(entity));
        }

        static void queueHoveredEntity(
            StateSystem& system,
            globalcontext::StateContext& state,
            entt::entity entity)
        {
            if (state.hoveredEntity != entt::null && Registry::Valid(state.hoveredEntity))
            {
                system.m_pendingHoverRemove.insert(state.hoveredEntity);
            }

            state.hoveredEntity = entity;
            if (Registry::Valid(entity))
            {
                system.m_pendingHoverAdd.insert(entity);
                system.m_pendingHoverRemove.erase(entity);
            }
        }

        static void queueHoverClear(StateSystem& system, globalcontext::StateContext& state, entt::entity entity)
        {
            if (Registry::Valid(entity))
            {
                system.m_pendingHoverRemove.insert(entity);
                system.m_pendingHoverAdd.erase(entity);
            }

            if (state.hoveredEntity == entity)
            {
                state.hoveredEntity = entt::null;
            }
        }

        static void queueActiveEntity(StateSystem& system, globalcontext::StateContext& state, entt::entity entity)
        {
            state.activeDragMoved = false;

            if (state.activeEntity != entt::null && Registry::Valid(state.activeEntity))
            {
                system.m_pendingActiveRemove.insert(state.activeEntity);
            }

            state.activeEntity = entity;
            if (Registry::Valid(entity))
            {
                system.m_pendingActiveAdd.insert(entity);
                system.m_pendingActiveRemove.erase(entity);
            }
        }

        static void queueActiveClear(StateSystem& system, globalcontext::StateContext& state, entt::entity entity)
        {
            if (Registry::Valid(entity))
            {
                system.m_pendingActiveRemove.insert(entity);
                system.m_pendingActiveAdd.erase(entity);
            }

            if (state.activeEntity == entity)
            {
                state.activeEntity = entt::null;
            }

            state.activeDragMoved = false;
        }

        static void handleHoverUpdate(
            const events::HitPointerMove& event,
            const globalcontext::StateContext& state)
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

        static void setFocus(entt::entity entity, SDL_Window* sdlWindow = nullptr)
        {
            auto& state = RuntimeFacade::current().state();

            if (state.focusedEntity != entt::null && Registry::Valid(state.focusedEntity))
            {
                ui::utils::MarkVisualChanged(state.focusedEntity);
                Registry::Remove<components::FocusedTag>(state.focusedEntity);
            }

            state.focusedEntity = entity;
            if (entity != entt::null && Registry::Valid(entity))
            {
                Registry::EmplaceOrReplace<components::FocusedTag>(entity);
                ui::utils::MarkVisualChanged(entity);

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

        static void clearFocus(SDL_Window* sdlWindow = nullptr)
        {
            auto& state = RuntimeFacade::current().state();

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

        static void handleEntityPress(const events::HitPointerButton& event)
        {
            if (event.hitEntity == entt::null)
            {
                return;
            }

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

        static void handleEntityRelease(
            StateSystem& system,
            const events::HitPointerButton& event,
            globalcontext::StateContext& state)
        {
            const entt::entity releasedEntity = state.activeEntity;
            const bool suppressClick = state.activeDragMoved;

            if (system.m_isDraggingSlider)
            {
                SliderStateHelpers::stopDrag(system);
            }

            if (state.hasScrollbarDrag())
            {
                if (Registry::Valid(state.dragScrollEntity))
                {
                    auto& scrollArea = Registry::Get<components::ScrollArea>(state.dragScrollEntity);
                    scrollArea.scrollbarPressed = false;
                    ui::utils::MarkVisualChanged(state.dragScrollEntity);
                }

                state.stopScrollbarDrag();
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
                if (!suppressClick && event.hitEntity != entt::null &&
                    Registry::TryGet<components::Clickable>(event.hitEntity) != nullptr)
                {
                    Logger::debug(
                        "StateSystem: Click Event (fallback) on entity {}",
                        static_cast<uint32_t>(event.hitEntity));
                    Dispatcher::Trigger<events::ClickEvent>(events::ClickEvent{event.hitEntity});
                }

                if (SDL_Window* sdlWindow = SDL_GetWindowFromID(event.raw.windowID))
                {
                    clearFocus(sdlWindow);
                }
            }

            if (releasedEntity != entt::null)
            {
                Dispatcher::Trigger<events::MouseReleaseEvent>(events::MouseReleaseEvent{releasedEntity});
            }
        }
    };

    struct WindowStateHelpers
    {
        static void handleClose(const events::CloseWindow& event)
        {
            if (Registry::Valid(event.entity))
            {
                StateSystem::destroyWidget(event.entity);
            }

            if (Registry::View<components::Window>().empty())
            {
                Dispatcher::Trigger<events::QuitRequested>();
            }
        }

        static void handlePixelSizeChanged(const events::WindowPixelSizeChanged& event)
        {
            const auto entity = RuntimeFacade::current().windowLookup().findById(event.windowID);
            if (!Registry::Valid(entity) || !Registry::AllOf<components::Size>(entity))
            {
                return;
            }

            auto& size = Registry::Get<components::Size>(entity);
            size.size.x() = static_cast<float>(event.width);
            size.size.y() = static_cast<float>(event.height);
            ui::utils::MarkLayoutAndVisualChanged(entity);
        }

        static void handleMoved(const events::WindowMoved& event)
        {
            const auto entity = RuntimeFacade::current().windowLookup().findById(event.windowID);
            if (!Registry::Valid(entity) || !Registry::AllOf<components::Position>(entity))
            {
                return;
            }

            auto& pos = Registry::Get<components::Position>(entity);
            pos.value.x() = static_cast<float>(event.x);
            pos.value.y() = static_cast<float>(event.y);
        }
    };

    struct EndFrameStateHelpers
    {
        static void flush(StateSystem& system)
        {
            for (entt::entity entity : system.m_pendingHoverRemove)
            {
                if (Registry::Valid(entity))
                {
                    Registry::Remove<components::HoveredTag>(entity);
                }
            }
            for (entt::entity entity : system.m_pendingHoverAdd)
            {
                if (Registry::Valid(entity))
                {
                    Registry::EmplaceOrReplace<components::HoveredTag>(entity);
                }
            }

            for (entt::entity entity : system.m_pendingActiveRemove)
            {
                if (Registry::Valid(entity))
                {
                    Registry::Remove<components::ActiveTag>(entity);
                }
            }
            for (entt::entity entity : system.m_pendingActiveAdd)
            {
                if (Registry::Valid(entity))
                {
                    Registry::EmplaceOrReplace<components::ActiveTag>(entity);
                }
            }

            system.m_pendingHoverAdd.clear();
            system.m_pendingHoverRemove.clear();
            system.m_pendingActiveAdd.clear();
            system.m_pendingActiveRemove.clear();
        }
    };

    static bool isWritableTextEdit(entt::entity entity) { return PointerStateHelpers::isWritableTextEdit(entity); }

    static bool shouldEmitPressForEntity(entt::entity entity)
    {
        return PointerStateHelpers::shouldEmitPressForEntity(entity);
    }

    static entt::entity findScrollTargetFromHit(entt::entity hitEntity)
    {
        return ScrollbarStateHelpers::findScrollTargetFromHit(hitEntity);
    }

    static entt::entity findScrollTargetAtPosition(const Vec2& pointerPosition)
    {
        return ScrollbarStateHelpers::findScrollTargetAtPosition(pointerPosition);
    }

    static void applyScrollWheelDelta(entt::entity target, const Vec2& scrollDelta)
    {
        ScrollbarStateHelpers::applyScrollWheelDelta(target, scrollDelta);
    }

    void queueHoveredEntity(globalcontext::StateContext& state, entt::entity entity)
    {
        PointerStateHelpers::queueHoveredEntity(*this, state, entity);
    }

    void queueHoverClear(globalcontext::StateContext& state, entt::entity entity)
    {
        PointerStateHelpers::queueHoverClear(*this, state, entity);
    }

    void queueActiveEntity(globalcontext::StateContext& state, entt::entity entity)
    {
        PointerStateHelpers::queueActiveEntity(*this, state, entity);
    }

    void queueActiveClear(globalcontext::StateContext& state, entt::entity entity)
    {
        PointerStateHelpers::queueActiveClear(*this, state, entity);
    }

    // ===================================================================
    // PointerState: Hover / Active 标签延迟状态机
    // ===================================================================

    /**
     * @brief 处理悬停事件 - 更新悬停状态（延迟应用）
     */
    void onHoverEvent(const events::HoverEvent& event)
    {
        auto& state = RuntimeFacade::current().state();
        queueHoveredEntity(state, event.entity);
    }

    /**
     * @brief 处理取消悬停事件（延迟应用）
     */
    void onUnhoverEvent(const events::UnhoverEvent& event)
    {
        auto& state = RuntimeFacade::current().state();
        queueHoverClear(state, event.entity);
    }

    /**
     * @brief 处理鼠标按下事件 - 设置激活状态（延迟应用）
     */
    void onMousePressEvent(const events::MousePressEvent& event)
    {
        auto& state = RuntimeFacade::current().state();
        queueActiveEntity(state, event.entity);
    }

    /**
     * @brief 处理鼠标释放事件 - 清除激活状态（延迟应用）
     */
    void onMouseReleaseEvent(const events::MouseReleaseEvent& event)
    {
        auto& state = RuntimeFacade::current().state();
        queueActiveClear(state, event.entity);
    }

    // ===================================================================
    // PointerState: 命中后的指针输入状态机
    // ===================================================================

    void onHitPointerMove(const events::HitPointerMove& event)
    {
        auto& state = RuntimeFacade::current().state();
        state.syncLatestPointer(event.raw.position, event.raw.delta);

        if (!state.activeDragMoved && state.activeEntity != entt::null && Registry::Valid(state.activeEntity) &&
            Registry::TryGet<components::Draggable>(state.activeEntity) != nullptr &&
            event.raw.delta != Vec2{0.0F, 0.0F})
        {
            state.activeDragMoved = true;
        }

        if (m_isDraggingSlider)
        {
            SliderStateHelpers::handleDrag(*this, event);
            return;
        }

        if (state.hasScrollbarDrag() && Registry::Valid(state.dragScrollEntity))
        {
            ScrollbarStateHelpers::handleDrag(event, state);
            return;
        }

        ScrollbarStateHelpers::updateHoverStates(event, state);
        PointerStateHelpers::handleHoverUpdate(event, state);
    }

    void onHitPointerButton(const events::HitPointerButton& event)
    {
        if (event.raw.button != SDL_BUTTON_LEFT)
        {
            return;
        }

        auto& state = RuntimeFacade::current().state();
        state.latestMousePosition = event.raw.position;

        if (event.raw.pressed)
        {
            if (ScrollbarStateHelpers::tryHandlePress(event, state))
            {
                return;
            }
            if (SliderStateHelpers::tryHandlePress(*this, event))
            {
                return;
            }
            PointerStateHelpers::handleEntityPress(event);
            return;
        }

        PointerStateHelpers::handleEntityRelease(*this, event, state);
    }

    void onHitPointerWheel(const events::HitPointerWheel& event)
    {
        auto& state = RuntimeFacade::current().state();
        state.syncLatestScroll(event.raw.delta);

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
        PointerStateHelpers::setFocus(entity, sdlWindow);
    }

    /**
     * @brief 清除当前焦点
     */
    static void clearFocus(SDL_Window* sdlWindow = nullptr)
    {
        PointerStateHelpers::clearFocus(sdlWindow);
    }

    // ===================================================================
    // WindowSyncState: 窗口生命周期与 SDL 同步
    // ===================================================================

    void onCloseWindow(const events::CloseWindow& event)
    {
        WindowStateHelpers::handleClose(event);
    }

    /**
     * @brief 处理窗口尺寸变化事件
     */
    void onWindowPixelSizeChanged(const events::WindowPixelSizeChanged& event)
    {
        WindowStateHelpers::handlePixelSizeChanged(event);
    }

    /**
     * @brief 处理窗口位置变化事件
     */
    void onWindowMoved(const events::WindowMoved& event)
    {
        WindowStateHelpers::handleMoved(event);
    }

    // ===================================================================
    // ScrollInteractionState: ScrollArea / Slider 交互
    // ===================================================================

    /**
     * @brief 更新所有可见 ScrollArea 的滚动条悬停状态
     */
    void updateScrollbarHoverStates(const events::HitPointerMove& event)
    {
        ScrollbarStateHelpers::updateHoverStates(event, RuntimeFacade::current().state());
    }

    void handleScrollbarDrag(const events::HitPointerMove& event, globalcontext::StateContext& state)
    {
        ScrollbarStateHelpers::handleDrag(event, state);
    }

    void handleHoverUpdate(const events::HitPointerMove& event, const globalcontext::StateContext& state)
    {
        PointerStateHelpers::handleHoverUpdate(event, state);
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
        return ScrollbarStateHelpers::tryHandlePress(event, state);
    }

    void handleEntityPress(const events::HitPointerButton& event)
    {
        PointerStateHelpers::handleEntityPress(event);
    }
    /**
     * @brief 处理实体释放事件
     * @param event 点击事件
     * @param state 状态上下文
     */
    void handleEntityRelease(const events::HitPointerButton& event, globalcontext::StateContext& state)
    {
        PointerStateHelpers::handleEntityRelease(*this, event, state);
    }

    bool tryHandleSliderPress(const events::HitPointerButton& event)
    {
        return SliderStateHelpers::tryHandlePress(*this, event);
    }

    void handleSliderDrag(const events::HitPointerMove& event)
    {
        SliderStateHelpers::handleDrag(*this, event);
    }

    void stopSliderDrag()
    {
        SliderStateHelpers::stopDrag(*this);
    }

    void updateSliderValueFromPointer(entt::entity entity, const Vec2& mousePos)
    {
        SliderStateHelpers::updateValueFromPointer(entity, mousePos);
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
        return ScrollbarStateHelpers::checkHit(entity, mousePos, outIsVertical);
    }

    void calculateScrollbarGeometry(entt::entity entity, bool isVertical, float& outTrackLen, float& outThumbSize)
    {
        ScrollbarStateHelpers::calculateGeometry(entity, isVertical, outTrackLen, outThumbSize);
    }

    /**
     * @brief 处理点击轨道跳转
     * @param entity 滚动区域实体
     * @param mousePos 鼠标位置
     * @param isVertical 是否为垂直滚动条
     */
    void handleTrackClick(entt::entity entity, const Vec2& mousePos, bool isVertical)
    {
        ScrollbarStateHelpers::handleTrackClick(entity, mousePos, isVertical);
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
        EndFrameStateHelpers::flush(*this);
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
                RuntimeFacade::current().windowLookup().invalidateId(windowComp->windowID);

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