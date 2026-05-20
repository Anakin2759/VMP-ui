/**
 * ************************************************************************
 *
 * @file CanvasRenderer.hpp
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2026-05-18
 * @version 0.1
 * @brief Canvas 渲染器 - 将 CanvasDrawList 中的绘图命令转换为 GPU 批次
 *
 * ************************************************************************
 * @copyright Copyright (c) 2026 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */
#pragma once

#include <cmath>
#include "../interface/IRenderer.hpp"
#include "../singleton/Registry.hpp"
#include "../common/Components.hpp"
#include "../common/Tags.hpp"
#include "../managers/BatchManager.hpp"
#include <SDL3/SDL_gpu.h>

namespace ui::renderers
{

/**
 * @brief Canvas 渲染器
 *
 * 负责渲染 CanvasTag + CanvasDrawList 实体。
 * 将绘图命令转换为 BatchManager 的 addRect 调用（线段/矩形/圆形近似）。
 */
class CanvasRenderer : public core::IRenderer
{
public:
    CanvasRenderer() = default;

    [[nodiscard]] bool canHandle(entt::entity entity) const override
    {
        return Registry::AnyOf<components::CanvasTag>(entity);
    }

    void collect(entt::entity entity, core::RenderContext& context) override
    {
        if (context.batchManager == nullptr || context.whiteTexture == nullptr)
        {
            return;
        }

        const auto* drawList = Registry::TryGet<components::CanvasDrawList>(entity);
        if (drawList == nullptr || drawList->commands.empty())
        {
            return;
        }

        const Eigen::Vector2f origin{context.position.x(), context.position.y()};

        for (const auto& cmd : drawList->commands)
        {
            switch (cmd.type)
            {
                case components::CanvasDrawType::LINE:
                    renderLine(cmd, origin, context);
                    break;
                case components::CanvasDrawType::RECT:
                    renderRectOutline(cmd, origin, context);
                    break;
                case components::CanvasDrawType::FILLED_RECT:
                    renderFilledRect(cmd, origin, context);
                    break;
                case components::CanvasDrawType::CIRCLE:
                    renderCircleOutline(cmd, origin, context);
                    break;
                case components::CanvasDrawType::FILLED_CIRCLE:
                    renderFilledCircle(cmd, origin, context);
                    break;
                case components::CanvasDrawType::POLYLINE:
                    renderPolyline(cmd, origin, context);
                    break;
                case components::CanvasDrawType::CUBIC_BEZIER:
                    renderCubicBezier(cmd, origin, context);
                    break;
            }
        }
    }

    [[nodiscard]] int getPriority() const override { return 20; }

private:
    static constexpr int CIRCLE_SEGMENTS = 24;

    static Eigen::Vector4f toVec4(const Color& color, float alpha)
    {
        return {color.red, color.green, color.blue, color.alpha * alpha};
    }

    static void beginWhiteBatch(core::RenderContext& context)
    {
        render::UiPushConstants pushConst{};
        pushConst.screen_size[0] = context.screenWidth;
        pushConst.screen_size[1] = context.screenHeight;
        pushConst.opacity = context.alpha;
        context.batchManager->beginBatch(context.whiteTexture, context.currentScissor, pushConst);
    }

    static void renderLine(const components::CanvasDrawCommand& cmd,
                           const Eigen::Vector2f& origin,
                           core::RenderContext& context)
    {
        const float startX = origin.x() + cmd.p1.x();
        const float startY = origin.y() + cmd.p1.y();
        const float endX   = origin.x() + cmd.p2.x();
        const float endY   = origin.y() + cmd.p2.y();
        renderSegment(startX, startY, endX, endY, cmd.lineWidth, toVec4(cmd.color, context.alpha), context);
    }

    static void renderFilledRect(const components::CanvasDrawCommand& cmd,
                                 const Eigen::Vector2f& origin,
                                 core::RenderContext& context)
    {
        const float posX   = origin.x() + cmd.p1.x();
        const float posY   = origin.y() + cmd.p1.y();
        const float width  = cmd.p2.x() - cmd.p1.x();
        const float height = cmd.p2.y() - cmd.p1.y();

        beginWhiteBatch(context);
        context.batchManager->addRect({posX, posY}, {width, height}, toVec4(cmd.color, context.alpha));
    }

