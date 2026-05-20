/**
 * ************************************************************************
 *
 * @file TextInputSystem.hpp
 * @brief 文本输入与键盘编辑系统
 *
 * 从 InteractionSystem 中拆出 TextEdit 相关编辑行为和键盘重复输入策略。
 *
 * ************************************************************************
 */

#pragma once

#include <SDL3/SDL.h>

#include "common/Events.hpp"
#include "core/TextEditingService.hpp"
#include "interface/Isystem.hpp"
#include "singleton/Dispatcher.hpp"

namespace ui::systems
{

class TextInputSystem : public ui::interface::EnableRegister<TextInputSystem>
{
public:
    void registerHandlersImpl()
    {
        s_current = this;
        Dispatcher::Sink<events::RawTextInput>().connect<&TextInputSystem::onRawTextInput>(*this);
        Dispatcher::Sink<events::RawKeyInput>().connect<&TextInputSystem::onRawKeyInput>(*this);
    }

    void unregisterHandlersImpl()
    {
        if (s_current == this) s_current = nullptr;
        Dispatcher::Sink<events::RawTextInput>().disconnect<&TextInputSystem::onRawTextInput>(*this);
        Dispatcher::Sink<events::RawKeyInput>().disconnect<&TextInputSystem::onRawKeyInput>(*this);
    }

    static void processKeyRepeat()
    {
        if (s_current != nullptr) s_current->doProcessKeyRepeat();
    }

private:
    void onRawTextInput(const events::RawTextInput& event) { core::TextEditingService::handleTextInput(event.text); }

    void onRawKeyInput(const events::RawKeyInput& event)
    {
        const auto key = static_cast<SDL_Keycode>(event.key);
        if (event.pressed)
        {
            if (event.repeat) return;
            beginKeyRepeat(key);
            core::TextEditingService::handleKeyDown(key, static_cast<SDL_Keymod>(event.modifiers));
            return;
        }

        handleKeyUp(key);
    }

    void doProcessKeyRepeat()
    {
        if (m_heldKey == SDLK_UNKNOWN) return;

        uint64_t now = SDL_GetTicks();
        if (now < m_keyPressTime + KEY_REPEAT_DELAY) return;
        if (now < m_lastRepeatTime + KEY_REPEAT_INTERVAL) return;

        m_lastRepeatTime = now;
        core::TextEditingService::handleKeyDown(m_heldKey, SDL_GetModState());
        Dispatcher::Trigger<ui::events::UpdateRendering>(ui::events::UpdateRendering{});
    }

    void beginKeyRepeat(SDL_Keycode key)
    {
        m_heldKey = key;
        m_keyPressTime = SDL_GetTicks();
        m_lastRepeatTime = m_keyPressTime;
    }

    void handleKeyUp(SDL_Keycode key)
    {
        if (key != m_heldKey) return;

        m_heldKey = SDLK_UNKNOWN;
        m_keyPressTime = 0;
        m_lastRepeatTime = 0;
    }

    SDL_Keycode m_heldKey = SDLK_UNKNOWN;
    uint64_t m_keyPressTime = 0;
    uint64_t m_lastRepeatTime = 0;
    static inline TextInputSystem* s_current = nullptr;
    static constexpr uint64_t KEY_REPEAT_DELAY = 500;
    static constexpr uint64_t KEY_REPEAT_INTERVAL = 50;
};

} // namespace ui::systems