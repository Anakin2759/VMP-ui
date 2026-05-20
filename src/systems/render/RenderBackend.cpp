/**
 * @file RenderBackend.cpp
 * @brief RenderSystem — 后端初始化与设备生命周期
 *
 * 包含：构造/析构/移动、cleanup()、ensureInitialized()、
 *       tryInitializeFallback()、onWindowsGraphicsContext*()
 */

#include "../RenderSystem.hpp"
#include <bit>
#include <cctype>
#include "singleton/Registry.hpp"
#include "common/Components.hpp"
#include "common/Tags.hpp"
#include "../../renderers/FallbackBackendRenderer.hpp"
#include "../../managers/IconManager.hpp"
#include "../../managers/ResourceProvider.hpp"
#include "../../common/CustomizationPoints.hpp"

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

std::shared_ptr<const ui::managers::IResourceProvider> GetUiResourceProvider()
{
    static const auto RESOURCE_PROVIDER = ui::managers::GetDefaultUiResourceProvider();
    return RESOURCE_PROVIDER;
}

ui::Result<ui::managers::BinaryResource> LoadUiResource(std::string_view resourcePath)
{
    const auto resourceProvider = GetUiResourceProvider();
    if (resourceProvider == nullptr)
    {
        ui::Logger::error("[RenderSystem] UI resource provider unavailable");
        return ui::MakeError(ui::ui_errc::backend_unavailable);
    }

    return ui::cpo::load_binary_resource(*resourceProvider, resourcePath);
}

} // namespace

