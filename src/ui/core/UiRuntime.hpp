/**
 * ************************************************************************
 *
 * @file UiRuntime.hpp
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2026-03-26
 * @version 0.1
 * @brief ui 运行时上下文 - 管理当前活跃的 Registry 和 Dispatcher 实例

    设计目标：
    - 提供一个明确的上下文边界，确保在不同的 UiRuntime 实例之间切换时，Registry 和 Dispatcher 的状态不会混淆
    - 支持多实例场景，例如同时管理多个独立的 UI 环境（如多个窗口或嵌入式 UI）
    - 通过 RAII 模式自动管理上下文切换，减少手动错误

    主要功能：
    - UiRuntime：持有 Registry 和 Dispatcher 的实例
    - UiRuntimeScope：在构造时切换到指定 UiRuntime 的上下文，在析构时恢复之前的上下文
 *
 * ************************************************************************
 * @copyright Copyright (c) 2026 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */
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