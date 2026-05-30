/**
 * ProgressBarRenderer.hpp
 * Render a progress bar (background + filled portion)
 */
#pragma once
#include "interface/IRenderer.hpp"
#include "singleton/Registry.hpp"
#include "common/components/Data.hpp"
#include "managers/BatchManager.hpp"
#include <SDL3/SDL_gpu.h>

namespace ui::renderers
{

class ProgressBarRenderer : public core::IRenderer
{
public:
    explicit ProgressBarRenderer(Registry& reg) : m_reg(&reg) {}

    bool canHandle(entt::entity entity) const override { return m_reg->any_of<components::ProgressBar>(entity); }

    void collect(entt::entity entity, core::RenderContext& context) override
    {
        if (context.batchManager == nullptr || context.deviceManager == nullptr || context.whiteTexture == nullptr)
        {
            return;
        }

        const auto* progressBar = m_reg->try_get<components::ProgressBar>(entity);
        if (progressBar == nullptr) return;

        // background
        render::UiPushConstants pc{};
        pc.screen_size[0] = context.screenWidth;
        pc.screen_size[1] = context.screenHeight;
        pc.rect_size[0] = context.size.x();
        pc.rect_size[1] = context.size.y();
        pc.radius[0] = 4.0F;
        pc.radius[1] = 4.0F;
        pc.radius[2] = 4.0F;
        pc.radius[3] = 4.0F;
        pc.opacity = context.alpha;

        Eigen::Vector4f bgColor(progressBar->backgroundColor.red,
                                progressBar->backgroundColor.green,
                                progressBar->backgroundColor.blue,
                                progressBar->backgroundColor.alpha);
        context.batchManager->beginBatch(context.whiteTexture, context.currentScissor, pc);
        context.batchManager->addRect(context.position, context.size, bgColor);

        // fill
        float progress = std::clamp(progressBar->progress, 0.0F, 1.0F);
        Eigen::Vector2f fillSize(context.size.x() * progress, context.size.y());
        Eigen::Vector2f fillPos = context.position;

        render::UiPushConstants fillPc = pc;
        fillPc.rect_size[0] = fillSize.x();
        fillPc.rect_size[1] = fillSize.y();

        Eigen::Vector4f fillColor(progressBar->fillColor.red,
                                  progressBar->fillColor.green,
                                  progressBar->fillColor.blue,
                                  progressBar->fillColor.alpha);
        context.batchManager->beginBatch(context.whiteTexture, context.currentScissor, fillPc);
        context.batchManager->addRect(fillPos, fillSize, fillColor);
    }

    int getPriority() const override { return 5; }

private:
    Registry* m_reg = nullptr;
};

} // namespace ui::renderers
