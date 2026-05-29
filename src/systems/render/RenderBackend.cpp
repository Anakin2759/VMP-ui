/**
 * @file RenderBackend.cpp
 * @brief RenderSystem — 后端初始化与设备生命周期
 *
 * 包含：构造/析构/移动、cleanup()、ensureInitialized()、
 *       tryInitializeFallback()、onWindowsGraphicsContext*()
 */

#include "systems/RenderSystem.hpp"
#include "RenderSystemImpl.hpp"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
#include <memory>
#include "common/Result.hpp"
#include "common/AppConfig.hpp"
#include <string_view>
#include "singleton/Logger.hpp"
#include "common/ErrorCodes.hpp"
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
#include "managers/CommandBuffer.hpp"
#include "singleton/Registry.hpp"
#include "renderers/FallbackBackendRenderer.hpp"
#include "managers/IconManager.hpp"
#include "managers/ResourceProvider.hpp"
#include "common/CustomizationPoints.hpp"

#ifndef UI_ASSETS_DIR
#define UI_ASSETS_DIR "assets"
#endif

namespace
{

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

std::shared_ptr<const ui::managers::IResourceProvider> GetUiResourceProvider()
{
    static const auto resourceProvider = ui::managers::GetDefaultUiResourceProvider();
    return resourceProvider;
}

ui::Result<ui::managers::BinaryResource> LoadUiResource(std::string_view resourcePath)
{
    const auto resourceProvider = GetUiResourceProvider();
    if (resourceProvider == nullptr)
    {
        ui::Logger::error("[RenderSystem] UI resource provider unavailable");
        return ui::MakeError(ui::UiErrc::BACKEND_UNAVAILABLE);
    }

    return ui::cpo::load_binary_resource(*resourceProvider, resourcePath);
}

} // namespace

namespace ui::systems
{

const RenderSystem::RenderStats& RenderSystem::getStats() const
{
    return m_impl->m_stats;
}

void RenderSystem::registerHandlersImpl()
{
    Logger::info("[RenderSystem] Registering event handlers");
    m_disp->sink<events::WindowGraphicsContextSetEvent>().connect<&RenderSystem::onWindowsGraphicsContextSet>(*this);
    m_disp->sink<events::WindowGraphicsContextUnsetEvent>().connect<&RenderSystem::onWindowsGraphicsContextUnset>(
        *this);
    m_disp->sink<events::UpdateRendering>().connect<&RenderSystem::update>(*this);
    Logger::info("[RenderSystem] Event handlers registered successfully");
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
    return interface::SystemPhase::Render;
}

RenderSystem::RenderSystem(entt::registry& /*reg*/, entt::dispatcher& disp)
    : m_disp(&disp), m_impl(std::make_unique<RenderSystemImpl>(
#ifdef UI_FORCE_CPU_RENDER
                         true
#else
                         IsTruthyEnvironmentValue(SDL_getenv("PESTMANKILL_FORCE_FALLBACK"))
#endif
                         ))
{
    if (m_impl->m_forceFallback)
    {
#ifdef UI_FORCE_CPU_RENDER
        Logger::warn("[RenderSystem] 编译选项 UI_FORCE_CPU_RENDER 已启用，强制使用 CPU software 后端");
#else
        Logger::warn("[RenderSystem] 检测到环境变量 PESTMANKILL_FORCE_FALLBACK，强制启用 SDL_Renderer fallback 后端");
#endif
    }
}

RenderSystemImpl::RenderSystemImpl(bool forceFallback)
    : m_deviceManager(std::make_unique<managers::DeviceManager>()),
      m_fontManager(std::make_unique<managers::FontManager>()),
      m_iconManager(std::make_unique<managers::IconManager>(m_deviceManager.get())),
      m_imageManager(std::make_unique<managers::ImageManager>(m_deviceManager.get())),
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

RenderSystem::RenderSystem(RenderSystem&& other) noexcept : m_disp(other.m_disp), m_impl(std::move(other.m_impl))
{
    other.m_disp = nullptr;
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

        m_disp = other.m_disp;
        other.m_disp = nullptr;
        m_impl = std::move(other.m_impl);
    }
    return *this;
}

