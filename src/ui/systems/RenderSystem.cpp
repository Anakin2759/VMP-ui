/**
 * ************************************************************************
 *
 * @file RenderSystem.cpp
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2026-01-30
 * @version 0.2
 * @brief SDL GPU 渲染系统实现 - 重构版
 *
 * ************************************************************************
 * @copyright Copyright (c) 2026 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */

#include "RenderSystem.hpp"
#include <bit>
#include <algorithm>
#include <cctype>
#include "../api/Utils.hpp"
#include "../renderers/ShapeRenderer.hpp"
#include "../renderers/TextRenderer.hpp"
#include "../renderers/IconRenderer.hpp"
#include "../renderers/ScrollBarRenderer.hpp"
#include "../renderers/SliderRenderer.hpp"
#include "../renderers/ProgressBarRenderer.hpp"
#include "../renderers/FallbackBackendRenderer.hpp"
#include "../common/CustomizationPoints.hpp"
#include "../managers/IconManager.hpp"
#include "../managers/ResourceProvider.hpp"

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
    std::transform(normalized.begin(),
                   normalized.end(),
                   normalized.begin(),
                   [](unsigned char character) { return static_cast<char>(std::tolower(character)); });

    return normalized == "1" || normalized == "true" || normalized == "on" || normalized == "yes" || normalized == "y";
}

std::shared_ptr<const ui::managers::IResourceProvider> GetUiResourceProvider()
{
    static const auto RESOURCE_PROVIDER = ui::managers::GetDefaultUiResourceProvider();
    return RESOURCE_PROVIDER;
}

std::expected<ui::managers::BinaryResource, std::string> LoadUiResource(std::string_view resourcePath)
{
    const auto resourceProvider = GetUiResourceProvider();
    if (resourceProvider == nullptr)
    {
        return std::unexpected("UI resource provider unavailable");
    }

    return ui::cpo::load_binary_resource(*resourceProvider, resourcePath);
}

} // namespace

namespace ui::systems
{

RenderSystem::RenderSystem()
    : m_deviceManager(std::make_unique<managers::DeviceManager>()),
      m_fontManager(std::make_unique<managers::FontManager>()),
      m_iconManager(std::make_unique<managers::IconManager>(m_deviceManager.get())), m_pipelineCache(nullptr),
      m_textTextureCache(nullptr), m_batchManager(std::make_unique<managers::BatchManager>()), m_commandBuffer(nullptr),
      m_backendRenderer(nullptr), m_forceFallback(IsTruthyEnvironmentValue(SDL_getenv("PESTMANKILL_FORCE_FALLBACK")))
{
    m_stats.frameCount = 0;
    m_stats.batchCount = 0;
    m_stats.vertexCount = 0;

    if (m_forceFallback)
    {
        Logger::warn("[RenderSystem] 检测到环境变量 PESTMANKILL_FORCE_FALLBACK，强制启用 SDL_Renderer fallback 后端");
    }
}

RenderSystem::~RenderSystem()
{
    cleanup();
    Logger::info("[RenderSystem] 析构完成");
}

RenderSystem::RenderSystem(RenderSystem&& other) noexcept
    : m_deviceManager(std::move(other.m_deviceManager)), m_fontManager(std::move(other.m_fontManager)),
      m_iconManager(std::move(other.m_iconManager)), m_pipelineCache(std::move(other.m_pipelineCache)),
      m_textTextureCache(std::move(other.m_textTextureCache)), m_batchManager(std::move(other.m_batchManager)),
      m_commandBuffer(std::move(other.m_commandBuffer)), m_backendRenderer(std::move(other.m_backendRenderer)),
      m_renderers(std::move(other.m_renderers)), m_stats(other.m_stats),
      m_whiteTexture(std::move(other.m_whiteTexture)), m_fallbackWhiteTextureTag(other.m_fallbackWhiteTextureTag),
      m_useFallback(other.m_useFallback), m_forceFallback(other.m_forceFallback),
      m_backendSelectionLogged(other.m_backendSelectionLogged), m_screenWidth(other.m_screenWidth),
      m_screenHeight(other.m_screenHeight)
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

    m_pipelineCache->createPipeline(sdlWindow);
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

    Logger::info("[RenderSystem] 清理设备管理器");
    m_deviceManager->cleanup();
    Logger::info("[RenderSystem] cleanup() 完成");
}
/**
 * @brief 创建一个 1x1 白色纹理
 */
