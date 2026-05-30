#include "StateSystem.hpp"
#include "api/Table.hpp"
#include "api/Utils.hpp"
#include "common/components/Data.hpp"
#include "common/components/Interaction.hpp"
#include "common/components/Layout.hpp"
#include "common/components/Window.hpp"
#include "common/Tags.hpp"
#include "singleton/Dispatcher.hpp"
#include "core/RuntimeFacade.hpp"
#include "HitTestSystem.hpp"
namespace ui::systems
{

namespace
{

[[nodiscard]] Rect GetHorizontalScrollbarTrackRect(entt::entity entity)
{
    static constexpr float TRACK_HEIGHT = 12.0F;
    static constexpr float TRACK_PADDING = 2.0F;
    const Rect entityRect = ui::utils::GetEntityRect(entity);
    return {entityRect.x(),
            entityRect.y() + entityRect.height() - TRACK_HEIGHT - TRACK_PADDING,
            entityRect.width(),
            TRACK_HEIGHT};
}

[[nodiscard]] Rect GetHorizontalScrollbarThumbRect(entt::entity entity, const components::ScrollArea& scrollArea)
{
    static constexpr float TRACK_HEIGHT = 12.0F;
    static constexpr float TRACK_PADDING = 2.0F;
    static constexpr float BAR_HEIGHT = 10.0F;
    const Rect entityRect = ui::utils::GetEntityRect(entity);
    const float viewportWidth = ui::utils::GetScrollViewportLength(entity, false);
    const float contentWidth = std::max(1.0F, scrollArea.contentSize.x());
    const float visibleRatio = viewportWidth / contentWidth;
    const float thumbWidth =
        std::max(components::ScrollArea::SCROLLBAR_THUMB_MIN_SIZE, entityRect.width() * visibleRatio);
    const float maxScroll = std::max(0.0F, contentWidth - viewportWidth);
    const float scrollRatio = maxScroll > 0.0F ? std::clamp(scrollArea.scrollOffset.x() / maxScroll, 0.0F, 1.0F) : 0.0F;
    const float thumbPos =
        std::clamp((entityRect.width() - thumbWidth) * scrollRatio, 0.0F, entityRect.width() - thumbWidth);

    return {entityRect.x() + thumbPos + 1.0F,
            entityRect.y() + entityRect.height() - BAR_HEIGHT - TRACK_PADDING - 1.0F,
            std::max(0.0F, thumbWidth - 4.0F),
            std::max(0.0F, BAR_HEIGHT - (TRACK_HEIGHT - BAR_HEIGHT))};
}

} // namespace

void StateSystem::registerHandlersImpl()
{
    // 窗口事件
    m_disp->sink<events::CloseWindow>().connect<&StateSystem::onCloseWindow>(*this);
    m_disp->sink<events::WindowPixelSizeChanged>().connect<&StateSystem::onWindowPixelSizeChanged>(*this);
    m_disp->sink<events::WindowMoved>().connect<&StateSystem::onWindowMoved>(*this);

    // 交互事件
    m_disp->sink<events::HoverEvent>().connect<&StateSystem::onHoverEvent>(*this);
    m_disp->sink<events::UnhoverEvent>().connect<&StateSystem::onUnhoverEvent>(*this);
    m_disp->sink<events::MousePressEvent>().connect<&StateSystem::onMousePressEvent>(*this);
    m_disp->sink<events::MouseReleaseEvent>().connect<&StateSystem::onMouseReleaseEvent>(*this);

    // 命中测试后的输入事件（由 HitTestSystem 发送）
    m_disp->sink<events::HitPointerMove>().connect<&StateSystem::onHitPointerMove>(*this);
    m_disp->sink<events::HitPointerButton>().connect<&StateSystem::onHitPointerButton>(*this);
    m_disp->sink<events::HitPointerWheel>().connect<&StateSystem::onHitPointerWheel>(*this);

    m_disp->sink<events::EndFrame>().connect<&StateSystem::onEndFrame>(*this);
}

void StateSystem::unregisterHandlersImpl()
{
    m_disp->sink<events::CloseWindow>().disconnect<&StateSystem::onCloseWindow>(*this);
    m_disp->sink<events::WindowPixelSizeChanged>().disconnect<&StateSystem::onWindowPixelSizeChanged>(*this);
    m_disp->sink<events::WindowMoved>().disconnect<&StateSystem::onWindowMoved>(*this);
    m_disp->sink<events::HoverEvent>().disconnect<&StateSystem::onHoverEvent>(*this);
    m_disp->sink<events::UnhoverEvent>().disconnect<&StateSystem::onUnhoverEvent>(*this);
    m_disp->sink<events::MousePressEvent>().disconnect<&StateSystem::onMousePressEvent>(*this);
    m_disp->sink<events::MouseReleaseEvent>().disconnect<&StateSystem::onMouseReleaseEvent>(*this);

    m_disp->sink<events::HitPointerMove>().disconnect<&StateSystem::onHitPointerMove>(*this);
    m_disp->sink<events::HitPointerButton>().disconnect<&StateSystem::onHitPointerButton>(*this);
    m_disp->sink<events::HitPointerWheel>().disconnect<&StateSystem::onHitPointerWheel>(*this);
    m_disp->sink<events::EndFrame>().disconnect<&StateSystem::onEndFrame>(*this);
}

// =============================================================================
// SliderStateHelpers
// =============================================================================

bool StateSystem::SliderStateHelpers::tryHandlePress(StateSystem& system, const events::HitPointerButton& event)
{
    const auto& reg = *system.m_reg;
    if (event.hitEntity == entt::null || !reg.valid(event.hitEntity))
    {
        return false;
    }

    if (reg.try_get<components::SliderInfo>(event.hitEntity) == nullptr)
    {
        return false;
    }

    system.m_isDraggingSlider = true;
    system.m_dragSliderEntity = event.hitEntity;
    updateValueFromPointer(system, event.hitEntity, event.raw.position);
    return true;
}

void StateSystem::SliderStateHelpers::handleDrag(StateSystem& system, const events::HitPointerMove& event)
{
    const auto& reg = *system.m_reg;
    if (!reg.valid(system.m_dragSliderEntity)
        || reg.try_get<components::SliderInfo>(system.m_dragSliderEntity) == nullptr)
    {
        stopDrag(system);
        return;
    }

    updateValueFromPointer(system, system.m_dragSliderEntity, event.raw.position);
}

void StateSystem::SliderStateHelpers::stopDrag(StateSystem& system)
{
    system.m_isDraggingSlider = false;
    system.m_dragSliderEntity = entt::null;
}

void StateSystem::SliderStateHelpers::updateValueFromPointer(StateSystem& system,
                                                             entt::entity entity,
                                                             const Vec2& mousePos)
{
    auto& reg = *system.m_reg;
    auto* slider = reg.try_get<components::SliderInfo>(entity);
    const auto* sizeComp = reg.try_get<components::Size>(entity);
    if (slider == nullptr || sizeComp == nullptr)
    {
        return;
    }

    Vec2 absPos = HitTestSystem::getAbsolutePosition(entity);
    const Vec2 size = sizeComp->size;

    float ratio = 0.0F;
    if (slider->vertical == policies::Orientation::VERTICAL)
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

// =============================================================================
// ScrollbarStateHelpers
// =============================================================================

entt::entity StateSystem::ScrollbarStateHelpers::findScrollTargetFromHit(StateSystem& system, entt::entity hitEntity)
{
    const auto& reg = *system.m_reg;
    entt::entity current = hitEntity;
    while (current != entt::null && reg.valid(current))
    {
        if (reg.any_of<components::ScrollArea>(current))
        {
            return current;
        }

        if (const auto* hierarchy = reg.try_get<components::Hierarchy>(current))
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

entt::entity StateSystem::ScrollbarStateHelpers::findScrollTargetAtPosition(StateSystem& system,
                                                                            const Vec2& pointerPosition)
{
    auto& reg = *system.m_reg;
    auto view = reg.view<components::ScrollArea, components::Size, components::VisibleTag, components::Position>();
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

void StateSystem::ScrollbarStateHelpers::applyScrollWheelDelta(StateSystem& system,
                                                               entt::entity target,
                                                               const Vec2& scrollDelta)
{
    auto& reg = *system.m_reg;
    auto& scroll = reg.get<components::ScrollArea>(target);
    bool changed = false;

    const bool canScrollVertical =
        scroll.scroll == policies::Scroll::VERTICAL || scroll.scroll == policies::Scroll::BOTH;
    if (canScrollVertical && scrollDelta.y() != 0.0F)
    {
        const float deltaY = -scrollDelta.y() * 30.0F;
        const float oldOffsetY = scroll.scrollOffset.y();
        scroll.scrollOffset.y() =
            std::clamp(scroll.scrollOffset.y() + deltaY, 0.0F, ui::utils::GetScrollMaxOffset(target, true));
        changed = changed || oldOffsetY != scroll.scrollOffset.y();
    }

    const bool canScrollHorizontal =
        scroll.scroll == policies::Scroll::HORIZONTAL || scroll.scroll == policies::Scroll::BOTH;
    if (canScrollHorizontal && scrollDelta.x() != 0.0F)
    {
        const float deltaX = -scrollDelta.x() * 30.0F;
        const float oldOffsetX = scroll.scrollOffset.x();
        scroll.scrollOffset.x() =
            std::clamp(scroll.scrollOffset.x() + deltaX, 0.0F, ui::utils::GetScrollMaxOffset(target, false));
        changed = changed || oldOffsetX != scroll.scrollOffset.x();
    }

    if (changed)
    {
        ui::utils::MarkLayoutAndVisualChanged(target);
    }
}

void StateSystem::ScrollbarStateHelpers::updateHoverStates(StateSystem& system,
                                                           const events::HitPointerMove& event,
                                                           const globalcontext::StateContext& state)
{
    auto& reg = *system.m_reg;
    auto view = reg.view<components::ScrollArea, components::Size, components::VisibleTag>();

    for (auto entity : view)
    {
        auto& scrollArea = view.get<components::ScrollArea>(entity);
        const auto* prevState = reg.try_get<components::ScrollBarInteractionState>(entity);
        const bool wasHovered = prevState != nullptr && (prevState->scrollbarHovered || prevState->trackHovered);

        if (state.hasScrollbarDrag() && state.dragScrollEntity == entity)
        {
            continue;
        }

        auto& interState = reg.get_or_emplace<components::ScrollBarInteractionState>(entity);
        interState.scrollbarHovered = false;
        interState.trackHovered = false;

        if (policies::HasFlag(scrollArea.scrollBar, policies::ScrollBar::NO_VISIBILITY))
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
            interState.trackHovered = true;

            if (geometry.thumbRect.contains(event.raw.position))
            {
                interState.scrollbarHovered = true;
            }
        }

        if (wasHovered != (interState.scrollbarHovered || interState.trackHovered))
        {
            ui::utils::MarkVisualChanged(entity);
        }
    }
}

void StateSystem::ScrollbarStateHelpers::handleDrag(StateSystem& system,
                                                    const events::HitPointerMove& event,
                                                    globalcontext::StateContext& state)
{
    const float deltaPix = state.isVerticalDrag ? (event.raw.position.y() - state.dragStartMousePos.y())
                                                : (event.raw.position.x() - state.dragStartMousePos.x());

    auto& reg = *system.m_reg;
    auto& scroll = reg.get<components::ScrollArea>(state.dragScrollEntity);
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

bool StateSystem::ScrollbarStateHelpers::tryHandlePress(StateSystem& system,
                                                        const events::HitPointerButton& event,
                                                        globalcontext::StateContext& state)
{
    auto& reg = *system.m_reg;
    entt::entity scrollEntity = entt::null;
    bool isVertical = true;
    bool clickedOnThumb = false;
    entt::entity current = event.hitEntity;

    while (current != entt::null && reg.valid(current))
    {
        if (reg.any_of<components::ScrollArea>(current))
        {
            const auto hitType = checkHit(system, current, event.raw.position, isVertical);
            if (hitType != ScrollbarHitType::NONE)
            {
                scrollEntity = current;
                clickedOnThumb = (hitType == ScrollbarHitType::THUMB);
                break;
            }
        }

        const auto* hierarchy = reg.try_get<components::Hierarchy>(current);
        current = hierarchy != nullptr ? hierarchy->parent : entt::null;
    }

    if (scrollEntity == entt::null)
    {
        return false;
    }

    auto& scroll = reg.get<components::ScrollArea>(scrollEntity);
    if (clickedOnThumb)
    {
        reg.get_or_emplace<components::ScrollBarInteractionState>(scrollEntity).scrollbarPressed = true;

        float trackLength = 0.0F;
        float thumbSize = 0.0F;
        calculateGeometry(scrollEntity, isVertical, trackLength, thumbSize);
        state.beginScrollbarDrag(
            scrollEntity, event.raw.position, scroll.scrollOffset, isVertical, trackLength, thumbSize);
    }
    else
    {
        handleTrackClick(system, scrollEntity, event.raw.position, isVertical);
    }

    ui::utils::MarkVisualChanged(scrollEntity);
    return true;
}

StateSystem::ScrollbarHitType StateSystem::ScrollbarStateHelpers::checkHit(StateSystem& system,
                                                                           entt::entity entity,
                                                                           const Vec2& mousePos,
                                                                           bool& outIsVertical)
{
    auto& reg = *system.m_reg;
    const auto* scrollArea = reg.try_get<components::ScrollArea>(entity);
    if (scrollArea == nullptr || policies::HasFlag(scrollArea->scrollBar, policies::ScrollBar::NO_VISIBILITY))
    {
        return ScrollbarHitType::NONE;
    }
    if (!policies::HasFlag(scrollArea->scrollBar, policies::ScrollBar::DRAGGABLE))
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

    const bool hasHorizontalScroll =
        scrollArea->scroll == policies::Scroll::HORIZONTAL || scrollArea->scroll == policies::Scroll::BOTH;
    const float viewportWidth = ui::utils::GetScrollViewportLength(entity, false);
    if (hasHorizontalScroll && scrollArea->contentSize.x() > viewportWidth)
    {
        const Rect trackRect = GetHorizontalScrollbarTrackRect(entity);
        if (trackRect.contains(mousePos))
        {
            outIsVertical = false;
            const Rect thumbRect = GetHorizontalScrollbarThumbRect(entity, *scrollArea);
            if (thumbRect.contains(mousePos))
            {
                return ScrollbarHitType::THUMB;
            }
            return ScrollbarHitType::TRACK;
        }
    }
    return ScrollbarHitType::NONE;
}

void StateSystem::ScrollbarStateHelpers::calculateGeometry(entt::entity entity,
                                                           bool isVertical,
                                                           float& outTrackLen,
                                                           float& outThumbSize)
{
    outTrackLen = isVertical ? ui::utils::GetEntityRect(entity).height() : ui::utils::GetEntityRect(entity).width();
    const float viewportSize = ui::utils::GetScrollViewportLength(entity, isVertical);
    const float contentSize = std::max(1.0F, ui::utils::GetScrollContentLength(entity, isVertical));
    const float visibleRatio = std::max(0.001F, viewportSize / contentSize);
    outThumbSize = std::max(components::ScrollArea::SCROLLBAR_THUMB_MIN_SIZE, outTrackLen * visibleRatio);
}

void StateSystem::ScrollbarStateHelpers::handleTrackClick(StateSystem& system,
                                                          entt::entity entity,
                                                          const Vec2& mousePos,
                                                          bool isVertical)
{
    auto& reg = *system.m_reg;
    auto& scrollArea = reg.get<components::ScrollArea>(entity);
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
    const float thumbSize = std::max(components::ScrollArea::SCROLLBAR_THUMB_MIN_SIZE, trackLen * visibleRatio);
    const float scrollableTrack = trackLen - thumbSize;
    float targetThumbPos = clickPos - (thumbSize * 0.5F);
    targetThumbPos = std::clamp(targetThumbPos, 0.0F, scrollableTrack);

    const float targetRatio = scrollableTrack > 0.0F ? (targetThumbPos / scrollableTrack) : 0.0F;
    scrollOffset = std::clamp(targetRatio * maxScroll, 0.0F, maxScroll);

    ui::utils::MarkLayoutAndVisualChanged(entity);
}

// =============================================================================
// PointerStateHelpers
// =============================================================================

bool StateSystem::PointerStateHelpers::isWritableTextEdit(StateSystem& system, entt::entity entity)
{
    const auto& reg = *system.m_reg;
    if (!reg.valid(entity) || !reg.any_of<components::TextEditTag>(entity))
    {
        return false;
    }

    const auto* edit = reg.try_get<components::TextEdit>(entity);
    return edit == nullptr || !policies::HasFlag(edit->inputMode, policies::TextFlag::READ_ONLY);
}

bool StateSystem::PointerStateHelpers::shouldEmitPressForEntity(StateSystem& system, entt::entity entity)
{
    const auto& reg = *system.m_reg;
    return reg.valid(entity)
        && (reg.any_of<components::Pressable>(entity) || reg.any_of<components::Clickable>(entity)
            || reg.any_of<components::TextEditTag>(entity));
}

void StateSystem::PointerStateHelpers::queueHoveredEntity(StateSystem& system,
                                                          globalcontext::StateContext& state,
                                                          entt::entity entity)
{
    const auto& reg = *system.m_reg;
    if (state.hoveredEntity != entt::null && reg.valid(state.hoveredEntity))
    {
        system.m_pendingHoverRemove.insert(state.hoveredEntity);
    }

    state.hoveredEntity = entity;
    if (reg.valid(entity))
    {
        system.m_pendingHoverAdd.insert(entity);
        system.m_pendingHoverRemove.erase(entity);
    }
}

void StateSystem::PointerStateHelpers::queueHoverClear(StateSystem& system,
                                                       globalcontext::StateContext& state,
                                                       entt::entity entity)
{
    const auto& reg = *system.m_reg;
    if (reg.valid(entity))
    {
        system.m_pendingHoverRemove.insert(entity);
        system.m_pendingHoverAdd.erase(entity);
    }

    if (state.hoveredEntity == entity)
    {
        state.hoveredEntity = entt::null;
    }
}

void StateSystem::PointerStateHelpers::queueActiveEntity(StateSystem& system,
                                                         globalcontext::StateContext& state,
                                                         entt::entity entity)
{
    state.activeDragMoved = false;

    const auto& reg = *system.m_reg;
    if (state.activeEntity != entt::null && reg.valid(state.activeEntity))
    {
        system.m_pendingActiveRemove.insert(state.activeEntity);
    }

    state.activeEntity = entity;
    if (reg.valid(entity))
    {
        system.m_pendingActiveAdd.insert(entity);
        system.m_pendingActiveRemove.erase(entity);
    }
}

void StateSystem::PointerStateHelpers::queueActiveClear(StateSystem& system,
                                                        globalcontext::StateContext& state,
                                                        entt::entity entity)
{
    const auto& reg = *system.m_reg;
    if (reg.valid(entity))
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

void StateSystem::PointerStateHelpers::handleHoverUpdate(StateSystem& system,
                                                         const events::HitPointerMove& event,
                                                         const globalcontext::StateContext& state)
{
    const auto& reg = *system.m_reg;
    auto& disp = *system.m_disp;
    if (event.hitEntity != state.hoveredEntity)
    {
        if (state.hoveredEntity != entt::null && reg.valid(state.hoveredEntity))
        {
            disp.enqueue<events::UnhoverEvent>(events::UnhoverEvent{state.hoveredEntity});
        }
        if (event.hitEntity != entt::null)
        {
            disp.enqueue<events::HoverEvent>(events::HoverEvent{event.hitEntity});
        }
    }
}

void StateSystem::PointerStateHelpers::setFocus(StateSystem& system, entt::entity entity, SDL_Window* sdlWindow)
{
    auto& state = RuntimeFacade::current().state();
    auto& reg = *system.m_reg;

    if (state.focusedEntity != entt::null && reg.valid(state.focusedEntity))
    {
        ui::utils::MarkVisualChanged(state.focusedEntity);
        reg.remove<components::FocusedTag>(state.focusedEntity);
    }

    state.focusedEntity = entity;
    if (entity != entt::null && reg.valid(entity))
    {
        reg.emplace_or_replace<components::FocusedTag>(entity);
        ui::utils::MarkVisualChanged(entity);

        if (reg.any_of<components::TextEditTag>(entity))
        {
            reg.emplace_or_replace<components::Caret>(entity);
            auto& caret = reg.get<components::Caret>(entity);
            caret.visible = true;
            caret.elapsedTime = 0.0F;

            if (sdlWindow != nullptr)
            {
                SDL_StartTextInput(sdlWindow);
            }
        }
    }
}

void StateSystem::PointerStateHelpers::clearFocus(StateSystem& system, SDL_Window* sdlWindow)
{
    auto& state = RuntimeFacade::current().state();
    auto& reg = *system.m_reg;

    if (state.focusedEntity != entt::null && reg.valid(state.focusedEntity))
    {
        ui::utils::MarkVisualChanged(state.focusedEntity);
        reg.remove<components::FocusedTag>(state.focusedEntity);
        state.focusedEntity = entt::null;
    }

    if (sdlWindow != nullptr)
    {
        SDL_StopTextInput(sdlWindow);
    }
}

void StateSystem::PointerStateHelpers::handleEntityPress(StateSystem& system, const events::HitPointerButton& event)
{
    if (event.hitEntity == entt::null)
    {
        return;
    }

    if (isWritableTextEdit(system, event.hitEntity))
    {
        if (SDL_Window* sdlWindow = SDL_GetWindowFromID(event.raw.windowID))
        {
            setFocus(system, event.hitEntity, sdlWindow);
        }
    }

    if (shouldEmitPressForEntity(system, event.hitEntity))
    {
        system.m_disp->trigger<events::MousePressEvent>(events::MousePressEvent{event.hitEntity});
    }
}

bool StateSystem::PointerStateHelpers::finishScrollbarDragRelease(StateSystem& system,
                                                                  globalcontext::StateContext& state,
                                                                  entt::entity releasedEntity)
{
    if (!state.hasScrollbarDrag())
    {
        return false;
    }

    auto& reg = *system.m_reg;
    if (reg.valid(state.dragScrollEntity))
    {
        if (auto* ist = reg.try_get<components::ScrollBarInteractionState>(state.dragScrollEntity))
        {
            ist->scrollbarPressed = false;
        }
        ui::utils::MarkVisualChanged(state.dragScrollEntity);
    }

    state.stopScrollbarDrag();
    if (releasedEntity != entt::null)
    {
        queueActiveClear(system, state, releasedEntity);
    }
    return true;
}

void StateSystem::PointerStateHelpers::handleEntityRelease(StateSystem& system,
                                                           const events::HitPointerButton& event,
                                                           globalcontext::StateContext& state)
{
    auto& reg = *system.m_reg;
    auto& disp = *system.m_disp;
    const entt::entity releasedEntity = state.activeEntity;
    const bool hadScrollbarDrag = state.hasScrollbarDrag();
    constexpr float CLICK_DRAG_THRESHOLD_SQ = 9.0F;
    const Vec2 releaseDragDistance = event.raw.position - state.activePressPosition;
    const bool suppressClick =
        state.activeDragMoved || hadScrollbarDrag || releaseDragDistance.squaredNorm() > CLICK_DRAG_THRESHOLD_SQ;

    if (system.m_isDraggingSlider)
    {
        SliderStateHelpers::stopDrag(system);
    }

    if (finishScrollbarDragRelease(system, state, releasedEntity))
    {
        return;
    }

    if (!suppressClick)
    {
        tryEmitTableCellClicked(system, event.hitEntity, event.raw.position);
    }

    if (!suppressClick && releasedEntity != entt::null && releasedEntity == event.hitEntity)
    {
        if (reg.try_get<components::Clickable>(releasedEntity) != nullptr)
        {
            disp.trigger<events::ClickEvent>(events::ClickEvent{releasedEntity});
        }

        if (SDL_Window* sdlWindow = SDL_GetWindowFromID(event.raw.windowID))
        {
            if (isWritableTextEdit(system, releasedEntity))
            {
                setFocus(system, releasedEntity, sdlWindow);
                return;
            }
            clearFocus(system, sdlWindow);
        }
    }
    else
    {
        if (!suppressClick && event.hitEntity != entt::null
            && reg.try_get<components::Clickable>(event.hitEntity) != nullptr)
        {
            Logger::debug("StateSystem: Click Event (fallback) on entity {}", static_cast<uint32_t>(event.hitEntity));
            disp.trigger<events::ClickEvent>(events::ClickEvent{event.hitEntity});
        }

        if (SDL_Window* sdlWindow = SDL_GetWindowFromID(event.raw.windowID))
        {
            clearFocus(system, sdlWindow);
        }
    }

    if (releasedEntity != entt::null)
    {
        disp.trigger<events::MouseReleaseEvent>(events::MouseReleaseEvent{releasedEntity});
    }
}

void StateSystem::PointerStateHelpers::tryEmitTableCellClicked(StateSystem& system,
                                                               entt::entity hitEntity,
                                                               const Vec2& pointerPosition)
{
    auto& reg = *system.m_reg;
    if (!reg.valid(hitEntity) || !reg.any_of<components::TableTag>(hitEntity))
    {
        return;
    }

    const auto* info = reg.try_get<components::TableInfo>(hitEntity);
    const auto* sizeComp = reg.try_get<components::Size>(hitEntity);
    if (info == nullptr || sizeComp == nullptr || info->columnCount <= 0)
    {
        return;
    }

    const Vec2 absPos = HitTestSystem::getAbsolutePosition(hitEntity);
    const float localX = pointerPosition.x() - absPos.x();
    const float localY = pointerPosition.y() - absPos.y();
    const float tableWidth = sizeComp->size.x();
    const float tableHeight = sizeComp->size.y();
    if (localX < 0.0F || localY < 0.0F || localX >= tableWidth || localY >= tableHeight)
    {
        return;
    }

    if (localY < info->headerHeight)
    {
        return;
    }

    float bodyY = localY - info->headerHeight;
    float contentX = localX; // 将本地坐标转换为内容坐标（加上水平滚动偏移）
    if (const auto* scroll = reg.try_get<components::ScrollArea>(hitEntity))
    {
        bodyY += scroll->scrollOffset.y();
        contentX += scroll->scrollOffset.x();
    }

    const float effectiveRowH = std::max(0.0F, std::max(info->rowHeight, info->minRowHeight));
    if (effectiveRowH <= 0.0F)
    {
        return;
    }
    const int row = static_cast<int>(bodyY / effectiveRowH);
    const int rowCount = static_cast<int>(info->cells.size());
    if (row < 0 || row >= rowCount)
    {
        return;
    }

    // 使用 ComputeColumnWidths 计算实际列宽（与渲染层共享逻辑）
    const std::vector<float> colWidths = ui::table::ComputeColumnWidths(*info, tableWidth);

    int col = -1;
    float xCursor = 0.0F;
    for (int i = 0; i < info->columnCount; ++i)
    {
        const float width = colWidths.at(static_cast<size_t>(i));
        const float xEnd = xCursor + width;
        if (contentX >= xCursor && contentX < xEnd)
        {
            col = i;
            break;
        }
        if (i == info->columnCount - 1 && contentX >= xCursor)
        {
            col = i; // 最后一列兜底
        }
        xCursor = xEnd;
    }

    if (col < 0 || col >= info->columnCount)
    {
        return;
    }

    system.m_disp->trigger<events::TableCellClicked>(events::TableCellClicked{hitEntity, row, col});
}

// =============================================================================
// WindowStateHelpers
// =============================================================================

void StateSystem::WindowStateHelpers::handleClose(StateSystem& system, const events::CloseWindow& event)
{
    auto& reg = *system.m_reg;
    if (reg.valid(event.entity))
    {
        system.destroyWidget(event.entity);
    }

    if (reg.view<components::Window>().empty())
    {
        system.m_disp->trigger<events::QuitRequested>();
    }
}

void StateSystem::WindowStateHelpers::handlePixelSizeChanged(StateSystem& system,
                                                             const events::WindowPixelSizeChanged& event)
{
    auto& reg = *system.m_reg;
    const auto entity = RuntimeFacade::current().windowLookup().findById(event.windowID);
    if (!reg.valid(entity) || !reg.all_of<components::Size>(entity))
    {
        return;
    }

    auto& size = reg.get<components::Size>(entity);
    size.size.x() = static_cast<float>(event.width);
    size.size.y() = static_cast<float>(event.height);
    ui::utils::MarkLayoutAndVisualChanged(entity);
}

void StateSystem::WindowStateHelpers::handleMoved(StateSystem& system, const events::WindowMoved& event)
{
    auto& reg = *system.m_reg;
    const auto entity = RuntimeFacade::current().windowLookup().findById(event.windowID);
    if (!reg.valid(entity) || !reg.all_of<components::Position>(entity))
    {
        return;
    }

    auto& pos = reg.get<components::Position>(entity);
    pos.value.x() = static_cast<float>(event.x);
    pos.value.y() = static_cast<float>(event.y);
}

// =============================================================================
// EndFrameStateHelpers
// =============================================================================

void StateSystem::EndFrameStateHelpers::flush(StateSystem& system)
{
    auto& reg = *system.m_reg;

    for (entt::entity entity : system.m_pendingHoverRemove)
    {
        if (reg.valid(entity))
        {
            reg.remove<components::HoveredTag>(entity);
        }
    }
    for (entt::entity entity : system.m_pendingHoverAdd)
    {
        if (reg.valid(entity))
        {
            reg.emplace_or_replace<components::HoveredTag>(entity);
        }
    }

    for (entt::entity entity : system.m_pendingActiveRemove)
    {
        if (reg.valid(entity))
        {
            reg.remove<components::ActiveTag>(entity);
        }
    }
    for (entt::entity entity : system.m_pendingActiveAdd)
    {
        if (reg.valid(entity))
        {
            reg.emplace_or_replace<components::ActiveTag>(entity);
        }
    }

    system.m_pendingHoverAdd.clear();
    system.m_pendingHoverRemove.clear();
    system.m_pendingActiveAdd.clear();
    system.m_pendingActiveRemove.clear();
}

// =============================================================================
// StateSystem — forwarding wrappers
// =============================================================================

bool StateSystem::isWritableTextEdit(entt::entity entity)
{
    return PointerStateHelpers::isWritableTextEdit(*this, entity);
}

bool StateSystem::shouldEmitPressForEntity(entt::entity entity)
{
    return PointerStateHelpers::shouldEmitPressForEntity(*this, entity);
}

entt::entity StateSystem::findScrollTargetFromHit(entt::entity hitEntity)
{
    return ScrollbarStateHelpers::findScrollTargetFromHit(*this, hitEntity);
}

entt::entity StateSystem::findScrollTargetAtPosition(const Vec2& pointerPosition)
{
    return ScrollbarStateHelpers::findScrollTargetAtPosition(*this, pointerPosition);
}

void StateSystem::applyScrollWheelDelta(entt::entity target, const Vec2& scrollDelta)
{
    ScrollbarStateHelpers::applyScrollWheelDelta(*this, target, scrollDelta);
}

void StateSystem::queueHoveredEntity(globalcontext::StateContext& state, entt::entity entity)
{
    PointerStateHelpers::queueHoveredEntity(*this, state, entity);
}

void StateSystem::queueHoverClear(globalcontext::StateContext& state, entt::entity entity)
{
    PointerStateHelpers::queueHoverClear(*this, state, entity);
}

void StateSystem::queueActiveEntity(globalcontext::StateContext& state, entt::entity entity)
{
    PointerStateHelpers::queueActiveEntity(*this, state, entity);
}

void StateSystem::queueActiveClear(globalcontext::StateContext& state, entt::entity entity)
{
    PointerStateHelpers::queueActiveClear(*this, state, entity);
}

// =============================================================================
// StateSystem — event handlers
// =============================================================================

/**
 */
void StateSystem::onHoverEvent(const events::HoverEvent& event)
{
    auto& state = RuntimeFacade::current().state();
    queueHoveredEntity(state, event.entity);
}

/**
 * @brief 处理取消悬停事件（延迟应用）
 */
void StateSystem::onUnhoverEvent(const events::UnhoverEvent& event)
{
    auto& state = RuntimeFacade::current().state();
    queueHoverClear(state, event.entity);
}

/**
 */
void StateSystem::onMousePressEvent(const events::MousePressEvent& event)
{
    auto& state = RuntimeFacade::current().state();
    queueActiveEntity(state, event.entity);
}

/**
 */
void StateSystem::onMouseReleaseEvent(const events::MouseReleaseEvent& event)
{
    auto& state = RuntimeFacade::current().state();
    queueActiveClear(state, event.entity);
}

void StateSystem::onHitPointerMove(const events::HitPointerMove& event)
{
    auto& reg = *m_reg;
    auto& state = RuntimeFacade::current().state();
    state.syncLatestPointer(event.raw.position, event.raw.delta);

    if (!state.activeDragMoved && state.activeEntity != entt::null && reg.valid(state.activeEntity))
    {
        constexpr float CLICK_DRAG_THRESHOLD_SQ = 9.0F;
        const Vec2 dragDistance = event.raw.position - state.activePressPosition;
        state.activeDragMoved = dragDistance.squaredNorm() > CLICK_DRAG_THRESHOLD_SQ;
    }

    if (m_isDraggingSlider)
    {
        SliderStateHelpers::handleDrag(*this, event);
        return;
    }

    if (state.hasScrollbarDrag() && reg.valid(state.dragScrollEntity))
    {
        ScrollbarStateHelpers::handleDrag(*this, event, state);
        return;
    }

    ScrollbarStateHelpers::updateHoverStates(*this, event, state);
    PointerStateHelpers::handleHoverUpdate(*this, event, state);
}

void StateSystem::onHitPointerButton(const events::HitPointerButton& event)
{
    if (event.raw.button != SDL_BUTTON_LEFT)
    {
        return;
    }

    auto& state = RuntimeFacade::current().state();
    state.latestMousePosition = event.raw.position;

    if (event.raw.pressed)
    {
        state.activePressPosition = event.raw.position;
        closeDropDownsOnOutsideClick(event.hitEntity);
        if (ScrollbarStateHelpers::tryHandlePress(*this, event, state))
        {
            return;
        }
        if (SliderStateHelpers::tryHandlePress(*this, event))
        {
            return;
        }
        PointerStateHelpers::handleEntityPress(*this, event);
        return;
    }

    PointerStateHelpers::handleEntityRelease(*this, event, state);
}

void StateSystem::onHitPointerWheel(const events::HitPointerWheel& event)
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
 */
void StateSystem::setFocus(entt::entity entity, SDL_Window* sdlWindow)
{
    PointerStateHelpers::setFocus(*this, entity, sdlWindow);
}

/**
 * @brief 清除当前焦点
 */
void StateSystem::clearFocus(SDL_Window* sdlWindow)
{
    PointerStateHelpers::clearFocus(*this, sdlWindow);
}

void StateSystem::onCloseWindow(const events::CloseWindow& event)
{
    WindowStateHelpers::handleClose(*this, event);
}

/**
 * @brief 处理窗口尺寸变化事件
 */
void StateSystem::onWindowPixelSizeChanged(const events::WindowPixelSizeChanged& event)
{
    WindowStateHelpers::handlePixelSizeChanged(*this, event);
}

/**
 * @brief 处理窗口位置变化事件
 */
void StateSystem::onWindowMoved(const events::WindowMoved& event)
{
    WindowStateHelpers::handleMoved(*this, event);
}

// =============================================================================
// StateSystem — ScrollInteractionState helpers
// =============================================================================

/**
 */
void StateSystem::updateScrollbarHoverStates(const events::HitPointerMove& event)
{
    ScrollbarStateHelpers::updateHoverStates(*this, event, RuntimeFacade::current().state());
}

void StateSystem::handleScrollbarDrag(const events::HitPointerMove& event, globalcontext::StateContext& state)
{
    ScrollbarStateHelpers::handleDrag(*this, event, state);
}

void StateSystem::handleHoverUpdate(const events::HitPointerMove& event, const globalcontext::StateContext& state)
{
    PointerStateHelpers::handleHoverUpdate(*this, event, state);
}

/**
 * @param event 点击事件
 * @param state 状态上下文
 * @return true 如果处理了滚动条按下事件
 * @return false 如果未处理滚动条按下事件
 */
bool StateSystem::tryHandleScrollbarPress(const events::HitPointerButton& event, globalcontext::StateContext& state)
{
    return ScrollbarStateHelpers::tryHandlePress(*this, event, state);
}

void StateSystem::handleEntityPress(const events::HitPointerButton& event)
{
    PointerStateHelpers::handleEntityPress(*this, event);
}

void StateSystem::closeDropDownsOnOutsideClick(entt::entity hitEntity)
{
    auto& reg = *m_reg;
    auto& disp = *m_disp;
    auto view = reg.view<components::DropDown>();
    for (auto ddEntity : view)
    {
        auto& dropDown = view.template get<components::DropDown>(ddEntity);
        if (!dropDown.open) continue;

        const auto inSubtree = [&reg, hitEntity](entt::entity ancestor) -> bool
        {
            if (ancestor == entt::null) return false;
            entt::entity cur = hitEntity;
            while (cur != entt::null && reg.valid(cur))
            {
                if (cur == ancestor) return true;
                const auto* hier = reg.try_get<components::Hierarchy>(cur);
                cur = (hier != nullptr) ? hier->parent : entt::null;
            }
            return false;
        };

        if (inSubtree(ddEntity)) continue;
        if (inSubtree(dropDown.popupEntity)) continue;

        disp.trigger<events::DropDownCloseRequested>({ddEntity});
    }
}

/**
 * @brief 处理实体释放事件
 * @param event 点击事件
 * @param state 状态上下文
 */
void StateSystem::handleEntityRelease(const events::HitPointerButton& event, globalcontext::StateContext& state)
{
    PointerStateHelpers::handleEntityRelease(*this, event, state);
}

bool StateSystem::tryHandleSliderPress(const events::HitPointerButton& event)
{
    return SliderStateHelpers::tryHandlePress(*this, event);
}

void StateSystem::handleSliderDrag(const events::HitPointerMove& event)
{
    SliderStateHelpers::handleDrag(*this, event);
}

void StateSystem::stopSliderDrag()
{
    SliderStateHelpers::stopDrag(*this);
}

void StateSystem::updateSliderValueFromPointer(entt::entity entity, const Vec2& mousePos)
{
    SliderStateHelpers::updateValueFromPointer(*this, entity, mousePos);
}

/**
 * @param entity 实体
 * @param mousePos 鼠标位置
 * @param outIsVertical 输出是否为垂直滚动条
 */
StateSystem::ScrollbarHitType
    StateSystem::checkScrollbarHit(entt::entity entity, const Vec2& mousePos, bool& outIsVertical)
{
    return ScrollbarStateHelpers::checkHit(*this, entity, mousePos, outIsVertical);
}

void StateSystem::calculateScrollbarGeometry(entt::entity entity,
                                             bool isVertical,
                                             float& outTrackLen,
                                             float& outThumbSize)
{
    ScrollbarStateHelpers::calculateGeometry(entity, isVertical, outTrackLen, outThumbSize);
}

/**
 * @brief 处理点击轨道跳转
 * @param entity 滚动区域实体
 * @param mousePos 鼠标位置
 * @param isVertical 是否为垂直滚动条
 */
void StateSystem::handleTrackClick(entt::entity entity, const Vec2& mousePos, bool isVertical)
{
    ScrollbarStateHelpers::handleTrackClick(*this, entity, mousePos, isVertical);
}

// =============================================================================
// StateSystem — frame end & widget destroy
// =============================================================================

/**
 */
void StateSystem::onEndFrame()
{
    EndFrameStateHelpers::flush(*this);
}

// =============================================================================
// 工具方法
// =============================================================================

void StateSystem::destroyWidget(entt::entity entity)
{
    auto& reg = *m_reg;
    auto& disp = *m_disp;
    if (!reg.valid(entity)) return;

    // 使用迭代代替递归，避免深层嵌套导致栈溢出
    std::vector<entt::entity> stack;
    std::vector<entt::entity> toDestroy;

    stack.push_back(entity);

    // 1. 深度优先遍历收集所有需要销毁的实体
    while (!stack.empty())
    {
        entt::entity current = stack.back();
        stack.pop_back();

        if (!reg.valid(current)) continue;

        toDestroy.push_back(current);

        // 将子节点加入栈（逆序添加以保持遍历顺序）
        if (auto* hierarchy = reg.try_get<components::Hierarchy>(current))
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
        if (!reg.valid(entity)) continue;

        // 如果是窗口，查找并销毁关联的 SDL_Window 资源
        if (auto* windowComp = reg.try_get<components::Window>(entity))
        {
            RuntimeFacade::current().windowLookup().invalidateId(windowComp->windowID);

            // 通过 WindowID 找回 SDL_Window 指针
            if (SDL_Window* sdlWindow = SDL_GetWindowFromID(windowComp->windowID))
            {
                // 通知 RenderSystem 解绑上下文（尽管 RenderSystem 可能目前未执行动作，保留逻辑完整性）
                disp.trigger<events::WindowGraphicsContextUnsetEvent>(events::WindowGraphicsContextUnsetEvent{entity});

                SDL_DestroyWindow(sdlWindow);
            }
        }

        reg.destroy(entity);
    }
}

} // namespace ui::systems
