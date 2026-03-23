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
#include "HitTestSystem.hpp"
#include "StateSystem.hpp"

namespace ui::systems
{

class InteractionSystem : public ui::interface::EnableRegister<InteractionSystem>
{
public:
    void registerHandlersImpl() { DetailExposed(); }

    void unregisterHandlersImpl() {}
    /**
     * @brief 处理 SDL每tick事件
     *
     * - 负责 SDL_PollEvent 事件
     * - 识别 Quit / Window Resized，并通过回调交由上层处理
     * - 直接从 SDL 事件中追踪鼠标状态
     */
    static void SDLEvent()
    {
        SDL_Event event{};

        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
                case SDL_EVENT_QUIT:
                    Dispatcher::Enqueue<ui::events::QuitRequested>();
                    break;

                case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                {
                    // 查找对应的 Window 实体
                    entt::entity targetWindow = entt::null;
                    auto view = Registry::View<components::Window>();
                    for (auto entity : view)
                    {
                        if (view.get<components::Window>(entity).windowID == event.window.windowID)
                        {
                            targetWindow = entity;
                            break;
                        }
                    }

                    if (Registry::Valid(targetWindow))
                    {
                        Dispatcher::Enqueue<ui::events::CloseWindow>(ui::events::CloseWindow{targetWindow});
                    }
                    break;
                }

                case SDL_EVENT_MOUSE_MOTION:
                {
                    // 不在此处进行命中/滚动处理：只记录并转发原始指针移动数据
                    {
                        float mx = static_cast<float>(event.motion.x);
                        float my = static_cast<float>(event.motion.y);
                        float dx = static_cast<float>(event.motion.xrel);
                        float dy = static_cast<float>(event.motion.yrel);
                        uint32_t winId = event.motion.windowID;
                        Dispatcher::Enqueue<ui::events::RawPointerMove>(
                            ui::events::RawPointerMove{Vec2(mx, my), Vec2(dx, dy), winId});
                    }
                    break;
                }
                case SDL_EVENT_MOUSE_BUTTON_DOWN:
                {
                    float mx = static_cast<float>(event.button.x);
                    float my = static_cast<float>(event.button.y);
                    uint32_t winId = event.button.windowID;
                    uint8_t button = event.button.button;
                    Dispatcher::Enqueue<ui::events::RawPointerButton>(
                        ui::events::RawPointerButton{Vec2(mx, my), winId, true, button});

                    break;
                }
                case SDL_EVENT_MOUSE_BUTTON_UP:
                {
                    float buttonx = event.button.x;
                    float buttony = event.button.y;
                    uint32_t winId = event.button.windowID;
                    uint8_t button = event.button.button;
                    Dispatcher::Enqueue<ui::events::RawPointerButton>(
                        ui::events::RawPointerButton{Vec2(buttonx, buttony), winId, false, button});

                    break;
                }
                case SDL_EVENT_TEXT_INPUT:
                    handleTextInput(event.text.text);

                    break;
                case SDL_EVENT_KEY_DOWN:
                    if (!event.key.repeat) // 只处理真实按下，忽略系统重复
                    {
                        HELD_KEY = event.key.key;
                        m_keyPressTime = SDL_GetTicks();
                        m_lastRepeatTime = m_keyPressTime;
                        handleKeyDown(event.key.key);
                    }

                    break;
                case SDL_EVENT_KEY_UP:
                    // 重置长按状态
                    if (event.key.key == HELD_KEY)
                    {
                        HELD_KEY = SDLK_UNKNOWN;
                        m_keyPressTime = 0;
                        m_lastRepeatTime = 0;
                    }
                    break;
                case SDL_EVENT_MOUSE_WHEEL:
                {
                    // 采样当前鼠标位置并转发原始滚轮事件
                    float mx = 0.0f, my = 0.0f;
                    SDL_GetMouseState(&mx, &my);
                    Dispatcher::Enqueue<ui::events::RawPointerWheel>(ui::events::RawPointerWheel{
                        Vec2(mx, my),
                        Vec2(static_cast<float>(event.wheel.x), static_cast<float>(event.wheel.y)),
                        event.wheel.windowID});

                    break;
                }
                default:
                    break;
            }
        }
        // 每帧处理一次键盘长按重复逻辑
        ProcessKeyRepeat();
    }

    static void ProcessKeyRepeat()
    {
        if (HELD_KEY == SDLK_UNKNOWN) return;

        uint64_t now = SDL_GetTicks();

        // 检查是否达到初始延迟
        if (now < m_keyPressTime + KEY_REPEAT_DELAY) return;

        // 检查是否达到重复间隔
        if (now < m_lastRepeatTime + KEY_REPEAT_INTERVAL) return;

        // 触发重复输入
        m_lastRepeatTime = now;
        handleKeyDown(HELD_KEY);
        Dispatcher::Trigger<ui::events::UpdateRendering>();
    }

    static void DetailExposed()
    {
        // Add event watch to handle blocking modal loops (e.g., resizing on Windows)
        SDL_AddEventWatch(
            [](void*, SDL_Event* event) -> bool
            {
                // 对于大小改变，因为是在 Windows 消息循环中阻塞发生的，需要立即触发事件
                if (event->type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED)
                {
                    Dispatcher::Trigger<ui::events::WindowPixelSizeChanged>(ui::events::WindowPixelSizeChanged{
                        event->window.windowID, event->window.data1, event->window.data2});
                }
                else if (event->type == SDL_EVENT_WINDOW_MOVED)
                {
                    Dispatcher::Trigger<ui::events::WindowMoved>(
                        ui::events::WindowMoved{event->window.windowID, event->window.data1, event->window.data2});
                }

                // 窗口事件发生时立即同步窗口属性
                if (event->type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED || event->type == SDL_EVENT_WINDOW_MOVED ||
                    event->type == SDL_EVENT_WINDOW_EXPOSED || event->type == SDL_EVENT_WINDOW_SHOWN ||
                    event->type == SDL_EVENT_WINDOW_HIDDEN)
                {
                    // 查找对应的窗口实体并同步属性
                    SDL_Window* sdlWindow = SDL_GetWindowFromID(event->window.windowID);
                    if (sdlWindow != nullptr)
                    {
                        auto view = Registry::View<components::Window>();
                        for (auto entity : view)
                        {
                            auto& windowComp = view.get<components::Window>(entity);
                            if (windowComp.windowID == event->window.windowID)
                            {
                                StateSystem::syncSDLWindowProperties(entity, windowComp, sdlWindow);
                                break;
                            }
                        }
                    }
                }

                // 无论是大小改变还是窗口重绘，都需要立即重新渲染以避免黑屏或卡顿
                if (event->type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED || event->type == SDL_EVENT_WINDOW_EXPOSED)
                {
                    // 强制触发布局和渲染更新
                    Dispatcher::Trigger<ui::events::UpdateLayout>(ui::events::UpdateLayout{});
                    Dispatcher::Trigger<ui::events::UpdateRendering>(ui::events::UpdateRendering{});
                }
                return true;
            },
            nullptr);
    }

