/**
 * ************************************************************************
 *
 * @file SliderRenderer.hpp
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2026-03-13
 * @version 0.1
 * @brief  滑动条渲染器，负责渲染滑动条组件的视觉元素，包括轨道和填充部分。
 *
 * ************************************************************************
 * @copyright Copyright (c) 2026 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */
#pragma once
#include "../interface/IRenderer.hpp"
#include "../singleton/Registry.hpp"
#include "../common/components/Data.hpp"
#include "../managers/BatchManager.hpp"
#include <SDL3/SDL_gpu.h>

namespace ui::renderers
{

class SliderRenderer : public core::IRenderer
{
public:
    SliderRenderer() = default;

    bool canHandle(entt::entity entity) const override { return Registry::AnyOf<components::SliderInfo>(entity); }

    void collect(entt::entity entity, core::RenderContext& context) override
    {
        if (context.batchManager == nullptr || context.deviceManager == nullptr || context.whiteTexture == nullptr)
        {
            return;
        }

        const auto* sliderPtr = Registry::TryGet<components::SliderInfo>(entity);
        if (sliderPtr == nullptr)
        {
            return;
        }
        const auto& info = *sliderPtr;

        const bool isVertical = (info.vertical == policies::Orientation::VERTICAL);
        const float trackW = context.size.x();
        const float trackH = context.size.y();
        const Eigen::Vector2f trackPos = context.position;

        const float thickness = info.trackThickness;

        // ---- 辅助 lambda：提交一个圆角矩形批次 ----
        auto submitRect =
            [&](const Eigen::Vector2f& pos, const Eigen::Vector2f& size, const Color& color, float radiusVal)
        {
            render::UiPushConstants pushConst{};
            pushConst.screen_size[0] = context.screenWidth;
            pushConst.screen_size[1] = context.screenHeight;
            pushConst.rect_size[0] = size.x();
            pushConst.rect_size[1] = size.y();
            pushConst.radius[0] = pushConst.radius[1] = pushConst.radius[2] = pushConst.radius[3] = radiusVal;
            pushConst.opacity = context.alpha;
            context.batchManager->beginBatch(context.whiteTexture, context.currentScissor, pushConst);
            const Eigen::Vector4f col{color.red, color.green, color.blue, color.alpha * context.alpha};
            context.batchManager->addRect(pos, size, col);
        };

        // ---- 进度比 ----
        float progress = 0.0F;
        if (info.maxValue > info.minValue)
        {
            progress = std::clamp((info.currentValue - info.minValue) / (info.maxValue - info.minValue), 0.0F, 1.0F);
        }

        if (isVertical)
        {
            // 轨道：水平居中，全高
            const float trkX = trackPos.x() + ((trackW - thickness) * 0.5F);
            submitRect({trkX, trackPos.y()}, {thickness, trackH}, info.trackColor, thickness * 0.5F);

            // 填充（从底部往上）
            const float fillH = trackH * progress;
            if (fillH > 0.001F)
            {
                submitRect({trkX, trackPos.y() + trackH - fillH}, {thickness, fillH}, info.fillColor, thickness * 0.5F);
            }

            // 滑块圆形
            const float thumbDia = info.thumbSize;
            const float thumbRad = info.thumbRadius;
            const float thumbX = trackPos.x() + ((trackW - thumbDia) * 0.5F);
            const float thumbY = (trackPos.y() + (trackH * (1.0F - progress))) - (thumbDia * 0.5F);
            submitRect({thumbX, thumbY}, {thumbDia, thumbDia}, info.thumbColor, thumbRad);
        }
        else
        {
            // 轨道：垂直居中，全宽
            const float trkY = trackPos.y() + ((trackH - thickness) * 0.5F);
            submitRect({trackPos.x(), trkY}, {trackW, thickness}, info.trackColor, thickness * 0.5F);

            // 填充（从左往右）
            const float fillW = trackW * progress;
            if (fillW > 0.001F)
            {
                submitRect({trackPos.x(), trkY}, {fillW, thickness}, info.fillColor, thickness * 0.5F);
            }

            // 滑块圆形
            const float thumbDia = info.thumbSize;
            const float thumbRad = info.thumbRadius;
            const float thumbX = (trackPos.x() + (trackW * progress)) - (thumbDia * 0.5F);
            const float thumbY = trackPos.y() + ((trackH - thumbDia) * 0.5F);
            submitRect({thumbX, thumbY}, {thumbDia, thumbDia}, info.thumbColor, thumbRad);
        }
    }

    int getPriority() const override { return 10; }
};

} // namespace ui::renderers