void RenderSystem::onWindowsGraphicsContextSet(const events::WindowGraphicsContextSetEvent& event)
{
    Logger::info("[RenderSystem] 收到窗口图形上下文设置事件，实体ID: {}", static_cast<uint32_t>(event.entity));
    ensureInitialized();
    uint32_t windowID = Registry::Get<components::Window>(event.entity).windowID;
    SDL_Window* sdlWindow = SDL_GetWindowFromID(windowID);
    if (sdlWindow == nullptr)
    {
        Logger::warn("[RenderSystem] 无法获取 SDL_Window (ID: {})", windowID);
        return;
    }

    if (m_impl->m_useFallback)
    {
        if (!tryInitializeFallback(sdlWindow))
        {
            Logger::error("[RenderSystem] Fallback 初始化失败 (ID: {})", windowID);
        }
        return;
    }

    if (!m_impl->m_deviceManager->claimWindow(sdlWindow))
    {
        Logger::error("[RenderSystem] 无法声明窗口 (ID: {})", windowID);
        return;
    }

    if (auto pipeResult = m_impl->m_pipelineCache->createPipeline(sdlWindow); !pipeResult.has_value())
    {
        Logger::warn("[RenderSystem] 初始化时创建管线失败: {}", pipeResult.error().message());
    }
    Logger::info("[RenderSystem] 窗口图形上下文设置完成 (Entity: {})", static_cast<uint32_t>(event.entity));
}

void RenderSystem::onWindowsGraphicsContextUnset(const events::WindowGraphicsContextUnsetEvent& event)
{
    if (m_impl->m_useFallback)
    {
        return;
    }

    if (auto* windowComp = Registry::TryGet<components::Window>(event.entity))
    {
        SDL_Window* sdlWindow = SDL_GetWindowFromID(windowComp->windowID);
        if (sdlWindow != nullptr)
        {
            m_impl->m_deviceManager->unclaimWindow(sdlWindow);
            Logger::info("已从 GPU 设备释放窗口 (ID: {})", windowComp->windowID);
        }
    }
}

