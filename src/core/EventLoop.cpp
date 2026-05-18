/**
 * EventLoop implementation
 */

#include "EventLoop.hpp"
#include "SDL3/SDL_video.h"
namespace ui
{
EventLoop::EventLoop()
    : m_ioContext(std::make_unique<asio::io_context>()), m_workGuard(asio::make_work_guard(*m_ioContext)),
      m_running(false)
{
}

EventLoop::~EventLoop()
{
    quit();
}

void EventLoop::exec()
{
    m_running.store(true);
    while (m_running.load())
    {
        m_ioContext->poll();
        if (m_ioContext->stopped()) m_ioContext->restart();

        if (m_defaultHandler)
        {
            m_defaultHandler();
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void EventLoop::quit()
{
    if (!m_running.load()) return;

    m_running.store(false);
    m_workGuard.reset();
}

} // namespace ui
