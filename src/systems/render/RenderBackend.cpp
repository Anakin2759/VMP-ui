/**
 * @file RenderBackend.cpp
 * @brief RenderSystem — 后端初始化与设备生命周期
 *
 * 包含：构造/析构/移动、cleanup()、ensureInitialized()、
 *       tryInitializeFallback()、onWindowsGraphicsContext*()
 */

#include "systems/RenderSystem.hpp"
#include "RenderSystemImpl.hpp"
#include "core/UiRuntime.hpp"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
#include <memory>
#include "ui/Result.hpp"
#include "common/AppConfig.hpp"
#include <string_view>
#include "utils/Logger.hpp"
#include "ui/ErrorCodes.hpp"
#include "managers/FontManager.hpp"
#include "managers/ImageManager.hpp"
#include "managers/BatchManager.hpp"
#include "managers/PipelineCache.hpp"
#include "managers/TextTextureCache.hpp"
#include "SDL3/SDL_stdinc.h"
#include <utility>
#include "common/Events.hpp"
#include "common/components/Window.hpp"
#include <cstddef>
#include <cstdint>
#include <vector>
#include "SDL3/SDL_video.h"
#include "SDL3/SDL_gpu.h"
#include "SDL3/SDL_init.h"
#include <exception>
#include <stdexcept>
#include "managers/CommandBuffer.hpp"
#include "utils/Registry.hpp"
#include "renderers/FallbackBackendRenderer.hpp"
#include "managers/IconManager.hpp"
#include "managers/ResourceProvider.hpp"
#include "common/CustomizationPoints.hpp"

#ifndef UI_ASSETS_DIR
#define UI_ASSETS_DIR "assets"
#endif

