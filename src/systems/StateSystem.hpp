/**
 * ************************************************************************
 *
 * @file StateSystem.h
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2025-12-11 (Updated)
 * @version 0.2
 *
 * 2. 同步窗口状态到ECS组件（如Resizable、Frameless等）
 *
 *
 * ************************************************************************
 * @copyright Copyright (c) 2025 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */

#pragma once
#include <entt/entt.hpp>
#include <unordered_set>
#include <SDL3/SDL.h>


#include "../interface/Isystem.hpp"
#include "../common/Events.hpp"

#include "../common/GlobalContext.hpp"


namespace ui::systems
{

// GlobalState now lives in ui::globalcontext::GlobalState (see GlobalContext.hpp)

class StateSystem : public ui::interface::EnableRegister<StateSystem>
{
public:
    enum class ScrollbarHitType : std::uint8_t
    {
        NONE,
        THUMB, // 命中滑块
        TRACK  // 命中轨道
    };

    void registerHandlersImpl();
    void unregisterHandlersImpl();

private:
    struct SliderStateHelpers
    {
        static bool tryHandlePress(StateSystem& system, const events::HitPointerButton& event);
        static void handleDrag(StateSystem& system, const events::HitPointerMove& event);
        static void stopDrag(StateSystem& system);
        static void updateValueFromPointer(entt::entity entity, const Vec2& mousePos);
    };

    struct ScrollbarStateHelpers
    {
        static entt::entity findScrollTargetFromHit(entt::entity hitEntity);
        static entt::entity findScrollTargetAtPosition(const Vec2& pointerPosition);
        static void applyScrollWheelDelta(entt::entity target, const Vec2& scrollDelta);
        static void updateHoverStates(
            const events::HitPointerMove& event,
            const globalcontext::StateContext& state);
        static void handleDrag(const events::HitPointerMove& event, globalcontext::StateContext& state);
        static bool tryHandlePress(
            const events::HitPointerButton& event,
            globalcontext::StateContext& state);
        static ScrollbarHitType checkHit(entt::entity entity, const Vec2& mousePos, bool& outIsVertical);
        static void calculateGeometry(entt::entity entity, bool isVertical, float& outTrackLen, float& outThumbSize);
        static void handleTrackClick(entt::entity entity, const Vec2& mousePos, bool isVertical);
    };

    struct PointerStateHelpers
    {
        static bool isWritableTextEdit(entt::entity entity);
        static bool shouldEmitPressForEntity(entt::entity entity);

        static void queueHoveredEntity(
            StateSystem& system,
            globalcontext::StateContext& state,
            entt::entity entity);

        static void queueHoverClear(StateSystem& system, globalcontext::StateContext& state, entt::entity entity);

        static void queueActiveEntity(StateSystem& system, globalcontext::StateContext& state, entt::entity entity);

        static void queueActiveClear(StateSystem& system, globalcontext::StateContext& state, entt::entity entity);

        static void handleHoverUpdate(
            const events::HitPointerMove& event,
            const globalcontext::StateContext& state);

        static void setFocus(entt::entity entity, SDL_Window* sdlWindow = nullptr);

        static void clearFocus(SDL_Window* sdlWindow = nullptr);

        static void handleEntityPress(const events::HitPointerButton& event);

        static void handleEntityRelease(
            StateSystem& system,
            const events::HitPointerButton& event,
            globalcontext::StateContext& state);

        static void tryEmitTableCellClicked(entt::entity hitEntity, const Vec2& pointerPosition);
    };

    struct WindowStateHelpers
    {
        static void handleClose(const events::CloseWindow& event);
        static void handlePixelSizeChanged(const events::WindowPixelSizeChanged& event);
        static void handleMoved(const events::WindowMoved& event);
    };

    struct EndFrameStateHelpers
    {
        static void flush(StateSystem& system);
    };

    static bool isWritableTextEdit(entt::entity entity);
    static bool shouldEmitPressForEntity(entt::entity entity);
    static entt::entity findScrollTargetFromHit(entt::entity hitEntity);
    static entt::entity findScrollTargetAtPosition(const Vec2& pointerPosition);
    static void applyScrollWheelDelta(entt::entity target, const Vec2& scrollDelta);
    void queueHoveredEntity(globalcontext::StateContext& state, entt::entity entity);
    void queueHoverClear(globalcontext::StateContext& state, entt::entity entity);
    void queueActiveEntity(globalcontext::StateContext& state, entt::entity entity);
    void queueActiveClear(globalcontext::StateContext& state, entt::entity entity);