namespace ui::systems
{

RenderSystem::RenderSystem()
    : m_deviceManager(std::make_unique<managers::DeviceManager>()),
      m_fontManager(std::make_unique<managers::FontManager>()),
      m_iconManager(std::make_unique<managers::IconManager>(m_deviceManager.get())),
      m_imageManager(std::make_unique<managers::ImageManager>(m_deviceManager.get())), m_pipelineCache(nullptr),
      m_textTextureCache(nullptr), m_batchManager(std::make_unique<managers::BatchManager>()), m_commandBuffer(nullptr),
      m_backendRenderer(nullptr), m_forceFallback(
#ifdef UI_FORCE_CPU_RENDER
                                      true
#else
                                      IsTruthyEnvironmentValue(SDL_getenv("PESTMANKILL_FORCE_FALLBACK"))
#endif
                                  )
{
    m_stats.frameCount = 0;
    m_stats.batchCount = 0;
    m_stats.vertexCount = 0;

    if (m_forceFallback)
    {
#ifdef UI_FORCE_CPU_RENDER
        Logger::warn("[RenderSystem] 编译选项 UI_FORCE_CPU_RENDER 已启用，强制使用 CPU software 后端");
#else
        Logger::warn("[RenderSystem] 检测到环境变量 PESTMANKILL_FORCE_FALLBACK，强制启用 SDL_Renderer fallback 后端");
#endif
    }
}

RenderSystem::~RenderSystem()
{
    cleanup();
    Logger::info("[RenderSystem] 析构完成");
}

RenderSystem::RenderSystem(RenderSystem&& other) noexcept
    : m_deviceManager(std::move(other.m_deviceManager)), m_fontManager(std::move(other.m_fontManager)),
      m_iconManager(std::move(other.m_iconManager)), m_imageManager(std::move(other.m_imageManager)),
      m_pipelineCache(std::move(other.m_pipelineCache)), m_textTextureCache(std::move(other.m_textTextureCache)),
      m_batchManager(std::move(other.m_batchManager)), m_commandBuffer(std::move(other.m_commandBuffer)),
      m_backendRenderer(std::move(other.m_backendRenderer)), m_renderers(std::move(other.m_renderers)),
      m_stats(other.m_stats), m_whiteTexture(std::move(other.m_whiteTexture)),
      m_fallbackWhiteTextureTag(other.m_fallbackWhiteTextureTag), m_useFallback(other.m_useFallback),
      m_forceFallback(other.m_forceFallback), m_backendSelectionLogged(other.m_backendSelectionLogged),
      m_screenWidth(other.m_screenWidth), m_screenHeight(other.m_screenHeight)
{
    other.m_useFallback = false;
    other.m_forceFallback = false;
    other.m_backendSelectionLogged = false;
    Logger::info("[RenderSystem] 移动构造完成");
}

RenderSystem& RenderSystem::operator=(RenderSystem&& other) noexcept
{
    if (this != &other)
    {
        Logger::info("[RenderSystem] 移动赋值开始");
        cleanup();

        m_deviceManager = std::move(other.m_deviceManager);
        m_fontManager = std::move(other.m_fontManager);
        m_iconManager = std::move(other.m_iconManager);
        m_imageManager = std::move(other.m_imageManager);
        m_pipelineCache = std::move(other.m_pipelineCache);
        m_textTextureCache = std::move(other.m_textTextureCache);
        m_batchManager = std::move(other.m_batchManager);
        m_commandBuffer = std::move(other.m_commandBuffer);
        m_backendRenderer = std::move(other.m_backendRenderer);
        m_renderers = std::move(other.m_renderers);
        m_stats = other.m_stats;
        m_whiteTexture = std::move(other.m_whiteTexture);
        m_fallbackWhiteTextureTag = other.m_fallbackWhiteTextureTag;
        m_useFallback = other.m_useFallback;
        m_forceFallback = other.m_forceFallback;
        m_backendSelectionLogged = other.m_backendSelectionLogged;
        m_screenWidth = other.m_screenWidth;
        m_screenHeight = other.m_screenHeight;

        other.m_iconManager = nullptr;
        other.m_useFallback = false;
        other.m_forceFallback = false;
        other.m_backendSelectionLogged = false;
        Logger::info("[RenderSystem] 移动赋值完成");
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

    if (m_useFallback)
    {
        if (!tryInitializeFallback(sdlWindow))
        {
            Logger::error("[RenderSystem] Fallback 初始化失败 (ID: {})", windowID);
        }
        return;
    }

    if (!m_deviceManager->claimWindow(sdlWindow))
    {
        Logger::error("[RenderSystem] 无法声明窗口 (ID: {})", windowID);
        return;
    }

    if (auto pipeResult = m_pipelineCache->createPipeline(sdlWindow); !pipeResult.has_value())
    {
        Logger::warn("[RenderSystem] 初始化时创建管线失败: {}", pipeResult.error().message());
    }
    Logger::info("[RenderSystem] 窗口图形上下文设置完成 (Entity: {})", static_cast<uint32_t>(event.entity));
}

void RenderSystem::onWindowsGraphicsContextUnset(const events::WindowGraphicsContextUnsetEvent& event)
{
    if (m_useFallback)
    {
        return;
    }

    if (auto* windowComp = Registry::TryGet<components::Window>(event.entity))
    {
        SDL_Window* sdlWindow = SDL_GetWindowFromID(windowComp->windowID);
        if (sdlWindow != nullptr)
        {
            m_deviceManager->unclaimWindow(sdlWindow);
            Logger::info("已从 GPU 设备释放窗口 (ID: {})", windowComp->windowID);
        }
    }
}

void RenderSystem::cleanup()
{
    Logger::info("[RenderSystem] cleanup() 开始");

    if (m_backendRenderer)
    {
        m_backendRenderer->cleanup();
        m_backendRenderer.reset();
    }

    if (!m_deviceManager)
    {
        Logger::info("[RenderSystem] m_deviceManager 为空，跳过 cleanup");
        return;
    }

    SDL_GPUDevice* device = m_deviceManager->getDevice();
    if (device == nullptr)
    {
        Logger::info("[RenderSystem] GPU 设备为空，执行轻量清理");
        m_renderers.clear();
        m_commandBuffer.reset();
        m_batchManager.reset();
        m_pipelineCache.reset();
        m_textTextureCache.reset();
        m_fontManager.reset();
        m_iconManager.reset();
        m_imageManager.reset();
        return;
    }

    Logger::info("[RenderSystem] 等待 GPU 空闲...");
    SDL_WaitForGPUIdle(device);

    if (m_textTextureCache)
    {
        Logger::info("[RenderSystem] 清理文本纹理缓存");
        m_textTextureCache->clear();
    }

    if (m_whiteTexture)
    {
        Logger::info("[RenderSystem] 释放白色纹理");
        m_whiteTexture.reset();
    }

    Logger::info("[RenderSystem] 清理渲染器");
    m_renderers.clear();
    m_commandBuffer.reset();
    m_batchManager.reset();
    m_pipelineCache.reset();
    m_textTextureCache.reset();
    m_fontManager.reset();
    m_iconManager.reset();
    m_imageManager.reset();

    Logger::info("[RenderSystem] 清理设备管理器");
    m_deviceManager->cleanup();
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

    if (m_forceFallback)
    {
        m_useFallback = true;

        if (!m_backendSelectionLogged)
        {
            Logger::info("[RenderSystem] 当前渲染后端: fallback (source=environment)");
            m_backendSelectionLogged = true;
        }
    }
    else if (!m_deviceManager->initialize())
    {
        Logger::warn("Failed to initialize RenderSystem GPU backend, switching to fallback renderer");
        m_useFallback = true;

        if (!m_backendSelectionLogged)
        {
            Logger::info("[RenderSystem] 当前渲染后端: fallback (source=gpu-init-failure)");
            m_backendSelectionLogged = true;
        }
    }

    if (!m_useFallback && !m_backendSelectionLogged)
    {
        Logger::info("[RenderSystem] 当前渲染后端: gpu ({})", m_deviceManager->getDriverName());
        m_backendSelectionLogged = true;
    }

    if (!m_fontManager->isLoaded())
    {
        constexpr std::string_view DEFAULT_FONT_RESOURCE = "assets/fonts/NotoSansSC-VariableFont_wght.ttf";
        if (auto fontResource = LoadUiResource(DEFAULT_FONT_RESOURCE); fontResource.has_value())
        {
            const auto& fontBytes = fontResource.value();
            const auto* fontData = std::bit_cast<const uint8_t*>(fontBytes.data());
            if (auto loadResult = m_fontManager->loadFromMemory(fontData, fontBytes.size(), 14.0F);
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

    if (m_useFallback)
    {
        if (m_backendRenderer == nullptr)
        {
            m_backendRenderer = std::make_unique<renderers::FallbackBackendRenderer>();
        }

        if (m_renderers.empty())
        {
            initializeRenderers();
        }

        return;
    }

    if (m_pipelineCache == nullptr)
    {
        m_pipelineCache = std::make_unique<managers::PipelineCache>(*m_deviceManager);
        m_pipelineCache->loadShaders();
    }

    if (m_textTextureCache == nullptr)
    {
        m_textTextureCache = std::make_unique<managers::TextTextureCache>(*m_deviceManager, *m_fontManager);
    }

    if (m_iconManager)
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
                        *m_iconManager, "MaterialSymbols", fontResource->bytes, codepointResource->bytes, 24);
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

    if (m_commandBuffer == nullptr)
    {
        m_commandBuffer = std::make_unique<managers::CommandBuffer>(*m_deviceManager, *m_pipelineCache);
    }

    if (m_renderers.empty())
    {
        initializeRenderers();
    }
}

ui::Result<void> RenderSystem::tryInitializeFallback(SDL_Window* window)
{
    if (m_backendRenderer == nullptr)
    {
        m_backendRenderer = std::make_unique<renderers::FallbackBackendRenderer>();
    }

    return m_backendRenderer->initialize(window);
}

} // namespace ui::systems
