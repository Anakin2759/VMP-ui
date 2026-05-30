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

#include "interface/IRenderer.hpp"
#include "singleton/Registry.hpp"
#include "common/components/Data.hpp"
#include "common/Tags.hpp"
#include "managers/BatchManager.hpp"

#include <SDL3/SDL_gpu.h>

namespace ui::renderers
{

class DropDownRenderer : public core::IRenderer
{
public:
    explicit DropDownRenderer(Registry& reg) : m_reg(&reg) {}

    [[nodiscard]] bool canHandle(entt::entity entity) const override
    {
        return m_reg->any_of<components::DropDownTag>(entity);
    }

    void collect(entt::entity entity, core::RenderContext& context) override
    {
        if (context.batchManager == nullptr || context.whiteTexture == nullptr) return;

        const auto* dropDown = m_reg->try_get<components::DropDown>(entity);
        if (dropDown == nullptr) return;

        constexpr float ARROW_W = 16.0F;
        constexpr float RADIUS = 4.0F;
        constexpr float TRI_W = 10.0F; // 下箭头三角形宽度
        constexpr float TRI_H = 6.0F;  // 下箭头三角形高度

        render::UiPushConstants pushConst{};
        pushConst.screen_size[0] = context.screenWidth;
        pushConst.screen_size[1] = context.screenHeight;
        pushConst.rect_size[0] = context.size.x();
        pushConst.rect_size[1] = context.size.y();
        pushConst.opacity = context.alpha;
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

        // ── 下箭头三角形 (▼) — 由三个顶点组成 ─────────────────
        const float triX = arrowX + ((ARROW_W - TRI_W) * 0.5F);
        const float triY = context.position.y() + ((context.size.y() - TRI_H) * 0.5F);

        render::UiPushConstants triPc = pushConst;
        triPc.rect_size[0] = TRI_W;
        triPc.rect_size[1] = TRI_H;
        triPc.radius[0] = triPc.radius[1] = triPc.radius[2] = triPc.radius[3] = 0.0F;
        triPc.draw_mode = 0.0F;

        const Eigen::Vector4f arrowColor{dropDown->arrowColor.red,
                                         dropDown->arrowColor.green,
                                         dropDown->arrowColor.blue,
                                         dropDown->arrowColor.alpha};

        context.batchManager->beginBatch(context.whiteTexture, context.currentScissor, triPc);
        const auto baseIndex = static_cast<uint16_t>(context.batchManager->currentVertexCount());

        const auto makeVertex = [&](float posX, float posY, float uvX, float uvY)
        {
            render::Vertex vtx{};
            vtx.position[0] = posX;
            vtx.position[1] = posY;
            vtx.texCoord[0] = uvX;
            vtx.texCoord[1] = uvY;
            vtx.color[0] = arrowColor.x();
            vtx.color[1] = arrowColor.y();
            vtx.color[2] = arrowColor.z();
            vtx.color[3] = arrowColor.w();
            // 与 addRect 一致：写入逐顶点 SDF 参数，使 frag shader 视作矩形内部（body_mask≈1）
            vtx.rect_size[0] = TRI_W;
            vtx.rect_size[1] = TRI_H;
            vtx.radius[0] = vtx.radius[1] = vtx.radius[2] = vtx.radius[3] = 0.0F;
            vtx.shadow_params[0] = 0.0F;
            vtx.shadow_params[1] = 0.0F;
            vtx.shadow_params[2] = 0.0F;
            vtx.shadow_params[3] = context.alpha;
            vtx.mode_params[0] = 0.0F; // padding_flag = 0 → 走主体 SDF 分支
            vtx.mode_params[1] = 0.0F;
            vtx.mode_params[2] = 0.0F; // draw_mode = 填充
            vtx.mode_params[3] = 0.0F;
            return vtx;
        };

        context.batchManager->addVertex(makeVertex(triX, triY, 0.0F, 0.0F));                          // 左上
        context.batchManager->addVertex(makeVertex(triX + TRI_W, triY, 1.0F, 0.0F));                  // 右上
        context.batchManager->addVertex(makeVertex(triX + (TRI_W * 0.5F), triY + TRI_H, 0.5F, 1.0F)); // 下中
        context.batchManager->addIndex(baseIndex);
        context.batchManager->addIndex(static_cast<uint16_t>(baseIndex + 1));
        context.batchManager->addIndex(static_cast<uint16_t>(baseIndex + 2));
    }

    int getPriority() const override { return 8; }

private:
    Registry* m_reg = nullptr;
};

} // namespace ui::renderers
