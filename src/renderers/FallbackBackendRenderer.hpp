/**
 * ************************************************************************
 *
 * @file FallbackBackendRenderer.hpp
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2026-02-24
 * @version 0.1
 * @brief SDL_Renderer 后备渲染器实现
 *
 * ************************************************************************
 * @copyright Copyright (c) 2026 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */
#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>
#include <SDL3/SDL.h>
#include "../interface/IBackendRenderer.hpp"
#include "../singleton/Logger.hpp"

namespace ui::renderers
{

class FallbackBackendRenderer final : public interface::IBackendRenderer
{
public:
    struct CachedBitmapTexture
    {
        SDL_Texture* texture = nullptr;
        int width = 0;
        int height = 0;
    };

    FallbackBackendRenderer() = default;
    ~FallbackBackendRenderer() override { cleanup(); }

    FallbackBackendRenderer(const FallbackBackendRenderer&) = delete;
    FallbackBackendRenderer& operator=(const FallbackBackendRenderer&) = delete;
    FallbackBackendRenderer(FallbackBackendRenderer&&) = delete;
    FallbackBackendRenderer& operator=(FallbackBackendRenderer&&) = delete;

    ui::Result<void> initialize(SDL_Window* window) override
    {
        if (window == nullptr)
        {
            return ui::MakeError(ui::ui_errc::invalid_argument);
        }

        SDL_WindowID windowID = SDL_GetWindowID(window);
        if (m_renderer != nullptr && m_windowID == windowID)
        {
            return ui::Ok();
        }

        cleanup();

        static constexpr std::array<const char*, 4> DRIVER_CANDIDATES = {
            "direct3d11", "opengl", "opengles2", "software"};

#ifdef UI_FORCE_CPU_RENDER
        // 编译时强制 CPU 渲染：直接使用 software 驱动，跳过所有硬件加速后端
        static constexpr std::array<const char*, 1> activeDrivers = {"software"};
#else
        const auto& activeDrivers = DRIVER_CANDIDATES;
#endif

        for (const char* driver : activeDrivers)
        {
            SDL_SetHint(SDL_HINT_RENDER_DRIVER, driver);
            m_renderer = SDL_CreateRenderer(window, driver);
            if (m_renderer != nullptr)
            {
                SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
                m_windowID = windowID;
                Logger::warn("[FallbackBackendRenderer] using SDL_Renderer driver: {}", driver);
                return ui::Ok();
            }
        }

        Logger::error("[FallbackBackendRenderer] create renderer failed: {}", SDL_GetError());
        return ui::MakeError(ui::ui_errc::backend_unavailable);
    }

    void cleanup() override
    {
        for (auto& cacheEntry : m_bitmapTextureCache)
        {
            if (cacheEntry.second.texture != nullptr)
            {
                SDL_DestroyTexture(cacheEntry.second.texture);
            }
        }
        m_bitmapTextureCache.clear();

        if (m_renderer != nullptr)
        {
            SDL_DestroyRenderer(m_renderer);
            m_renderer = nullptr;
        }
        m_windowID = 0;
    }

    ui::Result<void> beginFrame(const SDL_FColor& clearColor) override
    {
        if (m_renderer == nullptr)
        {
            return ui::MakeError(ui::ui_errc::backend_unavailable);
        }

        SDL_SetRenderClipRect(m_renderer, nullptr);
        SDL_SetRenderDrawColorFloat(m_renderer, clearColor.r, clearColor.g, clearColor.b, clearColor.a);
        SDL_RenderClear(m_renderer);
        return ui::Ok();
    }

    void drawBatch(const render::RenderBatch& batch, SDL_GPUTexture* whiteTextureTag) override
    {
        if (m_renderer == nullptr || batch.vertices.empty())
        {
            return;
        }

        if (batch.texture != nullptr && batch.texture != whiteTextureTag)
        {
            return;
        }

        if (batch.scissorRect.has_value())
        {
            SDL_SetRenderClipRect(m_renderer, &batch.scissorRect.value());
        }
        else
        {
            SDL_SetRenderClipRect(m_renderer, nullptr);
        }

        for (size_t i = 0; i + 3 < batch.vertices.size(); i += 4)
        {
            const auto& vertexTopLeft = batch.vertices[i];
            const auto& vertexTopRight = batch.vertices[i + 1];
            const auto& vertexBottomRight = batch.vertices[i + 2];
            const auto& vertexBottomLeft = batch.vertices[i + 3];

            const float minX = std::min({vertexTopLeft.position[0],
                                         vertexTopRight.position[0],
                                         vertexBottomRight.position[0],
                                         vertexBottomLeft.position[0]});
            const float minY = std::min({vertexTopLeft.position[1],
                                         vertexTopRight.position[1],
                                         vertexBottomRight.position[1],
                                         vertexBottomLeft.position[1]});
            const float maxX = std::max({vertexTopLeft.position[0],
                                         vertexTopRight.position[0],
                                         vertexBottomRight.position[0],
                                         vertexBottomLeft.position[0]});
            const float maxY = std::max({vertexTopLeft.position[1],
                                         vertexTopRight.position[1],
                                         vertexBottomRight.position[1],
                                         vertexBottomLeft.position[1]});

            const float alpha = std::clamp(vertexTopLeft.color[3] * batch.pushConstants.opacity, 0.0F, 1.0F);
            SDL_SetRenderDrawColorFloat(
                m_renderer, vertexTopLeft.color[0], vertexTopLeft.color[1], vertexTopLeft.color[2], alpha);

            SDL_FRect rect = {minX, minY, std::max(0.0F, maxX - minX), std::max(0.0F, maxY - minY)};
            if (rect.w <= 0.0F || rect.h <= 0.0F)
            {
                continue;
            }
            SDL_RenderFillRect(m_renderer, &rect);
        }
    }