    static void renderRectOutline(const components::CanvasDrawCommand& cmd,
                                  const Eigen::Vector2f& origin,
                                  core::RenderContext& context)
    {
        const float left    = origin.x() + cmd.p1.x();
        const float top     = origin.y() + cmd.p1.y();
        const float right   = origin.x() + cmd.p2.x();
        const float bottom  = origin.y() + cmd.p2.y();
        const float lineW   = cmd.lineWidth;
        const Eigen::Vector4f col = toVec4(cmd.color, context.alpha);

        beginWhiteBatch(context);
        // 上边
        context.batchManager->addRect({left, top}, {right - left, lineW}, col);
        // 下边
        context.batchManager->addRect({left, bottom - lineW}, {right - left, lineW}, col);
        // 左边
        context.batchManager->addRect({left, top + lineW}, {lineW, bottom - top - (2 * lineW)}, col);
        // 右边
        context.batchManager->addRect({right - lineW, top + lineW}, {lineW, bottom - top - (2 * lineW)}, col);
    }

    static void renderFilledCircle(const components::CanvasDrawCommand& cmd,
                                   const Eigen::Vector2f& origin,
                                   core::RenderContext& context)
    {
        const float centerX = origin.x() + cmd.p1.x();
        const float centerY = origin.y() + cmd.p1.y();
        const float radius  = cmd.p2.x();
        const Eigen::Vector4f col = toVec4(cmd.color, context.alpha);

        render::UiPushConstants pushConst{};
        pushConst.screen_size[0] = context.screenWidth;
        pushConst.screen_size[1] = context.screenHeight;
        pushConst.rect_size[0]   = 2.0F * radius;
        pushConst.rect_size[1]   = 2.0F * radius;
        pushConst.radius[0]      = radius;
        pushConst.radius[1]      = radius;
        pushConst.radius[2]      = radius;
        pushConst.radius[3]      = radius;
        pushConst.opacity        = context.alpha;
        pushConst.draw_mode      = 0.0F; // 填充

        context.batchManager->beginBatch(context.whiteTexture, context.currentScissor, pushConst);
        context.batchManager->addRect({centerX - radius, centerY - radius}, {2.0F * radius, 2.0F * radius}, col);
    }

    static void renderCircleOutline(const components::CanvasDrawCommand& cmd,
                                    const Eigen::Vector2f& origin,
                                    core::RenderContext& context)
    {
        const float centerX = origin.x() + cmd.p1.x();
        const float centerY = origin.y() + cmd.p1.y();
        const float radius  = cmd.p2.x();
        const float lineW   = cmd.lineWidth;
        const Eigen::Vector4f col = toVec4(cmd.color, context.alpha);

        render::UiPushConstants pushConst{};
        pushConst.screen_size[0] = context.screenWidth;
        pushConst.screen_size[1] = context.screenHeight;
        pushConst.rect_size[0]   = 2.0F * radius;
        pushConst.rect_size[1]   = 2.0F * radius;
        pushConst.radius[0]      = radius;
        pushConst.radius[1]      = radius;
        pushConst.radius[2]      = radius;
        pushConst.radius[3]      = radius;
        pushConst.opacity        = context.alpha;
        pushConst.draw_mode      = 1.0F; // 描边
        pushConst.stroke_width   = lineW;

        context.batchManager->beginBatch(context.whiteTexture, context.currentScissor, pushConst);
        context.batchManager->addRect({centerX - radius, centerY - radius}, {2.0F * radius, 2.0F * radius}, col);
    }