private:
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

    static void selectAll(components::TextEdit& edit)
    {
        if (edit.buffer.empty()) return;

        edit.hasSelection = true;
        edit.selectionStart = 0;
        edit.selectionEnd = edit.buffer.size();
        edit.cursorPosition = edit.selectionEnd;
    }

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
            const auto modeVal = static_cast<uint8_t>(edit.inputMode);
            const auto multiFlag = static_cast<uint8_t>(policies::TextFlag::Multiline);
            if ((modeVal & multiFlag) == 0)
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
            textComp.content = edit.buffer;

            // 标记为 Dirty
            Registry::EmplaceOrReplace<ui::components::LayoutDirtyTag>(entity);
            ui::utils::MarkRenderDirty(entity);

            // 触发文本改变回调
            if (edit.onTextChanged)
            {
                edit.onTextChanged(edit.buffer);
            }
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

            // ---- ReadOnly 安全操作（复制 / 全选）----
            if (ctrl && key == SDLK_C)
            {
                // Copy（ReadOnly 也允许）
                if (edit.hasSelection)
                {
                    copyToClipboard(edit);
                }
                continue;
            }
            if (ctrl && key == SDLK_A)
            {
                // Select All（ReadOnly 也允许）
                selectAll(edit);
                ui::utils::MarkRenderDirty(entity);
                continue;
            }

            // ---- 以下操作需要可编辑 ----
            if (policies::HasFlag(edit.inputMode, policies::TextFlag::ReadOnly)) continue;

            auto& textComp = view.get<components::Text>(entity);

            if (ctrl && key == SDLK_X)
            {
                // Cut
                if (edit.hasSelection)
                {
                    copyToClipboard(edit);
                    deleteSelection(edit);
                    textComp.content = edit.buffer;
                    Registry::EmplaceOrReplace<ui::components::LayoutDirtyTag>(entity);
                    ui::utils::MarkRenderDirty(entity);
                    // 触发文本改变回调
                    if (edit.onTextChanged)
                    {
                        edit.onTextChanged(edit.buffer);
                    }
                }
            }
            else if (ctrl && key == SDLK_V)
            {
                // Paste
                pasteFromClipboard(edit);
                textComp.content = edit.buffer;
                Registry::EmplaceOrReplace<ui::components::LayoutDirtyTag>(entity);
                ui::utils::MarkRenderDirty(entity);

                // 触发文本改变回调
                if (edit.onTextChanged)
                {
                    edit.onTextChanged(edit.buffer);
                }
            }
            else if (key == SDLK_BACKSPACE)
            {
                if (edit.hasSelection)
                {
                    deleteSelection(edit);
                }
                else if (edit.cursorPosition > 0)
                {
                    size_t prevPos = utils::PrevCharPos(edit.buffer, edit.cursorPosition);
                    edit.buffer.erase(prevPos, edit.cursorPosition - prevPos);
                    edit.cursorPosition = prevPos;
                }
                textComp.content = edit.buffer;
                Registry::EmplaceOrReplace<ui::components::LayoutDirtyTag>(entity);
                ui::utils::MarkRenderDirty(entity);

                // 触发文本改变回调
                if (edit.onTextChanged)
                {
                    edit.onTextChanged(edit.buffer);
                }
            }
            else if (key == SDLK_DELETE)
            {
                if (edit.hasSelection)
                {
                    deleteSelection(edit);
                }
                else if (edit.cursorPosition < edit.buffer.size())
                {
                    size_t nextPos = utils::NextCharPos(edit.buffer, edit.cursorPosition);
                    edit.buffer.erase(edit.cursorPosition, nextPos - edit.cursorPosition);
                }
                textComp.content = edit.buffer;
                Registry::EmplaceOrReplace<ui::components::LayoutDirtyTag>(entity);
                ui::utils::MarkRenderDirty(entity);

                // 触发文本改变回调
                if (edit.onTextChanged)
                {
                    edit.onTextChanged(edit.buffer);
                }
            }
            else if (key == SDLK_LEFT)
            {
                if (shift)
                {
                    // Extend selection left
                    moveCursorLeft(edit, true);
                }
                else
                {
                    if (edit.hasSelection)
                    {
                        // Move to start of selection
                        edit.cursorPosition = edit.selectionStart;
                        clearSelection(edit);
                    }
                    else
                    {
                        moveCursorLeft(edit, false);
                    }
                }
                ui::utils::MarkRenderDirty(entity);
            }
            else if (key == SDLK_RIGHT)
            {
                if (shift)
                {
                    // Extend selection right
                    moveCursorRight(edit, true);
                }
                else
                {
                    if (edit.hasSelection)
                    {
                        // Move to end of selection
                        edit.cursorPosition = edit.selectionEnd;
                        clearSelection(edit);
                    }
                    else
                    {
                        moveCursorRight(edit, false);
                    }
                }
                ui::utils::MarkRenderDirty(entity);
            }
            else if (key == SDLK_HOME)
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
                ui::utils::MarkRenderDirty(entity);
            }
            else if (key == SDLK_END)
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
                ui::utils::MarkRenderDirty(entity);
            }
            else if (key == SDLK_UP)
            {
                const auto modeVal = static_cast<uint8_t>(edit.inputMode);
                const auto multiFlag = static_cast<uint8_t>(policies::TextFlag::Multiline);
                if ((modeVal & multiFlag) != 0)
                {
                    // TODO: Implement multi-line cursor movement
                    // For now, just move to start
                    if (shift)
                    {
                        moveCursorToLineStart(edit, true);
                    }
                    else
                    {
                        clearSelection(edit);
                        moveCursorToLineStart(edit, false);
                    }
                    ui::utils::MarkRenderDirty(entity);
                }
            }
            else if (key == SDLK_DOWN)
            {
                const auto modeVal = static_cast<uint8_t>(edit.inputMode);
                const auto multiFlag = static_cast<uint8_t>(policies::TextFlag::Multiline);
                if ((modeVal & multiFlag) != 0)
                {
                    // TODO: Implement multi-line cursor movement
                    // For now, just move to end
                    if (shift)
                    {
                        moveCursorToLineEnd(edit, true);
                    }
                    else
                    {
                        clearSelection(edit);
                        moveCursorToLineEnd(edit, false);
                    }
                    ui::utils::MarkRenderDirty(entity);
                }
            }
            else if (key == SDLK_RETURN)
            {
                const auto modeVal = static_cast<uint8_t>(edit.inputMode);
                const auto multiFlag = static_cast<uint8_t>(policies::TextFlag::Multiline);
                if ((modeVal & multiFlag) != 0)
                {
                    // 多行模式：插入换行符
                    if (edit.buffer.size() + 1 <= edit.maxLength)
                    {
                        if (edit.hasSelection)
                        {
                            deleteSelection(edit);
                        }
                        edit.buffer.insert(edit.cursorPosition, "\n");
                        edit.cursorPosition++;
                        textComp.content = edit.buffer;
                        Registry::EmplaceOrReplace<ui::components::LayoutDirtyTag>(entity);
                        ui::utils::MarkRenderDirty(entity);

                        // 触发文本改变回调
                        if (edit.onTextChanged)
                        {
                            edit.onTextChanged(edit.buffer);
                        }
                    }
                }
                else
                {
                    // 单行模式：触发提交回调
                    if (edit.onSubmit)
                    {
                        edit.onSubmit();
                    }

                    // 如果有选中内容，删除选中文本
                    if (edit.hasSelection)
                    {
                        deleteSelection(edit);
                        textComp.content = edit.buffer;
                        Registry::EmplaceOrReplace<ui::components::LayoutDirtyTag>(entity);
                        ui::utils::MarkRenderDirty(entity);

                        // 触发文本改变回调
                        if (edit.onTextChanged)
                        {
                            edit.onTextChanged(edit.buffer);
                        }
                    }
                }
            }
            // 注意：普通字符输入由 SDL_EVENT_TEXT_INPUT 处理
        }
    }

    // 键盘长按状态
    inline static SDL_Keycode HELD_KEY = SDLK_UNKNOWN;        // 当前按下的按键
    inline static uint64_t m_keyPressTime = 0;                 // 按键按下时间（毫秒）
    inline static uint64_t m_lastRepeatTime = 0;               // 上次重复输入时间
    inline static constexpr uint64_t KEY_REPEAT_DELAY = 500;   // 长按触发延迟（毫秒）
    inline static constexpr uint64_t KEY_REPEAT_INTERVAL = 50; // 重复输入间隔（毫秒）
};

} // namespace ui::systems