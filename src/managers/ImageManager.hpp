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

#include "common/Result.hpp"

namespace ui::managers
{

class DeviceManager;

/**
 * @brief 图像纹理管理器（多实例，按渲染上下文持有）
 *
 * - 持有一个 DeviceManager* 句柄，所有纹理与该 device 绑定
 * - 缓存 `path -> SDL_GPUTexture*`，避免重复上传
 * - 析构时自动释放所有缓存纹理，无需调用方显式 releaseAll
 * - 支持 bmp（SDL_LoadBMP）和 png/jpeg（stb_image）格式
 */
class ImageManager
{
public:
    explicit ImageManager(DeviceManager* deviceManager) : m_deviceManager(deviceManager) {}
    ~ImageManager() noexcept;

    ImageManager(const ImageManager&) = delete;
    ImageManager& operator=(const ImageManager&) = delete;
    ImageManager(ImageManager&&) = delete;
    ImageManager& operator=(ImageManager&&) = delete;

    /**
     * @brief 加载图像文件并上传到 GPU。
     *
     * 返回的纹理生命周期由本 ImageManager 持有，禁止外部调用 SDL_ReleaseGPUTexture。
     *
     * @param path  文件路径（bmp / png / jpeg）
     * @return Result<SDL_GPUTexture*> 成功时持有非空纹理指针；失败时携带 ui_errc。
     */
    [[nodiscard]] ui::Result<SDL_GPUTexture*> loadTexture(const std::string& path);

    /**
     * @brief 主动释放所有已缓存纹理（析构会自动调用，正常路径下无需手动调用）。
     */
    void releaseAll();

private:
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
    SDL_GPUTexture* uploadToGpu(SDL_GPUDevice* device, const unsigned char* pixels, uint32_t width, uint32_t height);

    DeviceManager* m_deviceManager = nullptr;
    // 路径 -> 已上传的 GPU 纹理（原始指针，生命周期同 ImageManager）
    std::unordered_map<std::string, SDL_GPUTexture*> m_cache;
};

} // namespace ui::managers
