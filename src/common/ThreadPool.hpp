/**
 * ************************************************************************
 *
 * @file ThreadPool.hpp
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2026-05-19
 * @version 0.1
 * @brief UI 通用线程池（单线程 / 多线程双模式）
 *
 * 由 CMake 选项 UI_ENABLE_MULTITHREAD 决定行为：
 *   - 关（默认）：submit() 直接在调用线程同步执行任务，对调用方完全透明。
 *   - 开：初始化时启动 worker 线程，submit() 把任务投递到队列由 worker 消费。
 *
 * 设计要点：
 *   - 单例，沿用 Meyers singleton；首次 instance() 调用完成 worker 启动。
 *   - submit() 永远返回 std::future<R>，多/单模式下语义一致。
 *   - GPU 等不线程安全的资源不能直接丢进线程池；线程池只做 CPU 侧 IO/解码等工作。
 *
 * ************************************************************************
 * @copyright Copyright (c) 2026 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */

#pragma once

#include <cstddef>
#include <exception>
#include <future>
#include <atomic>
#include <memory>
#include <thread>
#include <tuple>
#include <type_traits>
#include <utility>

#include <asio/post.hpp>
#include <asio/thread_pool.hpp>

#include "../singleton/Logger.hpp"

#ifndef UI_ENABLE_MULTITHREAD
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define UI_ENABLE_MULTITHREAD 0
#endif

namespace ui::utils
{

/**
 * @brief 简易任务线程池
 *
 * - 任务类型：任意可调用对象，签名 R()，通过 submit() 返回 std::future<R>。
 * - 关闭语义：析构时设置停止标志，唤醒所有 worker，等待 join。
 *
 * 注意：本类不保证 GPU 设备调用线程安全；调用方负责确保提交到 worker 的任务
 *       只触达线程安全的资源（例如 stb_image 解码、文件 IO 等）。
 */
class ThreadPool
{
public:
    /**
     * @brief 获取全局唯一实例
     *
     * @param workerCount 仅在多线程模式且首次构造时生效，0 表示自动选择。
     */
    static ThreadPool& instance(std::size_t workerCount = 0)
    {
        static ThreadPool pool(workerCount);
        return pool;
    }

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;

    /**
     * @brief 是否启用多线程
     */
    [[nodiscard]] static constexpr bool isMultithreaded() noexcept { return UI_ENABLE_MULTITHREAD != 0; }

    /**
     * @brief 当前 worker 数量（单线程模式恒为 0）
     */
    [[nodiscard]] std::size_t workerCount() const noexcept
    {
        if constexpr (isMultithreaded())
        {
            if (!m_pool) { return 0; }
            return std::thread::hardware_concurrency();
        }
        return 0;
    }

    /**
     * @brief 提交一个任务，返回 future。
     *
     * 单线程模式：当场执行任务，future 立即就绪。
     * 多线程模式：任务投递到 asio::thread_pool 由 worker 异步执行。
     */
    template <typename Callable, typename... Args>
    auto submit(Callable&& callable, Args&&... args) -> std::future<std::invoke_result_t<Callable, Args...>>
    {
        using Result = std::invoke_result_t<Callable, Args...>;
        auto task = std::make_shared<std::packaged_task<Result()>>(
            [func = std::forward<Callable>(callable),
             tup  = std::tuple<std::decay_t<Args>...>(std::forward<Args>(args)...)]() mutable -> Result
            { return std::apply(std::move(func), std::move(tup)); });

        std::future<Result> future = task->get_future();

        if constexpr (isMultithreaded())
        {
            asio::post(*m_pool, [task]() noexcept
            {
                try { (*task)(); }
                catch (const std::exception& ex)
                {
                    Logger::error("[ThreadPool] Worker caught exception: {}", ex.what());
                }
                catch (...)
                {
                    Logger::error("[ThreadPool] Worker caught unknown exception.");
                }
            });
        }
        else
        {
            (*task)();
        }
        return future;
    }

    /**
     * @brief 等待所有通过 submitDetached 提交的任务完成
     *
     * 单线程模式下为空操作。多线程模式下自旋等待直至所有飞行任务归零。
     *
     * 典型用途：在销毁持有 WorkerMailbox 的 UiRuntime 前调用，防止 UAF。
     */
    void wait() noexcept
    {
        if constexpr (isMultithreaded())
        {
            while (m_inFlight.load(std::memory_order_acquire) > 0)
            {
                std::this_thread::yield();
            }
        }
    }

    /**
     * @brief 提交一个"发完即忘"的任务（无 future 返回值）
     *
     * 性能略优于 submit()，适合不需要回收结果的纯副作用任务。
     * 多线程模式下跟踪 in-flight 计数，支持 wait() 等待所有此类任务完成。
     */
    template <typename Callable, typename... Args>
    void submitDetached(Callable&& callable, Args&&... args)
    {
        if constexpr (isMultithreaded())
        {
            m_inFlight.fetch_add(1, std::memory_order_relaxed);
            asio::post(*m_pool,
                [this,
                 func = std::forward<Callable>(callable),
                 tup  = std::tuple<std::decay_t<Args>...>(std::forward<Args>(args)...)]() mutable noexcept
                {
                    try { std::apply(std::move(func), std::move(tup)); }
                    catch (const std::exception& ex)
                    {
                        Logger::error("[ThreadPool] Worker caught exception: {}", ex.what());
                    }
                    catch (...)
                    {
                        Logger::error("[ThreadPool] Worker caught unknown exception.");
                    }
                    m_inFlight.fetch_sub(1, std::memory_order_release);
                });
        }
        else
        {
            std::invoke(std::forward<Callable>(callable), std::forward<Args>(args)...);
        }
    }

private:
    explicit ThreadPool(std::size_t workerCount)
    {
        if constexpr (isMultithreaded())
        {
            if (workerCount == 0)
            {
                const unsigned hwCount = std::thread::hardware_concurrency();
                workerCount = (hwCount <= 1U) ? 1U : static_cast<std::size_t>(hwCount - 1U);
            }
            m_pool = std::make_unique<asio::thread_pool>(workerCount);
            Logger::info("[ThreadPool] Started with {} worker threads (asio::thread_pool).", workerCount);
        }
        else
        {
            (void)workerCount;
            Logger::info("[ThreadPool] Running in single-threaded mode (UI_ENABLE_MULTITHREAD=OFF).");
        }
    }

    ~ThreadPool()
    {
        if constexpr (isMultithreaded())
        {
            if (m_pool)
            {
                m_pool->join(); // 等待所有任务完成后销毁 worker 线程
            }
        }
    }

    // asio::thread_pool 仅在多线程模式下构造
    std::unique_ptr<asio::thread_pool> m_pool;
    std::atomic<uint32_t> m_inFlight{0}; ///< submitDetached 的飞行任务计数，Wait() 使用
};

} // namespace ui::utils
