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
 * 主线程在每帧 QueuedTask 阶段统一排干（drain）并执行。
 * 采用双缓冲 swap 模式：
 *   - 多写端（Worker 线程）：在互斥锁下向写缓冲区追加命令
 *   - 单读端（主线程）  ：O(1) swap 后在无锁状态下顺序执行
 *
 * ## 线程安全边界
 *
 * | 方法      | 调用方       | 说明               |
 * |-----------|--------------|--------------------|
 * | enqueue() | 任意线程     | 互斥锁保护写缓冲区 |
 * | drain()   | 仅主线程     | 执行阶段完全无锁   |
 *
 * ## 生命周期约束
 *
 * Worker 任务通过捕获 `WorkerMailbox*` 裸指针投递命令。
 * 调用方必须保证：持有此指针的所有 worker 任务在 UiRuntime 析构前完成。
 * 实践上通过 `ThreadPool::instance().wait()` 在 UiRuntime 析构时同步（见 UiRuntime.hpp）。
 *
 * ## 单线程模式说明（UI_ENABLE_MULTITHREAD=0）
 *
 * submitDetached() 在单线程模式下同步执行 worker lambda；
 * worker 内的 enqueue() 会将命令放入写缓冲区，下一帧的 drain() 才会执行。
 * 因此单线程模式下命令有 1 帧延迟，属已知行为，仅影响开发期调试场景。
 *
 * ************************************************************************
 * @copyright Copyright (c) 2026 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */
#pragma once

#include <exception>
#include <functional>
#include <mutex>
#include <variant>
#include <vector>

#include <entt/entt.hpp>

#include "../singleton/Logger.hpp"

namespace ui::worker
{

/**
 * @brief 向主线程写回 entt::registry 操作的命令
 *
 * Worker 线程在 lambda 中捕获计算结果，lambda 由主线程在 drain() 阶段执行，
 * 可安全地对 entt::registry 进行 emplace / patch / destroy 等写操作。
 *
 * @note 内部严禁调用任何 Dispatcher API（Trigger / Enqueue），
 *       避免在帧时序之外触发事件，破坏 QueuedTask → InputTask → RenderTask 的执行顺序。
 */
struct RegistryCommand
{
    std::move_only_function<void(entt::registry&)> apply;
};

/**
 * @brief 在主线程上执行的纯副作用回调命令（无 registry 访问）
 *
 * 适用于日志记录、通知回调等不需要访问 ECS 的场景。
 *
 * @note 内部严禁访问 entt::registry 或调用 Dispatcher API，
 *       以防绕过 WorkerMailbox 的保护机制直接触碰 ECS。
 */
struct InvokeCommand
{
    std::move_only_function<void()> invoke;
};

/// Worker 向主线程投递的命令变体类型
using WorkerCommand = std::variant<RegistryCommand, InvokeCommand>;

} // namespace ui::worker

namespace ui
{

/**
 * @brief 多写一读 Worker Mailbox
 *
 * 每个 UiRuntime 实例持有一个 WorkerMailbox。Worker 线程通过捕获其指针投递命令；
 * 主线程在 TaskChain::QueuedTask 帧开始阶段调用 drain() 统一执行，彻底消除
 * Worker 直接访问 entt::registry 的竞争窗口（C18 修复）。
 */
class WorkerMailbox
{
public:
    WorkerMailbox() = default;
    ~WorkerMailbox() = default;
    WorkerMailbox(const WorkerMailbox&)            = delete;
    WorkerMailbox& operator=(const WorkerMailbox&) = delete;
    WorkerMailbox(WorkerMailbox&&)                 = delete;
    WorkerMailbox& operator=(WorkerMailbox&&)      = delete;

    /**
     * @brief 从任意线程安全地投递一条命令（多写端）
     *
     * 在互斥锁下将命令追加到写缓冲区；锁持有时间极短（仅一次 push_back）。
     */
    void enqueue(worker::WorkerCommand cmd)
    {
        std::lock_guard lock(m_mutex);
        m_writeBuffer.push_back(std::move(cmd));
    }

    /**
     * @brief 主线程专用：排干所有待处理命令
     *
     * 分两个阶段：
     *   1. 在锁内 O(1) swap 写/读缓冲区，随即释放锁（Worker 可立即继续 push）
     *   2. 在锁外顺序执行读缓冲区中的所有命令；每条命令独立捕获异常，
     *      保证单条命令失败不影响后续命令的执行。
     *
     * @param registry 主线程激活的 entt::registry，供 RegistryCommand 使用
     */
    void drain(entt::registry& registry)
    {
        // Phase 1: 最小化锁持有时间，swap 后 Worker 可立即写入新命令
        {
            std::lock_guard lock(m_mutex);
            m_readBuffer.swap(m_writeBuffer);
        }

        // Phase 2: 无锁执行，m_readBuffer 由主线程独占
        for (auto& cmd : m_readBuffer)
        {
            try
            {
                std::visit(
                    [&registry](auto& command)
                    {
                        using T = std::decay_t<decltype(command)>;
                        if constexpr (std::is_same_v<T, worker::RegistryCommand>)
                        {
                            command.apply(registry);
                        }
                        else
                        {
                            command.invoke();
                        }
                    },
                    cmd);
            }
            catch (const std::exception& ex)
            {
                Logger::error("[WorkerMailbox] Command threw: {}", ex.what());
            }
            catch (...)
            {
                Logger::error("[WorkerMailbox] Command threw unknown exception.");
            }
        }
        m_readBuffer.clear();
    }

private:
    mutable std::mutex              m_mutex;       ///< 保护写缓冲区
    std::vector<worker::WorkerCommand> m_writeBuffer; ///< Worker 写入端（受锁保护）
    std::vector<worker::WorkerCommand> m_readBuffer;  ///< 主线程读出端（drain 后无锁访问）
};

} // namespace ui