    // ---- 折线渲染：逐段绘制为带方向感知的矩形 ----
    static void renderPolyline(const components::CanvasDrawCommand& cmd,
                               const Eigen::Vector2f& origin,
                               core::RenderContext& context)
    {
        if (cmd.points.size() < 2) return;

        const Eigen::Vector4f col = toVec4(cmd.color, context.alpha);
        const std::size_t count = cmd.points.size();

        for (std::size_t i = 0; i + 1 < count; ++i)
        {
            const float startX = origin.x() + cmd.points[i].x();
            const float startY = origin.y() + cmd.points[i].y();
            const float endX   = origin.x() + cmd.points[i + 1].x();
            const float endY   = origin.y() + cmd.points[i + 1].y();
            renderSegment(startX, startY, endX, endY, cmd.lineWidth, col, context);
        }
    }

    // ---- 三次贝塞尔曲线：自适应细分后逐段渲染 ----
    static void renderCubicBezier(const components::CanvasDrawCommand& cmd,
                                  const Eigen::Vector2f& origin,
                                  core::RenderContext& context)
    {
        // 将世界坐标映射到屏幕
        const Vec2 startPt{origin.x() + cmd.p1.x(), origin.y() + cmd.p1.y()};
        const Vec2 cp1{origin.x() + cmd.p2.x(), origin.y() + cmd.p2.y()};
        const Vec2 cp2{origin.x() + cmd.p3.x(), origin.y() + cmd.p3.y()};
        const Vec2 endPt{origin.x() + cmd.p4.x(), origin.y() + cmd.p4.y()};

        // 自适应细分，生成折线顶点
        std::vector<Vec2> pts;
        pts.reserve(32);
        pts.push_back(startPt);
        tessellateCubic(startPt, cp1, cp2, endPt, pts);

        const Eigen::Vector4f col = toVec4(cmd.color, context.alpha);
        const std::size_t count = pts.size();
        for (std::size_t i = 0; i + 1 < count; ++i)
        {
            renderSegment(pts[i].x(), pts[i].y(), pts[i + 1].x(), pts[i + 1].y(), cmd.lineWidth, col, context);
        }
    }

    // ---- 开始胶囊批次（draw_mode=2, rect_size=[totalW, totalH]）----
    static void beginCapsuleBatch(float totalWidth, float totalHeight,
                                  core::RenderContext& context)
    {
        render::UiPushConstants pushConst{};
        pushConst.screen_size[0] = context.screenWidth;
        pushConst.screen_size[1] = context.screenHeight;
        pushConst.rect_size[0]   = totalWidth;
        pushConst.rect_size[1]   = totalHeight;
        pushConst.draw_mode      = 2.0F; // capsule SDF
        pushConst.opacity        = context.alpha;
        context.batchManager->beginBatch(context.whiteTexture, context.currentScissor, pushConst);
    }

