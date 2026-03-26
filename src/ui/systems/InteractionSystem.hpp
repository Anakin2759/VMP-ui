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
#include <algorithm>
#include <string>
#include <SDL3/SDL.h>
#include "common/Events.hpp"
#include "singleton/Registry.hpp"
#include "singleton/Dispatcher.hpp"
#include "common/Components.hpp"
#include "common/Tags.hpp"
#include "api/Utils.hpp"
#include "core/TextUtils.hpp"
#include "interface/Isystem.hpp"
#include "common/Types.hpp"
#include "../common/WindowSync.hpp"

namespace ui::systems
{

class InteractionSystem : public ui::interface::EnableRegister<InteractionSystem>
{
public:
    void registerHandlersImpl() { registerPlatformEventWatch(); }

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
        // 每帧处理一次键盘长按重复逻辑
        processKeyRepeat();
    }

    static void processKeyRepeat()
    {
        if (heldKey == SDLK_UNKNOWN) return;

        uint64_t now = SDL_GetTicks();

        // 检查是否达到初始延迟
        if (now < keyPressTime + KEY_REPEAT_DELAY) return;

        // 检查是否达到重复间隔
        if (now < lastRepeatTime + KEY_REPEAT_INTERVAL) return;

        // 触发重复输入
        lastRepeatTime = now;
        handleKeyDown(heldKey);
        triggerImmediateRenderRefresh();
    }

    static void registerPlatformEventWatch()
    {
        // 平台事件监听属于特例通道。
        // 它用于处理 Windows 等平台在窗口 resize / expose 时阻塞主循环的情况。
        // 常规帧刷新仍然由 TaskChain::RenderTask 驱动。
        SDL_AddEventWatch(
            [](void*, SDL_Event* event) -> bool
            {
                handlePlatformWindowEvent(*event);
                return true;
            },
            nullptr);
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
                handleTextInput(event.text.text);
                break;

            case SDL_EVENT_KEY_DOWN:
                handleKeyDownEvent(event.key);
                break;

            case SDL_EVENT_KEY_UP:
                handleKeyUpEvent(event.key);
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
        const auto targetWindow = findWindowEntityById(windowId);
        if (!Registry::Valid(targetWindow)) return;

        Dispatcher::Enqueue<ui::events::CloseWindow>(ui::events::CloseWindow{targetWindow});
    }

    static void handleKeyDownEvent(const SDL_KeyboardEvent& keyEvent)
    {
        if (keyEvent.repeat) return;

        beginKeyRepeat(keyEvent.key);
        handleKeyDown(keyEvent.key);
    }

