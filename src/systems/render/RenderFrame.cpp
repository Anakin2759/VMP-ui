/**
 * @file RenderFrame.cpp
 * @brief RenderSystem — 每帧渲染逻辑与实体数据收集
 *
 * 包含：update()、collectRenderData()
 */

#include "../RenderSystem.hpp"
#include <algorithm>
#include <stack>
#include "singleton/Registry.hpp"
#include "common/Components.hpp"
#include "common/Tags.hpp"
#include "../../api/Utils.hpp"

namespace ui::systems
{

// NOLINTNEXTLINE(readability-function-cognitive-complexity,readability-function-size)
void RenderSystem::update() noexcept
{
    auto windowView = Registry::View<components::Window, components::RenderDirtyTag>();

    if (windowView.begin() == windowView.end())
    {
        return;
    }

    if (m_firstUpdate)
    {
        Logger::info("[RenderSystem] update first call");
        m_firstUpdate = false;
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
            if (auto claimResult = m_deviceManager->claimWindow(sdlWindow); !claimResult.has_value())
            {
                Logger::warn("[RenderSystem] claimWindow 失败: {}", claimResult.error().message());
            }
            if (auto pipeResult = m_pipelineCache->createPipeline(sdlWindow); !pipeResult.has_value())
            {
                Logger::warn("[RenderSystem] 重新创建管线失败: {}", pipeResult.error().message());
            }

            if (m_pipelineCache->getPipeline() == nullptr)
            {
                // 管线创建失败（通常是着色器二进制过期），切换到 fallback 渲染器
                Logger::warn("[RenderSystem] GPU 管线不可用，切换到 fallback 渲染器（运行 compile.bat "
                             "重新编译着色器可恢复 GPU 渲染）");
                m_useFallback = true;
                if (!tryInitializeFallback(sdlWindow))
                {
                    Logger::error("[RenderSystem] Fallback 初始化也失败，跳过本帧渲染");
                }
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
            rootContext.imageManager = m_imageManager.get();
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
            if (!tryInitializeFallback(sdlWindow)
                || !m_backendRenderer->beginFrame({.r = 0.0F, .g = 0.0F, .b = 0.0F, .a = 0.0F}))
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
                if (tryInitializeFallback(sdlWindow)
                    && m_backendRenderer->beginFrame({.r = 0.0F, .g = 0.0F, .b = 0.0F, .a = 0.0F}))
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

// NOLINTNEXTLINE(readability-function-cognitive-complexity,readability-function-size)
void RenderSystem::collectRenderData(entt::entity entity, core::RenderContext& context)
{
    struct StackFrame
    {
        entt::entity entity;
        core::RenderContext context;
    };

    std::stack<StackFrame> stack;
    stack.push({entity, context});

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

        float globalAlpha = currentContext.alpha * (alphaComp != nullptr ? alphaComp->value : 1.0F);
        Eigen::Vector2f absolutePos = currentContext.position + pos.value;
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
        core::RenderContext entityContext = currentContext;
        entityContext.position = absolutePos;
        entityContext.size = finalSize;
        entityContext.alpha = globalAlpha;
        core::RenderContext childBaseContext = entityContext;

        // 处理 ScrollArea
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
            // 容器裁剪：防止子元素因 Yoga 浮点误差或内容溢出而渲染超出容器边界
            const SDL_Rect containerScissor{static_cast<int>(absolutePos.x()),
                                            static_cast<int>(absolutePos.y()),
                                            static_cast<int>(std::max(0.0F, finalSize.x())),
                                            static_cast<int>(std::max(0.0F, finalSize.y()))};
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

        // 使用渲染器收集数据
        for (auto& renderer : m_renderers)
        {
            if (renderer->canHandle(currentEntity))
            {
                RenderItem item;
                item.entity = currentEntity;
                item.renderer = renderer.get();
                item.context = entityContext;

                // Build Key: High=Z, Low=Order
                item.sortKey = (encodedZ << 32) | (m_submissionIndex & 0xFFFFFFFF);

                m_renderQueue.push_back(item);
                m_submissionIndex++;
            }
        }

        // 迭代处理子元素（反序压栈，保证第一个子先弹出）
        const auto* hierarchy = Registry::TryGet<components::Hierarchy>(currentEntity);
        if (hierarchy != nullptr && !hierarchy->children.empty())
        {
            for (auto it = hierarchy->children.rbegin(); it != hierarchy->children.rend(); ++it)
            {
                core::RenderContext childContext = childBaseContext;
                childContext.position = absolutePos + contentOffset;
                stack.push({*it, std::move(childContext)});
            }
        }
    }
}

} // namespace ui::systems
