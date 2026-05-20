#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "ImageManager.hpp"
#include "DeviceManager.hpp"

#include <cstring>
#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>

namespace ui::managers
{

ui::Result<SDL_GPUTexture*> ImageManager::loadTexture(const std::string& path)
{
    if (path.empty())
    {
        Logger::error("[ImageManager] loadTexture: empty path");
        return ui::MakeError(ui::ui_errc::invalid_argument);
    }

    SDL_GPUDevice* device = (m_deviceManager != nullptr) ? m_deviceManager->getDevice() : nullptr;
    if (device == nullptr)
    {
        Logger::error("[ImageManager] loadTexture: device is null, path={}", path);
        return ui::MakeError(ui::ui_errc::device_unavailable);
    }

    // 缓存命中
    if (auto it = m_cache.find(path); it != m_cache.end())
    {
        return it->second;
    }

    SDL_GPUTexture* tex = nullptr;

    // 按扩展名分派加载器
    const auto dot = path.rfind('.');
    if (dot != std::string::npos)
    {
        std::string ext = path.substr(dot + 1);
        for (auto& ch : ext)
        {
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        }

        if (ext == "bmp")
        {
            tex = loadWithSdlBmp(path, device);
        }
        else
        {
            tex = loadWithStb(path, device);
        }
    }
    else
    {
        tex = loadWithStb(path, device);
    }

    if (tex != nullptr)
    {
        m_cache[path] = tex;
        Logger::info("[ImageManager] Loaded texture: {}", path);
        return tex;
    }

    Logger::error("[ImageManager] Failed to load texture: {}", path);
    return ui::MakeError(ui::ui_errc::asset_decode_failed);
}

void ImageManager::releaseAll()
{
    if (m_cache.empty()) return;
    SDL_GPUDevice* device = (m_deviceManager != nullptr) ? m_deviceManager->getDevice() : nullptr;
    if (device == nullptr)
    {
        // 设备已经销毁，只能丢弃缓存；上层应保证 ImageManager 早于 device 析构
        Logger::warn("[ImageManager] releaseAll: device already gone, leaking {} cached textures", m_cache.size());
        m_cache.clear();
        return;
    }
    for (auto& [path, tex] : m_cache)
    {
        if (tex != nullptr)
        {
            SDL_ReleaseGPUTexture(device, tex);
        }
    }
    m_cache.clear();
}

SDL_GPUTexture* ImageManager::loadWithStb(const std::string& path, SDL_GPUDevice* device)
{
    int width = 0;
    int height = 0;
    int channels = 0;

    // 强制加载为 RGBA 4 通道
    unsigned char* pixels = stbi_load(path.c_str(), &width, &height, &channels, 4);
    if (pixels == nullptr)
    {
        Logger::error("[ImageManager] stbi_load failed: {} — {}", path, stbi_failure_reason());
        return nullptr;
    }

    SDL_GPUTexture* tex = uploadToGpu(device, pixels, static_cast<uint32_t>(width), static_cast<uint32_t>(height));
    stbi_image_free(pixels);
    return tex;
}

SDL_GPUTexture* ImageManager::loadWithSdlBmp(const std::string& path, SDL_GPUDevice* device)
{
    SDL_Surface* surface = SDL_LoadBMP(path.c_str());
    if (surface == nullptr)
    {
        Logger::error("[ImageManager] SDL_LoadBMP failed: {} — {}", path, SDL_GetError());
        return nullptr;
    }

    // 转换为 RGBA8
    SDL_Surface* converted = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA8888);
    SDL_DestroySurface(surface);

    if (converted == nullptr)
    {
        Logger::error("[ImageManager] SDL_ConvertSurface failed: {}", SDL_GetError());
        return nullptr;
    }

    SDL_GPUTexture* tex = uploadToGpu(device,
                                      static_cast<const unsigned char*>(converted->pixels),
                                      static_cast<uint32_t>(converted->w),
                                      static_cast<uint32_t>(converted->h));
    SDL_DestroySurface(converted);
    return tex;
}

SDL_GPUTexture* ImageManager::uploadToGpu(SDL_GPUDevice* device,
                                          const unsigned char* pixels,
                                          uint32_t width,
                                          uint32_t height)
{
    // 创建 GPU 纹理
    SDL_GPUTextureCreateInfo texInfo = {};
    texInfo.type = SDL_GPU_TEXTURETYPE_2D;
    texInfo.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    texInfo.width = width;
    texInfo.height = height;
    texInfo.layer_count_or_depth = 1;
    texInfo.num_levels = 1;
    texInfo.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;

    SDL_GPUTexture* texture = SDL_CreateGPUTexture(device, &texInfo);
    if (texture == nullptr)
    {
        Logger::error("[ImageManager] SDL_CreateGPUTexture failed: {}", SDL_GetError());
        return nullptr;
    }

    const uint32_t dataSize = width * height * 4U;

    // 创建传输缓冲区
    SDL_GPUTransferBufferCreateInfo transferInfo = {};
    transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    transferInfo.size = dataSize;

    SDL_GPUTransferBuffer* transferBuffer = SDL_CreateGPUTransferBuffer(device, &transferInfo);
    if (transferBuffer == nullptr)
    {
        Logger::error("[ImageManager] SDL_CreateGPUTransferBuffer failed: {}", SDL_GetError());
        SDL_ReleaseGPUTexture(device, texture);
        return nullptr;
    }

    // 映射并拷贝像素
    void* mapped = SDL_MapGPUTransferBuffer(device, transferBuffer, false);
    if (mapped == nullptr)
    {
        Logger::error("[ImageManager] SDL_MapGPUTransferBuffer failed: {}", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, transferBuffer);
        SDL_ReleaseGPUTexture(device, texture);
        return nullptr;
    }

    SDL_memcpy(mapped, pixels, dataSize);
    SDL_UnmapGPUTransferBuffer(device, transferBuffer);

    // 提交上传命令
    SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(device);
    if (cmd == nullptr)
    {
        Logger::error("[ImageManager] SDL_AcquireGPUCommandBuffer failed: {}", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, transferBuffer);
        SDL_ReleaseGPUTexture(device, texture);
        return nullptr;
    }

    SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmd);

    SDL_GPUTextureTransferInfo srcInfo = {};
    srcInfo.transfer_buffer = transferBuffer;
    srcInfo.pixels_per_row = width;
    srcInfo.rows_per_layer = height;

    SDL_GPUTextureRegion dstRegion = {};
    dstRegion.texture = texture;
    dstRegion.w = width;
    dstRegion.h = height;
    dstRegion.d = 1;

    SDL_UploadToGPUTexture(copyPass, &srcInfo, &dstRegion, false);
    SDL_EndGPUCopyPass(copyPass);
    SDL_SubmitGPUCommandBuffer(cmd);

    SDL_ReleaseGPUTransferBuffer(device, transferBuffer);

    return texture;
}

} // namespace ui::managers
