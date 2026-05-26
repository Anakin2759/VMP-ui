/**
 * ************************************************************************
 *
 * @file ShortcutSystem.hpp
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2026-05-18
 * @version 0.1
 * @brief 快捷键系统 - 注册并响应键盘快捷键
 *
 * 设计原则：
 * - 监听 RawKeyInput [IMMEDIATE] 事件
 * - 支持单键 + 修饰键组合（Ctrl/Alt/Shift/GUI）
 * - 支持 lambda 和任意可调用对象
 * - 线程安全：所有操作在事件循环线程执行
 *
 * 用法：
 *   ui::shortcut::Register(SDL_SCANCODE_S, ui::shortcut::Mod::CTRL, []{ save(); });
 *   ui::shortcut::Register(SDL_SCANCODE_F5, []{ refresh(); });
 *   auto id = ui::shortcut::Register(SDL_SCANCODE_W, ui::shortcut::Mod::CTRL, []{ closeTab(); });
 *   ui::shortcut::Unregister(id);
 *
 * ************************************************************************
 * @copyright Copyright (c) 2026 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */
#pragma once

#include <cstdint>
#include <vector>
#include <algorithm>
#include "../api/Shortcut.hpp"
#include "../common/Events.hpp"
#include "../core/RuntimeFacade.hpp"
#include "../interface/Isystem.hpp"

namespace ui::systems
{

/**
 * @brief 快捷键系统
 *
 * 持有全局快捷键注册表，监听 RawKeyInput 事件并分派回调。
 * 由 SystemManager 在启动时注册一次。
 */
class ShortcutSystem final : public ui::interface::EnableRegister<ShortcutSystem>
{
public:
    void registerHandlersImpl()
    {
        auto& dispatcher = RuntimeFacade::current().enttDispatcher();
        dispatcher.sink<ui::events::RawKeyInput>().connect<&ShortcutSystem::onRawKeyInput>(*this);
    }

    void unregisterHandlersImpl()
    {
        auto& dispatcher = RuntimeFacade::current().enttDispatcher();
        dispatcher.sink<ui::events::RawKeyInput>().disconnect<&ShortcutSystem::onRawKeyInput>(*this);
    }

    /**
     * @brief 注册快捷键（通过 SDL keycode，支持修饰键）
     * @param key      SDL 虚拟键码（SDLK_* 系列）
     * @param mod      修饰键掩码，默认 NONE
     * @param callback 触发时执行的回调（支持 lambda）
     * @return 快捷键 ID，可用于 Unregister
     */
    static shortcut::ShortcutId registerKey(int32_t key,
                                            shortcut::Mod mod,
                                            shortcut::Callback callback)
    {
        const shortcut::ShortcutId newId = s_nextId++;
        s_shortcuts.push_back(Entry{newId, key, mod, std::move(callback)});
        return newId;
    }

    /**
     * @brief 注销快捷键
     * @param id 注册时返回的 ID
     */
    static void unregister(shortcut::ShortcutId shortcutId)
    {
        auto iter = std::find_if(s_shortcuts.begin(), s_shortcuts.end(),
                                 [shortcutId](const Entry& entry) { return entry.id == shortcutId; });
        if (iter != s_shortcuts.end())
        {
            s_shortcuts.erase(iter);
        }
    }

    /**
     * @brief 清除所有已注册快捷键
     */
    static void clearAll()
    {
        s_shortcuts.clear();
    }

private:
    struct Entry
    {
        shortcut::ShortcutId  id;
        int32_t               key;       // SDL 虚拟键码
        shortcut::Mod         mod;       // 修饰键掩码
        shortcut::Callback    callback;
    };

    void onRawKeyInput(const ui::events::RawKeyInput& event)
    {
        // 只处理按下事件（非重复）
        if (!event.pressed || event.repeat)
        {
            return;
        }

        // 将当前修饰键状态归一化（忽略左右区分）
        const auto activeMod = normalizeModifiers(event.modifiers);

        for (auto& entry : s_shortcuts)
        {
            if (entry.key == event.key && entry.mod == activeMod)
            {
                entry.callback();
            }
        }
    }

    static shortcut::Mod normalizeModifiers(uint16_t sdlMod)
    {
        // 直接使用 Mod 枚举掩码（数值与 SDL_KMOD_* 完全一致，无需包含 SDL 头）
        uint16_t result = 0;
        if ((sdlMod & static_cast<uint16_t>(shortcut::Mod::SHIFT)) != 0) result |= static_cast<uint16_t>(shortcut::Mod::SHIFT);
        if ((sdlMod & static_cast<uint16_t>(shortcut::Mod::CTRL))  != 0) result |= static_cast<uint16_t>(shortcut::Mod::CTRL);
        if ((sdlMod & static_cast<uint16_t>(shortcut::Mod::ALT))   != 0) result |= static_cast<uint16_t>(shortcut::Mod::ALT);
        if ((sdlMod & static_cast<uint16_t>(shortcut::Mod::GUI))   != 0) result |= static_cast<uint16_t>(shortcut::Mod::GUI);
        return static_cast<shortcut::Mod>(result);
    }

    inline static std::vector<Entry>   s_shortcuts{}; 
    inline static shortcut::ShortcutId s_nextId{1};   
};

} // namespace ui::systems
