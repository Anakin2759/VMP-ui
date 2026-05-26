/**
 * @file RenderFrame.cpp
 *
 */

#include "../RenderSystem.hpp"
#include "RenderSystemImpl.hpp"
#include <algorithm>
#include <cstdint>
#include <ranges>
#include <stack>
#include "common/components/Window.hpp"
#include "singleton/Logger.hpp"
#include "SDL3/SDL_gpu.h"
#include "SDL3/SDL_video.h"
#include "core/RenderContext.hpp"
#include "managers/BatchManager.hpp"
#include "managers/CommandBuffer.hpp"
#include "managers/DeviceManager.hpp"
#include "managers/PipelineCache.hpp"
#include "interface/IBackendRenderer.hpp"
#include "interface/IRenderer.hpp"
#include "Eigen/src/Core/Matrix.h"
#include "common/components/Layout.hpp"
#include "entt/entity/fwd.hpp"
#include <utility>
#include "common/components/Visual.hpp"
#include "common/components/Interaction.hpp"
#include "common/Types.hpp"
#include "SDL3/SDL_rect.h"
#include "singleton/Registry.hpp"
#include "common/Tags.hpp"
#include "../../api/Utils.hpp"

namespace ui::systems
{

// NOLINTNEXTLINE(readability-function-cognitive-complexity,readability-function-size)
void RenderSystem::update()
{
    auto windowView = Registry::View<components::Window, components::RenderDirtyTag>();

    if (windowView.begin() == windowView.end())
    {
        return;
    }

    if (m_impl->m_firstUpdate)
    {
        Logger::info("[RenderSystem] update first call");
        m_impl->m_firstUpdate = false;
    }

    ensureInitialized();

    if (m_impl->m_useFallback)
    {
        if (m_impl->m_backendRenderer == nullptr)
        {
            Logger::warn("Fallback backend not ready");
            return;
        }
    }
    else
    {
        SDL_GPUDevice const* device = m_impl->m_deviceManager->getDevice();
        if (device == nullptr)
        {
            Logger::warn("GPU device not ready");
            return;
        }
    }

    if (!m_impl->m_useFallback && m_impl->m_pipelineCache == nullptr)
    {
        Logger::warn("Pipeline cache not initialized");
        return;
    }

    if (!m_impl->m_useFallback && m_impl->m_whiteTexture == nullptr)
    {
        createWhiteTexture();
    }

    m_impl->m_stats.frameCount++;
    m_impl->m_stats.batchCount = 0;
    m_impl->m_stats.vertexCount = 0;

    for (auto windowEntity : windowView)
    {
        auto& windowComp = windowView.get<components::Window>(windowEntity);
        SDL_Window* sdlWindow = SDL_GetWindowFromID(windowComp.windowID);
        if (sdlWindow == nullptr)
        {
            Logger::warn("Window entity has no SDL window");
            continue;
        }

        int width = 0;
        int height = 0;
        SDL_GetWindowSizeInPixels(sdlWindow, &width, &height);
        if (width <= 0 || height <= 0)
        {
            continue;
        }

        if (!m_impl->m_useFallback && m_impl->m_pipelineCache->getPipeline() == nullptr)
        {
            if (auto claimResult = m_impl->m_deviceManager->claimWindow(sdlWindow); !claimResult.has_value())
            {
                Logger::warn("[RenderSystem] claimWindow failed: {}", claimResult.error().message());
            }
            if (auto pipeResult = m_impl->m_pipelineCache->createPipeline(sdlWindow); !pipeResult.has_value())
            {
                Logger::warn("[RenderSystem] pipeline creation failed: {}", pipeResult.error().message());
            }

            if (m_impl->m_pipelineCache->getPipeline() == nullptr)
            {
                Logger::warn("[RenderSystem] GPU pipeline unavailable; switching to fallback renderer. "
                             "Rebuild shaders with compile.bat to restore GPU rendering.");
                m_impl->m_useFallback = true;
                if (!tryInitializeFallback(sdlWindow))
                {
                    Logger::error("[RenderSystem] fallback initialization failed; skipping this frame");
                }
                continue;
            }
        }

        m_impl->m_screenWidth = static_cast<float>(width);
        m_impl->m_screenHeight = static_cast<float>(height);

        m_impl->m_batchManager->clear();
        m_impl->m_renderQueue.clear();
        m_impl->m_submissionIndex = 0;

        if (Registry::AnyOf<components::VisibleTag>(windowEntity))
        {
            core::RenderContext rootContext;
            rootContext.screenWidth = m_impl->m_screenWidth;
            rootContext.screenHeight = m_impl->m_screenHeight;
            rootContext.deviceManager = m_impl->m_deviceManager.get();
            rootContext.fontManager = m_impl->m_fontManager.get();
            rootContext.imageManager = m_impl->m_imageManager.get();
            rootContext.textTextureCache = m_impl->m_textTextureCache.get();
            rootContext.batchManager = m_impl->m_batchManager.get();
            rootContext.backendRenderer = m_impl->m_useFallback ? m_impl->m_backendRenderer.get() : nullptr;
            rootContext.sdlWindow = sdlWindow;
            rootContext.whiteTexture =
                m_impl->m_useFallback ? m_impl->m_fallbackWhiteTextureTag : m_impl->m_whiteTexture.get();

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
        std::ranges::sort(m_impl->m_renderQueue, {}, &RenderSystemImpl::RenderItem::sortKey);

        if (m_impl->m_useFallback)
        {
            if (!tryInitializeFallback(sdlWindow)
                || !m_impl->m_backendRenderer->beginFrame({.r = 0.0F, .g = 0.0F, .b = 0.0F, .a = 0.0F}))
            {
                continue;
            }

            for (auto& renderItem : m_impl->m_renderQueue)
            {
                renderItem.renderer->collect(renderItem.entity, renderItem.context);

                m_impl->m_batchManager->optimize();
                const auto& fallbackBatches = m_impl->m_batchManager->getBatches();
                for (const auto& batch : fallbackBatches)
                {
                    m_impl->m_backendRenderer->drawBatch(batch, m_impl->m_fallbackWhiteTextureTag);
                }

                m_impl->m_stats.batchCount += static_cast<uint32_t>(fallbackBatches.size());
                m_impl->m_stats.vertexCount += static_cast<uint32_t>(m_impl->m_batchManager->getTotalVertexCount());
                m_impl->m_batchManager->clear();
            }

            m_impl->m_backendRenderer->endFrame();
            continue;
        }

        // Execute collected render commands
        for (auto& item : m_impl->m_renderQueue)
        {
            item.renderer->collect(item.entity, item.context);
        }

        m_impl->m_batchManager->optimize();

        const auto& batches = m_impl->m_batchManager->getBatches();
        if (!batches.empty())
        {
            if (m_impl->m_useFallback)
            {
                if (tryInitializeFallback(sdlWindow)
                    && m_impl->m_backendRenderer->beginFrame({.r = 0.0F, .g = 0.0F, .b = 0.0F, .a = 0.0F}))
                {
                    for (const auto& batch : batches)
                    {
                        m_impl->m_backendRenderer->drawBatch(batch, m_impl->m_fallbackWhiteTextureTag);
                    }
                    m_impl->m_backendRenderer->endFrame();
                }
            }
            else
            {
                m_impl->m_commandBuffer->execute(sdlWindow, width, height, batches);
            }
            m_impl->m_stats.batchCount += static_cast<uint32_t>(batches.size());
            m_impl->m_stats.vertexCount += static_cast<uint32_t>(m_impl->m_batchManager->getTotalVertexCount());
        }
    }

    auto dirtyView = Registry::View<components::RenderDirtyTag>();
    for (auto entity : dirtyView)
    {
        Registry::Remove<components::RenderDirtyTag>(entity);
    }
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity,readability-function-size)
void RenderSystem::collectRenderData(entt::entity entity, core::RenderContext& context)
{
    struct StackFrame
    {
        entt::entity entity;
        core::RenderContext context;
    };

    std::stack<StackFrame> stack;
    stack.push({.entity = entity, .context = context});

    while (!stack.empty())
    {
        auto [currentEntity, currentContext] = std::move(stack.top());
        stack.pop();

        if (!Registry::AnyOf<components::VisibleTag>(currentEntity)) continue;
        if (Registry::AnyOf<components::SpacerTag>(currentEntity)) continue;

        const auto& pos = Registry::Get<components::Position>(currentEntity);
        const auto& size = Registry::Get<components::Size>(currentEntity);
        const auto* alphaComp = Registry::TryGet<components::Alpha>(currentEntity);
        const auto* scaleComp = Registry::TryGet<components::Scale>(currentEntity);
        const auto* offsetComp = Registry::TryGet<components::RenderOffset>(currentEntity);

        float const globalAlpha = currentContext.alpha * (alphaComp != nullptr ? alphaComp->value : 1.0F);
        Eigen::Vector2f absolutePos = currentContext.position + pos.value;
        Eigen::Vector2f finalSize = size.size;

        if (offsetComp != nullptr)
        {
            absolutePos += offsetComp->value;
        }

        if (scaleComp != nullptr)
        {
            Eigen::Vector2f const scaleDiff = size.size.cwiseProduct(Eigen::Vector2f::Ones() - scaleComp->value);
            absolutePos += scaleDiff * 0.5F;
            finalSize = size.size.cwiseProduct(scaleComp->value);
        }

        Eigen::Vector2f contentOffset(0.0F, 0.0F);

        core::RenderContext entityContext = currentContext;
        entityContext.position = absolutePos;
        entityContext.size = finalSize;
        entityContext.alpha = globalAlpha;
        core::RenderContext childBaseContext = entityContext;

        const auto* scrollArea = Registry::TryGet<components::ScrollArea>(currentEntity);
        if (scrollArea != nullptr)
        {
            const Rect viewportRect = ui::utils::GetScrollViewportRect(currentEntity);
            SDL_Rect scissorRect{};
            scissorRect.x = static_cast<int>(viewportRect.x());
            scissorRect.y = static_cast<int>(viewportRect.y());
            scissorRect.w = static_cast<int>(std::max(0.0F, viewportRect.width()));
            scissorRect.h = static_cast<int>(std::max(0.0F, viewportRect.height()));

            childBaseContext.pushScissor(scissorRect);
            contentOffset = -scrollArea->scrollOffset;
        }
        else if (Registry::AnyOf<components::LayoutInfo>(currentEntity))
        {
            const SDL_Rect containerScissor{.x = static_cast<int>(absolutePos.x()),
                                            .y = static_cast<int>(absolutePos.y()),
                                            .w = static_cast<int>(std::max(0.0F, finalSize.x())),
                                            .h = static_cast<int>(std::max(0.0F, finalSize.y()))};
            childBaseContext.pushScissor(containerScissor);
        }

        // Determine Z-Order
        int32_t zOrder = 0;
        if (const auto* zOrderComp = Registry::TryGet<components::ZOrderIndex>(currentEntity))
        {
            zOrder = zOrderComp->value;
        }

        // Shift to positive range for unsigned sorting (int32_min -> 0)
        auto encodedZ = static_cast<uint64_t>(static_cast<int64_t>(zOrder) + 2147483648LL);

        for (auto& renderer : m_impl->m_renderers)
        {
            if (renderer->canHandle(currentEntity))
            {
                RenderSystemImpl::RenderItem item;
                item.entity = currentEntity;
                item.renderer = renderer.get();
                item.context = entityContext;

                // Build Key: High=Z, Low=Order
                item.sortKey = (encodedZ << 32) | (m_impl->m_submissionIndex & 0xFFFFFFFF);

                m_impl->m_renderQueue.push_back(item);
                m_impl->m_submissionIndex++;
            }
        }

        const auto* hierarchy = Registry::TryGet<components::Hierarchy>(currentEntity);
        if (hierarchy != nullptr && !hierarchy->children.empty())
        {
            for (auto childEntity : std::views::reverse(hierarchy->children))
            {
                core::RenderContext childContext = childBaseContext;
                childContext.position = absolutePos + contentOffset;
                stack.push({.entity = childEntity, .context = std::move(childContext)});
            }
        }
    }
}

} // namespace ui::systems
