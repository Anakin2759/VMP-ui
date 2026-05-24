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
#include <string_view>

namespace
{

std::atomic<ui::log::Callback>& CallbackStorage()
{
    static std::atomic<ui::log::Callback> callback{nullptr};
    return callback;
}

std::atomic<ui::log::Level>& LevelStorage()
{
    static std::atomic<ui::log::Level> level{ui::log::Level::DEBUG};
    return level;
}

} // namespace

namespace ui::log
{

void SetLevel(Level level)
{
    LevelStorage().store(level, std::memory_order_relaxed);
}

void SetCallback(Callback callback)
{
    CallbackStorage().store(callback, std::memory_order_relaxed);
}

void SetFilePath(std::string_view path)
{
    ui::Logger::reconfigure(path);
}

void LogImpl(Level level, std::string_view message)
{
    if (level < LevelStorage().load(std::memory_order_relaxed)) return;

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
    if (auto* callback = CallbackStorage().load(std::memory_order_relaxed); callback != nullptr)
    {
        callback(level, message);
    }
}

} // namespace ui::log

