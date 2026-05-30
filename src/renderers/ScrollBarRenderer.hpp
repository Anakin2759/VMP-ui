/**
 * ************************************************************************
 *
 * @file ScrollBarRenderer.hpp
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2026-01-30
 * @version 0.1
 * @brief 滚动条渲染器 - 处理滚动条的渲染
 *
 * ************************************************************************
 * @copyright Copyright (c) 2026 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */

#pragma once
#include "interface/IRenderer.hpp"
#include "singleton/Registry.hpp"
#include "common/RenderTypes.hpp"
#include "common/components/Interaction.hpp"
#include "common/components/Layout.hpp"
#include "managers/BatchManager.hpp"
#include <algorithm>
#include <SDL3/SDL_gpu.h>

namespace ui::renderers
{

/**
 * @brief 滚动条渲染器
 *
 */
class ScrollBarRenderer : public core::IRenderer
{
public:
    explicit ScrollBarRenderer(Registry& reg) : m_reg(&reg) {}

    bool canHandle(entt::entity entity) const override { return m_reg->any_of<components::ScrollArea>(entity); }

    void collect(entt::entity entity, core::RenderContext& context) override
    {
        if (context.batchManager == nullptr || context.deviceManager == nullptr)
        {
            return;
        }

        const auto* scrollArea = m_reg->try_get<components::ScrollArea>(entity);
        if (scrollArea == nullptr || policies::HasFlag(scrollArea->scrollBar, policies::ScrollBar::NO_VISIBILITY))
        {
            return;
        }
        drawScrollBars(entity, context.position, context.size, *scrollArea, context.alpha, context);
    }

    int getPriority() const override
    {
        return 30; // 滚动条在所有内容渲染之后，裁剪区域弹出之前渲染
    }

private:
    [[nodiscard]] static render::UiPushConstants makeRoundedRectConstants(const Eigen::Vector2f& size,
                                                                          float radius,
                                                                          float alpha,
                                                                          const core::RenderContext& context)
    {
        render::UiPushConstants pushConstants{};
        pushConstants.screen_size[0] = context.screenWidth;
        pushConstants.screen_size[1] = context.screenHeight;
        pushConstants.rect_size[0] = size.x();
        pushConstants.rect_size[1] = size.y();
        pushConstants.radius[0] = radius;
        pushConstants.radius[1] = radius;
        pushConstants.radius[2] = radius;
        pushConstants.radius[3] = radius;
        pushConstants.opacity = alpha;
        pushConstants.shadow_soft = 0.0F;
        pushConstants.shadow_offset_x = 0.0F;
        pushConstants.shadow_offset_y = 0.0F;
        return pushConstants;
    }

    static void drawRoundedRect(const Eigen::Vector2f& pos,
                                const Eigen::Vector2f& size,
                                const Eigen::Vector4f& color,
                                float radius,
                                float alpha,
                                core::RenderContext& context)
    {
        const auto pushConstants = makeRoundedRectConstants(size, radius, alpha, context);
        context.batchManager->beginBatch(context.whiteTexture, context.currentScissor, pushConstants);
        context.batchManager->addRect(pos, size, color);
    }

    [[nodiscard]] Eigen::Vector4f verticalTrackColor(entt::entity entity) const
    {
        Eigen::Vector4f trackColor = {0.2F, 0.2F, 0.2F, 0.3F};
        if (const auto* ist = m_reg->try_get<components::ScrollBarInteractionState>(entity))
        {
            if (ist->trackHovered || ist->scrollbarHovered || ist->scrollbarPressed)
            {
                trackColor = {0.25F, 0.25F, 0.25F, 0.5F};
            }
        }
        return trackColor;
    }

    [[nodiscard]] Eigen::Vector4f verticalThumbColor(entt::entity entity) const
    {
        Eigen::Vector4f thumbColor = {0.6F, 0.6F, 0.6F, 0.7F};
        if (const auto* ist = m_reg->try_get<components::ScrollBarInteractionState>(entity))
        {
            if (ist->scrollbarPressed)
            {
                thumbColor = {0.5F, 0.5F, 0.5F, 0.95F};
            }
            else if (ist->scrollbarHovered)
            {
                thumbColor = {0.7F, 0.7F, 0.7F, 0.85F};
            }
        }
        return thumbColor;
    }

