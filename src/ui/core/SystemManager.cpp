/**
 * SystemManager implementation
 */

#include "pch.hpp"
#include "SystemManager.hpp"
#include "singleton/Registry.hpp"

// 引入所有子系统头文件
#include "../systems/RenderSystem.hpp"
#include "../systems/TweenSystem.hpp"
#include "../systems/InteractionSystem.hpp"
#include "../systems/HitTestSystem.hpp"
#include "../systems/LayoutSystem.hpp"
#include "../systems/StateSystem.hpp" // 保持与 Application.h 中的一致
#include "../systems/ActionSystem.hpp"
#include "../systems/TimerSystem.hpp"
namespace ui
{
SystemManager::SystemManager()
{
    Logger::info("[SystemManager] 正在注册 InteractionSystem...");
    m_systems.emplace_back(systems::InteractionSystem{});

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

    Logger::info("[SystemManager] 系统管理器初始化完成，已注册 {} 个系统", m_systems.size());
}

SystemManager::~SystemManager()
{
    unregisterAllHandlers();
}

void SystemManager::registerAllHandlers()
{
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
