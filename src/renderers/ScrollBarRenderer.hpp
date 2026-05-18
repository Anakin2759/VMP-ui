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
#include "../interface/IRenderer.hpp"
#include "../singleton/Registry.hpp"
#include "../common/Components.hpp"
#include "../managers/BatchManager.hpp"
#include <SDL3/SDL_gpu.h>

namespace ui::renderers
{

/**
 * @brief 滚动条渲染器
 *
 * 负责渲染：
 * - 垂直滚动条
 * - 水平滚动条
 */
class ScrollBarRenderer : public core::IRenderer
{
public:
    ScrollBarRenderer() = default;

    bool canHandle(entt::entity entity) const override { return Registry::AnyOf<components::ScrollArea>(entity); }

    void collect(entt::entity entity, core::RenderContext& context) override
    {
        if (context.batchManager == nullptr || context.deviceManager == nullptr)
        {
            return;
        }

        const auto* scrollArea = Registry::TryGet<components::ScrollArea>(entity);
        if (scrollArea == nullptr || policies::HasFlag(scrollArea->scrollBar, policies::ScrollBar::NoVisibility))
        {
            return;
        }
        // 渲染滚动条 (在裁剪之前)
        drawScrollBars(entity, context.position, context.size, *scrollArea, context.alpha, context);
    }

    int getPriority() const override
    {
        return 30; // 滚动条在所有内容渲染之后，裁剪区域弹出之前渲染
    }

private:
    void drawScrollBars(entt::entity entity,
                        const Eigen::Vector2f& pos,
                        const Eigen::Vector2f& size,
                        const components::ScrollArea& scrollArea,
                        float alpha,
                        core::RenderContext& context)
    {
        float viewportHeight = size.y();
        if (const auto* padding = Registry::TryGet<components::Padding>(entity))
        {
            viewportHeight = std::max(0.0F, size.y() - padding->values.x() - padding->values.z());
        }

        // 简单绘制一个垂直滚动条
        bool hasVerticalScroll =
            (scrollArea.scroll == policies::Scroll::Vertical || scrollArea.scroll == policies::Scroll::Both);
        if (hasVerticalScroll && scrollArea.contentSize.y() > viewportHeight)
        {
            float trackHeight = size.y();
            float visibleRatio = viewportHeight / scrollArea.contentSize.y();
            float thumbSize = std::max(20.0F, trackHeight * visibleRatio);
            float maxScroll = std::max(0.0F, scrollArea.contentSize.y() - viewportHeight);
            float scrollRatio =
                maxScroll > 0.0F ? std::clamp(scrollArea.scrollOffset.y() / maxScroll, 0.0F, 1.0F) : 0.0F;
            float thumbPos = (trackHeight - thumbSize) * scrollRatio;

            // 确保滑块位置不超出轨道
            thumbPos = std::clamp(thumbPos, 0.0F, trackHeight - thumbSize);

            // 滚动条样式参数
            float barWidth = 10.0F;
            float trackWidth = 12.0F;
            float trackPadding = 2.0F;

            // 轨道位置和大小
            Eigen::Vector2f trackPos(pos.x() + size.x() - trackWidth - trackPadding, pos.y());
            Eigen::Vector2f trackSize(trackWidth, size.y());

            // 根据交互状态调整轨道颜色
            Eigen::Vector4f trackColor = {0.2F, 0.2F, 0.2F, 0.3F}; // 默认半透明深色
            if (scrollArea.trackHovered || scrollArea.scrollbarHovered || scrollArea.scrollbarPressed)
            {
                trackColor = {0.25F, 0.25F, 0.25F, 0.5F}; // 悬停时更明显
            }

            // 绘制轨道背景
            render::UiPushConstants trackPushConstants{};
            trackPushConstants.screen_size[0] = context.screenWidth;
            trackPushConstants.screen_size[1] = context.screenHeight;
            trackPushConstants.rect_size[0] = trackSize.x();
            trackPushConstants.rect_size[1] = trackSize.y();
            trackPushConstants.radius[0] = 6.0F; // 圆角轨道
            trackPushConstants.radius[1] = 6.0F;
            trackPushConstants.radius[2] = 6.0F;
            trackPushConstants.radius[3] = 6.0F;
            trackPushConstants.opacity = alpha;
            trackPushConstants.shadow_soft = 0.0F;
            trackPushConstants.shadow_offset_x = 0.0F;
            trackPushConstants.shadow_offset_y = 0.0F;

            context.batchManager->beginBatch(context.whiteTexture, context.currentScissor, trackPushConstants);
            context.batchManager->addRect(trackPos, trackSize, trackColor);

            // 滑块位置和大小
            Eigen::Vector2f barPos(pos.x() + size.x() - barWidth - trackPadding - 1.0F, pos.y() + thumbPos + 2.0F);
            Eigen::Vector2f barSize(barWidth, thumbSize - 4.0F); // 稍微小一点，留出间隙

            // 根据交互状态调整滑块颜色
            Eigen::Vector4f thumbColor = {0.6F, 0.6F, 0.6F, 0.7F}; // 默认中灰色
            if (scrollArea.scrollbarPressed)
            {
                thumbColor = {0.5F, 0.5F, 0.5F, 0.95F}; // 按下时更暗更不透明
            }
            else if (scrollArea.scrollbarHovered)
            {
                thumbColor = {0.7F, 0.7F, 0.7F, 0.85F}; // 悬停时更亮
            }

            // 绘制滑块
            render::UiPushConstants pushConstants{};
            pushConstants.screen_size[0] = context.screenWidth;
            pushConstants.screen_size[1] = context.screenHeight;
            pushConstants.rect_size[0] = barSize.x();
            pushConstants.rect_size[1] = barSize.y();
            pushConstants.radius[0] = 5.0F;
            pushConstants.radius[1] = 5.0F;
            pushConstants.radius[2] = 5.0F;
            pushConstants.radius[3] = 5.0F;
            pushConstants.opacity = alpha;
            pushConstants.shadow_soft = 0.0F;
            pushConstants.shadow_offset_x = 0.0F;
            pushConstants.shadow_offset_y = 0.0F;

            context.batchManager->beginBatch(context.whiteTexture, context.currentScissor, pushConstants);
            context.batchManager->addRect(barPos, barSize, thumbColor);
        }
    }
};

} // namespace ui::renderers