namespace
{
/**
 * @brief 是否将环境变量值视为 true
 * @param value 环境变量的值
 * @return true 如果值表示 true
 * @return false 如果值表示 false
 */
bool IsTruthyEnvironmentValue(const char* value)
{
    if (value == nullptr)
    {
        return false;
    }

    std::string normalized(value);
    for (char& char1 : normalized)
    {
        char1 = static_cast<char>(std::tolower(static_cast<unsigned char>(char1)));
    }

    return normalized == "1" || normalized == "true" || normalized == "on" || normalized == "yes" || normalized == "y";
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

std::shared_ptr<const ui::managers::IResourceProvider> GetUiResourceProvider(ui::utils::Logger& logger)
{
    static_cast<void>(logger);
    return ui::managers::GetDefaultUiResourceProvider();
}

ui::Result<ui::managers::BinaryResource> LoadUiResource(std::string_view resourcePath, ui::utils::Logger& logger)
{
    const auto resourceProvider = GetUiResourceProvider(logger);
    if (resourceProvider == nullptr)
    {
        logger.error("[RenderSystem] UI resource provider unavailable");
        return ui::Err(ui::UiErrc::BACKEND_UNAVAILABLE, std::string(resourcePath));
    }

    return ui::cpo::load_binary_resource(*resourceProvider, resourcePath);
}

}  // namespace

namespace ui::systems
{
namespace
{
void resetGpuInitializationNode(RenderSystemImpl& impl, render::GpuInitializationTransaction::Node node)
{
    switch (node)
    {
        case render::GpuInitializationTransaction::Node::COMMAND_BUFFER:
            impl.m_commandBuffer.reset();
            break;
        case render::GpuInitializationTransaction::Node::TEXT_TEXTURE_CACHE:
            impl.m_textTextureCache.reset();
            break;
        case render::GpuInitializationTransaction::Node::PIPELINE_CACHE:
            impl.m_pipelineCache.reset();
            break;
    }
}
}  // namespace

const RenderSystem::RenderStats& RenderSystem::getStats() const
{
    return m_impl->m_stats;
}

void RenderSystem::registerHandlersImpl()
{
    m_logger->info("[RenderSystem] Registering event handlers");
    m_disp->sink<events::WindowGraphicsContextSetEvent>().connect<&RenderSystem::onWindowsGraphicsContextSet>(*this);
    m_disp->sink<events::WindowGraphicsContextUnsetEvent>().connect<&RenderSystem::onWindowsGraphicsContextUnset>(
        *this);
    m_disp->sink<events::UpdateRendering>().connect<&RenderSystem::update>(*this);
    m_logger->info("[RenderSystem] Event handlers registered successfully");
}

void RenderSystem::unregisterHandlersImpl()
{
    m_disp->sink<events::WindowGraphicsContextSetEvent>().disconnect<&RenderSystem::onWindowsGraphicsContextSet>(*this);
    m_disp->sink<events::WindowGraphicsContextUnsetEvent>().disconnect<&RenderSystem::onWindowsGraphicsContextUnset>(
        *this);
    m_disp->sink<events::UpdateRendering>().disconnect<&RenderSystem::update>(*this);
}

interface::SystemPhase RenderSystem::getPhase()
{
    return interface::SystemPhase::RENDER;
}

RenderSystem::RenderSystem(UiRuntime& runtime)
    : m_reg(&runtime.registry()),
      m_disp(&runtime.dispatcher()),
      m_logger(&runtime.logger()),
      m_impl(std::make_unique<RenderSystemImpl>(
#ifdef UI_FORCE_CPU_RENDER
          true
#else
          IsTruthyEnvironmentValue(SDL_getenv("VMPUI_FORCE_FALLBACK"))
#endif
          , *m_logger))
{
    if (m_impl->m_forceFallback)
    {
#ifdef UI_FORCE_CPU_RENDER
        m_logger->warn(
            "[RenderSystem] 编译选项 UI_FORCE_CPU_RENDER 已启用，强制使用 CPU software 后端");
#else
        m_logger->warn(
            "[RenderSystem] 检测到环境变量 VMPUI_FORCE_FALLBACK，强制启用 SDL_Renderer fallback 后端");
#endif
    }
}

RenderSystemImpl::RenderSystemImpl(bool forceFallback, utils::Logger& logger)
        : m_deviceManager(std::make_unique<managers::DeviceManager>(logger)),
            m_fontManager(std::make_unique<managers::FontManager>(logger)),
            m_iconManager(std::make_unique<managers::IconManager>(m_deviceManager.get(), logger)),
            m_imageManager(std::make_unique<managers::ImageManager>(m_deviceManager.get(), logger)),
      m_batchManager(std::make_unique<managers::BatchManager>()),
      m_fallbackWhiteTextureTag(std::bit_cast<SDL_GPUTexture*>(&m_fallbackWhiteTextureCookie)),
      m_forceFallback(forceFallback)
{
}

RenderSystem::~RenderSystem()
{
    try
    {
        cleanup();
    }
    catch (...)
    {
        WriteStderr("[RenderSystem] destructor cleanup failed\n");
    }
}

RenderSystem::RenderSystem(RenderSystem&& other) noexcept
    : m_reg(other.m_reg), m_disp(other.m_disp), m_logger(other.m_logger), m_impl(std::move(other.m_impl))
{
    other.m_reg = nullptr;
    other.m_disp = nullptr;
    other.m_logger = nullptr;
}

RenderSystem& RenderSystem::operator=(RenderSystem&& other) noexcept
{
    if (this != &other)
    {
        try
        {
            cleanup();
        }
        catch (...)
        {
            WriteStderr("[RenderSystem] move assignment cleanup failed\n");
        }

        m_reg = other.m_reg;
        other.m_reg = nullptr;
        m_disp = other.m_disp;
        other.m_disp = nullptr;
        m_logger = other.m_logger;
        other.m_logger = nullptr;
        m_impl = std::move(other.m_impl);
    }
    return *this;
}

void RenderSystem::onWindowsGraphicsContextSet(const events::WindowGraphicsContextSetEvent& event)
{
    m_logger->info("[RenderSystem] 收到窗口图形上下文设置事件，实体ID: {}",
                                           static_cast<uint32_t>(event.entity));
    ensureInitialized();
    uint32_t windowID = m_reg->get<components::Window>(event.entity).windowID;
    SDL_Window* sdlWindow = SDL_GetWindowFromID(windowID);
    if (sdlWindow == nullptr)
    {
        m_logger->warn("[RenderSystem] 无法获取 SDL_Window (ID: {})", windowID);
        return;
    }

    if (m_impl->m_useFallback)
    {
        if (!tryInitializeFallback(sdlWindow))
        {
            m_logger->error("[RenderSystem] Fallback 初始化失败 (ID: {})", windowID);
        }
        return;
    }

    if (!m_impl->m_deviceManager->claimWindow(sdlWindow))
    {
        m_logger->error("[RenderSystem] 无法声明窗口 (ID: {})", windowID);
        return;
    }

    m_impl->m_deviceClaimState.MarkDeviceLocked();
    ensureGpuResourcesInitialized();
    if (!m_impl->m_deviceClaimState.AreResourcesReady())
    {
        m_logger->error("[RenderSystem] GPU 资源初始化失败 (ID: {})", windowID);
        return;
    }

    if (auto pipeResult = m_impl->m_pipelineCache->createPipeline(sdlWindow); !pipeResult.has_value())
    {
        m_logger->warn("[RenderSystem] 初始化时创建管线失败: {}",
                                               pipeResult.error().ToString());
    }
    m_logger->info("[RenderSystem] 窗口图形上下文设置完成 (Entity: {})",
                                           static_cast<uint32_t>(event.entity));
}

void RenderSystem::onWindowsGraphicsContextUnset(const events::WindowGraphicsContextUnsetEvent& event)
{
    if (auto* windowComp = m_reg->try_get<components::Window>(event.entity))
    {
        m_impl->m_windowStates.erase(windowComp->windowID);
        if (m_impl->m_useFallback)
        {
            return;
        }
        SDL_Window* sdlWindow = SDL_GetWindowFromID(windowComp->windowID);
        if (sdlWindow != nullptr)
        {
            m_impl->m_deviceManager->unclaimWindow(sdlWindow);
            m_logger->info("已从 GPU 设备释放窗口 (ID: {})", windowComp->windowID);
        }
    }
}

void RenderSystem::cleanup()
{
    if (!m_impl)
    {
        return;
    }
    m_logger->info("[RenderSystem] cleanup() 开始");

    m_impl->m_windowStates.clear();

    SDL_GPUDevice* device = m_impl->m_deviceManager != nullptr ? m_impl->m_deviceManager->getDevice() : nullptr;
    if (device != nullptr)
    {
        m_logger->info("[RenderSystem] 等待 GPU 空闲...");
        SDL_WaitForGPUIdle(device);
    }

    // 按 GPU 资源 DAG 逆序停止借用并销毁全部外部 owner：renderer/batch ->
    // CommandBuffer -> TextTextureCache -> PipelineCache -> Icon/Image；DeviceManager 必须最后清理。
    m_logger->info("[RenderSystem] 清理渲染器");
    m_impl->m_renderQueue.clear();
    m_impl->m_renderers.clear();
    m_impl->m_batchManager.reset();
    m_impl->m_gpuInitialization.Shutdown([this](render::GpuInitializationTransaction::Node node)
                                         { resetGpuInitializationNode(*m_impl, node); });
    m_impl->m_iconManager.reset();
    m_impl->m_imageManager.reset();
    m_impl->m_fontManager.reset();

    // DAG 根收尾：white/claim -> token invalidate -> device。保留句柄用于清理后读取可观测计数；
    // 正常路径要求 late release skipped=0，非零表示有 owner 越过代际失效并触发防御性跳过。
    std::uint64_t lateReleaseSkipped = 0;
    std::uint64_t generationId = 0;
    if (m_impl->m_deviceManager != nullptr)
    {
        const auto generation = m_impl->m_deviceManager->getGeneration();
        generationId = generation.Id();
        m_logger->info("[RenderSystem] 清理设备管理器");
        m_impl->m_deviceManager->cleanup();
        lateReleaseSkipped = generation.LateReleaseSkipped();
    }
    m_impl->m_deviceClaimState.Reset();
    m_logger->info("[RenderSystem] cleanup() 完成");
    if (lateReleaseSkipped != 0U)
    {
        m_logger->error(
            "[RenderSystem] GPU shutdown detected {} late resource releases for generation {}", lateReleaseSkipped,
            generationId);
        throw std::logic_error("RenderSystem GPU owners outlived their device generation");
    }
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity,readability-function-size)
void RenderSystem::ensureInitialized()
{
    if ((SDL_WasInit(SDL_INIT_VIDEO) & SDL_INIT_VIDEO) == 0)
    {
        m_logger->warn("[RenderSystem] SDL_INIT_VIDEO not initialized");
        return;
    }

    const bool commandLineForcesFallback = config::AppConfig::instance().forceFallbackRenderer();
    if (!m_impl->m_forceFallback && commandLineForcesFallback)
    {
        m_impl->m_forceFallback = true;
        m_logger->warn(
            "[RenderSystem] 命令行后端 cpu/software/fallback 已启用，强制使用 SDL_Renderer fallback 后端");
    }

    if (m_impl->m_forceFallback)
    {
        m_impl->m_useFallback = true;

        if (!m_impl->m_backendSelectionLogged)
        {
            m_logger->info("[RenderSystem] 当前渲染后端: fallback (source={})",
                                                   commandLineForcesFallback ? "command-line" : "environment");
            m_impl->m_backendSelectionLogged = true;
        }
    }
    else if (!m_impl->m_deviceManager->initialize())
    {
        m_logger->warn(
            "Failed to initialize RenderSystem GPU backend, switching to fallback renderer");
        m_impl->m_useFallback = true;

        if (!m_impl->m_backendSelectionLogged)
        {
            m_logger->info("[RenderSystem] 当前渲染后端: fallback (source=gpu-init-failure)");
            m_impl->m_backendSelectionLogged = true;
        }
    }

    if (!m_impl->m_fontManager->isLoaded())
    {
        constexpr std::string_view DEFAULT_FONT_RESOURCE = "assets/fonts/NotoSansSC-VariableFont_wght.ttf";
        if (auto fontResource = LoadUiResource(DEFAULT_FONT_RESOURCE, *m_logger); fontResource.has_value())
        {
            const auto& fontBytes = fontResource.value();
            std::vector<uint8_t> fontData(fontBytes.size());
            std::ranges::transform(fontBytes.bytes, fontData.begin(),
                                   [](std::byte byte) { return std::to_integer<uint8_t>(byte); });
            if (auto loadResult = m_impl->m_fontManager->loadFromMemory(fontData.data(), fontData.size(), 14.0F);
                !loadResult.has_value())
            {
                m_logger->error("[RenderSystem] 默认字体加载失败: {}",
                                                        loadResult.error().ToString());
            }
        }
        else
        {
            m_logger->error("[RenderSystem] 默认字体资源加载失败: {} ({})",
                                                    DEFAULT_FONT_RESOURCE, fontResource.error().ToString());
        }
    }

    if (m_impl->m_useFallback)
    {
        if (m_impl->m_renderers.empty())
        {
            initializeRenderers();
        }

        return;
    }
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity,readability-function-size)
void RenderSystem::ensureGpuResourcesInitialized()
{
    if (m_impl->m_useFallback || m_impl->m_deviceClaimState.AreResourcesReady() ||
        !m_impl->m_deviceClaimState.IsDeviceLocked())
    {
        return;
    }

    if (!m_impl->m_backendSelectionLogged)
    {
        m_logger->info("[RenderSystem] 当前渲染后端: gpu ({})",
                                               m_impl->m_deviceManager->getDriverName());
        m_impl->m_backendSelectionLogged = true;
    }

    if (!m_impl->m_gpuInitialization.Begin())
    {
        return;
    }

    const auto rollback = [this](render::GpuInitializationTransaction::Node node)
    { resetGpuInitializationNode(*m_impl, node); };

    try
    {
        auto pipelineCache = std::make_unique<managers::PipelineCache>(
            *m_impl->m_deviceManager, *m_logger, GetUiResourceProvider(*m_logger));
        if (auto shaderResult = pipelineCache->loadShaders(); !shaderResult.has_value())
        {
            m_logger->error("[RenderSystem] GPU 初始化事务失败: {}",
                                                    shaderResult.error().ToString());
            m_impl->m_gpuInitialization.FailAndRollback(rollback);
            return;
        }
        m_impl->m_pipelineCache = std::move(pipelineCache);
        static_cast<void>(
            m_impl->m_gpuInitialization.Commit(render::GpuInitializationTransaction::Node::PIPELINE_CACHE));

        auto textTextureCache =
            std::make_unique<managers::TextTextureCache>(*m_impl->m_deviceManager, *m_impl->m_fontManager, *m_logger);
        m_impl->m_textTextureCache = std::move(textTextureCache);
        static_cast<void>(
            m_impl->m_gpuInitialization.Commit(render::GpuInitializationTransaction::Node::TEXT_TEXTURE_CACHE));

        if (m_impl->m_iconManager && !m_impl->m_iconsLoaded)
        {
            m_logger->info("[RenderSystem] 初始化 IconManager 并加载默认图标字体");
            constexpr std::string_view ICON_FONT_RESOURCE =
                "assets/icons/MaterialSymbolsRounded[FILL,GRAD,opsz,wght].ttf";
            constexpr std::string_view ICON_CODEPOINT_RESOURCE =
                "assets/icons/MaterialSymbolsRounded[FILL,GRAD,opsz,wght].codepoints";

            auto fontResource = LoadUiResource(ICON_FONT_RESOURCE, *m_logger);
            auto codepointResource = LoadUiResource(ICON_CODEPOINT_RESOURCE, *m_logger);

            if (fontResource.has_value() && codepointResource.has_value())
            {
                auto loadResult = ui::cpo::load_icon_font_from_memory(
                    *m_impl->m_iconManager, "MaterialSymbols", fontResource->bytes, codepointResource->bytes, 24);
                if (loadResult.has_value())
                {
                    m_logger->info("[RenderSystem]默认图标字体加载完成");
                }
                else
                {
                    m_logger->error("[RenderSystem] 默认图标字体加载失败: {}",
                                                            loadResult.error().ToString());
                }
            }
            else
            {
                if (!fontResource.has_value())
                {
                    m_logger->warn("[RenderSystem] 默认图标字体资源不存在: {} ({})",
                                                           ICON_FONT_RESOURCE, fontResource.error().ToString());
                }
                if (!codepointResource.has_value())
                {
                    m_logger->warn("[RenderSystem] 默认图标码点资源不存在: {} ({})",
                                                           ICON_CODEPOINT_RESOURCE,
                                                           codepointResource.error().ToString());
                }
            }
            m_impl->m_iconsLoaded = true;
        }

        auto commandBuffer =
            std::make_unique<managers::CommandBuffer>(*m_impl->m_deviceManager, *m_impl->m_pipelineCache, *m_logger);
        m_impl->m_commandBuffer = std::move(commandBuffer);
        static_cast<void>(
            m_impl->m_gpuInitialization.Commit(render::GpuInitializationTransaction::Node::COMMAND_BUFFER));

        if (m_impl->m_renderers.empty())
        {
            initializeRenderers();
        }

        if (m_impl->m_renderers.empty())
        {
            m_impl->m_renderers.clear();
            m_impl->m_gpuInitialization.FailAndRollback(rollback);
            return;
        }
        static_cast<void>(m_impl->m_gpuInitialization.Complete());
        static_cast<void>(m_impl->m_deviceClaimState.MarkResourcesReady());
    }
    catch (const std::exception& exception)
    {
        WriteStderr("[RenderSystem] GPU initialization transaction failed with exception\n");
        static_cast<void>(exception);
        m_impl->m_renderers.clear();
        m_impl->m_gpuInitialization.FailAndRollback(rollback);
    }
    catch (...)
    {
        WriteStderr("[RenderSystem] GPU initialization transaction failed with unknown exception\n");
        m_impl->m_renderers.clear();
        m_impl->m_gpuInitialization.FailAndRollback(rollback);
    }
}

ui::Result<interface::IBackendRenderer*> RenderSystem::tryInitializeFallback(SDL_Window* window)
{
    if (window == nullptr)
    {
        return ui::Err(ui::UiErrc::INVALID_ARGUMENT, "window is null");
    }

    const std::uint32_t windowID = SDL_GetWindowID(window);
    auto& state = m_impl->m_windowStates[windowID];
    state.windowID = windowID;
    if (state.fallbackBackend == nullptr)
    {
        state.fallbackBackend = std::make_unique<renderers::FallbackBackendRenderer>(*m_logger);
    }
    if (auto result = state.fallbackBackend->initialize(window); !result.has_value())
    {
        return ui::Err(result.error());
    }
    return ui::Ok(state.fallbackBackend.get());
}

}  // namespace ui::systems
