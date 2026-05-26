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
#include "WorkerMailbox.hpp"

#include "../common/ThreadPool.hpp"
#include "../singleton/Dispatcher.hpp"
#include "../singleton/Registry.hpp"

namespace ui
{

class UiRuntime
{
public:
    UiRuntime() = default;

    /**
     * @brief 析构前等待本运行时所属线程池中全部 worker 任务完成，防止 UAF（R1 方案 D）。
     * @see ThreadPool::wait()
     */
    ~UiRuntime()
    {
        m_threadPool.wait(); // 等待所属 worker 完成后再销毁 m_mailbox
    }

    UiRuntime(const UiRuntime&) = delete;
    UiRuntime& operator=(const UiRuntime&) = delete;
    UiRuntime(UiRuntime&&) = delete;
    UiRuntime& operator=(UiRuntime&&) = delete;

    [[nodiscard]] Registry& registry() noexcept { return m_registry; }

    [[nodiscard]] const Registry& registry() const noexcept { return m_registry; }

    [[nodiscard]] Dispatcher& dispatcher() noexcept { return m_dispatcher; }

    [[nodiscard]] const Dispatcher& dispatcher() const noexcept { return m_dispatcher; }

    /**
     * @brief 获取本运行时的 Worker Mailbox（供主线程 drain 和 Worker 线程 enqueue）
     */
    [[nodiscard]] WorkerMailbox& mailbox() noexcept { return m_mailbox; }

    [[nodiscard]] const WorkerMailbox& mailbox() const noexcept { return m_mailbox; }

private:
    friend class UiRuntimeScope;
    friend class RuntimeFacade;

    /// 每个运行时独立的线程池，声明在最前保证最后析构（workers 停止后 mailbox/registry 才释放）
    utils::ThreadPool m_threadPool;
    Registry    m_registry;
    Dispatcher  m_dispatcher;
    WorkerMailbox m_mailbox;
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
