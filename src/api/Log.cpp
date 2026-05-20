/**
 * ************************************************************************
 *
 * @file Log.cpp
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2026-03-23
 * @version 0.3
 * @brief PmkUi 公共日志接口实现
 *
 * ************************************************************************
 * @copyright Copyright (c) 2026 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */
#include "Log.hpp"

#include "../singleton/Logger.hpp"

#include <atomic>

namespace
{

std::atomic<ui::log::Callback> g_callback{nullptr};
std::atomic<ui::log::Level>    g_level{ui::log::Level::DEBUG};

} // namespace

namespace ui::log
{

void SetLevel(Level level)
{
    g_level.store(level, std::memory_order_relaxed);
}

void SetCallback(Callback callback)
{
    g_callback.store(callback, std::memory_order_relaxed);
}

void LogImpl(Level level, std::string_view message)
{
    if (level < g_level.load(std::memory_order_relaxed)) return;

    // 转发到内部 spdlog Logger
    switch (level)
    {
    case Level::DEBUG:    ui::Logger::debug("{}", message);            break;
    case Level::INFO:     ui::Logger::info("{}", message);             break;
    case Level::WARNING:  ui::Logger::warn("{}", message);             break;
    case Level::ERROR:    ui::Logger::error("{}", message);            break;
    case Level::CRITICAL: ui::Logger::error("[CRITICAL] {}", message); break;
    default: break;
    }

    // 如果有外部回调也一并通知
    if (auto* cb = g_callback.load(std::memory_order_relaxed); cb != nullptr)
    {
        cb(level, message);
    }
}

} // namespace ui::log

