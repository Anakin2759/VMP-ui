/**
 * ************************************************************************
 *
 * @file CheckBoxRenderer.hpp
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2026-05-18
 * @version 0.1
 * @brief 复选框渲染器 — 绘制方框、勾选标记与标签文字
 *
 * ************************************************************************
 * @copyright Copyright (c) 2026 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */
#pragma once

#include "interface/IRenderer.hpp"
#include "singleton/Registry.hpp"
#include "common/components/Data.hpp"
#include "common/Tags.hpp"
#include "managers/BatchManager.hpp"

#include <SDL3/SDL_gpu.h>

namespace ui::renderers
{

class CheckBoxRenderer : public core::IRenderer
{
public:
    CheckBoxRenderer() = default;

    bool canHandle(entt::entity entity) const override { return Registry::AnyOf<components::CheckBoxTag>(entity); }

    void collect(entt::entity entity, core::RenderContext& context) override
    {
        if (context.batchManager == nullptr || context.whiteTexture == nullptr) return;

        const auto* checkBox = Registry::TryGet<components::CheckBox>(entity);
        if (checkBox == nullptr) return;

        constexpr float BOX_SIZE = 16.0F;
        constexpr float BOX_RADIUS = 3.0F;
        constexpr float BOX_MARGIN = 4.0F; // box 与左边缘距离

        render::UiPushConstants pushConst{};
        pushConst.screen_size[0] = context.screenWidth;
        pushConst.screen_size[1] = context.screenHeight;
        pushConst.opacity = context.alpha;
        pushConst.radius[0] = pushConst.radius[1] = pushConst.radius[2] = pushConst.radius[3] = BOX_RADIUS;

        // ── 方框背景 ──────────────────────────────────────────
        const float boxY = context.position.y() + ((context.size.y() - BOX_SIZE) * 0.5F);
        const Eigen::Vector2f boxPos{context.position.x() + BOX_MARGIN, boxY};
        const Eigen::Vector2f boxSize{BOX_SIZE, BOX_SIZE};

        pushConst.rect_size[0] = boxSize.x();
        pushConst.rect_size[1] = boxSize.y();

        const Eigen::Vector4f boxColor{
            checkBox->boxColor.red, checkBox->boxColor.green, checkBox->boxColor.blue, checkBox->boxColor.alpha};
        context.batchManager->beginBatch(context.whiteTexture, context.currentScissor, pushConst);
        context.batchManager->addRect(boxPos, boxSize, boxColor);

        // ── 勾选填充（选中时） ─────────────────────────────────
        if (checkBox->checked)
        {
            constexpr float INSET = 3.0F;
            const Eigen::Vector2f fillPos{boxPos.x() + INSET, boxPos.y() + INSET};
            const Eigen::Vector2f fillSize{(BOX_SIZE - (2.0F * INSET)), (BOX_SIZE - (2.0F * INSET))};

            render::UiPushConstants fillPc = pushConst;
            fillPc.rect_size[0] = fillSize.x();
            fillPc.rect_size[1] = fillSize.y();
            fillPc.radius[0] = fillPc.radius[1] = fillPc.radius[2] = fillPc.radius[3] = 2.0F;

            const Eigen::Vector4f checkColor{checkBox->checkColor.red,
                                             checkBox->checkColor.green,
                                             checkBox->checkColor.blue,
                                             checkBox->checkColor.alpha};
            context.batchManager->beginBatch(context.whiteTexture, context.currentScissor, fillPc);
            context.batchManager->addRect(fillPos, fillSize, checkColor);
        }
    }

    int getPriority() const override { return 7; }
};

} // namespace ui::renderers
