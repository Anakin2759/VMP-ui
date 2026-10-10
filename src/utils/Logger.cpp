#include "Logger.hpp"

#include <array>
#include <charconv>
#include <cstdlib>
#include <cstdio>
#include <limits>
#include <mutex>

#if defined(_WIN32)
#include <Windows.h>
#include <io.h>
#else
#include <unistd.h>
#endif

namespace ui::utils
{
namespace
{
// 进程内所有内部 Logger 共用；不协调宿主直接写 stderr 的行为。
std::mutex& StderrMutex()
{
    static std::mutex mutex;
    return mutex;
}

const char* LevelName(LogLevel level) noexcept
{
    switch (level)
    {
        case LogLevel::debug:
            return "debug";
        case LogLevel::info:
            return "info";
        case LogLevel::warn:
            return "warn";
        case LogLevel::error:
            return "error";
        case LogLevel::off:
            return "off";
    }
    return "unknown";
}

[[nodiscard]] bool SupportsColorTerminal() noexcept
{
#if defined(_WIN32)
    static std::once_flag enableVirtualTerminalOnce;
    static bool supported = false;
    std::call_once(enableVirtualTerminalOnce,
                   []
                   {
                       auto* const handle = GetStdHandle(STD_ERROR_HANDLE);
                       if (handle == INVALID_HANDLE_VALUE || handle == nullptr)
                       {
                           return;
                       }

                       DWORD mode = 0;
                       if (!GetConsoleMode(handle, &mode))
                       {
                           return;
                       }

                       if ((mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING) == 0U &&
                           !SetConsoleMode(handle, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING))
                       {
                           return;
                       }
                       supported = true;
                   });
    return supported;
#else
    const char* term = std::getenv("TERM");
    return ::isatty(fileno(stderr)) != 0 && term != nullptr && std::string_view(term) != "dumb";
#endif
}

[[nodiscard]] const char* LevelColor(LogLevel level) noexcept
{
    switch (level)
    {
        case LogLevel::debug:
            return "\x1b[36m";  // cyan
        case LogLevel::info:
            return "\x1b[32m";  // green
        case LogLevel::warn:
            return "\x1b[33m";  // yellow
        case LogLevel::error:
            return "\x1b[31m";  // red
        case LogLevel::off:
            return "";
    }
    return "";
}
}  // namespace

Logger::Logger(OutputCallback callback) noexcept
{
    SetCallback(std::move(callback));
}

void Logger::SetLevel(LogLevel level) noexcept
{
    m_level.store(level, std::memory_order_relaxed);
}

LogLevel Logger::GetLevel() const noexcept
{
    return m_level.load(std::memory_order_relaxed);
}

void Logger::SetCallback(OutputCallback callback) noexcept
{
    try
    {
        auto replacement = callback ? std::make_shared<OutputCallback>(std::move(callback)) : nullptr;
        {
            const std::scoped_lock lock(m_mutex);
            m_callback.swap(replacement);
        }
        // 用户捕获对象的析构也必须在配置锁外执行。
    }
    catch (...)
    {
        writeStderr(LogLevel::error, "<log callback configuration failed>", std::source_location::current());
    }
}

void Logger::output(LogLevel level, std::string_view message, std::source_location location) noexcept
{
    static thread_local bool inOutput = false;
    if (inOutput)
    {
        writeStderr(level, message, location);
        return;
    }

    inOutput = true;
    try
    {
        std::shared_ptr<OutputCallback> callback;
        {
            const std::scoped_lock lock(m_mutex);
            callback = m_callback;
        }
        if (callback)
        {
            (*callback)(level, message, location);
        }
        else
        {
            writeStderr(level, message, location);
        }
    }
    catch (...)
    {
        writeStderr(LogLevel::error, "<log callback/output failed>", location);
        writeStderr(level, message, location);
    }
    inOutput = false;
}

void Logger::writeStderr(LogLevel level, std::string_view message, std::source_location location) noexcept
{
    try
    {
        const std::scoped_lock lock(StderrMutex());
        const auto write = [](std::string_view text)
        {
            if (!text.empty())
            {
                std::fwrite(text.data(), 1, text.size(), stderr);
            }
        };
        const bool useColor = SupportsColorTerminal();
        if (useColor)
        {
            write(LevelColor(level));
        }
        std::array<char, std::numeric_limits<std::uint_least32_t>::digits10 + 1> line{};
        const auto converted = std::to_chars(line.data(), std::to_address(line.end()), location.line());
        write("[");
        write(LevelName(level));
        write("] ");
        write(message);
        write(" [");
        write(location.file_name());
        write(":");
        write(std::string_view(line.data(), static_cast<std::size_t>(converted.ptr - line.data())));
        write(" ");
        write(location.function_name());
        if (useColor)
        {
            write("\x1b[0m");
        }
        write("]\n");
        if (level >= LogLevel::warn)
        {
            std::fflush(stderr);
        }
    }
    catch (...)
    {
        // 最小降级路径不分配、不调用回调、不递归记录错误；输出不可用时尽力而为。
        return;
    }
}
}  // namespace ui::utils