    static void handleKeyUpEvent(const SDL_KeyboardEvent& keyEvent)
    {
        if (keyEvent.key != heldKey) return;
        resetKeyRepeat();
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

    static void beginKeyRepeat(SDL_Keycode key)
    {
        heldKey = key;
        keyPressTime = SDL_GetTicks();
        lastRepeatTime = keyPressTime;
    }

    static void resetKeyRepeat()
    {
        heldKey = SDLK_UNKNOWN;
        keyPressTime = 0;
        lastRepeatTime = 0;
    }

    static entt::entity findWindowEntityById(uint32_t windowId)
    {
        auto view = Registry::View<components::Window>();
        for (auto entity : view)
        {
            if (view.get<components::Window>(entity).windowID == windowId)
            {
                return entity;
            }
        }
        return entt::null;
    }

    static void triggerImmediateRenderRefresh()
    {
        Dispatcher::Trigger<ui::events::UpdateRendering>(ui::events::UpdateRendering{});
    }

    static void triggerImmediateLayoutAndRenderRefresh()
    {
        Dispatcher::Trigger<ui::events::UpdateLayout>(ui::events::UpdateLayout{});
        triggerImmediateRenderRefresh();
    }

    static bool shouldSyncWindowPropertiesImmediately(Uint32 eventType)
    {
        return eventType == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED || eventType == SDL_EVENT_WINDOW_MOVED ||
               eventType == SDL_EVENT_WINDOW_EXPOSED || eventType == SDL_EVENT_WINDOW_SHOWN ||
               eventType == SDL_EVENT_WINDOW_HIDDEN;
    }

    static bool requiresImmediatePlatformRefresh(Uint32 eventType)
    {
        return eventType == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED || eventType == SDL_EVENT_WINDOW_EXPOSED;
    }

    static void dispatchImmediateWindowEvent(const SDL_WindowEvent& windowEvent)
    {
        if (windowEvent.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED)
        {
            Dispatcher::Trigger<ui::events::WindowPixelSizeChanged>(
                ui::events::WindowPixelSizeChanged{windowEvent.windowID, windowEvent.data1, windowEvent.data2});
            return;
        }

        if (windowEvent.type == SDL_EVENT_WINDOW_MOVED)
        {
            Dispatcher::Trigger<ui::events::WindowMoved>(
                ui::events::WindowMoved{windowEvent.windowID, windowEvent.data1, windowEvent.data2});
        }
    }

    static void handlePlatformWindowEvent(const SDL_Event& event)
    {
        const auto eventType = event.type;
        dispatchImmediateWindowEvent(event.window);

        if (shouldSyncWindowPropertiesImmediately(eventType))
        {
            syncWindowPropertiesImmediately(event.window.windowID);
        }

        if (requiresImmediatePlatformRefresh(eventType))
        {
            triggerImmediateLayoutAndRenderRefresh();
        }
    }

    static void syncWindowPropertiesImmediately(uint32_t windowId)
    {
        SDL_Window* sdlWindow = SDL_GetWindowFromID(windowId);
        if (sdlWindow == nullptr) return;

        const auto windowEntity = findWindowEntityById(windowId);
        if (!Registry::Valid(windowEntity)) return;

        auto& windowComp = Registry::Get<components::Window>(windowEntity);
        window_sync::SyncWindowProperties(windowEntity, windowComp, sdlWindow);
    }

    // =====================================================================
    // Text Editing Helper Functions
    // =====================================================================
    // These functions implement basic text editing operations for TextEdit
    // components including cursor movement, text selection, and clipboard
    // integration.
    //
    // Mouse selection is not yet implemented - would require:
    // 1. Converting mouse coordinates to text buffer positions
    // 2. Tracking mouse down/drag/up states
    // 3. Coordinating with TextRenderer for glyph positioning
    // 4. Handling multi-line text layout
    // =====================================================================

    static void clearSelection(components::TextEdit& edit)
    {
        edit.hasSelection = false;
        edit.selectionStart = 0;
        edit.selectionEnd = 0;
    }

    static void deleteSelection(components::TextEdit& edit)
    {
        if (!edit.hasSelection) return;

        size_t start = edit.selectionStart;
        size_t end = edit.selectionEnd;

        edit.buffer.erase(start, end - start);
        edit.cursorPosition = start;
        clearSelection(edit);
    }
    /**
     * @brief 选择全部文本
     * @param edit 文本编辑组件
     */
    static void selectAll(components::TextEdit& edit)
    {
        if (edit.buffer.empty()) return;

        edit.hasSelection = true;
        edit.selectionStart = 0;
        edit.selectionEnd = edit.buffer.size();
        edit.cursorPosition = edit.selectionEnd;
    }
    /**
     * @brief 将光标向左移动
     * @param edit {comment}
     * @param extend {comment}
     */
    static void moveCursorLeft(components::TextEdit& edit, bool extend)
    {
        if (edit.cursorPosition > 0)
        {
            size_t newPos = utils::PrevCharPos(edit.buffer, edit.cursorPosition);
            if (extend)
            {
                if (!edit.hasSelection)
                {
                    edit.hasSelection = true;
                    edit.selectionStart = edit.cursorPosition;
                    edit.selectionEnd = edit.cursorPosition;
                }
                edit.cursorPosition = newPos;
                if (edit.cursorPosition < edit.selectionStart)
                {
                    edit.selectionStart = edit.cursorPosition;
                }
                else
                {
                    edit.selectionEnd = edit.cursorPosition;
                }
            }
            else
            {
                edit.cursorPosition = newPos;
            }
        }
    }

    static void moveCursorRight(components::TextEdit& edit, bool extend)
    {
        if (edit.cursorPosition < edit.buffer.size())
        {
            size_t newPos = utils::NextCharPos(edit.buffer, edit.cursorPosition);
            if (extend)
            {
                if (!edit.hasSelection)
                {
                    edit.hasSelection = true;
                    edit.selectionStart = edit.cursorPosition;
                    edit.selectionEnd = edit.cursorPosition;
                }
                edit.cursorPosition = newPos;
                if (edit.cursorPosition > edit.selectionEnd)
                {
                    edit.selectionEnd = edit.cursorPosition;
                }
                else
                {
                    edit.selectionStart = edit.cursorPosition;
                }
            }
            else
            {
                edit.cursorPosition = newPos;
            }
        }
    }

    static void moveCursorToLineStart(components::TextEdit& edit, bool extend)
    {
        // Find the start of current line
        size_t lineStart = edit.cursorPosition;
        while (lineStart > 0 && edit.buffer[lineStart - 1] != '\n')
        {
            lineStart--;
        }

        if (extend)
        {
            if (!edit.hasSelection)
            {
                edit.hasSelection = true;
                edit.selectionEnd = edit.cursorPosition;
            }
            edit.selectionStart = lineStart;
        }

        edit.cursorPosition = lineStart;
    }

    static void moveCursorToLineEnd(components::TextEdit& edit, bool extend)
    {
        // Find the end of current line
        size_t lineEnd = edit.cursorPosition;
        while (lineEnd < edit.buffer.size() && edit.buffer[lineEnd] != '\n')
        {
            lineEnd++;
        }

        if (extend)
        {
            if (!edit.hasSelection)
            {
                edit.hasSelection = true;
                edit.selectionStart = edit.cursorPosition;
            }
            edit.selectionEnd = lineEnd;
        }

        edit.cursorPosition = lineEnd;
    }

    static void copyToClipboard(const components::TextEdit& edit)
    {
        if (!edit.hasSelection) return;

        std::string selectedText = edit.buffer.substr(edit.selectionStart, edit.selectionEnd - edit.selectionStart);
        SDL_SetClipboardText(selectedText.c_str());
    }

    static void pasteFromClipboard(components::TextEdit& edit)
    {
        if (!SDL_HasClipboardText()) return;

        char* clipboardText = SDL_GetClipboardText();
        if (clipboardText == nullptr) return;

        std::string text(clipboardText);
        SDL_free(clipboardText);

        if (text.empty()) return;

        // Filter newlines for single-line input
        const auto modeVal = static_cast<uint8_t>(edit.inputMode);
        const auto multiFlag = static_cast<uint8_t>(policies::TextFlag::Multiline);
        if ((modeVal & multiFlag) == 0)
        {
            text.erase(std::remove(text.begin(), text.end(), '\n'), text.end());
            text.erase(std::remove(text.begin(), text.end(), '\r'), text.end());
        }

        // Delete selection if exists
        if (edit.hasSelection)
        {
            deleteSelection(edit);
        }

        // Check max length
        if (edit.buffer.size() + text.size() > edit.maxLength)
        {
            size_t available = edit.maxLength - edit.buffer.size();
            if (available == 0) return;
            text = text.substr(0, available);
        }

        // Insert at cursor position
        edit.buffer.insert(edit.cursorPosition, text);
        edit.cursorPosition += text.size();
    }

    static bool isMultilineInput(const components::TextEdit& edit)
    {
        const auto modeValue = static_cast<uint8_t>(edit.inputMode);
        const auto multilineFlag = static_cast<uint8_t>(policies::TextFlag::Multiline);
        return (modeValue & multilineFlag) != 0;
    }

    static void syncTextContent(components::TextEdit& edit, components::Text& textComp)
    {
        textComp.content = edit.buffer;
    }

    static void notifyTextChanged(components::TextEdit& edit)
    {
        if (edit.onTextChanged)
        {
            edit.onTextChanged(edit.buffer);
        }
    }

    static void applyContentChange(entt::entity entity, components::TextEdit& edit, components::Text& textComp)
    {
        syncTextContent(edit, textComp);
        ui::utils::MarkLayoutAndVisualChanged(entity);
        notifyTextChanged(edit);
    }

    static bool handleReadOnlyShortcut(entt::entity entity, components::TextEdit& edit, SDL_Keycode key, bool ctrl)
    {
        if (ctrl && key == SDLK_C)
        {
            if (edit.hasSelection)
            {
                copyToClipboard(edit);
            }
            return true;
        }

        if (ctrl && key == SDLK_A)
        {
            selectAll(edit);
            ui::utils::MarkVisualChanged(entity);
            return true;
        }

        return false;
    }

    static bool handleClipboardShortcut(
        entt::entity entity, components::TextEdit& edit, components::Text& textComp, SDL_Keycode key, bool ctrl)
    {
        if (ctrl && key == SDLK_X)
        {
            if (!edit.hasSelection) return true;

            copyToClipboard(edit);
            deleteSelection(edit);
            applyContentChange(entity, edit, textComp);
            return true;
        }

        if (ctrl && key == SDLK_V)
        {
            pasteFromClipboard(edit);
            applyContentChange(entity, edit, textComp);
            return true;
        }

        return false;
    }

    static bool
        handleDeletionKey(entt::entity entity, components::TextEdit& edit, components::Text& textComp, SDL_Keycode key)
    {
        if (key == SDLK_BACKSPACE)
        {
            if (edit.hasSelection)
            {
                deleteSelection(edit);
            }
            else if (edit.cursorPosition > 0)
            {
                const size_t prevPosition = utils::PrevCharPos(edit.buffer, edit.cursorPosition);
                edit.buffer.erase(prevPosition, edit.cursorPosition - prevPosition);
                edit.cursorPosition = prevPosition;
            }

            applyContentChange(entity, edit, textComp);
            return true;
        }

        if (key == SDLK_DELETE)
        {
            if (edit.hasSelection)
            {
                deleteSelection(edit);
            }
            else if (edit.cursorPosition < edit.buffer.size())
            {
                const size_t nextPosition = utils::NextCharPos(edit.buffer, edit.cursorPosition);
                edit.buffer.erase(edit.cursorPosition, nextPosition - edit.cursorPosition);
            }

            applyContentChange(entity, edit, textComp);
            return true;
        }

        return false;
    }
    /**
     * @brief 处理水平导航键（左右箭头）
     * @param entity {comment}
     * @param edit {comment}
     * @param key {comment}
     * @param shift {comment}
     * @return true {comment}
     * @return false {comment}
     */
    static bool handleHorizontalNavigation(entt::entity entity, components::TextEdit& edit, SDL_Keycode key, bool shift)
    {
        if (key == SDLK_LEFT)
        {
            if (shift)
            {
                moveCursorLeft(edit, true);
            }
            else if (edit.hasSelection)
            {
                edit.cursorPosition = edit.selectionStart;
                clearSelection(edit);
            }
            else
            {
                moveCursorLeft(edit, false);
            }

            ui::utils::MarkVisualChanged(entity);
            return true;
        }

        if (key == SDLK_RIGHT)
        {
            if (shift)
            {
                moveCursorRight(edit, true);
            }
            else if (edit.hasSelection)
            {
                edit.cursorPosition = edit.selectionEnd;
                clearSelection(edit);
            }
            else
            {
                moveCursorRight(edit, false);
            }

            ui::utils::MarkVisualChanged(entity);
            return true;
        }

        return false;
    }

    static bool handleBoundaryNavigation(entt::entity entity, components::TextEdit& edit, SDL_Keycode key, bool shift)
    {
        if (key == SDLK_HOME)
        {
            if (shift)
            {
                moveCursorToLineStart(edit, true);
            }
            else
            {
                clearSelection(edit);
                moveCursorToLineStart(edit, false);
            }

            ui::utils::MarkVisualChanged(entity);
            return true;
        }

        if (key == SDLK_END)
        {
            if (shift)
            {
                moveCursorToLineEnd(edit, true);
            }
            else
            {
                clearSelection(edit);
                moveCursorToLineEnd(edit, false);
            }

            ui::utils::MarkVisualChanged(entity);
            return true;
        }

        return false;
    }

    static bool handleVerticalNavigation(entt::entity entity, components::TextEdit& edit, SDL_Keycode key, bool shift)
    {
        if (!isMultilineInput(edit)) return false;

        if (key == SDLK_UP)
        {
            if (shift)
            {
                moveCursorToLineStart(edit, true);
            }
            else
            {
                clearSelection(edit);
                moveCursorToLineStart(edit, false);
            }

            ui::utils::MarkVisualChanged(entity);
            return true;
        }

        if (key == SDLK_DOWN)
        {
            if (shift)
            {
                moveCursorToLineEnd(edit, true);
            }
            else
            {
                clearSelection(edit);
                moveCursorToLineEnd(edit, false);
            }

            ui::utils::MarkVisualChanged(entity);
            return true;
        }

        return false;
    }

    static bool handleReturnKey(entt::entity entity, components::TextEdit& edit, components::Text& textComp)
    {
        if (isMultilineInput(edit))
        {
            if (edit.buffer.size() + 1 > edit.maxLength) return true;

            if (edit.hasSelection)
            {
                deleteSelection(edit);
            }

            edit.buffer.insert(edit.cursorPosition, "\n");
            edit.cursorPosition++;
            applyContentChange(entity, edit, textComp);
            return true;
        }

        if (edit.onSubmit)
        {
            edit.onSubmit();
        }

        if (!edit.hasSelection) return true;

        deleteSelection(edit);
        applyContentChange(entity, edit, textComp);
        return true;
    }

    static void handleTextInput(const char* text)
    {
        auto view = Registry::View<components::FocusedTag, components::TextEdit, components::Text>();
        for (auto entity : view)
        {
            if (!Registry::AnyOf<components::TextEditTag>(entity)) continue;

            auto& edit = view.get<components::TextEdit>(entity);
            if (policies::HasFlag(edit.inputMode, policies::TextFlag::ReadOnly)) continue;

            std::string input = text != nullptr ? std::string(text) : std::string();
            if (input.empty()) continue;

            // 单行输入框过滤换行
            if (!isMultilineInput(edit))
            {
                input.erase(std::remove(input.begin(), input.end(), '\n'), input.end());
                input.erase(std::remove(input.begin(), input.end(), '\r'), input.end());
            }

            // 如果有选中内容，先删除选中内容
            if (edit.hasSelection)
            {
                deleteSelection(edit);
            }

            // 限制最大长度（按字节计）
            if (edit.buffer.size() + input.size() > edit.maxLength)
            {
                size_t available = edit.maxLength - edit.buffer.size();
                if (available == 0) continue;
                input = input.substr(0, available);
            }

            // 在光标位置插入文本
            edit.buffer.insert(edit.cursorPosition, input);
            edit.cursorPosition += input.size();

            // 同步渲染文本
            auto& textComp = view.get<components::Text>(entity);
            applyContentChange(entity, edit, textComp);
        }
    }

    static void handleKeyDown(SDL_Keycode key)
    {
        auto view = Registry::View<components::FocusedTag, components::TextEdit, components::Text>();
        for (auto entity : view)
        {
            if (!Registry::AnyOf<components::TextEditTag>(entity)) continue;

            auto& edit = view.get<components::TextEdit>(entity);

            // Get modifier keys
            SDL_Keymod modState = SDL_GetModState();
            bool ctrl = (modState & SDL_KMOD_CTRL) != 0;
            bool shift = (modState & SDL_KMOD_SHIFT) != 0;

            if (handleReadOnlyShortcut(entity, edit, key, ctrl)) continue;

            // ---- 以下操作需要可编辑 ----
            if (policies::HasFlag(edit.inputMode, policies::TextFlag::ReadOnly)) continue;

            auto& textComp = view.get<components::Text>(entity);
            if (handleClipboardShortcut(entity, edit, textComp, key, ctrl)) continue;
            if (handleDeletionKey(entity, edit, textComp, key)) continue;
            if (handleHorizontalNavigation(entity, edit, key, shift)) continue;
            if (handleBoundaryNavigation(entity, edit, key, shift)) continue;
            if (handleVerticalNavigation(entity, edit, key, shift)) continue;
            if (key == SDLK_RETURN)
            {
                handleReturnKey(entity, edit, textComp);
            }
            // 注意：普通字符输入由 SDL_EVENT_TEXT_INPUT 处理
        }
    }

    // 键盘长按状态
    inline static SDL_Keycode heldKey = SDLK_UNKNOWN;   // 当前按下的按键
    inline static uint64_t keyPressTime = 0;            // 按键按下时间（毫秒）
    inline static uint64_t lastRepeatTime = 0;          // 上次重复输入时间
    static constexpr uint64_t KEY_REPEAT_DELAY = 500;   // 长按触发延迟（毫秒）
    static constexpr uint64_t KEY_REPEAT_INTERVAL = 50; // 重复输入间隔（毫秒）
};

} // namespace ui::systems