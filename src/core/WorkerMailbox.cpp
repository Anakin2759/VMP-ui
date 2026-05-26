/**
 * ************************************************************************
 *
 * @file WorkerMailbox.cpp
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2026-05-26
 * @version 0.1
 * @brief WorkerMailbox::flush() 实现（从 .hpp 拆出，减少编译时间膨胀）
 *
 * ************************************************************************
 * @copyright Copyright (c) 2026 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */

#include "WorkerMailbox.hpp"

#include <exception>

#include "../singleton/Logger.hpp"

namespace ui
{

void WorkerMailbox::flush(entt::registry& registry)
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
            std::get<worker::RegistryCommand>(cmd).apply(registry);
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

} // namespace ui