void RenderSystem::createWhiteTexture()
{
    SDL_GPUDevice* device = m_deviceManager->getDevice();
    if (device == nullptr) return;

    SDL_GPUTextureCreateInfo texInfo = {};
    texInfo.type = SDL_GPU_TEXTURETYPE_2D;
    texInfo.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    texInfo.width = 1;
    texInfo.height = 1;
    texInfo.layer_count_or_depth = 1;
    texInfo.num_levels = 1;
    texInfo.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;

    m_whiteTexture = wrappers::MakeGpuResource<wrappers::UniqueGPUTexture>(device, SDL_CreateGPUTexture, &texInfo);
    if (!m_whiteTexture) return;

    uint32_t whitePixel = 0xFFFFFFFF;
    SDL_GPUTransferBufferCreateInfo transferInfo = {};
    transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    transferInfo.size = sizeof(whitePixel);

    auto transfer = wrappers::MakeGpuResource<wrappers::UniqueGPUTransferBuffer>(
        device, SDL_CreateGPUTransferBuffer, &transferInfo);

    void* data = SDL_MapGPUTransferBuffer(device, transfer.get(), false);
    SDL_memcpy(data, &whitePixel, sizeof(whitePixel));
    SDL_UnmapGPUTransferBuffer(device, transfer.get());

    SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(device);
    SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmd);

    SDL_GPUTextureTransferInfo srcInfo = {};
    srcInfo.transfer_buffer = transfer.get();
    srcInfo.pixels_per_row = 1;
    srcInfo.rows_per_layer = 1;

    SDL_GPUTextureRegion dstRegion = {};
    dstRegion.texture = m_whiteTexture.get();
    dstRegion.w = 1;
    dstRegion.h = 1;
    dstRegion.d = 1;

    SDL_UploadToGPUTexture(copyPass, &srcInfo, &dstRegion, false);
    SDL_EndGPUCopyPass(copyPass);
    SDL_SubmitGPUCommandBuffer(cmd);
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity,readability-function-size)
void RenderSystem::update() noexcept
{
    static bool firstUpdate = true;
    auto windowView = Registry::View<components::Window, components::RenderDirtyTag>();

    if (windowView.begin() == windowView.end())
    {
        return;
    }

    if (firstUpdate)
    {
        Logger::info("[RenderSystem] update first call");
        firstUpdate = false;
    }

    ensureInitialized();

    if (m_useFallback)
    {
        if (m_backendRenderer == nullptr)
        {
            Logger::warn("Fallback backend not ready");
            return;
        }
    }
    else
    {
        SDL_GPUDevice* device = m_deviceManager->getDevice();
        if (device == nullptr)
        {
            Logger::warn("GPU device not ready");
            return;
        }
    }

    if (!m_useFallback && m_pipelineCache == nullptr)
    {
        Logger::warn("Pipeline cache not initialized");
        return;
    }

    if (!m_useFallback && m_whiteTexture == nullptr)
    {
        createWhiteTexture();
    }

    m_stats.frameCount++;
    m_stats.batchCount = 0;
    m_stats.vertexCount = 0;

    for (auto windowEntity : windowView)
    {
        auto& windowComp = windowView.get<components::Window>(windowEntity);
        SDL_Window* sdlWindow = SDL_GetWindowFromID(windowComp.windowID);
        if (sdlWindow == nullptr)
        {
            Logger::warn("窗口实体的 sdlWindow 为空");
            continue;
        }

        int width = 0;
        int height = 0;
        SDL_GetWindowSizeInPixels(sdlWindow, &width, &height);
        if (width <= 0 || height <= 0) continue;

        if (!m_useFallback && m_pipelineCache->getPipeline() == nullptr)
        {
            m_deviceManager->claimWindow(sdlWindow);
            m_pipelineCache->createPipeline(sdlWindow);

            if (m_pipelineCache->getPipeline() == nullptr)
            {
                // Still failed? Likely shader loading issue or device context issue
                // Don't log continuously to avoid spamming
                continue;
            }
        }

        m_screenWidth = static_cast<float>(width);
        m_screenHeight = static_cast<float>(height);

        m_batchManager->clear();
        m_renderQueue.clear();
        m_submissionIndex = 0;

        if (Registry::AnyOf<components::VisibleTag>(windowEntity))
        {
            core::RenderContext rootContext;
            rootContext.screenWidth = m_screenWidth;
            rootContext.screenHeight = m_screenHeight;
            rootContext.deviceManager = m_deviceManager.get();
            rootContext.fontManager = m_fontManager.get();
            rootContext.textTextureCache = m_textTextureCache.get();
            rootContext.batchManager = m_batchManager.get();
            rootContext.backendRenderer = m_useFallback ? m_backendRenderer.get() : nullptr;
            rootContext.sdlWindow = sdlWindow;
            rootContext.whiteTexture = m_useFallback ? m_fallbackWhiteTextureTag : m_whiteTexture.get();

            Eigen::Vector2f rootOffset = Eigen::Vector2f(0, 0);
            if (const auto* pos = Registry::TryGet<components::Position>(windowEntity))
            {
                rootOffset = -pos->value;
            }

            rootContext.position = rootOffset;
            rootContext.alpha = 1.0F;

            collectRenderData(windowEntity, rootContext);
        }

        // Sort render queue by RenderKey (Z-Order primarily)
        std::sort(m_renderQueue.begin(), m_renderQueue.end());

        if (m_useFallback)
        {
            if (!tryInitializeFallback(sdlWindow) ||
                !m_backendRenderer->beginFrame({.r = 0.0F, .g = 0.0F, .b = 0.0F, .a = 0.0F}))
            {
                continue;
            }

            for (auto& renderItem : m_renderQueue)
            {
                renderItem.renderer->collect(renderItem.entity, renderItem.context);

                m_batchManager->optimize();
                const auto& fallbackBatches = m_batchManager->getBatches();
                for (const auto& batch : fallbackBatches)
                {
                    m_backendRenderer->drawBatch(batch, m_fallbackWhiteTextureTag);
                }

                m_stats.batchCount += static_cast<uint32_t>(fallbackBatches.size());
                m_stats.vertexCount += static_cast<uint32_t>(m_batchManager->getTotalVertexCount());
                m_batchManager->clear();
            }

            m_backendRenderer->endFrame();
            continue;
        }

        // Execute collected render commands
        for (auto& item : m_renderQueue)
        {
            item.renderer->collect(item.entity, item.context);
        }

        m_batchManager->optimize();

        const auto& batches = m_batchManager->getBatches();
        if (!batches.empty())
        {
            if (m_useFallback)
            {
                if (tryInitializeFallback(sdlWindow) &&
                    m_backendRenderer->beginFrame({.r = 0.0F, .g = 0.0F, .b = 0.0F, .a = 0.0F}))
                {
                    for (const auto& batch : batches)
                    {
                        m_backendRenderer->drawBatch(batch, m_fallbackWhiteTextureTag);
                    }
                    m_backendRenderer->endFrame();
                }
            }
            else
            {
                m_commandBuffer->execute(sdlWindow, width, height, batches);
            }
            m_stats.batchCount += static_cast<uint32_t>(batches.size());
            m_stats.vertexCount += static_cast<uint32_t>(m_batchManager->getTotalVertexCount());
        }
    }

    auto dirtyView = Registry::View<components::RenderDirtyTag>();
    for (auto entity : dirtyView)
    {
        Registry::Remove<components::RenderDirtyTag>(entity);
    }
}
/**
 * @brief 确保渲染系统已初始化
 */
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
            if (auto loadResult = m_fontManager->loadFromMemory(
                    fontData, fontBytes.size(), 14.0F);
                !loadResult.has_value())
            {
                Logger::error("[RenderSystem] 默认字体加载失败: {}", loadResult.error().message());
            }
        }
        else
        {
            Logger::error("[RenderSystem] 默认字体资源加载失败: {} ({})", DEFAULT_FONT_RESOURCE, fontResource.error());
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
                        Logger::warn(
                            "[RenderSystem] 默认图标字体资源不存在: {} ({})", ICON_FONT_RESOURCE, fontResource.error());
                    }
                    if (!codepointResource.has_value())
                    {
                        Logger::warn("[RenderSystem] 默认图标码点资源不存在: {} ({})",
                                     ICON_CODEPOINT_RESOURCE,
                                     codepointResource.error());
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

bool RenderSystem::tryInitializeFallback(SDL_Window* window)
{
    if (m_backendRenderer == nullptr)
    {
        m_backendRenderer = std::make_unique<renderers::FallbackBackendRenderer>();
    }

    return m_backendRenderer->initialize(window);
}

void RenderSystem::initializeRenderers()
{
    // 按优先级顺序添加渲染器
    // 优先级小的先渲染（背景 -> 文本 -> 图标 -> 滚动条）

    m_renderers.push_back(std::make_unique<renderers::ShapeRenderer>());
    m_renderers.push_back(std::make_unique<renderers::ProgressBarRenderer>());
    m_renderers.push_back(std::make_unique<renderers::SliderRenderer>());
    m_renderers.push_back(std::make_unique<renderers::TextRenderer>());
    if (m_iconManager) m_renderers.push_back(std::make_unique<renderers::IconRenderer>(m_iconManager.get()));

    m_renderers.push_back(std::make_unique<renderers::ScrollBarRenderer>());

    // 按优先级排序
    std::sort(m_renderers.begin(),
              m_renderers.end(),
                  [](const std::unique_ptr<core::IRenderer>& leftRenderer,
                      const std::unique_ptr<core::IRenderer>& rightRenderer)
                  { return leftRenderer->getPriority() < rightRenderer->getPriority(); });

    Logger::info("[RenderSystem] 初始化了 {} 个渲染器", m_renderers.size());
}

// NOLINTNEXTLINE(misc-no-recursion)
void RenderSystem::collectRenderData(entt::entity entity, core::RenderContext& context)
{
    if (!Registry::AnyOf<components::VisibleTag>(entity)) return;
    if (Registry::AnyOf<components::SpacerTag>(entity)) return;

    const auto& pos = Registry::Get<components::Position>(entity);
    const auto& size = Registry::Get<components::Size>(entity);
    const auto* alphaComp = Registry::TryGet<components::Alpha>(entity);
    const auto* scaleComp = Registry::TryGet<components::Scale>(entity);
    const auto* offsetComp = Registry::TryGet<components::RenderOffset>(entity);

    float globalAlpha = context.alpha * (alphaComp != nullptr ? alphaComp->value : 1.0F);
    Eigen::Vector2f absolutePos = context.position + pos.value;
    Eigen::Vector2f finalSize = size.size;

    // 应用渲染偏移
    if (offsetComp != nullptr)
    {
        absolutePos += offsetComp->value;
    }

    // 应用缩放（基于中心点）
    if (scaleComp != nullptr)
    {
        Eigen::Vector2f scaleDiff = size.size.cwiseProduct(Eigen::Vector2f::Ones() - scaleComp->value);
        absolutePos += scaleDiff * 0.5F;
        finalSize = size.size.cwiseProduct(scaleComp->value);
    }

    Eigen::Vector2f contentOffset(0.0F, 0.0F);

    // 更新上下文
    core::RenderContext entityContext = context;
    entityContext.position = absolutePos;
    entityContext.size = finalSize;
    entityContext.alpha = globalAlpha;
    core::RenderContext childBaseContext = entityContext;

    // 处理 ScrollArea
    const auto* scrollArea = Registry::TryGet<components::ScrollArea>(entity);
    bool pushScissor = false;
    if (scrollArea != nullptr)
    {
        const Rect viewportRect = ui::utils::GetScrollViewportRect(entity);
        SDL_Rect currentScissor{};
        currentScissor.x = static_cast<int>(viewportRect.x());
        currentScissor.y = static_cast<int>(viewportRect.y());
        currentScissor.w = static_cast<int>(std::max(0.0F, viewportRect.width()));
        currentScissor.h = static_cast<int>(std::max(0.0F, viewportRect.height()));

        childBaseContext.pushScissor(currentScissor);
        pushScissor = true;
        contentOffset = -scrollArea->scrollOffset;
    }

    // Determine Z-Order
    int32_t zOrder = 0;
    if (const auto* zOrderComp = Registry::TryGet<components::ZOrderIndex>(entity))
    {
        zOrder = zOrderComp->value;
    }

    // Shift to positive range for unsigned sorting (int32_min -> 0)
    auto encodedZ = static_cast<uint64_t>(static_cast<int64_t>(zOrder) + 2147483648LL);

    // 使用渲染器收集数据
    for (auto& renderer : m_renderers)
    {
        if (renderer->canHandle(entity))
        {
            RenderItem item;
            item.entity = entity;
            item.renderer = renderer.get();
            item.context = entityContext;

            // Build Key: High=Z, Low=Order
            item.sortKey = (encodedZ << 32) | (m_submissionIndex & 0xFFFFFFFF);

            m_renderQueue.push_back(item);
            m_submissionIndex++;
        }
    }

    // 递归处理子元素
    const auto* hierarchy = Registry::TryGet<components::Hierarchy>(entity);
    if (hierarchy != nullptr && !hierarchy->children.empty())
    {
        for (entt::entity child : hierarchy->children)
        {
            core::RenderContext childContext = childBaseContext;
            childContext.position = absolutePos + contentOffset;
            collectRenderData(child, childContext);
        }
    }

    if (pushScissor)
    {
        childBaseContext.popScissor();
    }
}

} // namespace ui::systems
