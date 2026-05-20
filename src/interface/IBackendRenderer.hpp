/**
 * ************************************************************************
 *
 * @file IBackendRenderer.hpp
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2026-03-13
 * @version 0.1
 * @brief 后端渲染器接口，定义了渲染器的基本功能和行为规范。
 *
 * ************************************************************************
 * @copyright Copyright (c) 2026 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */
#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <SDL3/SDL.h>
#include "../common/RenderTypes.hpp"
#include "../common/Result.hpp"
#include "../common/ErrorCodes.hpp"

namespace ui::interface
{

enum class BackendType : std::uint8_t
{
    GPU,
    FALLBACK
};

enum class BackendCapability : std::uint8_t
{
    CACHED_BITMAP
};

class IBackendRenderer
{
public:
    virtual ~IBackendRenderer() = default;
    IBackendRenderer() = default;
    IBackendRenderer(const IBackendRenderer&) = delete;
    IBackendRenderer& operator=(const IBackendRenderer&) = delete;
    IBackendRenderer(IBackendRenderer&&) = delete;
    IBackendRenderer& operator=(IBackendRenderer&&) = delete;
    /**
     * @brief 初始化渲染器
     * @param window 指向 SDL_Window 的指针，用于渲染的目标窗口
     * @return Result<void> 成功返回 Ok()，失败返回 ui_errc 错误码
     */
    virtual ui::Result<void> initialize(SDL_Window* window) = 0;

    /**
     * @brief 清理渲染器资源，释放占用的内存和 GPU 资源
     */
    virtual void cleanup() = 0;
    /**
     * @brief 开始渲染一帧
     * @param clearColor 用于清屏的颜色
     * @return Result<void> 成功返回 Ok()，失败返回 ui_errc::swapchain_unavailable 等
     */
    virtual ui::Result<void> beginFrame(const SDL_FColor& clearColor) = 0;

    /**
     * @brief 绘制一个渲染批次
     * @param batch 包含要绘制的渲染命令和相关数据的渲染批次对象
     * @param whiteTextureTag 用于绘制纯色图形的白色纹理标签
     */
    virtual void drawBatch(const render::RenderBatch& batch, SDL_GPUTexture* whiteTextureTag) = 0;

    virtual ui::Result<void> drawCachedBitmap(std::string_view cacheKey,
                                              std::span<const std::uint8_t> rgbaPixels,
                                              int bitmapWidth,
                                              int bitmapHeight,
                                              const SDL_FRect& destinationRect,
                                              const std::optional<SDL_Rect>& scissorRect,
                                              std::uint8_t alphaMod)
    {
        static_cast<void>(cacheKey);
        static_cast<void>(rgbaPixels);
        static_cast<void>(bitmapWidth);
        static_cast<void>(bitmapHeight);
        static_cast<void>(destinationRect);
        static_cast<void>(scissorRect);
        static_cast<void>(alphaMod);
        return ui::MakeError(ui::ui_errc::not_implemented);
    }

    /**
     * @brief 结束渲染一帧
     */

    virtual void endFrame() = 0;

    /**
     * @brief 获取渲染器类型
     * @return BackendType 渲染器类型
     */
    [[nodiscard]] virtual BackendType getType() const = 0;

    [[nodiscard]] virtual bool supports(BackendCapability capability) const
    {
        static_cast<void>(capability);
        return false;
    }
};

} // namespace ui::interface
