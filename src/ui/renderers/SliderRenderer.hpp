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
#include "../common/Components.hpp"
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
            return;

        const auto* s = Registry::TryGet<components::SliderInfo>(entity);
        if (!s) return;

        // draw track (background)
        Eigen::Vector4f trackColor{0.28F, 0.28F, 0.28F, 1.0F};
        Eigen::Vector2f trackPos = context.position;
        Eigen::Vector2f trackSize = context.size;

        // make track thinner in the minor axis
        if (s->vertical == policies::Orientation::Vertical)
        {
            trackSize.x() = std::max(8.0F, trackSize.x());
        }
        else
        {
            trackSize.y() = std::max(8.0F, trackSize.y());
        }

        render::UiPushConstants pc{};
        pc.screen_size[0] = context.screenWidth;
        pc.screen_size[1] = context.screenHeight;
        pc.rect_size[0] = trackSize.x();
        pc.rect_size[1] = trackSize.y();
        pc.radius[0] = 6.0F;
        pc.radius[1] = 6.0F;
        pc.radius[2] = 6.0F;
        pc.radius[3] = 6.0F;
        pc.opacity = context.alpha;

        context.batchManager->beginBatch(context.whiteTexture, context.currentScissor, pc);
        context.batchManager->addRect(trackPos, trackSize, trackColor);

        // draw fill/thumb
        float progress = 0.0F;
        if (s->maxValue > s->minValue)
            progress = std::clamp((s->currentValue - s->minValue) / (s->maxValue - s->minValue), 0.0F, 1.0F);

        Eigen::Vector4f thumbColor{0.2F, 0.6F, 1.0F, 1.0F};

        if (s->vertical == policies::Orientation::Vertical)
        {
            float fillH = trackSize.y() * progress;
            Eigen::Vector2f fillPos(trackPos.x(), trackPos.y() + trackSize.y() - fillH);
            Eigen::Vector2f fillSize(trackSize.x(), fillH);
            pc.rect_size[0] = fillSize.x();
            pc.rect_size[1] = fillSize.y();
            context.batchManager->beginBatch(context.whiteTexture, context.currentScissor, pc);
            context.batchManager->addRect(fillPos, fillSize, thumbColor);
        }
        else
        {
            float fillW = trackSize.x() * progress;
            Eigen::Vector2f fillPos(trackPos.x(), trackPos.y());
            Eigen::Vector2f fillSize(fillW, trackSize.y());
            pc.rect_size[0] = fillSize.x();
            pc.rect_size[1] = fillSize.y();
            context.batchManager->beginBatch(context.whiteTexture, context.currentScissor, pc);
            context.batchManager->addRect(fillPos, fillSize, thumbColor);
        }
    }

    int getPriority() const override { return 10; }
};

} // namespace ui::renderers