    // ===================================================================
    // PointerState: Hover / Active 标签延迟状态机
    // ===================================================================

    /**
     */
    void onHoverEvent(const events::HoverEvent& event);

    /**
     * @brief 处理取消悬停事件（延迟应用）
     */
    void onUnhoverEvent(const events::UnhoverEvent& event);

    /**
     */
    void onMousePressEvent(const events::MousePressEvent& event);

    /**
     */
    void onMouseReleaseEvent(const events::MouseReleaseEvent& event);

    // ===================================================================
    // PointerState: 命中后的指针输入状态机
    // ===================================================================

    void onHitPointerMove(const events::HitPointerMove& event);
    void onHitPointerButton(const events::HitPointerButton& event);
    void onHitPointerWheel(const events::HitPointerWheel& event);

    /**
     */
    static void setFocus(entt::entity entity, SDL_Window* sdlWindow = nullptr);

    /**
     * @brief 清除当前焦点
     */
    static void clearFocus(SDL_Window* sdlWindow = nullptr);

    // ===================================================================
    // ===================================================================

    void onCloseWindow(const events::CloseWindow& event);

    /**
     * @brief 处理窗口尺寸变化事件
     */
    void onWindowPixelSizeChanged(const events::WindowPixelSizeChanged& event);

    /**
     * @brief 处理窗口位置变化事件
     */
    void onWindowMoved(const events::WindowMoved& event);

    // ===================================================================
    // ScrollInteractionState: ScrollArea / Slider 交互
    // ===================================================================

    /**
     */
    void updateScrollbarHoverStates(const events::HitPointerMove& event);

    void handleScrollbarDrag(const events::HitPointerMove& event, globalcontext::StateContext& state);

    void handleHoverUpdate(const events::HitPointerMove& event, const globalcontext::StateContext& state);

    /**
     * @param event 点击事件
     * @param state 状态上下文
     * @return true 如果处理了滚动条按下事件
     * @return false 如果未处理滚动条按下事件
     */
    bool tryHandleScrollbarPress(const events::HitPointerButton& event, globalcontext::StateContext& state);

    void handleEntityPress(const events::HitPointerButton& event);

    static void closeDropDownsOnOutsideClick(entt::entity hitEntity);
    /**
     * @brief 处理实体释放事件
     * @param event 点击事件
     * @param state 状态上下文
     */
    void handleEntityRelease(const events::HitPointerButton& event, globalcontext::StateContext& state);

    bool tryHandleSliderPress(const events::HitPointerButton& event);

    void handleSliderDrag(const events::HitPointerMove& event);

    void stopSliderDrag();

    void updateSliderValueFromPointer(entt::entity entity, const Vec2& mousePos);

    /**
     * @param entity 实体
     * @param mousePos 鼠标位置
     * @param outIsVertical 输出是否为垂直滚动条
     */
    ScrollbarHitType checkScrollbarHit(entt::entity entity, const Vec2& mousePos, bool& outIsVertical);

    void calculateScrollbarGeometry(entt::entity entity, bool isVertical, float& outTrackLen, float& outThumbSize);

    /**
     * @brief 处理点击轨道跳转
     * @param entity 滚动区域实体
     * @param mousePos 鼠标位置
     * @param isVertical 是否为垂直滚动条
     */
    void handleTrackClick(entt::entity entity, const Vec2& mousePos, bool isVertical);

    // ===================================================================
    // PointerState: 帧尾标签合并
    // ===================================================================

    /**
     * @note 使用 unordered_set 自动去重，避免同一实体多次操作
     */
    std::unordered_set<entt::entity> m_pendingHoverAdd;
    std::unordered_set<entt::entity> m_pendingHoverRemove;
    std::unordered_set<entt::entity> m_pendingActiveAdd;
    std::unordered_set<entt::entity> m_pendingActiveRemove;
    bool m_isDraggingSlider = false;
    entt::entity m_dragSliderEntity = entt::null;

    /**
     */
    void onEndFrame();

    // ===================================================================
    // 工具方法
    // ===================================================================

    static void destroyWidget(entt::entity entity);

};


} // namespace ui::systems
