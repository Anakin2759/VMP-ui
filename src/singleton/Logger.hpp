/**
 * ************************************************************************
 *
 * @file Logger.h
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2026-01-26
 * @version 0.1
 * @brief ui 模块日志系统封装
  - 基于 spdlog 实现的日志系统封装
  - 提供统一的日志接口，支持控制台和文件输出
  - 日志文件自动轮转，防止过大
  - 支持源码位置记录，便于调试
  - 线程安全的一次性初始化
 *
 * ************************************************************************
 * @copyright Copyright (c) 2026 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */
/**
 * ************************************************************************
 * @file Logger.h
 * @brief 基于 spdlog 实现的日志系统封装 (C++20 优化版)
 * ************************************************************************
 */
#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <memory>
#include <mutex>
#include <source_location>
#include <string>
#include <vector>
#include "SingletonBase.hpp"

namespace ui
{
/**
 * @brief 辅助结构体：用于在调用点自动捕获位置和格式化字符串
 */
struct LogLocation
{
    spdlog::string_view_t fmt;
    std::source_location loc;

    // 构造函数通过默认参数捕获调用处的位置
    template <typename T>
        requires std::convertible_to<T, spdlog::string_view_t>
    constexpr LogLocation(const T& s, std::source_location l = std::source_location::current()) : fmt(s), loc(l)
    {
    }
};

class Logger : public SingletonBase<Logger>
{
    static constexpr size_t MAX_LOG_FILE_SIZE = 1024 * 1024 * 5; // 5MB
    static constexpr size_t MAX_LOG_FILE_COUNT = 1;

    friend class SingletonBase<Logger>;

public:
    /**
     * @brief 警告日志
     */
    template <typename... Args>
    static void warn(LogLocation msg, Args&&... args)
    {
        getInstance().log_impl(spdlog::level::warn, msg, std::forward<Args>(args)...);
    }

    /**
     * @brief 信息日志
     */
    template <typename... Args>
    static void info(LogLocation msg, Args&&... args)
    {
        getInstance().log_impl(spdlog::level::info, msg, std::forward<Args>(args)...);
    }

    /**
     * @brief 错误日志
     */
    template <typename... Args>
    static void error(LogLocation msg, Args&&... args)
    {
        getInstance().log_impl(spdlog::level::err, msg, std::forward<Args>(args)...);
    }

    /**
     * @brief 调试日志
     */
    template <typename... Args>
    static void debug(LogLocation msg, Args&&... args)
    {
        getInstance().log_impl(spdlog::level::debug, msg, std::forward<Args>(args)...);
    }

private:
    Logger()
    {
        // 1. 创建控制台 sink
        auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        consoleSink->set_pattern("%^[%T] [%l] %n: %v%$");

        // 2. 创建文件 sink
        auto fileSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            "logs/pestmankill.log", MAX_LOG_FILE_SIZE, MAX_LOG_FILE_COUNT);
        fileSink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%s:%# %!] %v");

        // 3. 创建 logger
        std::vector<spdlog::sink_ptr> sinks{consoleSink, fileSink};
        m_logger = std::make_shared<spdlog::logger>("PestManKill", sinks.begin(), sinks.end());

        m_logger->set_level(spdlog::level::debug);
        m_logger->flush_on(spdlog::level::warn); // 警告及以上立即刷新
    }

    // 内部统一打印逻辑
    template <typename... Args>
    void log_impl(spdlog::level::level_enum lvl, const LogLocation& msg, Args&&... args)
    {
        m_logger->log(
            spdlog::source_loc{msg.loc.file_name(), static_cast<int>(msg.loc.line()), msg.loc.function_name()},
            lvl,
            fmt::runtime(msg.fmt), // 由于 fmt 被包裹在结构体中，需显式转为 runtime
            std::forward<Args>(args)...);
    }

    std::shared_ptr<spdlog::logger> m_logger;
};

// 辅助工具：路径规范化
inline std::string normalizePath(const char* path)
{
    std::string result = path ? path : "";
    for (auto& ch : result)
    {
        if (ch == '\\') ch = '/';
    }
    return result;
}

} // namespace ui