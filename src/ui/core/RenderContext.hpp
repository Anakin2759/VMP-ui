/**
 * ************************************************************************
 *
 * @file RenderContext.hpp
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2026-01-30
 * @version 0.1
 * @brief 渲染上下文 - 在渲染过程中传递共享状态
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
 * @brief 渲染上下文 - 封装渲染过程中需要的共享状态
 *
 * 这个上下文对象在渲染树遍历时传递，包含：
 * - 位置和大小信息
 * - 透明度和变换
 * - 裁剪区域栈
 * - 屏幕尺寸
 * - 资源管理器引用
 */
struct RenderContext
{
    // 当前位置（世界坐标）
    Eigen::Vector2f position{0.0F, 0.0F};

    // 当前大小
    Eigen::Vector2f size{0.0F, 0.0F};

    // 累积透明度
    float alpha = 1.0F;

    // 屏幕尺寸
    float screenWidth = 0.0F;
    float screenHeight = 0.0F;
    // 裁剪区域栈
    std::vector<SDL_Rect> scissorStack;

    // 当前有效的裁剪区域
    std::optional<SDL_Rect> currentScissor;

    // 资源管理器引用
    managers::DeviceManager* deviceManager = nullptr;
    managers::FontManager* fontManager = nullptr;
    managers::TextTextureCache* textTextureCache = nullptr;
    managers::BatchManager* batchManager = nullptr;
    interface::IBackendRenderer* backendRenderer = nullptr;

    // SDL窗口指针（用于IME等）
    SDL_Window* sdlWindow = nullptr;

    // 白色纹理（用于纯色渲染）
    SDL_GPUTexture* whiteTexture = nullptr;

    /**
     * @brief 推入新的裁剪区域
     */
    void pushScissor(const SDL_Rect& rect)
    {
        // 计算新的裁剪区域
        SDL_Rect newScissor = rect;

        // 与父级裁剪区域求交集
        if (!scissorStack.empty())
        {
            const SDL_Rect& parent = scissorStack.back();
            if (!SDL_GetRectIntersection(&newScissor, &parent, &newScissor))
            {
                // 不可见，设置为空区域
                newScissor.w = 0;
                newScissor.h = 0;
            }
        }

        scissorStack.push_back(newScissor);
        currentScissor = newScissor;
    }

    /**
     * @brief 弹出裁剪区域
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
     * @brief 创建子上下文（用于递归渲染子元素）
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
