/**
 * Implementation for Application
 */

#include "Application.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <exception>
#include <stdexcept>
#include <string>

#include "RuntimeFacade.hpp"
#include "SDL3/SDL_init.h"
#include "SDL3/SDL_error.h"
#include "SDL3/SDL_timer.h"
#include "TaskChain.hpp"

#include "../api/Factory.hpp"
#include "../common/GlobalContext.hpp"
#include "../singleton/Logger.hpp"
#include "common/Events.hpp"
#include "singleton/Dispatcher.hpp"

static constexpr uint32_t MAX_FRAME_TIME_MS = 250; // 防止卡顿时长时间更新
static constexpr uint32_t LOOP_DELAY_MS = 1;       // 主循环延迟，防止100% CPU占用

namespace
{
void OnDropDownCloseRequested(const ui::events::DropDownCloseRequested& event)
{
    ui::factory::CloseDropDownPopup(event.entity);
}

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

namespace ui
{
Application::Application(std::span<char*> arg) // NOLINT
    : m_runtimeScope(m_runtime)
{
    (void)arg;
    auto& runtime = RuntimeFacade::current();

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
    {
        throw std::runtime_error(std::string("SDL_Init failed: ") + SDL_GetError());
    }

    Logger::info("SDL 初始化成功");
    (void)runtime.ensureContext<globalcontext::FrameContext>();
    (void)runtime.ensureContext<globalcontext::StateContext>();

    m_systems.registerAllHandlers();
    auto taskChain = tasks::QueuedTask{} | tasks::InputTask{.systems = &m_systems} | tasks::RenderTask{};
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

    runtime.sink<ui::events::QuitRequested>().connect<&Application::onQuitRequested>(*this);
    Dispatcher::Sink<events::DropDownCloseRequested>().connect<&OnDropDownCloseRequested>();
}

Application::~Application() noexcept
{
    try
    {
        RuntimeFacade::current().sink<ui::events::QuitRequested>().disconnect<&Application::onQuitRequested>(*this);
        Dispatcher::Sink<events::DropDownCloseRequested>().disconnect<&OnDropDownCloseRequested>();
        m_systems.unregisterAllHandlers();
        SDL_Quit();
    }
    catch (const std::exception& exception)
    {
        WriteStderr("[Application] destructor cleanup failed: ");
        WriteStderr(exception.what());
        WriteStderr("\n");
    }
    catch (...)
    {
        WriteStderr("[Application] destructor cleanup failed with unknown exception\n");
    }
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
