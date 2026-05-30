/**
 * ************************************************************************
 *
 * @file ShapeRenderer.hpp
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2026-01-30
 * @version 0.1
 * @brief 形状渲染器 - 处理矩形、圆角、阴影等基础形状渲染
 *
 * ************************************************************************
 * @copyright Copyright (c) 2026 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */

#pragma once
#include "interface/IRenderer.hpp"
#include "api/Theme.hpp"
#include "singleton/Registry.hpp"

#include "common/components/Visual.hpp"
#include "common/Policies.hpp"

#include "common/Tags.hpp"
#include "managers/BatchManager.hpp"
#include <SDL3/SDL_gpu.h>

namespace ui::renderers
{

/**
 * @brief 形状渲染器
 *
 * 负责渲染：
 * - 背景矩形
 * - 圆角矩形
 * - 阴影效果
 * - 边框
 */
class ShapeRenderer : public core::IRenderer
{
public:
    explicit ShapeRenderer(Registry& reg) : m_reg(&reg) {}

    [[nodiscard]] bool canHandle(entt::entity entity) const override
    {
        // 任何有背景或边框的实体都需要形状渲染
        return m_reg->any_of<components::Background, components::Border>(entity);
    }

    void collect(entt::entity entity, core::RenderContext& context) override
    {
        if (context.batchManager == nullptr || context.deviceManager == nullptr || context.whiteTexture == nullptr)
        {
            return;
        }

        // 渲染背景
        renderBackground(entity, context);

        // 渲染边框
        renderBorder(entity, context);
    }

    [[nodiscard]] int getPriority() const override
    {
        return 0; // 背景应该最先渲染
    }

private:
    /**
     * @brief 初始化基础推送常量
     *
     * 设置所有推送常量的默认值，包括屏幕尺寸、矩形尺寸、不透明度和阴影默认值。
     * 阴影参数默认设置为0，调用者可以根据需要在之后覆盖这些值。
     *
     * @param pushConstants 要初始化的推送常量
     * @param context 渲染上下文
     * @param rectSize 矩形尺寸
     */
    void initBasicPushConstants(render::UiPushConstants& pushConstants,
                                const core::RenderContext& context,
                                const Eigen::Vector2f& rectSize) const
    {
        pushConstants.screen_size[0] = context.screenWidth;
        pushConstants.screen_size[1] = context.screenHeight;
        pushConstants.rect_size[0] = rectSize.x();
        pushConstants.rect_size[1] = rectSize.y();
        pushConstants.opacity = context.alpha;
        // 默认无阴影，调用者可以覆盖
        pushConstants.shadow_soft = 0.0F;
        pushConstants.shadow_offset_x = 0.0F;
        pushConstants.shadow_offset_y = 0.0F;
        pushConstants.padding = 0.0F;
        pushConstants.stroke_width = 0.0F;
        pushConstants.draw_mode = 0.0F;
        pushConstants.reserved0 = 0.0F;
    }

    void renderBackground(entt::entity entity, core::RenderContext& context)
    {
        const auto* background = m_reg->try_get<components::Background>(entity);
        if (background == nullptr || background->enabled != policies::Feature::ENABLED)
        {
            return;
        }

        // 准备推送常量
        render::UiPushConstants pushConstants{};
        initBasicPushConstants(pushConstants, context, context.size);

        pushConstants.radius[0] = background->borderRadius.x();
        pushConstants.radius[1] = background->borderRadius.y();
        pushConstants.radius[2] = background->borderRadius.z();
        pushConstants.radius[3] = background->borderRadius.w();

        // 处理阴影
        const auto* shadow = m_reg->try_get<components::Shadow>(entity);
        if (shadow != nullptr && shadow->enabled == policies::Feature::ENABLED)
        {
            pushConstants.shadow_soft = shadow->softness;
            pushConstants.shadow_offset_x = shadow->offset.x();
            pushConstants.shadow_offset_y = shadow->offset.y();
        }

        // 开始批次
        context.batchManager->beginBatch(context.whiteTexture, context.currentScissor, pushConstants);

        // 添加矩形
        Eigen::Vector4f color(
            background->color.red, background->color.green, background->color.blue, background->color.alpha);
        context.batchManager->addRect(context.position, context.size, color);
    }

    void renderBorder(entt::entity entity, core::RenderContext& context)
    {
        const auto* border = m_reg->try_get<components::Border>(entity);
        const bool focused = m_reg->any_of<components::FocusedTag>(entity);

        // 早期返回：既没有焦点也没有有效边框
        if (!focused && (border == nullptr || border->thickness <= 0.0F))
        {
            return;
        }

        Eigen::Vector4f color(0.0F, 0.0F, 0.0F, 1.0F);
        float thickness = 0.0F;

        // 设置边框属性
        if (border != nullptr && border->thickness > 0.0F)
        {
            color = Eigen::Vector4f(border->color.red, border->color.green, border->color.blue, border->color.alpha);
            thickness = border->thickness;
        }

        // 焦点状态仅提升边框粗细；颜色由 ThemeSystem 写入 Border 组件
        if (focused)
        {
            thickness = std::max(thickness, theme::CurrentTheme().focusBorderMinThickness);
        }

        // 渲染边框线条
        if (thickness > 0.0F)
        {
            renderRoundedBorder(entity, context, color, thickness);
        }
    }

    void renderRoundedBorder(entt::entity entity,
                             core::RenderContext& context,
                             const Eigen::Vector4f& color,
                             float thickness)
    {
        render::UiPushConstants pushConstants{};
        initBasicPushConstants(pushConstants, context, context.size);
        pushConstants.stroke_width = thickness;
        pushConstants.draw_mode = 1.0F;

        if (const auto* border = m_reg->try_get<components::Border>(entity))
        {
            pushConstants.radius[0] = border->borderRadius.x();
            pushConstants.radius[1] = border->borderRadius.y();
            pushConstants.radius[2] = border->borderRadius.z();
            pushConstants.radius[3] = border->borderRadius.w();
        }

        if (const auto* background = m_reg->try_get<components::Background>(entity))
        {
            const float radiusSum =
                pushConstants.radius[0] + pushConstants.radius[1] + pushConstants.radius[2] + pushConstants.radius[3];
            if (radiusSum <= 0.0F)
            {
                pushConstants.radius[0] = background->borderRadius.x();
                pushConstants.radius[1] = background->borderRadius.y();
                pushConstants.radius[2] = background->borderRadius.z();
                pushConstants.radius[3] = background->borderRadius.w();
            }
        }

        context.batchManager->beginBatch(context.whiteTexture, context.currentScissor, pushConstants);
        context.batchManager->addRect(context.position, context.size, color);
    }

    Registry* m_reg = nullptr;
};

} // namespace ui::renderers
