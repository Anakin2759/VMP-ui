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
        Dispatcher::Sink<events::RawTextInput>().connect<&TextInputSystem::onRawTextInput>(*this);
        Dispatcher::Sink<events::RawKeyInput>().connect<&TextInputSystem::onRawKeyInput>(*this);
    }

    void unregisterHandlersImpl()
    {
        Dispatcher::Sink<events::RawTextInput>().disconnect<&TextInputSystem::onRawTextInput>(*this);
        Dispatcher::Sink<events::RawKeyInput>().disconnect<&TextInputSystem::onRawKeyInput>(*this);
    }

    static void processKeyRepeat()
    {
        if (heldKey == SDLK_UNKNOWN) return;

        uint64_t now = SDL_GetTicks();
        if (now < keyPressTime + KEY_REPEAT_DELAY) return;
        if (now < lastRepeatTime + KEY_REPEAT_INTERVAL) return;

        lastRepeatTime = now;
        core::TextEditingService::handleKeyDown(heldKey, SDL_GetModState());
        Dispatcher::Trigger<ui::events::UpdateRendering>(ui::events::UpdateRendering{});
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

    static void beginKeyRepeat(SDL_Keycode key)
    {
        heldKey = key;
        keyPressTime = SDL_GetTicks();
        lastRepeatTime = keyPressTime;
    }

    static void handleKeyUp(SDL_Keycode key)
    {
        if (key != heldKey) return;

        heldKey = SDLK_UNKNOWN;
        keyPressTime = 0;
        lastRepeatTime = 0;
    }

    inline static SDL_Keycode heldKey = SDLK_UNKNOWN;
    inline static uint64_t keyPressTime = 0;
    inline static uint64_t lastRepeatTime = 0;
    static constexpr uint64_t KEY_REPEAT_DELAY = 500;
    static constexpr uint64_t KEY_REPEAT_INTERVAL = 50;
};

} // namespace ui::systems