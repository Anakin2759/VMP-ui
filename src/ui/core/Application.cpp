/**
 * Implementation for Application
 */

#include "Application.hpp"
#include "singleton/Dispatcher.hpp"
#include "singleton/Logger.hpp"
#include <SDL3/SDL.h>
#include "TaskChain.hpp"
#include "../common/GlobalContext.hpp"
#include <algorithm>
#include <cstdint>
static constexpr uint32_t DEFAULT_WIDTH = 800;
static constexpr uint32_t DEFAULT_HEIGHT = 600;
static constexpr uint32_t FRAME_DELAY_MS = 16;     // ~60 FPS
static constexpr uint32_t RENDER_DELAY_MS = 0;     // ~60 FPS
static constexpr uint32_t MAX_FRAME_TIME_MS = 250; // 防止卡顿时长时间更新
static constexpr uint32_t LOOP_DELAY_MS = 1;       // 主循环延迟，防止100% CPU占用
namespace ui
{
Application::Application(std::span<char*> arg) // NOLINT
    : m_runtimeScope(m_runtime)
{
    (void)arg;

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
    {
        throw std::runtime_error(std::string("SDL_Init failed: ") + SDL_GetError());
    }

    Logger::info("SDL 初始化成功");
    Registry::ctx().emplace<globalcontext::FrameContext>();
    Registry::ctx().emplace<globalcontext::StateContext>();

    m_systems.registerAllHandlers();
    auto taskChain = tasks::QueuedTask{} | tasks::InputTask{} | tasks::RenderTask{};
    m_eventLoop.registerDefaultHandler(
        [this, taskChain]() mutable
        {
            auto now = std::chrono::steady_clock::now();

            // 1. 使用 duration_cast 显式转换精度，并直接获取 count
            // 建议先转为默认的有符号 milliseconds，再 count() 之后 static_cast
            auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastUpdateTime);
            auto dtMs = static_cast<uint32_t>(diff.count());

            // 2. 更新时间点
            m_lastUpdateTime = now;

            // 3. 安全保护
            dtMs = std::min(dtMs, MAX_FRAME_TIME_MS);

            // 4. 执行任务链
            taskChain(dtMs);

            SDL_Delay(LOOP_DELAY_MS);
        });

    Dispatcher::Sink<ui::events::QuitRequested>().connect<&Application::onQuitRequested>(*this);
}

Application::~Application()
{
    Dispatcher::Sink<ui::events::QuitRequested>().disconnect<&Application::onQuitRequested>(*this);
    m_systems.unregisterAllHandlers();
    SDL_Quit();
}

void Application::onQuitRequested([[maybe_unused]] ui::events::QuitRequested& /*event*/)
{
    m_eventLoop.quit();
}

void Application::exec()
{
    m_eventLoop.exec();
}

} // namespace ui
