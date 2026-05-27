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
 * 向库内部日志系统写入。支持 std::format 格式化参数。
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

#include "Chains.hpp"

#ifdef ERROR
#undef ERROR
#endif

namespace ui::log
{

// ---- 日志级别 -------------------------------------------------------

enum class Level : std::uint8_t
{
    NO_DEBUG = 0,
    DEBUG = 1,
    INFO = 2,
    WARNING = 3,
    ERROR = 4,
    CRITICAL = 5
};

/// 自定义回调类型：接收日志级别与已格式化的消息
using Callback = void (*)(Level, std::string_view);

// ---- 配置接口 ------------------------------------------------------

/// 设置过滤级别（低于该级别的日志不会转发到回调）
void SetLevel(Level level);

/// 设置自定义回调（传 nullptr 则清除）
void SetCallback(Callback callback);

/// 设置日志文件路径（替换当前文件 sink，立即生效）
void SetFilePath(std::string_view path);

// ---- 内部实现桥接（由 Log.cpp 提供，模板函数调用）------------------

void LogImpl(Level level, std::string_view message);

// ---- string_view 重载（运行时字符串，无 consteval 约束）-------------

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

// ---- 格式化日志接口（编译期格式字符串）------------------------------

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

} // namespace ui::log

// ---- ui::chains 日志动作（实体链式操作后追加日志输出）--------------
// 用法：entity | Size(100, 40) | LogInfo("按钮已创建");
//       entity | Show() | LogInfo("entity {} shown", alias);

namespace ui::chains
{

inline auto LogDebug(std::string_view message)
{
    return Chain{[msg = std::string(message)](::entt::entity) { ui::log::Debug(msg); }};
}

inline auto LogInfo(std::string_view message)
{
    return Chain{[msg = std::string(message)](::entt::entity) { ui::log::Info(msg); }};
}

inline auto LogWarning(std::string_view message)
{
    return Chain{[msg = std::string(message)](::entt::entity) { ui::log::Warning(msg); }};
}

inline auto LogError(std::string_view message)
{
    return Chain{[msg = std::string(message)](::entt::entity) { ui::log::Error(msg); }};
}

inline auto LogCritical(std::string_view message)
{
    return Chain{[msg = std::string(message)](::entt::entity) { ui::log::Critical(msg); }};
}

template <typename... Args>
inline auto LogDebug(std::format_string<Args...> fmt, Args&&... args)
{
    return Chain{[msg = std::format(fmt, std::forward<Args>(args)...)](::entt::entity) { ui::log::Debug(msg); }};
}

template <typename... Args>
inline auto LogInfo(std::format_string<Args...> fmt, Args&&... args)
{
    return Chain{[msg = std::format(fmt, std::forward<Args>(args)...)](::entt::entity) { ui::log::Info(msg); }};
}

template <typename... Args>
inline auto LogWarning(std::format_string<Args...> fmt, Args&&... args)
{
    return Chain{[msg = std::format(fmt, std::forward<Args>(args)...)](::entt::entity) { ui::log::Warning(msg); }};
}

template <typename... Args>
inline auto LogError(std::format_string<Args...> fmt, Args&&... args)
{
    return Chain{[msg = std::format(fmt, std::forward<Args>(args)...)](::entt::entity) { ui::log::Error(msg); }};
}

template <typename... Args>
inline auto LogCritical(std::format_string<Args...> fmt, Args&&... args)
{
    return Chain{[msg = std::format(fmt, std::forward<Args>(args)...)](::entt::entity) { ui::log::Critical(msg); }};
}

} // namespace ui::chains
