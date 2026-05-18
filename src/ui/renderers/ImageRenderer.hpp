/**
 * ************************************************************************
 *
 * @file ImageRenderer.hpp
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2026-05-18
 * @version 0.1
 * @brief 图像渲染器 - 渲染 Image 组件，支持从 ImageSource 路径懒加载纹理
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
#include "../common/Tags.hpp"
#include "../managers/BatchManager.hpp"
#include "../managers/DeviceManager.hpp"
#include "../managers/ImageManager.hpp"
#include <SDL3/SDL_gpu.h>

namespace ui::renderers
{

/**
 * @brief 图像渲染器
 *
 * 负责渲染：
 * - 带纹理的 Image 组件（直接使用 textureId）
 * - ImageSource 路径懒加载纹理（首帧自动上传到 GPU）
 */
class ImageRenderer : public core::IRenderer
{
public:
    ImageRenderer() = default;

    [[nodiscard]] bool canHandle(entt::entity entity) const override
    {
        return Registry::AnyOf<components::ImageTag>(entity);
    }

    void collect(entt::entity entity, core::RenderContext& context) override
    {
        if (context.batchManager == nullptr || context.whiteTexture == nullptr)
        {
            return;
        }

        // 懒加载：若有 ImageSource 且尚未加载
        if (auto* src = Registry::TryGet<components::ImageSource>(entity))
        {
            if (!src->loaded && !src->loadFailed && !src->path.empty())
            {
                SDL_GPUDevice* device =
                    (context.deviceManager != nullptr) ? context.deviceManager->getDevice() : nullptr;

                SDL_GPUTexture* tex = managers::ImageManager::instance().loadTexture(src->path, device);
                if (tex != nullptr)
                {
                    auto& img = Registry::GetOrEmplace<components::Image>(entity);
                    img.textureId = static_cast<void*>(tex);
                    src->loaded = true;
                }
                else
                {
                    src->loadFailed = true;
                }
            }
        }

        const auto* img = Registry::TryGet<components::Image>(entity);
        if (img == nullptr || img->textureId == nullptr)
        {
            return;
        }

        auto* tex = static_cast<SDL_GPUTexture*>(img->textureId);

        render::UiPushConstants pushConstants{};
        pushConstants.screen_size[0] = context.screenWidth;
        pushConstants.screen_size[1] = context.screenHeight;
        pushConstants.rect_size[0] = context.size.x();
        pushConstants.rect_size[1] = context.size.y();
        pushConstants.opacity = context.alpha;
        // 纹理模式：draw_mode=1.0 由 shader 采样纹理
        pushConstants.draw_mode = 1.0F;

        context.batchManager->beginBatch(tex, context.currentScissor, pushConstants);

        const Eigen::Vector4f tint{
            img->tintColor.red, img->tintColor.green, img->tintColor.blue, img->tintColor.alpha};

        context.batchManager->addRect(context.position,
                                      context.size,
                                      tint,
                                      Eigen::Vector2f{img->uvMin.x(), img->uvMin.y()},
                                      Eigen::Vector2f{img->uvMax.x(), img->uvMax.y()});
    }

    [[nodiscard]] int getPriority() const override { return 5; }
};

} // namespace ui::renderers
