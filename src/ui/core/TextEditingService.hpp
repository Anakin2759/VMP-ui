/**
 * ************************************************************************
 *
 * @file TextEditingService.hpp
 * @brief TextEdit 编辑命令服务
 *
 * 从 TextInputSystem 中抽离文本编辑算法，使系统本身只负责事件桥接和重复输入策略。
 *
 * ************************************************************************
 */

#pragma once

#include <SDL3/SDL.h>

#include <algorithm>
#include <string>

#include "api/Utils.hpp"
#include "common/Components.hpp"
#include "core/TextUtils.hpp"
#include "singleton/Registry.hpp"

namespace ui::core
{

namespace detail
{

struct TextEditSelectionOps
{
    static void clear(components::TextEdit& edit)
    {
        edit.hasSelection = false;
        edit.selectionStart = 0;
        edit.selectionEnd = 0;
    }

    static void eraseSelection(components::TextEdit& edit)
    {
        if (!edit.hasSelection) return;

        size_t start = edit.selectionStart;
        size_t end = edit.selectionEnd;
        edit.buffer.erase(start, end - start);
        edit.cursorPosition = start;
        clear(edit);
    }

    static void selectAll(components::TextEdit& edit)
    {
        if (edit.buffer.empty()) return;

        edit.hasSelection = true;
        edit.selectionStart = 0;
        edit.selectionEnd = edit.buffer.size();
        edit.cursorPosition = edit.selectionEnd;
    }
};

struct TextEditModeOps
{
    static bool isMultiline(const components::TextEdit& edit)
    {
        const auto modeValue = static_cast<uint8_t>(edit.inputMode);
        const auto multilineFlag = static_cast<uint8_t>(policies::TextFlag::Multiline);
        return (modeValue & multilineFlag) != 0;
    }

    static void normalizeInputForMode(std::string& input, const components::TextEdit& edit)
    {
        if (isMultiline(edit)) return;

        input.erase(std::remove(input.begin(), input.end(), '\n'), input.end());
        input.erase(std::remove(input.begin(), input.end(), '\r'), input.end());
    }
};

struct TextEditContentOps
{
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

    static bool
        insertText(entt::entity entity, components::TextEdit& edit, components::Text& textComp, std::string input)
    {
        if (input.empty()) return false;

        TextEditModeOps::normalizeInputForMode(input, edit);
        if (input.empty()) return false;

        if (edit.hasSelection)
        {
            TextEditSelectionOps::eraseSelection(edit);
        }

        if (edit.buffer.size() + input.size() > edit.maxLength)
        {
            size_t available = edit.maxLength - edit.buffer.size();
            if (available == 0) return false;
            input = input.substr(0, available);
        }

        edit.buffer.insert(edit.cursorPosition, input);
        edit.cursorPosition += input.size();
        applyContentChange(entity, edit, textComp);
        return true;
    }