    void drawVerticalScrollBar(entt::entity entity,
                               const Eigen::Vector2f& pos,
                               const Eigen::Vector2f& size,
                               const components::ScrollArea& scrollArea,
                               float viewportHeight,
                               float alpha,
                               core::RenderContext& context)
    {
        const bool hasVerticalScroll =
            (scrollArea.scroll == policies::Scroll::VERTICAL || scrollArea.scroll == policies::Scroll::BOTH);
        if (!hasVerticalScroll || scrollArea.contentSize.y() <= viewportHeight)
        {
            return;
        }

        const float trackHeight = size.y();
        const float visibleRatio = viewportHeight / scrollArea.contentSize.y();
        const float thumbSize = std::max(20.0F, trackHeight * visibleRatio);
        const float maxScroll = std::max(0.0F, scrollArea.contentSize.y() - viewportHeight);
        const float scrollRatio =
            maxScroll > 0.0F ? std::clamp(scrollArea.scrollOffset.y() / maxScroll, 0.0F, 1.0F) : 0.0F;
        const float thumbPos = std::clamp((trackHeight - thumbSize) * scrollRatio, 0.0F, trackHeight - thumbSize);

        const float barWidth = 10.0F;
        const float trackWidth = 12.0F;
        const float trackPadding = 2.0F;

        const Eigen::Vector2f trackPos(pos.x() + size.x() - trackWidth - trackPadding, pos.y());
        const Eigen::Vector2f trackSize(trackWidth, size.y());
        drawRoundedRect(trackPos, trackSize, verticalTrackColor(entity), 6.0F, alpha, context);

        const Eigen::Vector2f barPos(pos.x() + size.x() - barWidth - trackPadding - 1.0F, pos.y() + thumbPos + 2.0F);
        const Eigen::Vector2f barSize(barWidth, thumbSize - 4.0F);
        drawRoundedRect(barPos, barSize, verticalThumbColor(entity), 5.0F, alpha, context);
    }

    void drawHorizontalScrollBar(const Eigen::Vector2f& pos,
                                 const Eigen::Vector2f& size,
                                 const components::ScrollArea& scrollArea,
                                 float alpha,
                                 core::RenderContext& context)
    {
        const bool hasHorizontalScroll =
            (scrollArea.scroll == policies::Scroll::HORIZONTAL || scrollArea.scroll == policies::Scroll::BOTH);
        if (!hasHorizontalScroll || scrollArea.contentSize.x() <= size.x())
        {
            return;
        }

        const float trackHeight = 12.0F;
        const float trackPadding = 2.0F;
        const float barHeight = 10.0F;
        const float visibleRatio = size.x() / scrollArea.contentSize.x();
        const float thumbSize = std::max(20.0F, size.x() * visibleRatio);
        const float maxScroll = std::max(0.0F, scrollArea.contentSize.x() - size.x());
        const float scrollRatio =
            maxScroll > 0.0F ? std::clamp(scrollArea.scrollOffset.x() / maxScroll, 0.0F, 1.0F) : 0.0F;
        const float thumbPos = std::clamp((size.x() - thumbSize) * scrollRatio, 0.0F, size.x() - thumbSize);

        const Eigen::Vector2f trackPos(pos.x(), pos.y() + size.y() - trackHeight - trackPadding);
        const Eigen::Vector2f trackSize(size.x(), trackHeight);
        drawRoundedRect(trackPos, trackSize, {0.2F, 0.2F, 0.2F, 0.3F}, 6.0F, alpha, context);

        const Eigen::Vector2f barPos(pos.x() + thumbPos + 1.0F, pos.y() + size.y() - barHeight - trackPadding - 1.0F);
        const Eigen::Vector2f barSize(thumbSize - 4.0F, barHeight - 2.0F);
        drawRoundedRect(barPos, barSize, {0.6F, 0.6F, 0.6F, 0.7F}, 5.0F, alpha, context);
    }

    void drawScrollBars(entt::entity entity,
                        const Eigen::Vector2f& pos,
                        const Eigen::Vector2f& size,
                        const components::ScrollArea& scrollArea,
                        float alpha,
                        core::RenderContext& context)
    {
        float viewportHeight = size.y();
        if (const auto* padding = m_reg->try_get<components::Padding>(entity))
        {
            viewportHeight = std::max(0.0F, size.y() - padding->values.x() - padding->values.z());
        }

        drawVerticalScrollBar(entity, pos, size, scrollArea, viewportHeight, alpha, context);
        drawHorizontalScrollBar(pos, size, scrollArea, alpha, context);
    }

    Registry* m_reg = nullptr;
};

} // namespace ui::renderers