void RenderSystem::cleanup()
{
    if (!m_impl) return;
    Logger::info("[RenderSystem] cleanup() 开始");

    if (m_impl->m_backendRenderer)
    {
        m_impl->m_backendRenderer->cleanup();
        m_impl->m_backendRenderer.reset();
    }

    if (!m_impl->m_deviceManager)
    {
        Logger::info("[RenderSystem] m_deviceManager 为空，跳过 cleanup");
        return;
    }

    SDL_GPUDevice* device = m_impl->m_deviceManager->getDevice();
    if (device == nullptr)
    {
        Logger::info("[RenderSystem] GPU 设备为空，执行轻量清理");
        m_impl->m_renderers.clear();
        m_impl->m_commandBuffer.reset();
        m_impl->m_batchManager.reset();
        m_impl->m_pipelineCache.reset();
        m_impl->m_textTextureCache.reset();
        m_impl->m_fontManager.reset();
        m_impl->m_iconManager.reset();
        m_impl->m_imageManager.reset();
        return;
    }

    Logger::info("[RenderSystem] 等待 GPU 空闲...");
    SDL_WaitForGPUIdle(device);

    if (m_impl->m_textTextureCache)
    {
        Logger::info("[RenderSystem] 清理文本纹理缓存");
        m_impl->m_textTextureCache->clear();
    }

    if (m_impl->m_whiteTexture)
    {
        Logger::info("[RenderSystem] 释放白色纹理");
        m_impl->m_whiteTexture.reset();
    }

    Logger::info("[RenderSystem] 清理渲染器");
    m_impl->m_renderers.clear();
    m_impl->m_commandBuffer.reset();
    m_impl->m_batchManager.reset();
    m_impl->m_pipelineCache.reset();
    m_impl->m_textTextureCache.reset();
    m_impl->m_fontManager.reset();
    m_impl->m_iconManager.reset();
    m_impl->m_imageManager.reset();

    Logger::info("[RenderSystem] 清理设备管理器");
    m_impl->m_deviceManager->cleanup();
    Logger::info("[RenderSystem] cleanup() 完成");
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity,readability-function-size)
void RenderSystem::ensureInitialized()
{
    if ((SDL_WasInit(SDL_INIT_VIDEO) & SDL_INIT_VIDEO) == 0)
    {
        Logger::warn("[RenderSystem] SDL_INIT_VIDEO not initialized");
        return;
    }

    const bool commandLineForcesFallback = config::AppConfig::instance().forceFallbackRenderer();
    if (!m_impl->m_forceFallback && commandLineForcesFallback)
    {
        m_impl->m_forceFallback = true;
        Logger::warn("[RenderSystem] 命令行后端 cpu/software/fallback 已启用，强制使用 SDL_Renderer fallback 后端");
    }

    if (m_impl->m_forceFallback)
    {
        m_impl->m_useFallback = true;

        if (!m_impl->m_backendSelectionLogged)
        {
            Logger::info("[RenderSystem] 当前渲染后端: fallback (source={})",
                         commandLineForcesFallback ? "command-line" : "environment");
            m_impl->m_backendSelectionLogged = true;
        }
    }
    else if (!m_impl->m_deviceManager->initialize())
    {
        Logger::warn("Failed to initialize RenderSystem GPU backend, switching to fallback renderer");
        m_impl->m_useFallback = true;

        if (!m_impl->m_backendSelectionLogged)
        {
            Logger::info("[RenderSystem] 当前渲染后端: fallback (source=gpu-init-failure)");
            m_impl->m_backendSelectionLogged = true;
        }
    }

    if (!m_impl->m_useFallback && !m_impl->m_backendSelectionLogged)
    {
        Logger::info("[RenderSystem] 当前渲染后端: gpu ({})", m_impl->m_deviceManager->getDriverName());
        m_impl->m_backendSelectionLogged = true;
    }

    if (!m_impl->m_fontManager->isLoaded())
    {
        constexpr std::string_view DEFAULT_FONT_RESOURCE = "assets/fonts/NotoSansSC-VariableFont_wght.ttf";
        if (auto fontResource = LoadUiResource(DEFAULT_FONT_RESOURCE); fontResource.has_value())
        {
            const auto& fontBytes = fontResource.value();
            std::vector<uint8_t> fontData(fontBytes.size());
            std::ranges::transform(
                fontBytes.bytes, fontData.begin(), [](std::byte byte) { return std::to_integer<uint8_t>(byte); });
            if (auto loadResult = m_impl->m_fontManager->loadFromMemory(fontData.data(), fontData.size(), 14.0F);
                !loadResult.has_value())
            {
                Logger::error("[RenderSystem] 默认字体加载失败: {}", loadResult.error().message());
            }
        }
        else
        {
            Logger::error(
                "[RenderSystem] 默认字体资源加载失败: {} ({})", DEFAULT_FONT_RESOURCE, fontResource.error().message());
        }
    }

    if (m_impl->m_useFallback)
    {
        if (m_impl->m_backendRenderer == nullptr)
        {
            m_impl->m_backendRenderer = std::make_unique<renderers::FallbackBackendRenderer>();
        }

        if (m_impl->m_renderers.empty())
        {
            initializeRenderers();
        }

        return;
    }

    if (m_impl->m_pipelineCache == nullptr)
    {
        m_impl->m_pipelineCache = std::make_unique<managers::PipelineCache>(*m_impl->m_deviceManager);
        m_impl->m_pipelineCache->loadShaders();
    }

    if (m_impl->m_textTextureCache == nullptr)
    {
        m_impl->m_textTextureCache =
            std::make_unique<managers::TextTextureCache>(*m_impl->m_deviceManager, *m_impl->m_fontManager);
    }

    if (m_impl->m_iconManager)
    {
        static bool iconsLoaded = false;
        if (!iconsLoaded)
        {
            Logger::info("[RenderSystem] 初始化 IconManager 并加载默认图标字体");
            try
            {
                constexpr std::string_view ICON_FONT_RESOURCE =
                    "assets/icons/MaterialSymbolsRounded[FILL,GRAD,opsz,wght].ttf";
                constexpr std::string_view ICON_CODEPOINT_RESOURCE =
                    "assets/icons/MaterialSymbolsRounded[FILL,GRAD,opsz,wght].codepoints";

                auto fontResource = LoadUiResource(ICON_FONT_RESOURCE);
                auto codepointResource = LoadUiResource(ICON_CODEPOINT_RESOURCE);

                if (fontResource.has_value() && codepointResource.has_value())
                {
                    auto loadResult = ui::cpo::load_icon_font_from_memory(
                        *m_impl->m_iconManager, "MaterialSymbols", fontResource->bytes, codepointResource->bytes, 24);
                    if (loadResult.has_value())
                    {
                        Logger::info("[RenderSystem]默认图标字体加载完成");
                    }
                    else
                    {
                        Logger::error("[RenderSystem] 默认图标字体加载失败: {}", loadResult.error().message());
                    }
                }
                else
                {
                    if (!fontResource.has_value())
                    {
                        Logger::warn("[RenderSystem] 默认图标字体资源不存在: {} ({})",
                                     ICON_FONT_RESOURCE,
                                     fontResource.error().message());
                    }
                    if (!codepointResource.has_value())
                    {
                        Logger::warn("[RenderSystem] 默认图标码点资源不存在: {} ({})",
                                     ICON_CODEPOINT_RESOURCE,
                                     codepointResource.error().message());
                    }
                }
            }
            catch (const std::exception& e)
            {
                Logger::error("[RenderSystem] 加载默认图标字体失败: {}", e.what());
            }
            iconsLoaded = true;
        }
    }

    if (m_impl->m_commandBuffer == nullptr)
    {
        m_impl->m_commandBuffer =
            std::make_unique<managers::CommandBuffer>(*m_impl->m_deviceManager, *m_impl->m_pipelineCache);
    }

    if (m_impl->m_renderers.empty())
    {
        initializeRenderers();
    }
}

ui::Result<void> RenderSystem::tryInitializeFallback(SDL_Window* window)
{
    if (m_impl->m_backendRenderer == nullptr)
    {
        m_impl->m_backendRenderer = std::make_unique<renderers::FallbackBackendRenderer>();
    }

    return m_impl->m_backendRenderer->initialize(window);
}

} // namespace ui::systems
