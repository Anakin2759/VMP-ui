/**
 * EventLoop implementation
 */

#include "EventLoop.hpp"
#include "asio/io_context.hpp"
#include "asio/executor_work_guard.hpp"
#include "asio/error_code.hpp"
#include <chrono>
#include <cstdio>
#include <cstring>
#include <exception>
#include <memory>
namespace ui
{
namespace
{
void WriteStderr(const char* text) noexcept
{
    if (text == nullptr)
    {
        return;
    }

    const auto textSize = std::strlen(text);
    if (std::fwrite(text, 1U, textSize, stderr) != textSize)
    {
        std::clearerr(stderr);
    }
}
} // namespace

EventLoop::EventLoop()
    : m_ioContext(std::make_unique<asio::io_context>()), m_workGuard(asio::make_work_guard(*m_ioContext)),
      m_frameTimer(*m_ioContext), m_running(false)
{
}

EventLoop::~EventLoop() noexcept
{
    try
    {
        quit();
    }
    catch (const std::exception& exception)
    {
        WriteStderr("[EventLoop] destructor cleanup failed: ");
        WriteStderr(exception.what());
        WriteStderr("\n");
    }
    catch (...)
    {
        WriteStderr("[EventLoop] destructor cleanup failed with unknown exception\n");
    }
}

void EventLoop::scheduleNextFrame()
{
    m_frameTimer.expires_after(std::chrono::milliseconds(16));
    m_frameTimer.async_wait(
        [this](const asio::error_code& errCode)
        {
            // errCode == operation_aborted 时表示 cancel() 被调用（quit 路径），直接返回
            if (errCode || !m_running.load()) return;

            if (m_defaultHandler)
            {
                m_defaultHandler();
            }

            scheduleNextFrame();
        });
}

void EventLoop::exec()
{
    m_running.store(true);
    scheduleNextFrame();
    // run() 阻塞直到 io_context 被 stop()；
    // invoke() 投递的任务与帧回调共享同一 io_context，均在此消费，无需轮询。
    m_ioContext->run();
}

void EventLoop::quit()
{
    if (!m_running.exchange(false)) return;

    m_frameTimer.cancel();
    m_workGuard.reset();
    // io_context 在 work guard 释放后自然 drain；
    // 不调用 stop()，避免丢弃未消费的 GPU 帧 handler。
}

} // namespace ui