    static bool
        handleDeletionKey(entt::entity entity, components::TextEdit& edit, components::Text& textComp, SDL_Keycode key)
    {
        if (key == SDLK_BACKSPACE)
        {
            if (edit.hasSelection)
            {
                TextEditSelectionOps::eraseSelection(edit);
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
                TextEditSelectionOps::eraseSelection(edit);
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

    static bool handleReturnKey(entt::entity entity, components::TextEdit& edit, components::Text& textComp)
    {
        if (TextEditModeOps::isMultiline(edit))
        {
            return insertText(entity, edit, textComp, "\n");
        }

        if (edit.onSubmit)
        {
            edit.onSubmit();
        }

        if (!edit.hasSelection) return true;

        TextEditSelectionOps::eraseSelection(edit);
        applyContentChange(entity, edit, textComp);
        return true;
    }
};

struct TextEditClipboardOps
{
    static void copySelection(const components::TextEdit& edit)
    {
        if (!edit.hasSelection) return;

        std::string selectedText = edit.buffer.substr(edit.selectionStart, edit.selectionEnd - edit.selectionStart);
        SDL_SetClipboardText(selectedText.c_str());
    }

    static bool pasteText(components::TextEdit& edit)
    {
        if (!SDL_HasClipboardText()) return false;

        char* clipboardText = SDL_GetClipboardText();
        if (clipboardText == nullptr) return false;

        std::string text(clipboardText);
        SDL_free(clipboardText);
        if (text.empty()) return false;

        TextEditModeOps::normalizeInputForMode(text, edit);
        if (text.empty()) return false;

        if (edit.hasSelection)
        {
            TextEditSelectionOps::eraseSelection(edit);
        }

        if (edit.buffer.size() + text.size() > edit.maxLength)
        {
            size_t available = edit.maxLength - edit.buffer.size();
            if (available == 0) return false;
            text = text.substr(0, available);
        }

        edit.buffer.insert(edit.cursorPosition, text);
        edit.cursorPosition += text.size();
        return true;
    }
};

struct TextEditNavigationOps
{
    static void moveCursorLeft(components::TextEdit& edit, bool extend)
    {
        if (edit.cursorPosition <= 0) return;

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
            return;
        }

        edit.cursorPosition = newPos;
    }

    static void moveCursorRight(components::TextEdit& edit, bool extend)
    {
        if (edit.cursorPosition >= edit.buffer.size()) return;

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
            return;
        }

        edit.cursorPosition = newPos;
    }

    static void moveCursorToLineStart(components::TextEdit& edit, bool extend)
    {
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

    static bool handleHorizontal(entt::entity entity, components::TextEdit& edit, SDL_Keycode key, bool shift)
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
                TextEditSelectionOps::clear(edit);
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
                TextEditSelectionOps::clear(edit);
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

    static bool handleBoundary(entt::entity entity, components::TextEdit& edit, SDL_Keycode key, bool shift)
    {
        if (key == SDLK_HOME)
        {
            if (shift)
            {
                moveCursorToLineStart(edit, true);
            }
            else
            {
                TextEditSelectionOps::clear(edit);
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
                TextEditSelectionOps::clear(edit);
                moveCursorToLineEnd(edit, false);
            }

            ui::utils::MarkVisualChanged(entity);
            return true;
        }

        return false;
    }

    static bool handleVertical(entt::entity entity, components::TextEdit& edit, SDL_Keycode key, bool shift)
    {
        if (!TextEditModeOps::isMultiline(edit)) return false;

        if (key == SDLK_UP)
        {
            if (shift)
            {
                moveCursorToLineStart(edit, true);
            }
            else
            {
                TextEditSelectionOps::clear(edit);
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
                TextEditSelectionOps::clear(edit);
                moveCursorToLineEnd(edit, false);
            }

            ui::utils::MarkVisualChanged(entity);
            return true;
        }

        return false;
    }
};

} // namespace detail

class TextEditingService
{
public:
    static void handleTextInput(const std::string& rawText)
    {
        auto view = Registry::View<components::FocusedTag, components::TextEdit, components::Text>();
        for (auto entity : view)
        {
            if (!Registry::AnyOf<components::TextEditTag>(entity)) continue;

            auto& edit = view.get<components::TextEdit>(entity);
            if (policies::HasFlag(edit.inputMode, policies::TextFlag::ReadOnly)) continue;

            auto& textComp = view.get<components::Text>(entity);
            detail::TextEditContentOps::insertText(entity, edit, textComp, rawText);
        }
    }

    static void handleKeyDown(SDL_Keycode key, SDL_Keymod modState)
    {
        auto view = Registry::View<components::FocusedTag, components::TextEdit, components::Text>();
        for (auto entity : view)
        {
            if (!Registry::AnyOf<components::TextEditTag>(entity)) continue;

            auto& edit = view.get<components::TextEdit>(entity);
            bool ctrl = (modState & SDL_KMOD_CTRL) != 0;
            bool shift = (modState & SDL_KMOD_SHIFT) != 0;

            if (handleReadOnlyShortcut(entity, edit, key, ctrl)) continue;
            if (policies::HasFlag(edit.inputMode, policies::TextFlag::ReadOnly)) continue;

            auto& textComp = view.get<components::Text>(entity);
            if (handleClipboardShortcut(entity, edit, textComp, key, ctrl)) continue;
            if (detail::TextEditContentOps::handleDeletionKey(entity, edit, textComp, key)) continue;
            if (detail::TextEditNavigationOps::handleHorizontal(entity, edit, key, shift)) continue;
            if (detail::TextEditNavigationOps::handleBoundary(entity, edit, key, shift)) continue;
            if (detail::TextEditNavigationOps::handleVertical(entity, edit, key, shift)) continue;
            if (key == SDLK_RETURN)
            {
                detail::TextEditContentOps::handleReturnKey(entity, edit, textComp);
            }
        }
    }

private:
    static bool handleReadOnlyShortcut(entt::entity entity, components::TextEdit& edit, SDL_Keycode key, bool ctrl)
    {
        if (ctrl && key == SDLK_C)
        {
            if (edit.hasSelection)
            {
                detail::TextEditClipboardOps::copySelection(edit);
            }
            return true;
        }

        if (ctrl && key == SDLK_A)
        {
            detail::TextEditSelectionOps::selectAll(edit);
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

            detail::TextEditClipboardOps::copySelection(edit);
            detail::TextEditSelectionOps::eraseSelection(edit);
            detail::TextEditContentOps::applyContentChange(entity, edit, textComp);
            return true;
        }

        if (ctrl && key == SDLK_V)
        {
            if (!detail::TextEditClipboardOps::pasteText(edit)) return true;
            detail::TextEditContentOps::applyContentChange(entity, edit, textComp);
            return true;
        }

        return false;
    }
};

} // namespace ui::core
