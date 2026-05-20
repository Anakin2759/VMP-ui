/**
 * EventLoop implementation
 */

#include "EventLoop.hpp"
#include <chrono>
namespace ui
{
EventLoop::EventLoop()
    : m_ioContext(std::make_unique<asio::io_context>()),
      m_workGuard(asio::make_work_guard(*m_ioContext)),
      m_frameTimer(*m_ioContext),
      m_running(false)
{
}

EventLoop::~EventLoop()
{
    quit();
}

void EventLoop::scheduleNextFrame()
{
    m_frameTimer.expires_after(std::chrono::milliseconds(16));
    m_frameTimer.async_wait(
        [this](const asio::error_code& errCode)
        {
            // errCode == operation_aborted 时表示 cancel() 被调用（quit 路径），直接返回
            if (errCode || !m_running.load()) return;

            if (m_defaultHandler) { m_defaultHandler(); }

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
    m_ioContext->stop();
}

} // namespace ui
