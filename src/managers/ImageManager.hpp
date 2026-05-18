/**
 * ************************************************************************
 *
 * @file ImageManager.hpp
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2026-05-18
 * @version 0.1
 * @brief 图像文件管理器 - 从文件加载 bmp/png/jpeg 并上传到 GPU
 *
 * ************************************************************************
 * @copyright Copyright (c) 2026 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */
#pragma once

#include <string>
#include <unordered_map>
#include <SDL3/SDL_gpu.h>
#include "../common/GPUWrappers.hpp"
#include "../singleton/Logger.hpp"

namespace ui::managers
{

/**
 * @brief 图像纹理管理器（单例）
 *
 * 缓存已加载的 GPU 纹理，避免重复上传。
 * 支持 bmp（SDL_LoadBMP）和 png/jpeg（stb_image）格式。
 */
class ImageManager
{
public:
    ImageManager(const ImageManager&) = delete;
    ImageManager& operator=(const ImageManager&) = delete;
    ImageManager(ImageManager&&) = delete;
    ImageManager& operator=(ImageManager&&) = delete;

    /**
     * @brief 获取全局单例
     */
    static ImageManager& instance()
    {
        static ImageManager inst;
        return inst;
    }

    /**
     * @brief 加载图像文件并上传到 GPU，返回纹理指针（原始，生命周期由 ImageManager 管理）
     * @param path  文件路径（bmp / png / jpeg）
     * @param device SDL_GPUDevice 指针
     * @return SDL_GPUTexture* 成功时返回纹理指针，失败返回 nullptr
     */
    SDL_GPUTexture* loadTexture(const std::string& path, SDL_GPUDevice* device);

    /**
     * @brief 释放所有已缓存纹理
     * @param device SDL_GPUDevice 指针（用于调用 SDL_ReleaseGPUTexture）
     */
    void releaseAll(SDL_GPUDevice* device);

private:
    ImageManager() = default;
    ~ImageManager() = default;

    /**
     * @brief 通过 stb_image 加载 png/jpeg 并上传
     */
    SDL_GPUTexture* loadWithStb(const std::string& path, SDL_GPUDevice* device);

    /**
     * @brief 通过 SDL_LoadBMP 加载 bmp 并上传
     */
    SDL_GPUTexture* loadWithSdlBmp(const std::string& path, SDL_GPUDevice* device);

    /**
     * @brief 将 RGBA 像素数据上传到 GPU 并返回纹理
     */
    SDL_GPUTexture* uploadToGpu(SDL_GPUDevice* device,
                                const unsigned char* pixels,
                                uint32_t width,
                                uint32_t height);

    // 路径 -> 已上传的 GPU 纹理（原始指针，生命周期同 ImageManager）
    std::unordered_map<std::string, SDL_GPUTexture*> m_cache;
};

} // namespace ui::managers
