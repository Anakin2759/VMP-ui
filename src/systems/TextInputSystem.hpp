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

#include <entt/entt.hpp>
#include <SDL3/SDL.h>

#include "common/Events.hpp"
#include "core/TextEditingService.hpp"
#include "interface/ISystem.hpp"
#include "singleton/Dispatcher.hpp"

namespace ui::systems
{

class TextInputSystem : public ui::interface::EnableRegister<TextInputSystem>
{
public:
    TextInputSystem() = default;
    explicit TextInputSystem(Registry& /*reg*/, Dispatcher& disp) : m_disp(&disp) {}

    void registerHandlersImpl()
    {
        m_disp->sink<events::TickKeyRepeat>().connect<&TextInputSystem::doProcessKeyRepeat>(*this);
        m_disp->sink<events::RawTextInput>().connect<&TextInputSystem::onRawTextInput>(*this);
        m_disp->sink<events::RawKeyInput>().connect<&TextInputSystem::onRawKeyInput>(*this);
    }

    void unregisterHandlersImpl()
    {
        m_disp->sink<events::TickKeyRepeat>().disconnect<&TextInputSystem::doProcessKeyRepeat>(*this);
        m_disp->sink<events::RawTextInput>().disconnect<&TextInputSystem::onRawTextInput>(*this);
        m_disp->sink<events::RawKeyInput>().disconnect<&TextInputSystem::onRawKeyInput>(*this);
    }

    ui::interface::SystemPhase getPhase() { return ui::interface::SystemPhase::INPUT; }

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
        m_disp->trigger<ui::events::UpdateRendering>(ui::events::UpdateRendering{});
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
    static constexpr uint64_t KEY_REPEAT_DELAY = 500;
    static constexpr uint64_t KEY_REPEAT_INTERVAL = 50;
    Dispatcher* m_disp = nullptr;
};

} // namespace ui::systems