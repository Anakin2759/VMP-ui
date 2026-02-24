#pragma once

#include <cstdint>
#include <SDL3/SDL.h>
#include "../common/RenderTypes.hpp"

namespace ui::interface
{

enum class BackendType : std::uint8_t
{
    GPU,
    FALLBACK
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

    virtual bool initialize(SDL_Window* window) = 0;
    virtual void cleanup() = 0;

    virtual bool beginFrame(const SDL_FColor& clearColor) = 0;
    virtual void drawBatch(const render::RenderBatch& batch, SDL_GPUTexture* whiteTextureTag) = 0;
    virtual void endFrame() = 0;

    [[nodiscard]] virtual BackendType getType() const = 0;
};

} // namespace ui::interface
