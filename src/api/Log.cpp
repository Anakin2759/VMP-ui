/**
 * ************************************************************************
 *
 * @file Log.cpp
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2026-03-23
 * @version 0.3
 * @brief VMP-ui 公共日志接口实现
 *
 * ************************************************************************
 * @copyright Copyright (c) 2026 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */
#include "ui/api/Log.hpp"

#include <atomic>
#include <string_view>

// Log.hpp 虽已 #undef ERROR，但其后的 UiRuntime.hpp/Logger.hpp 会经 spdlog 重新引入
// Windows wingdi.h 的 ERROR 宏，导致下方 `case Level::ERROR:` 被展开为 `case Level::0:`。
// 因此在对所有头完成包含之后再次消除该宏。
#ifdef ERROR
#undef ERROR
#endif

namespace
{

std::atomic<ui::log::Callback>& CallbackStorage()
{
    static std::atomic<ui::log::Callback> callback{nullptr};
    return callback;
}

std::atomic<ui::log::Level>& LevelStorage()
{
#ifdef NDEBUG
    static std::atomic<ui::log::Level> level{ui::log::Level::NO_DEBUG};
#else
    static std::atomic<ui::log::Level> level{ui::log::Level::DEBUG};
#endif
    return level;
}

}  // namespace

namespace ui::log
{

void SetLevel(Level level)
{
    SetDebugLevel(level);
}

void SetDebugLevel(Level level)
{
    LevelStorage().store(level, std::memory_order_relaxed);
}

void SetCallback(Callback callback)
{
    CallbackStorage().store(callback, std::memory_order_relaxed);
}

void SetFilePath(std::string_view path)
{
    // 公共日志不再依赖 Runtime；文件输出由外部 Callback 或应用自身负责。
    static_cast<void>(path);
}

void LogImpl(Level level, std::string_view message)
{
    const auto configuredLevel = LevelStorage().load(std::memory_order_relaxed);
    if (configuredLevel == Level::NO_DEBUG || level < configuredLevel)
    {
        return;
    }

    // 公共日志只通知外部回调，不转发到 Runtime 内部 Logger。
    if (auto* callback = CallbackStorage().load(std::memory_order_relaxed); callback != nullptr)
    {
        callback(level, message);
    }
}

}  // namespace ui::log
