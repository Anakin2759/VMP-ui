/**
 * ************************************************************************
 *
 * @file UiRuntime.hpp
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2026-03-26
 * @version 0.1

    Runtime owns Registry and Dispatcher instances.
    UiRuntimeScope temporarily activates a runtime and restores the previous one on destruction.
 *
 * ************************************************************************
 * @copyright Copyright (c) 2026 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */
#pragma once

#include "RuntimeFacade.hpp"

#include "../singleton/Dispatcher.hpp"
#include "../singleton/Registry.hpp"

namespace ui
{

class UiRuntime
{
public:
    UiRuntime() = default;
    ~UiRuntime() = default;

    UiRuntime(const UiRuntime&) = delete;
    UiRuntime& operator=(const UiRuntime&) = delete;
    UiRuntime(UiRuntime&&) = delete;
    UiRuntime& operator=(UiRuntime&&) = delete;

    [[nodiscard]] Registry& registry() noexcept { return m_registry; }

    [[nodiscard]] const Registry& registry() const noexcept { return m_registry; }

    [[nodiscard]] Dispatcher& dispatcher() noexcept { return m_dispatcher; }

    [[nodiscard]] const Dispatcher& dispatcher() const noexcept { return m_dispatcher; }

private:
    friend class UiRuntimeScope;
    friend class RuntimeFacade;

    Registry m_registry;
    Dispatcher m_dispatcher;
};

class UiRuntimeScope
{
public:
    explicit UiRuntimeScope(UiRuntime& runtime)
    : m_previousRuntime(RuntimeFacade::current().activateRuntime(runtime))
    {
    }

    UiRuntimeScope(const UiRuntimeScope&) = delete;
    UiRuntimeScope& operator=(const UiRuntimeScope&) = delete;
    UiRuntimeScope(UiRuntimeScope&&) = delete;
    UiRuntimeScope& operator=(UiRuntimeScope&&) = delete;
    ~UiRuntimeScope();

private:
    RuntimeFacade::ActiveRuntimeState m_previousRuntime;
};

inline UiRuntimeScope::~UiRuntimeScope()
{
    RuntimeFacade::current().restoreRuntime(m_previousRuntime);
}

} // namespace ui
