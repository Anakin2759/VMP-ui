

/**
 * Implementation for Application
 */

#include "ui/Application.hpp"
#include "ApplicationLifecycle.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <exception>
#include <memory>
#include <stdexcept>
#include <string>

#include "SystemManager.hpp"
#include "SdlEventWakeup.hpp"
#include "UiRuntime.hpp"
#include "UiRuntimeScope.hpp"
#include "SDL3/SDL_error.h"
#include "SDL3/SDL_hints.h"
#include "SDL3/SDL_init.h"
#include "SDL3/SDL_timer.h"
#include "TaskChain.hpp"

#include "common/AppConfig.hpp"
#include "common/Events.hpp"
#include "common/GlobalContext.hpp"
#include "core/EventLoop.hpp"
#include "core/PlatformWindow.hpp"
#include "ui/api/Factory.hpp"
#include "utils/Dispatcher.hpp"
#include "utils/Logger.hpp"

static constexpr uint32_t MAX_FRAME_TIME_MS = 250;  // 防止卡顿时长时间更新
static constexpr uint32_t LOOP_DELAY_MS = 1;        // 主循环延迟，防止100% CPU占用

namespace
{
void WriteStderr(const char* text) noexcept;
}  // namespace

namespace ui
{
class ApplicationImpl
{
   public:
    explicit ApplicationImpl(std::span<char*> arg);
    ApplicationImpl(const ApplicationImpl&) = delete;
    ApplicationImpl& operator=(const ApplicationImpl&) = delete;
    ApplicationImpl(ApplicationImpl&&) = delete;
    ApplicationImpl& operator=(ApplicationImpl&&) = delete;
    ~ApplicationImpl() noexcept;

    void onQuitRequested(events::QuitRequested& event);
    void onDropDownCloseRequested(const events::DropDownCloseRequested& event);
    void exec();
   private:
    std::unique_ptr<UiRuntime> m_runtime;            // 管理全局状态和资源
    std::unique_ptr<UiRuntimeScope> m_runtimeScope;  // 保持 legacy API 在应用生命周期内绑定当前 Runtime
    EventLoop m_eventLoop;
    detail::ApplicationLifecycle m_lifecycle;
    std::unique_ptr<detail::SdlEventWakeup> m_eventWakeup;

    // 核心 ECS 系统封装
    std::unique_ptr<SystemManager> m_systems;

    std::chrono::steady_clock::time_point m_lastUpdateTime = std::chrono::steady_clock::now();
};

ApplicationImpl::ApplicationImpl(std::span<char*> arg)  // NOLINT
    : m_runtime(std::make_unique<UiRuntime>()),
      m_runtimeScope(std::make_unique<UiRuntimeScope>(*m_runtime)),
      m_systems(std::make_unique<SystemManager>(m_runtime.get()))
{
    config::AppConfig::instance().parseCommandLine(arg);

    // 策略 A：旧路径配置只用于迁移提示，不再创建文件或执行轮转。
    if (const auto logPath = config::AppConfig::instance().logFilePath(); !logPath.empty())
    {
        m_runtime->logger().warn("内部日志文件配置已废弃（{}）；使用 stderr 或内部 Logger 回调，落盘由宿主负责",
                                 logPath);
    }

    if (auto backend = config::AppConfig::instance().preferredBackend(); !backend.empty())
    {
        m_runtime->logger().info("命令行指定 GPU 后端: {}", backend);
    }
    if (config::AppConfig::instance().platformScalingEnabled())
    {
#ifdef _WIN32
        (void)SDL_SetHint("SDL_WINDOWS_DPI_AWARENESS", "permonitorv2");
        (void)SDL_SetHint("SDL_WINDOWS_DPI_SCALING", "1");
#endif
    }

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
    {
        throw std::runtime_error(std::string("SDL_Init failed: ") + SDL_GetError());
    }
    m_lifecycle.ArmSdl([] { SDL_Quit(); });

    if (config::AppConfig::instance().platformScalingEnabled() &&
        config::AppConfig::instance().forcedPlatformScale() <= 0.0F)
    {
        config::AppConfig::instance().setPlatformUiScale(platform::GetPrimaryDisplayUiScale());
    }

    m_runtime->logger().info("平台 UI 缩放: {:.2f}, framebuffer 初始缩放由窗口实时测量",
                             config::AppConfig::instance().platformUiScale());
    m_runtime->logger().info("SDL 初始化成功");

    // 确保 FrameContext / StateContext 在系统初始化前可用
    (void)m_runtime->ensureContext<globalcontext::FrameContext>();
    (void)m_runtime->ensureContext<globalcontext::StateContext>();

    m_systems->registerAllHandlers();
    auto frameTick = tasks::FrameTick{*m_runtime, *m_systems};
    m_eventLoop.registerDefaultHandler(
        [this, frameTick]
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

            // M3 前沿用 EventLoop 现有 16 ms 调度及本地 dt；每个 callback 只执行一个 FrameTick。
            frameTick(dtMs);

            SDL_Delay(LOOP_DELAY_MS);
        });

    m_runtime->dispatcher().sink<events::QuitRequested>().connect<&ApplicationImpl::onQuitRequested>(*this);
    m_runtime->dispatcher().sink<events::DropDownCloseRequested>().connect<&ApplicationImpl::onDropDownCloseRequested>(*this);
    m_eventWakeup = std::make_unique<detail::SdlEventWakeup>(m_eventLoop);
}

ApplicationImpl::~ApplicationImpl() noexcept
{
    m_eventWakeup.reset();
    try
    {
        m_runtime->dispatcher().sink<events::QuitRequested>().disconnect<&ApplicationImpl::onQuitRequested>(*this);
        m_runtime->dispatcher().sink<events::DropDownCloseRequested>().disconnect<
            &ApplicationImpl::onDropDownCloseRequested>(*this);
        m_systems->unregisterAllHandlers();
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

    m_lifecycle.Shutdown([this] { m_systems.reset(); });
}

void ApplicationImpl::onQuitRequested([[maybe_unused]] events::QuitRequested& /*event*/)
{
    m_eventLoop.quit();
}

void ApplicationImpl::onDropDownCloseRequested(const events::DropDownCloseRequested& event)
{
    factory::CloseDropDownPopup(event.entity);
}

void ApplicationImpl::exec()
{
    m_eventLoop.exec();
}

Application::Application(std::span<char*> arg) : m_impl(std::make_unique<ApplicationImpl>(arg))
{
}

Application::~Application() noexcept = default;

void Application::onQuitRequested([[maybe_unused]] ui::events::QuitRequested& event)
{
    m_impl->onQuitRequested(event);
}

void Application::exec()
{
    m_impl->exec();
}

}  // namespace ui

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
}  // namespace
