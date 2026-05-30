/**
 * ************************************************************************
 *
 * @file IconRenderer.hpp
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2026-01-30
 * @version 0.1
 * @brief 图标渲染器 - 处理所有图标的渲染

    两种图标：
    - 一种由png jpg 转换的
    - 一种由字体图标 ttf文件转换的
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
#include "common/components/Layout.hpp"
#include "common/CustomizationPoints.hpp"
#include "managers/IconManager.hpp"
#include "managers/FontManager.hpp"
#include "core/RenderContext.hpp"
#include "managers/BatchManager.hpp"
namespace ui::renderers
{

/**
 * @brief 图标渲染器
 *
 * 负责渲染：
 * - 纹理图标
 * - 字体图标
 */
class IconRenderer : public core::IRenderer
{
public:
    IconRenderer(Registry& reg, managers::IconManager* iconManager) : m_reg(&reg), m_iconManager(iconManager) {}
    /**
     * @brief 判断能否处理
     * @param entity 控件
     * @return true 控件可处理
     * @return false 控件不可处理
     */
    [[nodiscard]] bool canHandle(entt::entity entity) const override { return m_reg->any_of<components::Icon>(entity); }

    void collect(entt::entity entity, core::RenderContext& context) override
    {
        if (context.batchManager == nullptr)
        {
            return;
        }

        const auto* iconComp = m_reg->try_get<components::Icon>(entity);
        if (iconComp == nullptr) return;

        // 计算图标的绘制位置和大小
        // 使用组件定义的尺寸和颜色
        Eigen::Vector2f iconDrawSize = iconComp->size;
        Eigen::Vector4f tint = Eigen::Vector4f(
            iconComp->tintColor.red, iconComp->tintColor.green, iconComp->tintColor.blue, iconComp->tintColor.alpha);

        SDL_GPUTexture* iconTexture = nullptr;
        Eigen::Vector2f uvMin = {0.0F, 0.0F};
        Eigen::Vector2f uvMax = {1.0F, 1.0F};
        Eigen::Vector2f actualIconSize = iconDrawSize;

        if (HasFlag(iconComp->type, policies::IconFlag::TEXTURE))
        {
            // 纹理图标
            if (const auto* textureInfo = m_iconManager->getTextureInfo(iconComp->textureId))
            {
                iconTexture = textureInfo->texture.get();
                uvMin = textureInfo->uvMin;
                uvMax = textureInfo->uvMax;
                actualIconSize = iconDrawSize.cwiseMin(Eigen::Vector2f(textureInfo->width, textureInfo->height));
            }
            else
            {
                return; // 纹理不存在
            }
        }
        else if (!HasFlag(iconComp->type, policies::IconFlag::TEXTURE))
        {
            // 字体图标
            // fontHandle 暂时存储为字体名称字符串指针，或者为空则使用默认
            std::string fontName = "MaterialSymbols";
            if (iconComp->fontHandle != nullptr)
            {
                // 注意：这里假设 fontHandle 是一个 const char*
                fontName = static_cast<const char*>(iconComp->fontHandle);
            }

            if (const auto* textureInfo =
                    m_iconManager->getTextureInfo(fontName, iconComp->codepoint, iconComp->size.y()))
            {
                iconTexture = textureInfo->texture.get();
                uvMin = textureInfo->uvMin;
                uvMax = textureInfo->uvMax;
                actualIconSize = {textureInfo->width, textureInfo->height};
            }
            else
            {
                return; // 图标渲染失败
            }
        }

        if (iconTexture != nullptr)
        {
            // 计算绘制位置：考虑 Padding 与是否存在文本
            Eigen::Vector2f contentPos = context.position;
            Eigen::Vector2f contentSize = context.size;

            if (const auto* padding = m_reg->try_get<components::Padding>(entity))
            {
                // Padding values: x=Top, y=Right, z=Bottom, w=Left
                contentPos.x() += padding->values.w();
                contentPos.y() += padding->values.x();
                contentSize.x() = std::max(0.0F, contentSize.x() - padding->values.y() - padding->values.w());
                contentSize.y() = std::max(0.0F, contentSize.y() - padding->values.x() - padding->values.z());
            }

            Eigen::Vector2f drawPos = contentPos;

            // 如果存在文本且非空，则将图标与文本一起居中（图标在文本左侧）
            if (const auto* textComp = m_reg->try_get<components::Text>(entity);
                textComp != nullptr && !textComp->content.empty() && context.fontManager != nullptr)
            {
                auto textWidth = static_cast<float>(
                    ui::cpo::measure_text_width(*context.fontManager, textComp->content, textComp->fontSize));
                float totalWidth = actualIconSize.x() + iconComp->spacing + textWidth;
                drawPos.x() = contentPos.x() + std::max(0.0F, (contentSize.x() - totalWidth) * 0.5F);
                drawPos.y() = contentPos.y() + std::max(0.0F, (contentSize.y() - actualIconSize.y()) * 0.5F);
            }
            else
            {
                // 仅图标时，直接在控件内容区居中
                drawPos.x() = contentPos.x() + std::max(0.0F, (contentSize.x() - actualIconSize.x()) * 0.5F);
                drawPos.y() = contentPos.y() + std::max(0.0F, (contentSize.y() - actualIconSize.y()) * 0.5F);
            }

            // 对齐到整数像素
            drawPos.x() = std::round(drawPos.x());
            drawPos.y() = std::round(drawPos.y());

            render::UiPushConstants pushConstants{};
            pushConstants.screen_size[0] = context.screenWidth;
            pushConstants.screen_size[1] = context.screenHeight;
            pushConstants.rect_size[0] = actualIconSize.x();
            pushConstants.rect_size[1] = actualIconSize.y();
            pushConstants.opacity = context.alpha;
            pushConstants.padding = 1.0F; // 标记纹理为预乘 Alpha

            context.batchManager->beginBatch(iconTexture, context.currentScissor, pushConstants);
            context.batchManager->addRect(drawPos, actualIconSize, tint, uvMin, uvMax);
        }
    }
    /**
     * @brief 获取渲染优先级
     * @return 优先级数值，数值越小越先渲染
     */
    [[nodiscard]] int getPriority() const override
    {
        return 20; // 图标在文本之后渲染
    }

private:
    Registry* m_reg = nullptr;
    managers::IconManager* m_iconManager;
};

} // namespace ui::renderers
