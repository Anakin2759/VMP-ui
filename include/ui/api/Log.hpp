/**
 * ************************************************************************
 *
 * @file Log.hpp
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2026-03-23
 * @version 0.3
 * @brief VMP-ui 公共日志接口
 *
 * 客户端可通过 ui::log::SetCallback 注入自定义输出函数，
 * 也可直接调用 ui::log::Info / Debug / Warning / Error / Critical
 * 向外部诊断回调发送消息。该接口不依赖 UiRuntime，支持 std::format
 * 格式化参数。
 *
 * ************************************************************************
 * @copyright Copyright (c) 2026 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */
#pragma once

#include <cstdint>
#include <format>
#include <string>
#include <string_view>

#include "ui/api/Chains.hpp"

#ifdef ERROR
#undef ERROR
#endif

namespace ui::log
{
enum class Level : std::uint8_t
{
    NO_DEBUG = 0,
    DEBUG = 1,
    INFO = 2,
    WARNING = 3,
    ERROR = 4,
    CRITICAL = 5
};

using Callback = void (*)(Level, std::string_view);

/// 兼容接口：设置库诊断等级，等价于 SetDebugLevel。
void SetLevel(Level level);
/// 设置库外部诊断输出等级；不依赖 UiRuntime 或线程局部上下文。
void SetDebugLevel(Level level);
/// 设置外部诊断回调；传入 nullptr 可解除回调。
void SetCallback(Callback callback);
void SetFilePath(std::string_view path);
void LogImpl(Level level, std::string_view message);

inline void Debug(std::string_view message)
{
    LogImpl(Level::DEBUG, message);
}
inline void Info(std::string_view message)
{
    LogImpl(Level::INFO, message);
}
inline void Warning(std::string_view message)
{
    LogImpl(Level::WARNING, message);
}
inline void Error(std::string_view message)
{
    LogImpl(Level::ERROR, message);
}
inline void Critical(std::string_view message)
{
    LogImpl(Level::CRITICAL, message);
}

template <typename... Args>
inline void Debug(std::format_string<Args...> fmt, Args&&... args)
{
    LogImpl(Level::DEBUG, std::format(fmt, std::forward<Args>(args)...));
}
template <typename... Args>
inline void Info(std::format_string<Args...> fmt, Args&&... args)
{
    LogImpl(Level::INFO, std::format(fmt, std::forward<Args>(args)...));
}
template <typename... Args>
inline void Warning(std::format_string<Args...> fmt, Args&&... args)
{
    LogImpl(Level::WARNING, std::format(fmt, std::forward<Args>(args)...));
}
template <typename... Args>
inline void Error(std::format_string<Args...> fmt, Args&&... args)
{
    LogImpl(Level::ERROR, std::format(fmt, std::forward<Args>(args)...));
}
template <typename... Args>
inline void Critical(std::format_string<Args...> fmt, Args&&... args)
{
    LogImpl(Level::CRITICAL, std::format(fmt, std::forward<Args>(args)...));
}
}  // namespace ui::log

namespace ui::chains
{
inline auto LogDebug(std::string_view message)
{
    return Chain{[msg = std::string(message)](ui::entity) { ui::log::Debug(msg); }};
}
inline auto LogInfo(std::string_view message)
{
    return Chain{[msg = std::string(message)](ui::entity) { ui::log::Info(msg); }};
}
inline auto LogWarning(std::string_view message)
{
    return Chain{[msg = std::string(message)](ui::entity) { ui::log::Warning(msg); }};
}
inline auto LogError(std::string_view message)
{
    return Chain{[msg = std::string(message)](ui::entity) { ui::log::Error(msg); }};
}
inline auto LogCritical(std::string_view message)
{
    return Chain{[msg = std::string(message)](ui::entity) { ui::log::Critical(msg); }};
}

template <typename... Args>
inline auto LogDebug(std::format_string<Args...> fmt, Args&&... args)
{
    return Chain{[msg = std::format(fmt, std::forward<Args>(args)...)](ui::entity) {
        ui::log::Debug(msg);
    }};
}
template <typename... Args>
inline auto LogInfo(std::format_string<Args...> fmt, Args&&... args)
{
    return Chain{[msg = std::format(fmt, std::forward<Args>(args)...)](ui::entity) {
        ui::log::Info(msg);
    }};
}
template <typename... Args>
inline auto LogWarning(std::format_string<Args...> fmt, Args&&... args)
{
    return Chain{[msg = std::format(fmt, std::forward<Args>(args)...)](ui::entity) {
        ui::log::Warning(msg);
    }};
}
template <typename... Args>
inline auto LogError(std::format_string<Args...> fmt, Args&&... args)
{
    return Chain{[msg = std::format(fmt, std::forward<Args>(args)...)](ui::entity) {
        ui::log::Error(msg);
    }};
}
template <typename... Args>
inline auto LogCritical(std::format_string<Args...> fmt, Args&&... args)
{
    return Chain{[msg = std::format(fmt, std::forward<Args>(args)...)](ui::entity) {
        ui::log::Critical(msg);
    }};
}
}  // namespace ui::chains