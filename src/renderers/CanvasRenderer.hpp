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
        // 将线段扩展为矩形（沿垂直方向偏移 lineWidth/2）
        const float startX = origin.x() + cmd.p1.x();
        const float startY = origin.y() + cmd.p1.y();
        const float endX   = origin.x() + cmd.p2.x();
        const float endY   = origin.y() + cmd.p2.y();

        const float deltaX = endX - startX;
        const float deltaY = endY - startY;
        const float len = std::sqrt((deltaX * deltaX) + (deltaY * deltaY));
        if (len < 0.001F) return;

        const float halfWidth = cmd.lineWidth * 0.5F;

        beginWhiteBatch(context);
        // 用近似矩形表示宽线段（addRect 不支持任意四边形）
        const Eigen::Vector4f col = toVec4(cmd.color, context.alpha);
        const Eigen::Vector2f pos{std::min(startX, endX) - halfWidth, std::min(startY, endY) - halfWidth};
        const Eigen::Vector2f lineSize{std::abs(deltaX) + cmd.lineWidth, std::abs(deltaY) + cmd.lineWidth};
        context.batchManager->addRect(pos, lineSize, col);
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
};

} // namespace ui::renderers
