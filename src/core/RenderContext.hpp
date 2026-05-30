/**
 * ************************************************************************
 *
 * @file RenderContext.hpp
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2026-01-30
 * @version 0.1
 *
 * ************************************************************************
 * @copyright Copyright (c) 2026 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */

#pragma once
#include <Eigen/Dense>
#include <SDL3/SDL_rect.h>
#include <SDL3/SDL_gpu.h>
#include <vector>
#include <optional>

namespace ui::managers
{
class DeviceManager;
class FontManager;
class IconManager;
class ImageManager;
class TextTextureCache;
class BatchManager;
} // namespace ui::managers

namespace ui::interface
{
class IBackendRenderer;
} // namespace ui::interface

namespace ui::core
{

/**
 *
 */
struct RenderContext
{
    Eigen::Vector2f position{0.0F, 0.0F};

    Eigen::Vector2f size{0.0F, 0.0F};

    float alpha = 1.0F;

    float screenWidth = 0.0F;
    float screenHeight = 0.0F;
    float dpiScale = 1.0F;
    std::vector<SDL_Rect> scissorStack;

    std::optional<SDL_Rect> currentScissor;

    managers::DeviceManager* deviceManager = nullptr;
    managers::FontManager* fontManager = nullptr;
    managers::ImageManager* imageManager = nullptr;
    managers::TextTextureCache* textTextureCache = nullptr;
    managers::BatchManager* batchManager = nullptr;
    interface::IBackendRenderer* backendRenderer = nullptr;

    SDL_Window* sdlWindow = nullptr;

    SDL_GPUTexture* whiteTexture = nullptr;

    /**
     */
    void pushScissor(const SDL_Rect& rect)
    {
        SDL_Rect newScissor = rect;

        if (!scissorStack.empty())
        {
            const SDL_Rect& parent = scissorStack.back();
            if (!SDL_GetRectIntersection(&newScissor, &parent, &newScissor))
            {
                newScissor.w = 0;
                newScissor.h = 0;
            }
        }

        scissorStack.push_back(newScissor);
        currentScissor = newScissor;
    }

    /**
     */
    void popScissor()
    {
        if (!scissorStack.empty())
        {
            scissorStack.pop_back();

            if (!scissorStack.empty())
            {
                currentScissor = scissorStack.back();
            }
            else
            {
                currentScissor.reset();
            }
        }
    }

    /**
     */
    [[nodiscard]] RenderContext
        createChildContext(const Eigen::Vector2f& childPos, const Eigen::Vector2f& childSize, float childAlpha) const
    {
        RenderContext child = *this;
        child.position = position + childPos;
        child.size = childSize;
        child.alpha = alpha * childAlpha;
        return child;
    }
};

} // namespace ui::core
