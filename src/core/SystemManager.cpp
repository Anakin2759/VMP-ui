/**
 * SystemManager implementation
 */

#include "SystemManager.hpp"
#include "singleton/Logger.hpp"
#include "singleton/Registry.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <vector>
#include <utility>

// 引入所有子系统头文件
#include "../systems/RenderSystem.hpp"
#include "../systems/TweenSystem.hpp"
#include "../systems/InteractionSystem.hpp"
#include "../systems/PlatformWindowSystem.hpp"
#include "../systems/TextInputSystem.hpp"
#include "../systems/HitTestSystem.hpp"
#include "../systems/LayoutSystem.hpp"
#include "../systems/StateSystem.hpp" // 保持与 Application.h 中的一致
#include "../systems/ActionSystem.hpp"
#include "../systems/TimerSystem.hpp"
#include "../systems/ShortcutSystem.hpp"
namespace ui
{
SystemManager::SystemManager()
{
    Logger::info("[SystemManager] 正在注册 PlatformWindowSystem...");
    m_systems.emplace_back(systems::PlatformWindowSystem{});

    Logger::info("[SystemManager] 正在注册 InteractionSystem...");
    m_systems.emplace_back(systems::InteractionSystem{});

    Logger::info("[SystemManager] 正在注册 TextInputSystem...");
    m_systems.emplace_back(systems::TextInputSystem{});

    Logger::info("[SystemManager] 正在注册 HitTestSystem...");
    m_systems.emplace_back(systems::HitTestSystem{});

    Logger::info("[SystemManager] 正在注册 TweenSystem...");
    m_systems.emplace_back(systems::TweenSystem{});

    Logger::info("[SystemManager] 正在注册 LayoutSystem...");
    m_systems.emplace_back(systems::LayoutSystem{});

    Logger::info("[SystemManager] 正在注册 RenderSystem...");
    m_systems.emplace_back(systems::RenderSystem{});

    Logger::info("[SystemManager] 正在注册 StateSystem...");
    m_systems.emplace_back(systems::StateSystem{});

    Logger::info("[SystemManager] 正在注册 ActionSystem...");
    m_systems.emplace_back(systems::ActionSystem{});

    Logger::info("[SystemManager] 正在注册 TimerSystem...");
    m_systems.emplace_back(systems::TimerSystem{});

    Logger::info("[SystemManager] 正在注册 ShortcutSystem...");
    m_systems.emplace_back(systems::ShortcutSystem{});

    Logger::info("[SystemManager] 系统管理器初始化完成，已注册 {} 个系统", m_systems.size());
}

SystemManager::~SystemManager()
{
    unregisterAllHandlers();
}

void SystemManager::registerAllHandlers()
{
    // OP-22: 按阶段排序，确保事件处理器按 Input→Logic→Animation→Layout→Render→Frame 顺序订阅
    // entt::poly 是 move-only 类型，无法直接用于 stable_sort 比较器；改用索引排序后重组
    std::vector<std::size_t> indices(m_systems.size());
    std::ranges::iota(indices, std::size_t{0});
    std::ranges::stable_sort(indices,
                             [this](std::size_t leftIndex, std::size_t rightIndex)
                             { return m_systems.at(leftIndex)->getPhase() < m_systems.at(rightIndex)->getPhase(); });
    decltype(m_systems) sorted;
    sorted.reserve(m_systems.size());
    for (auto systemIndex : indices)
    {
        sorted.push_back(std::move(m_systems.at(systemIndex)));
    }
    m_systems = std::move(sorted);
    for (auto& system : m_systems)
    {
        system->registerHandlers();
    }
}

void SystemManager::unregisterAllHandlers()
{
    for (auto& system : m_systems)
    {
        system->unregisterHandlers();
    }
}

void SystemManager::pollInput()
{
    for (auto& system : m_systems)
    {
        system->pollInput();
    }
}

void SystemManager::removeSystem(uint8_t index)
{
    if (index < m_systems.size())
    {
        m_systems.erase(m_systems.begin() + index);
    }
}

void SystemManager::clear()
{
    Registry::Clear();
}

} // namespace ui
