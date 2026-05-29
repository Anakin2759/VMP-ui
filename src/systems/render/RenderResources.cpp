/**
 * @file RenderResources.cpp
 * @brief RenderSystem -- GPU 资源创建与渲染器注册
 *
 * 包含：createWhiteTexture()、initializeRenderers()
 */

#include "systems/RenderSystem.hpp"
#include "RenderSystemImpl.hpp"
#include <algorithm>
#include <cstdint>
#include <memory>
#include "renderers/ShapeRenderer.hpp"
#include "renderers/TextRenderer.hpp"
#include "renderers/IconRenderer.hpp"
#include "renderers/ScrollBarRenderer.hpp"
#include "renderers/SliderRenderer.hpp"
#include "renderers/ProgressBarRenderer.hpp"
#include "renderers/ImageRenderer.hpp"
#include "renderers/CanvasRenderer.hpp"
#include "renderers/TableRenderer.hpp"
#include "renderers/CheckBoxRenderer.hpp"
#include "renderers/DropDownRenderer.hpp"
#include "SDL3/SDL_gpu.h"
#include "common/GPUWrappers.hpp"
#include "SDL3/SDL_stdinc.h"
#include "interface/IRenderer.hpp"
#include "singleton/Logger.hpp"

namespace ui::systems
{

void RenderSystem::createWhiteTexture()
{
    SDL_GPUDevice* device = m_impl->m_deviceManager->getDevice();
    if (device == nullptr) return;

    SDL_GPUTextureCreateInfo texInfo = {};
    texInfo.type = SDL_GPU_TEXTURETYPE_2D;
    texInfo.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    texInfo.width = 1;
    texInfo.height = 1;
    texInfo.layer_count_or_depth = 1;
    texInfo.num_levels = 1;
    texInfo.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;

    m_impl->m_whiteTexture =
        wrappers::MakeGpuResource<wrappers::UniqueGPUTexture>(device, SDL_CreateGPUTexture, &texInfo);
    if (!m_impl->m_whiteTexture) return;

    uint32_t whitePixel = 0xFFFFFFFF;
    SDL_GPUTransferBufferCreateInfo transferInfo = {};
    transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    transferInfo.size = sizeof(whitePixel);

    auto transfer = wrappers::MakeGpuResource<wrappers::UniqueGPUTransferBuffer>(
        device, SDL_CreateGPUTransferBuffer, &transferInfo);

    void* data = SDL_MapGPUTransferBuffer(device, transfer.get(), false);
    SDL_memcpy(data, &whitePixel, sizeof(whitePixel));
    SDL_UnmapGPUTransferBuffer(device, transfer.get());

    SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(device);
    SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmd);

    SDL_GPUTextureTransferInfo srcInfo = {};
    srcInfo.transfer_buffer = transfer.get();
    srcInfo.pixels_per_row = 1;
    srcInfo.rows_per_layer = 1;

    SDL_GPUTextureRegion dstRegion = {};
    dstRegion.texture = m_impl->m_whiteTexture.get();
    dstRegion.w = 1;
    dstRegion.h = 1;
    dstRegion.d = 1;

    SDL_UploadToGPUTexture(copyPass, &srcInfo, &dstRegion, false);
    SDL_EndGPUCopyPass(copyPass);
    SDL_SubmitGPUCommandBuffer(cmd);
}

void RenderSystem::initializeRenderers()
{
    m_impl->m_renderers.push_back(std::make_unique<renderers::ShapeRenderer>());
    m_impl->m_renderers.push_back(std::make_unique<renderers::ProgressBarRenderer>());
    m_impl->m_renderers.push_back(std::make_unique<renderers::SliderRenderer>());
    m_impl->m_renderers.push_back(std::make_unique<renderers::TextRenderer>());
    if (m_impl->m_iconManager)
    {
        m_impl->m_renderers.push_back(std::make_unique<renderers::IconRenderer>(m_impl->m_iconManager.get()));
    }
    m_impl->m_renderers.push_back(std::make_unique<renderers::ScrollBarRenderer>());
    m_impl->m_renderers.push_back(std::make_unique<renderers::ImageRenderer>());
    m_impl->m_renderers.push_back(std::make_unique<renderers::CanvasRenderer>());
    m_impl->m_renderers.push_back(std::make_unique<renderers::TableRenderer>());
    m_impl->m_renderers.push_back(std::make_unique<renderers::CheckBoxRenderer>());
    m_impl->m_renderers.push_back(std::make_unique<renderers::DropDownRenderer>());

    std::ranges::sort(
        m_impl->m_renderers,
        [](const std::unique_ptr<core::IRenderer>& leftRenderer, const std::unique_ptr<core::IRenderer>& rightRenderer)
        { return leftRenderer->getPriority() < rightRenderer->getPriority(); });

    Logger::info("[RenderSystem] 初始化了 {} 个渲染器", m_impl->m_renderers.size());
}

} // namespace ui::systems