    ui::Result<void> drawCachedBitmap(std::string_view cacheKey,
                                      std::span<const std::uint8_t> rgbaPixels,
                                      int bitmapWidth,
                                      int bitmapHeight,
                                      const SDL_FRect& destinationRect,
                                      const std::optional<SDL_Rect>& scissorRect,
                                      std::uint8_t alphaMod) override
    {
        if (m_renderer == nullptr || bitmapWidth <= 0 || bitmapHeight <= 0 || rgbaPixels.empty())
        {
            return ui::MakeError(ui::ui_errc::invalid_argument);
        }

        CachedBitmapTexture* cachedTexture = getOrCreateBitmapTexture(cacheKey, rgbaPixels, bitmapWidth, bitmapHeight);
        if (cachedTexture == nullptr || cachedTexture->texture == nullptr)
        {
            return ui::MakeError(ui::ui_errc::asset_upload_failed);
        }

        if (scissorRect.has_value())
        {
            SDL_SetRenderClipRect(m_renderer, &scissorRect.value());
        }
        else
        {
            SDL_SetRenderClipRect(m_renderer, nullptr);
        }

        SDL_SetTextureAlphaMod(cachedTexture->texture, alphaMod);
        if (!SDL_RenderTexture(m_renderer, cachedTexture->texture, nullptr, &destinationRect))
        {
            return ui::MakeError(ui::ui_errc::asset_upload_failed);
        }
        return ui::Ok();
    }

    void endFrame() override
    {
        if (m_renderer == nullptr)
        {
            return;
        }

        SDL_SetRenderClipRect(m_renderer, nullptr);
        SDL_RenderPresent(m_renderer);
    }

    [[nodiscard]] interface::BackendType getType() const override { return interface::BackendType::FALLBACK; }

    [[nodiscard]] bool supports(interface::BackendCapability capability) const override
    {
        return capability == interface::BackendCapability::CACHED_BITMAP;
    }

private:
    CachedBitmapTexture* getOrCreateBitmapTexture(std::string_view cacheKey,
                                                  std::span<const std::uint8_t> rgbaPixels,
                                                  int width,
                                                  int height)
    {
        const std::string key(cacheKey);
        auto cacheIterator = m_bitmapTextureCache.find(key);
        const bool needsRecreate = (cacheIterator == m_bitmapTextureCache.end())
                                || cacheIterator->second.texture == nullptr || cacheIterator->second.width != width
                                || cacheIterator->second.height != height;

        if (needsRecreate)
        {
            if (cacheIterator != m_bitmapTextureCache.end() && cacheIterator->second.texture != nullptr)
            {
                SDL_DestroyTexture(cacheIterator->second.texture);
            }

            CachedBitmapTexture newEntry{};
            newEntry.texture =
                SDL_CreateTexture(m_renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, width, height);
            newEntry.width = width;
            newEntry.height = height;

            if (newEntry.texture == nullptr)
            {
                Logger::error("[FallbackBackendRenderer] create SDL_Texture failed: {}", SDL_GetError());
                m_bitmapTextureCache.erase(key);
                return nullptr;
            }

            SDL_SetTextureBlendMode(newEntry.texture, SDL_BLENDMODE_BLEND);
            cacheIterator = m_bitmapTextureCache.insert_or_assign(key, newEntry).first;
        }

        if (!SDL_UpdateTexture(cacheIterator->second.texture, nullptr, rgbaPixels.data(), width * 4))
        {
            Logger::error("[FallbackBackendRenderer] update SDL_Texture failed: {}", SDL_GetError());
            return nullptr;
        }

        return &cacheIterator->second;
    }

    SDL_Renderer* m_renderer = nullptr;
    SDL_WindowID m_windowID = 0;
    std::unordered_map<std::string, CachedBitmapTexture> m_bitmapTextureCache;
};

} // namespace ui::renderers
