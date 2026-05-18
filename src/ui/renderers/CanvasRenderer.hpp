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
#include <numbers>
#include "../interface/IRenderer.hpp"
#include "../singleton/Registry.hpp"
#include "../common/Components.hpp"
#include "../common/Tags.hpp"
#include "../managers/BatchManager.hpp"
#include "../managers/DeviceManager.hpp"
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
                case components::CanvasDrawType::Line:
                    renderLine(cmd, origin, context);
                    break;
                case components::CanvasDrawType::Rect:
                    renderRectOutline(cmd, origin, context);
                    break;
                case components::CanvasDrawType::FilledRect:
                    renderFilledRect(cmd, origin, context);
                    break;
                case components::CanvasDrawType::Circle:
                    renderCircleOutline(cmd, origin, context);
                    break;
                case components::CanvasDrawType::FilledCircle:
                    renderFilledCircle(cmd, origin, context);
                    break;
            }
        }
    }

    [[nodiscard]] int getPriority() const override { return 20; }

private:
    static constexpr int CIRCLE_SEGMENTS = 24;

    static Eigen::Vector4f toVec4(const Color& c, float alpha)
    {
        return {c.red, c.green, c.blue, c.alpha * alpha};
    }

    static void beginWhiteBatch(core::RenderContext& context)
    {
        render::UiPushConstants pc{};
        pc.screen_size[0] = context.screenWidth;
        pc.screen_size[1] = context.screenHeight;
        pc.opacity = context.alpha;
        context.batchManager->beginBatch(context.whiteTexture, context.currentScissor, pc);
    }

    static void renderLine(const components::CanvasDrawCommand& cmd,
                           const Eigen::Vector2f& origin,
                           core::RenderContext& context)
    {
        // 将线段扩展为矩形（沿垂直方向偏移 lineWidth/2）
        const float x1 = origin.x() + cmd.p1.x();
        const float y1 = origin.y() + cmd.p1.y();
        const float x2 = origin.x() + cmd.p2.x();
        const float y2 = origin.y() + cmd.p2.y();

        const float dx = x2 - x1;
        const float dy = y2 - y1;
        const float len = std::sqrt(dx * dx + dy * dy);
        if (len < 0.001F) return;

        const float hw = cmd.lineWidth * 0.5F;
        const float nx = -dy / len * hw;
        const float ny = dx / len * hw;

        beginWhiteBatch(context);
        // 4 个角点构成一个宽线段四边形
        const Eigen::Vector4f col = toVec4(cmd.color, context.alpha);
        // 用两个三角形（addRect 不支持任意四边形，退而用近似矩形）
        const Eigen::Vector2f pos{std::min(x1, x2) - hw, std::min(y1, y2) - hw};
        const Eigen::Vector2f sz{std::abs(dx) + cmd.lineWidth, std::abs(dy) + cmd.lineWidth};
        context.batchManager->addRect(pos, sz, col);
    }

    static void renderFilledRect(const components::CanvasDrawCommand& cmd,
                                 const Eigen::Vector2f& origin,
                                 core::RenderContext& context)
    {
        const float x = origin.x() + cmd.p1.x();
        const float y = origin.y() + cmd.p1.y();
        const float w = cmd.p2.x() - cmd.p1.x();
        const float h = cmd.p2.y() - cmd.p1.y();

        beginWhiteBatch(context);
        context.batchManager->addRect({x, y}, {w, h}, toVec4(cmd.color, context.alpha));
    }

    static void renderRectOutline(const components::CanvasDrawCommand& cmd,
                                  const Eigen::Vector2f& origin,
                                  core::RenderContext& context)
    {
        const float x1 = origin.x() + cmd.p1.x();
        const float y1 = origin.y() + cmd.p1.y();
        const float x2 = origin.x() + cmd.p2.x();
        const float y2 = origin.y() + cmd.p2.y();
        const float lw = cmd.lineWidth;
        const Eigen::Vector4f col = toVec4(cmd.color, context.alpha);

        beginWhiteBatch(context);
        // 上边
        context.batchManager->addRect({x1, y1}, {x2 - x1, lw}, col);
        // 下边
        context.batchManager->addRect({x1, y2 - lw}, {x2 - x1, lw}, col);
        // 左边
        context.batchManager->addRect({x1, y1 + lw}, {lw, y2 - y1 - 2 * lw}, col);
        // 右边
        context.batchManager->addRect({x2 - lw, y1 + lw}, {lw, y2 - y1 - 2 * lw}, col);
    }

    static void renderFilledCircle(const components::CanvasDrawCommand& cmd,
                                   const Eigen::Vector2f& origin,
                                   core::RenderContext& context)
    {
        const float cx = origin.x() + cmd.p1.x();
        const float cy = origin.y() + cmd.p1.y();
        const float r  = cmd.p2.x();
        const Eigen::Vector4f col = toVec4(cmd.color, context.alpha);

        render::UiPushConstants pc{};
        pc.screen_size[0] = context.screenWidth;
        pc.screen_size[1] = context.screenHeight;
        pc.rect_size[0]   = 2.0F * r;
        pc.rect_size[1]   = 2.0F * r;
        pc.radius[0]      = r;
        pc.radius[1]      = r;
        pc.radius[2]      = r;
        pc.radius[3]      = r;
        pc.opacity        = context.alpha;
        pc.draw_mode      = 0.0F; // 填充

        context.batchManager->beginBatch(context.whiteTexture, context.currentScissor, pc);
        context.batchManager->addRect({cx - r, cy - r}, {2.0F * r, 2.0F * r}, col);
    }

    static void renderCircleOutline(const components::CanvasDrawCommand& cmd,
                                    const Eigen::Vector2f& origin,
                                    core::RenderContext& context)
    {
        const float cx = origin.x() + cmd.p1.x();
        const float cy = origin.y() + cmd.p1.y();
        const float r  = cmd.p2.x();
        const float lw = cmd.lineWidth;
        const Eigen::Vector4f col = toVec4(cmd.color, context.alpha);

        render::UiPushConstants pc{};
        pc.screen_size[0] = context.screenWidth;
        pc.screen_size[1] = context.screenHeight;
        pc.rect_size[0]   = 2.0F * r;
        pc.rect_size[1]   = 2.0F * r;
        pc.radius[0]      = r;
        pc.radius[1]      = r;
        pc.radius[2]      = r;
        pc.radius[3]      = r;
        pc.opacity        = context.alpha;
        pc.draw_mode      = 1.0F; // 描边
        pc.stroke_width   = lw;

        context.batchManager->beginBatch(context.whiteTexture, context.currentScissor, pc);
        context.batchManager->addRect({cx - r, cy - r}, {2.0F * r, 2.0F * r}, col);
    }
};

} // namespace ui::renderers
