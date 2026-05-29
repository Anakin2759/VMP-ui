/**
 * ************************************************************************
 *
 * @file WorkerMailbox.hpp
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2026-05-25
 * @version 0.1
 * @brief 多写一读 Worker Mailbox — C18 修复
 *
 * ## 设计原理
 *
 * Worker 线程只允许向此 Mailbox 投递命令（enqueue），
 * 主线程在每帧 QueuedTask 阶段统一排干（flush）并执行。
 * 采用双缓冲 swap 模式：
 *   - 多写端（Worker 线程）：在互斥锁下向写缓冲区追加命令
 *   - 单读端（主线程）  ：O(1) swap 后在无锁状态下顺序执行
 *
 * ## 线程安全边界
 *
 * | 方法      | 调用方       | 说明               |
 * |-----------|--------------|--------------------|
 * | enqueue() | 任意线程     | 互斥锁保护写缓冲区 |
 * | flush()   | 仅主线程     | 执行阶段完全无锁   |
 *
 * ## 生命周期约束
 *
 * Worker 任务通过捕获 `WorkerMailbox*` 裸指针投递命令。
 * 调用方必须保证：持有此指针的所有 worker 任务在 UiRuntime 析构前完成。
 * 实践上通过 `m_threadPool.wait()` 在 UiRuntime 析构时同步（见 UiRuntime.hpp）。
 *
 * ## 单线程模式说明（UI_ENABLE_MULTITHREAD=0）
 *
 * submitDetached() 在单线程模式下同步执行 worker lambda；
 * worker 内的 enqueue() 会将命令放入写缓冲区，下一帧的 flush() 才会执行。
 * 因此单线程模式下命令有 1 帧延迟，属已知行为，仅影响开发期调试场景。
 *
 * ************************************************************************
 * @copyright Copyright (c) 2026 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */
#pragma once

#include <functional>
#include <mutex>
#include <variant>
#include <vector>

#include <entt/entt.hpp>

#include "common/ThreadPool.hpp"
#include "singleton/Registry.hpp"

namespace ui::worker
{

/**
 * @brief 向主线程写回 entt::registry 操作的命令
 *
 * Worker 线程在 lambda 中捕获计算结果，lambda 由主线程在 flush() 阶段执行，
 * 可安全地对 entt::registry 进行 emplace / patch / destroy 等写操作。
 *
 * @note 内部严禁调用任何 Dispatcher API（Trigger / Enqueue），
 *       避免在帧时序之外触发事件，破坏 QueuedTask → InputTask → RenderTask 的执行顺序。
 */
struct RegistryCommand
{
    std::move_only_function<void(entt::registry&)> apply;
};

/// Worker 向主线程投递的命令（variant 仅保留 RegistryCommand）
using WorkerCommand = std::variant<RegistryCommand>;

} // namespace ui::worker

namespace ui
{

/**
 * @brief 多写一读 Worker Mailbox
 *
 * 每个 UiRuntime 实例持有一个 WorkerMailbox。Worker 线程通过捕获其指针投递命令；
 * 主线程在 TaskChain::QueuedTask 帧开始阶段调用 flush() 统一执行，彻底消除
 * Worker 直接访问 entt::registry 的竞争窗口（C18 修复）。
 */
class WorkerMailbox
{
public:
    WorkerMailbox() = default;
    ~WorkerMailbox() = default;
    WorkerMailbox(const WorkerMailbox&) = delete;
    WorkerMailbox& operator=(const WorkerMailbox&) = delete;
    WorkerMailbox(WorkerMailbox&&) = delete;
    WorkerMailbox& operator=(WorkerMailbox&&) = delete;

    /**
     * @brief 从任意线程安全地投递一条命令（多写端）
     *
     * 单线程模式（UI_ENABLE_MULTITHREAD=0）下命令在调用栈内立即执行，
     * 消除多线程与单线程之间"命令延迟一帧"的行为差异（R2 修复）。
     *
     * @note 单线程 bypass 下须确保 UiRuntimeScope 已激活（Registry::current() 可用），
     *       且调用方不持有 registry 写锁或其他互斥资源。
     */
    void enqueue(worker::WorkerCommand cmd)
    {
        if constexpr (!utils::ThreadPool::isMultithreaded())
        {
            // 单线程模式：命令在调用栈直接执行，绕过双缓冲
            std::get<worker::RegistryCommand>(cmd).apply(Registry::current().raw());
            return;
        }
        std::lock_guard lock(m_mutex);
        m_writeBuffer.push_back(std::move(cmd));
    }

    /**
     * @brief 主线程专用：排干所有待处理命令（实现见 WorkerMailbox.cpp）
     *
     * 分两个阶段：
     *   1. 在锁内 O(1) swap 写/读缓冲区，随即释放锁（Worker 可立即继续 push）
     *   2. 在锁外顺序执行读缓冲区中的所有命令；每条命令独立捕获异常，
     *      保证单条命令失败不影响后续命令的执行。
     *
     * @param registry 主线程激活的 entt::registry，供 RegistryCommand 使用
     */
    void flush(entt::registry& registry);

    /**
     * @brief 调试用：检查写缓冲区是否有待处理命令（仅 Debug 构建有效）
     *
     * @warning Release 构建永远返回 false，仅供开发期诊断，避免 TOCTOU 竞态误用。
     */
    [[nodiscard]] bool hasPending() const
    {
#ifndef NDEBUG
        std::lock_guard lock(m_mutex);
        return !m_writeBuffer.empty();
#else
        return false;
#endif
    }

private:
    mutable std::mutex m_mutex;                       ///< 保护写缓冲区
    std::vector<worker::WorkerCommand> m_writeBuffer; ///< Worker 写入端（受锁保护）
    std::vector<worker::WorkerCommand> m_readBuffer;  ///< 主线程读出端（flush 后无锁访问）
};

} // namespace ui
