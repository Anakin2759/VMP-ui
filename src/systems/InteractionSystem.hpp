/**
 * ************************************************************************
 *
 * @file InteractionSystem.h
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2026-01-28 (Refactored)
 * @version 0.3
 * @brief 交互处理系统 - SDL事件捕获与分发层
 *
 * 职责：
 * 1. 捕获SDL原始事件（鼠标、键盘、滚轮、窗口）
 * 2. 将事件转换为内部事件并分发到事件总线
 * 3. 调用HitTestSystem进行碰撞检测
 * 4. 触发StateSystem和ActionSystem处理后续逻辑
 *
 * 刷新路径说明：
 * 1. 常规帧刷新由 TaskChain::RenderTask 统一触发 UpdateLayout / UpdateRendering / EndFrame
 * 2. 本系统只在少数需要立即反馈的场景直接刷新：
 *    - 键盘长按重复输入后的即时重绘
 *    - 平台窗口消息阻塞时的即时布局与渲染补救
 *
 * 事件链条：
 * SDL事件捕获（InteractionSystem）
 *   ├─→ 鼠标/滚轮事件 → HitTestSystem碰撞检测 → 触发Hover/Press/Release事件
 *   │                                              ↓
 *   │                                   StateSystem状态管理（Hover/Active/Focus）
 *   │                                              ↓
 *   │                                   ActionSystem执行回调
 *   │
 *   ├─→ 键盘事件 → 文本输入/按键处理 → 更新TextEdit组件
 *   │
 *   └─→ 窗口事件（DetailExposed监听）→ StateSystem窗口同步 → RenderSystem渲染更新

 * 键盘长按处理：
    * - 记录按下时间戳
    * - 定时检查是否达到重复输入条件
    * - 触发重复按键事件
    * - 500ms初始延迟，之后每33s重复一次

 *鼠标长按和拖动处理：
    * - 在HitTestSystem中处理
    * - 记录按下位置和时间
    * - 超过阈值触发拖动开始事件
    * - 鼠标移动时持续触发拖动事件
    * - 鼠标释放时触发拖动结束事件

    * - 400ms 不超过4个像素算长按 超过是拖动
    * - 支持拖动的控件优先拖动处理
    * - 不支持点击的控件按照拖动处理
    * - 不支持拖动的控件忽略拖动事件

 *
 * ************************************************************************
 */
#pragma once
#include <entt/entt.hpp>
#include <string>
#include <SDL3/SDL.h>
#include "common/Events.hpp"
#include "core/RuntimeFacade.hpp"
#include "singleton/Registry.hpp"
#include "singleton/Dispatcher.hpp"
#include "interface/Isystem.hpp"
#include "common/Types.hpp"

namespace ui::systems
{

class InteractionSystem : public ui::interface::EnableRegister<InteractionSystem>
{
public:
    void registerHandlersImpl() {}

    void unregisterHandlersImpl() {}
    /**
     * @brief 处理 SDL每tick事件
     *
     * - 负责 SDL_PollEvent 事件
     * - 识别 Quit / Window Resized，并通过回调交由上层处理
     * - 直接从 SDL 事件中追踪鼠标状态
     */
    static void pollSdlEvents()
    {
        SDL_Event event{};

        while (SDL_PollEvent(&event))
        {
            dispatchPolledEvent(event);
        }
    }

private:
    /**
     * @brief 分发 SDL 轮询事件到相应处理函数
     * @param event SDL 事件对象
     */
    static void dispatchPolledEvent(const SDL_Event& event)
    {
        switch (event.type)
        {
            case SDL_EVENT_QUIT:
                Dispatcher::Enqueue<ui::events::QuitRequested>();
                break;

            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                enqueueCloseWindowRequest(event.window.windowID);
                break;

            case SDL_EVENT_MOUSE_MOTION:
                enqueueRawPointerMove(event.motion);
                break;

            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                enqueueRawPointerButton(event.button, true);
                break;

            case SDL_EVENT_MOUSE_BUTTON_UP:
                enqueueRawPointerButton(event.button, false);
                break;

            case SDL_EVENT_TEXT_INPUT:
                dispatchRawTextInput(event.text.text);
                break;

            case SDL_EVENT_KEY_DOWN:
                dispatchRawKeyInput(event.key, true);
                break;

            case SDL_EVENT_KEY_UP:
                dispatchRawKeyInput(event.key, false);
                break;

            case SDL_EVENT_MOUSE_WHEEL:
                enqueueRawPointerWheel(event.wheel);
                break;

            default:
                break;
        }
    }

    static void enqueueCloseWindowRequest(uint32_t windowId)
    {
        const auto targetWindow = RuntimeFacade::current().windowLookup().findById(windowId);
        if (!Registry::Valid(targetWindow)) return;

        Dispatcher::Enqueue<ui::events::CloseWindow>(ui::events::CloseWindow{targetWindow});
    }

    static void dispatchRawTextInput(const char* text)
    {
        std::string input = text != nullptr ? std::string(text) : std::string();
        if (input.empty()) return;

        Dispatcher::Trigger<ui::events::RawTextInput>(ui::events::RawTextInput{std::move(input)});
    }

    static void enqueueRawPointerMove(const SDL_MouseMotionEvent& motionEvent)
    {
        const auto pointerX = motionEvent.x;
        const auto pointerY = motionEvent.y;
        const auto deltaX = motionEvent.xrel;
        const auto deltaY = motionEvent.yrel;
        Dispatcher::Enqueue<ui::events::RawPointerMove>(
            ui::events::RawPointerMove{Vec2(pointerX, pointerY), Vec2(deltaX, deltaY), motionEvent.windowID});
    }

    static void enqueueRawPointerButton(const SDL_MouseButtonEvent& buttonEvent, bool pressed)
    {
        const auto pointerX = buttonEvent.x;
        const auto pointerY = buttonEvent.y;
        Dispatcher::Enqueue<ui::events::RawPointerButton>(
            ui::events::RawPointerButton{Vec2(pointerX, pointerY), buttonEvent.windowID, pressed, buttonEvent.button});
    }

    static void enqueueRawPointerWheel(const SDL_MouseWheelEvent& wheelEvent)
    {
        float pointerX = 0.0F;
        float pointerY = 0.0F;
        SDL_GetMouseState(&pointerX, &pointerY);

        Dispatcher::Enqueue<ui::events::RawPointerWheel>(ui::events::RawPointerWheel{
            Vec2(pointerX, pointerY), Vec2(wheelEvent.x, wheelEvent.y), wheelEvent.windowID});
    }

    static void dispatchRawKeyInput(const SDL_KeyboardEvent& keyEvent, bool pressed)
    {
        Dispatcher::Trigger<ui::events::RawKeyInput>(ui::events::RawKeyInput{
            static_cast<int32_t>(keyEvent.key), pressed, keyEvent.repeat, static_cast<uint16_t>(keyEvent.mod)});
    }
};

} // namespace ui::systems