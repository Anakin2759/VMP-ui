#include "Shortcut.hpp"
#include "systems/ShortcutSystem.hpp"
#include <utility>

namespace ui::shortcut
{

ShortcutId Register(KeyCode key, Callback callback)
{
    return systems::ShortcutSystem::registerKey(key, Mod::NONE, std::move(callback));
}

ShortcutId Register(KeyCode key, Mod mod, Callback callback)
{
    return systems::ShortcutSystem::registerKey(key, mod, std::move(callback));
}

void Unregister(ShortcutId shortcutId)
{
    systems::ShortcutSystem::unregister(shortcutId);
}

void ClearAll()
{
    systems::ShortcutSystem::clearAll();
}

} // namespace ui::shortcut
