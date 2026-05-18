/**
 * ************************************************************************
 *
 * @file Shortcut.hpp
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2026-05-18
 * @version 0.1
 * @brief 快捷键注册公开 API
 *
 * 此头文件不依赖任何 SDL 头文件，可安全包含于任意翻译单元。
 * KeyCode = int32_t，与 SDL_Keycode (Sint32) 完全兼容，
 * 用户传入 SDLK_* 常量时会隐式转换，无需额外包含 SDL 头文件。
 *
 * 示例：
 *   #include <SDL3/SDL_keycode.h>  // 仅需要 SDLK_* 常量时
 *   auto id = ui::shortcut::Register(SDLK_F5, []{ refresh(); });
 *   auto id2 = ui::shortcut::Register(SDLK_S, ui::shortcut::Mod::CTRL, []{ save(); });
 *   ui::shortcut::Unregister(id);
 *
 * Chain DSL：
 *   btn | OnKeyPress(SDLK_RETURN, []{ submit(); });
 *   btn | OnKeyPress(SDLK_S, ui::shortcut::Mod::CTRL, []{ save(); });
 *
 * ************************************************************************
 * @copyright Copyright (c) 2026 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */
#pragma once

#include <cstdint>
#include <memory>
#include <entt/entt.hpp>
#include "../common/Types.hpp"
#include "Chains.hpp"

namespace ui::shortcut
{

/**
 * @brief 修饰键掩码
 *
 * 数值与 SDL_Keymod 完全兼容（无需包含 SDL 头文件）：
 *   SHIFT = SDL_KMOD_SHIFT = 0x0003
 *   CTRL  = SDL_KMOD_CTRL  = 0x00C0
 *   ALT   = SDL_KMOD_ALT   = 0x0300
 *   GUI   = SDL_KMOD_GUI   = 0x0C00
 */
enum class Mod : uint16_t
{
    NONE  = 0x0000,
    SHIFT = 0x0003,
    CTRL  = 0x00C0,
    ALT   = 0x0300,
    GUI   = 0x0C00,
};

inline Mod operator|(Mod lhs, Mod rhs)
{
    return static_cast<Mod>(static_cast<uint16_t>(lhs) | static_cast<uint16_t>(rhs));
}

using ShortcutId = uint32_t;
using Callback   = ::ui::VoidCallback;
/// int32_t 与 SDL_Keycode (Sint32) 完全兼容，可直接传入 SDLK_* 常量
using KeyCode    = int32_t;

/**
 * @brief 注册全局快捷键（单键，无修饰）
 * @param key      键码（传入 SDLK_* 常量即可）
 * @param callback 触发时执行的回调，支持 lambda
 * @return ShortcutId 可用于 Unregister
 */
ShortcutId Register(KeyCode key, Callback callback);

/**
 * @brief 注册全局快捷键（键 + 修饰键组合）
 * @param key      键码
 * @param mod      修饰键掩码（可用 | 组合，如 Mod::CTRL | Mod::SHIFT）
 * @param callback 触发时执行的回调，支持 lambda
 * @return ShortcutId 可用于 Unregister
 */
ShortcutId Register(KeyCode key, Mod mod, Callback callback);

/**
 * @brief 注销已注册的快捷键
 * @param id Register 返回的 ID
 */
void Unregister(ShortcutId shortcutId);

/**
 * @brief 清除所有已注册快捷键
 */
void ClearAll();

} // namespace ui::shortcut

namespace ui::chains
{

/**
 * @brief Chain DSL：绑定全局快捷键（单键，无修饰）
 *
 * 语法：entity | OnKeyPress(SDLK_RETURN, []{ submit(); });
 *
 * @note 注册为全局快捷键；entity 参数作为接口占位符（与 Chain DSL 统一）。
 */
inline auto OnKeyPress(ui::shortcut::KeyCode key, ui::shortcut::Callback callback)
{
    auto sharedCb = std::make_shared<ui::shortcut::Callback>(std::move(callback));
    return Chain{[key, sharedCb](entt::entity /*entity*/) mutable
    {
        ui::shortcut::Register(key, [sharedCb] { (*sharedCb)(); });
    }};
}

/**
 * @brief Chain DSL：绑定全局快捷键（键 + 修饰键组合）
 *
 * 语法：entity | OnKeyPress(SDLK_S, ui::shortcut::Mod::CTRL, []{ save(); });
 */
inline auto OnKeyPress(ui::shortcut::KeyCode key, ui::shortcut::Mod mod, ui::shortcut::Callback callback)
{
    auto sharedCb = std::make_shared<ui::shortcut::Callback>(std::move(callback));
    return Chain{[key, mod, sharedCb](entt::entity /*entity*/) mutable
    {
        ui::shortcut::Register(key, mod, [sharedCb] { (*sharedCb)(); });
    }};
}

} // namespace ui::chains
