/**
 * ************************************************************************
 *
 * @file ThemeSystem.hpp
 * @brief 最小 ThemeSystem：为尚未主题化的实体补默认样式，并支持最小交互态主题更新。
 *
 * 实现已移至 ThemeSystem.cpp，减少头文件编译依赖。
 *
 * ************************************************************************
 */
#pragma once

#include <entt/entt.hpp>
#include "interface/ISystem.hpp"

namespace ui::systems
{

class ThemeSystem : public ui::interface::EnableRegister<ThemeSystem>
{
public:
    ThemeSystem() = default;
    explicit ThemeSystem(entt::registry& reg, entt::dispatcher& disp) : m_reg(&reg), m_disp(&disp) {}

    void registerHandlersImpl();
    void unregisterHandlersImpl();
    ui::interface::SystemPhase getPhase();

private:
    void update();
    entt::registry* m_reg = nullptr;
    entt::dispatcher* m_disp = nullptr;
};

} // namespace ui::systems