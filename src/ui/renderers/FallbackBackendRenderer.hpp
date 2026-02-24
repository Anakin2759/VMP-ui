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
#include <SDL3/SDL.h>
#include "../interface/IBackendRenderer.hpp"
#include "../singleton/Logger.hpp"

namespace ui::renderers
{

class FallbackBackendRenderer final : public interface::IBackendRenderer
{
public:
    FallbackBackendRenderer() = default;
    ~FallbackBackendRenderer() override { cleanup(); }

    FallbackBackendRenderer(const FallbackBackendRenderer&) = delete;
    FallbackBackendRenderer& operator=(const FallbackBackendRenderer&) = delete;
    FallbackBackendRenderer(FallbackBackendRenderer&&) = delete;
    FallbackBackendRenderer& operator=(FallbackBackendRenderer&&) = delete;

    bool initialize(SDL_Window* window) override
    {
        if (window == nullptr)
        {
            return false;
        }

        SDL_WindowID windowID = SDL_GetWindowID(window);
        if (m_renderer != nullptr && m_windowID == windowID)
        {
            return true;
        }

        cleanup();

        static constexpr std::array<const char*, 4> DRIVER_CANDIDATES = {
            "direct3d11", "opengl", "opengles2", "software"};

        for (const char* driver : DRIVER_CANDIDATES)
        {
            SDL_SetHint(SDL_HINT_RENDER_DRIVER, driver);
            m_renderer = SDL_CreateRenderer(window, driver);
            if (m_renderer != nullptr)
            {
                SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
                m_windowID = windowID;
                Logger::warn("[FallbackBackendRenderer] using SDL_Renderer driver: {}", driver);
                return true;
            }
        }

        Logger::error("[FallbackBackendRenderer] create renderer failed: {}", SDL_GetError());
        return false;
    }

    void cleanup() override
    {
        if (m_renderer != nullptr)
        {
            SDL_DestroyRenderer(m_renderer);
            m_renderer = nullptr;
        }
        m_windowID = 0;
    }

    bool beginFrame(const SDL_FColor& clearColor) override
    {
        if (m_renderer == nullptr)
        {
            return false;
        }

        SDL_SetRenderClipRect(m_renderer, nullptr);
        SDL_SetRenderDrawColorFloat(m_renderer, clearColor.r, clearColor.g, clearColor.b, clearColor.a);
        SDL_RenderClear(m_renderer);
        return true;
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

private:
    SDL_Renderer* m_renderer = nullptr;
    SDL_WindowID m_windowID = 0;
};

} // namespace ui::renderers
