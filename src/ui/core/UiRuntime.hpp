#pragma once

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

private:
    friend class UiRuntimeScope;

    Registry m_registry;
    Dispatcher m_dispatcher;
};

class UiRuntimeScope
{
public:
    explicit UiRuntimeScope(UiRuntime& runtime)
        : m_previousRegistry(Registry::swapActiveInstance(&runtime.m_registry)),
          m_previousDispatcher(Dispatcher::swapActiveInstance(&runtime.m_dispatcher))
    {
    }

    UiRuntimeScope(const UiRuntimeScope&) = delete;
    UiRuntimeScope& operator=(const UiRuntimeScope&) = delete;
    UiRuntimeScope(UiRuntimeScope&&) = delete;
    UiRuntimeScope& operator=(UiRuntimeScope&&) = delete;
    ~UiRuntimeScope();

private:
    Registry* m_previousRegistry;
    Dispatcher* m_previousDispatcher;
};

inline UiRuntimeScope::~UiRuntimeScope()
{
    Dispatcher::swapActiveInstance(m_previousDispatcher);
    Registry::swapActiveInstance(m_previousRegistry);
}

} // namespace ui