    // ---- 通用线段 → 胶囊 SDF（有向旋转四边形，亚像素抗锯齿）----
    static void renderSegment(float startX, float startY, float endX, float endY,
                              float lineWidth,
                              const Eigen::Vector4f& col,
                              core::RenderContext& context)
    {
        const float dirX = endX - startX;
        const float dirY = endY - startY;
        const float len = std::sqrt((dirX * dirX) + (dirY * dirY));
        if (len < 0.001F) return;

        // 单位切向 / 法向
        const float tanX = dirX / len;
        const float tanY = dirY / len;
        const float normX = -tanY;
        const float normY =  tanX;

        // 胶囊外包矩形尺寸（沿切向扩 radius，沿法向扩 radius）
        const float halfRadius = lineWidth * 0.5F;
        // 1px 外扩让 smoothstep 有空间，避免边缘被裁
        const float expand = halfRadius + 1.0F;
        const float totalW = len + (2.0F * expand); // shader 里 half_l = (totalW/2) - halfRadius
        const float totalH = lineWidth + 2.0F;      // shader 里 half_r = totalH/2

        // 中心点
        const float midX = (startX + endX) * 0.5F;
        const float midY = (startY + endY) * 0.5F;

        // 有向四边形的四个角（切向 ± expand，法向 ± (halfRadius+1)）
        const float halfTanX = tanX  * (totalW * 0.5F);
        const float halfTanY = tanY  * (totalW * 0.5F);
        const float halfNrmX = normX * (totalH * 0.5F);
        const float halfNrmY = normY * (totalH * 0.5F);

        // 左上、右上、右下、左下（以线段切向为 X 轴）
        const Eigen::Vector2f vertTopLeft{midX - halfTanX + halfNrmX, midY - halfTanY + halfNrmY};
        const Eigen::Vector2f vertTopRight{midX + halfTanX + halfNrmX, midY + halfTanY + halfNrmY};
        const Eigen::Vector2f vertBotRight{midX + halfTanX - halfNrmX, midY + halfTanY - halfNrmY};
        const Eigen::Vector2f vertBotLeft{midX - halfTanX - halfNrmX, midY - halfTanY - halfNrmY};

        // UV = 归一化本地坐标 [0,1]，shader 内通过 rect_size 还原为像素偏移
        const Eigen::Vector2f uvTopLeft{0.0F, 0.0F};
        const Eigen::Vector2f uvTopRight{1.0F, 0.0F};
        const Eigen::Vector2f uvBotRight{1.0F, 1.0F};
        const Eigen::Vector2f uvBotLeft{0.0F, 1.0F};

        beginCapsuleBatch(totalW, totalH, context);
        context.batchManager->addOrientedQuad(vertTopLeft, vertTopRight, vertBotRight, vertBotLeft,
                                              uvTopLeft, uvTopRight, uvBotRight, uvBotLeft, col);
    }

    // ---- de Casteljau 自适应细分（目标弦误差 ≤ 0.5px²，迭代实现） ----
    static void tessellateCubic(Vec2 ptA, Vec2 ptB, Vec2 ptC, Vec2 ptD,
                                std::vector<Vec2>& out)
    {
        constexpr float FLATNESS_SQ = 0.25F; // 0.5px²
        constexpr int   MAX_DEPTH   = 8;

        struct Seg { Vec2 ptA, ptB, ptC, ptD; int segDepth; };
        std::vector<Seg> segStack;
        segStack.reserve(32);
        segStack.push_back({ptA, ptB, ptC, ptD, 0});

        while (!segStack.empty())
        {
            auto [curA, curB, curC, curD, curDepth] = segStack.back();
            segStack.pop_back();

            if (curDepth >= MAX_DEPTH)
            {
                out.push_back(curD);
                continue;
            }

            const Vec2 vecDA{curD.x() - curA.x(), curD.y() - curA.y()};
            const Vec2 vecBA{curB.x() - curA.x(), curB.y() - curA.y()};
            const Vec2 vecCA{curC.x() - curA.x(), curC.y() - curA.y()};
            const float cross1 = (vecBA.x() * vecDA.y()) - (vecBA.y() * vecDA.x());
            const float cross2 = (vecCA.x() * vecDA.y()) - (vecCA.y() * vecDA.x());

            if (((cross1 * cross1) + (cross2 * cross2)) <= FLATNESS_SQ)
            {
                out.push_back(curD);
                continue;
            }

            const Vec2 midAB{(curA.x() + curB.x()) * 0.5F, (curA.y() + curB.y()) * 0.5F};
            const Vec2 midBC{(curB.x() + curC.x()) * 0.5F, (curB.y() + curC.y()) * 0.5F};
            const Vec2 midCD{(curC.x() + curD.x()) * 0.5F, (curC.y() + curD.y()) * 0.5F};
            const Vec2 midABC{(midAB.x() + midBC.x()) * 0.5F, (midAB.y() + midBC.y()) * 0.5F};
            const Vec2 midBCD{(midBC.x() + midCD.x()) * 0.5F, (midBC.y() + midCD.y()) * 0.5F};
            const Vec2 midPt{(midABC.x() + midBCD.x()) * 0.5F, (midABC.y() + midBCD.y()) * 0.5F};

            segStack.push_back({midPt, midBCD, midCD, curD, curDepth + 1});
            segStack.push_back({curA, midAB, midABC, midPt, curDepth + 1});
        }
    }
};

} // namespace ui::renderers
