/**
 * ************************************************************************
 *
 * @file Logger.hpp
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2026-01-26
 * @version 0.1
 * @brief ui 模块日志系统封装
 *
 * ************************************************************************
 * @copyright Copyright (c) 2026 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */
#pragma once

#include <atomic>
#include <concepts>
#include <format>
#include <functional>
#include <memory>
#include <mutex>
#include <source_location>
#include <string_view>
#include <cstdint>
#include <utility>

namespace ui::utils
{

enum class LogLevel : std::uint8_t
{
    debug,
    info,
    warn,
    error,
    off
};

/**
 * @brief 辅助结构体：用于在调用点自动捕获位置和格式化字符串
 */
struct LogLocation
{
    std::string_view fmt;
    std::source_location loc;

    template <typename T>
        requires std::convertible_to<T, std::string_view>
    // NOLINTNEXTLINE(google-explicit-constructor) 保留 Logger::info("...") 调用点位置捕获语义。
    constexpr LogLocation(const T& format, std::source_location location = std::source_location::current())
        : fmt(format), loc(location)
    {
    }
};

/**
 * @brief 常规 Logger 类（非单例），每个实例维护独立的输出配置
 */
class Logger
{
   public:
    using OutputCallback = std::move_only_function<void(LogLevel, std::string_view, std::source_location)>;

    /**
        * @brief 默认输出至 stderr，文件与轮转由宿主负责。
     */
    Logger() = default;

    /**
     * @brief 使用外部同步回调构造 Logger，主要用于测试或宿主接管输出。
     */
    explicit Logger(OutputCallback callback) noexcept;

    void SetLevel(LogLevel level) noexcept;
    [[nodiscard]] LogLevel GetLevel() const noexcept;

    /**
     * @brief 回调接管输出；空回调恢复 stderr。配置失败保留旧回调。
     * 同步运行于调用线程，允许并发调用，捕获状态须自行同步。
     * 消息视图仅当次有效；替换不等待在途回调，其所有权保留至调用结束。
     * 允许回调内替换配置；同线程嵌套日志跳过所有内部 Logger 回调，改写 stderr。
     * Logger 销毁前宿主须停止并等待所有调用。构造回调参数本身的异常由调用方负责。
     */
    void SetCallback(OutputCallback callback) noexcept;

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    Logger(Logger&&) = delete;
    Logger& operator=(Logger&&) = delete;
    ~Logger() = default;

    /**
     * @brief 警告日志
     */
    template <typename... Args>
    void warn(LogLocation msg, Args&&... args) noexcept
    {
        log(LogLevel::warn, msg, std::forward<Args>(args)...);
    }

    /**
     * @brief 信息日志
     */
    template <typename... Args>
    void info(LogLocation msg, Args&&... args) noexcept
    {
        log(LogLevel::info, msg, std::forward<Args>(args)...);
    }

    /**
     * @brief 错误日志
     */
    template <typename... Args>
    void error(LogLocation msg, Args&&... args) noexcept
    {
        log(LogLevel::error, msg, std::forward<Args>(args)...);
    }

    /**
     * @brief 调试日志
     */
    template <typename... Args>
    void debug(LogLocation msg, Args&&... args) noexcept
    {
        log(LogLevel::debug, msg, std::forward<Args>(args)...);
    }

   private:
    std::shared_ptr<OutputCallback> m_callback;
    std::atomic<LogLevel> m_level{LogLevel::debug};
    mutable std::mutex m_mutex;

    void output(LogLevel level, std::string_view message, std::source_location location) noexcept;
    static void writeStderr(LogLevel level, std::string_view message, std::source_location location) noexcept;

    template <typename... Args>
    // NOLINTNEXTLINE(cppcoreguidelines-missing-std-forward) make_format_args 必须接收命名左值。
    void log(LogLevel level, const LogLocation& msg, Args&&... args) noexcept
    {
        if (level < GetLevel())
        {
            return;
        }

        try
        {
            const auto message = std::vformat(msg.fmt, std::make_format_args(args...));
            output(level, message, msg.loc);
        }
        catch (...)
        {
            writeStderr(level, "<log formatting failed>", msg.loc);
        }
    }
};

}  // namespace ui::utils
