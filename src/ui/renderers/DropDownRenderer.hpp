/**
 * ************************************************************************
 *
 * @file DropDownRenderer.hpp
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2026-05-18
 * @version 0.1
 * @brief 下拉框渲染器 — 绘制选框背景、当前选中文字、箭头图示
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

class DropDownRenderer : public core::IRenderer
{
public:
    DropDownRenderer() = default;

    bool canHandle(entt::entity entity) const override
    {
        return Registry::AnyOf<components::DropDownTag>(entity);
    }

    void collect(entt::entity entity, core::RenderContext& context) override
    {
        if (context.batchManager == nullptr || context.whiteTexture == nullptr) return;

        const auto* dropDown = Registry::TryGet<components::DropDown>(entity);
        if (dropDown == nullptr) return;

        constexpr float ARROW_W  = 16.0F;
        constexpr float RADIUS   = 4.0F;
        constexpr float ARROW_H  = 5.0F;   // 箭头矩形高度（视觉近似）

        render::UiPushConstants pushConst{};
        pushConst.screen_size[0] = context.screenWidth;
        pushConst.screen_size[1] = context.screenHeight;
        pushConst.rect_size[0]   = context.size.x();
        pushConst.rect_size[1]   = context.size.y();
        pushConst.opacity        = context.alpha;
        pushConst.radius[0] = pushConst.radius[1] = pushConst.radius[2] = pushConst.radius[3] = RADIUS;

        // ── 背景已由 ShapeRenderer 绘制，这里只画箭头标志 ─────
        // 右侧三角指示区域（深色背景条）
        const float arrowX = context.position.x() + context.size.x() - ARROW_W;
        const Eigen::Vector2f arrowBgPos{arrowX, context.position.y()};
        const Eigen::Vector2f arrowBgSize{ARROW_W, context.size.y()};

        render::UiPushConstants arrowBgPc = pushConst;
        arrowBgPc.rect_size[0] = arrowBgSize.x();
        arrowBgPc.rect_size[1] = arrowBgSize.y();
        arrowBgPc.radius[0] = arrowBgPc.radius[1] = 0.0F; // 左侧不圆角
        arrowBgPc.radius[2] = arrowBgPc.radius[3] = RADIUS;

        const Eigen::Vector4f arrowBgColor{0.18F, 0.18F, 0.22F, context.alpha};
        context.batchManager->beginBatch(context.whiteTexture, context.currentScissor, arrowBgPc);
        context.batchManager->addRect(arrowBgPos, arrowBgSize, arrowBgColor);

        // 用扁矩形近似箭头（▼）
        const float arrowMidY = context.position.y() + ((context.size.y() - ARROW_H) * 0.5F);
        const float arrowMidX = arrowX + ((ARROW_W - 8.0F) * 0.5F);
        const Eigen::Vector2f arrowPos{arrowMidX, arrowMidY};
        const Eigen::Vector2f arrowSize{8.0F, ARROW_H};

        render::UiPushConstants arrowPc = pushConst;
        arrowPc.rect_size[0] = arrowSize.x();
        arrowPc.rect_size[1] = arrowSize.y();
        arrowPc.radius[0] = arrowPc.radius[1] = arrowPc.radius[2] = arrowPc.radius[3] = 1.0F;

        const Eigen::Vector4f arrowColor{dropDown->arrowColor.red, dropDown->arrowColor.green,
                                         dropDown->arrowColor.blue, dropDown->arrowColor.alpha};
        context.batchManager->beginBatch(context.whiteTexture, context.currentScissor, arrowPc);
        context.batchManager->addRect(arrowPos, arrowSize, arrowColor);
    }

    int getPriority() const override { return 8; }
};

} // namespace ui::renderers
