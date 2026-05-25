/**
 * ************************************************************************
 *
 * @file TextTextureCache.hpp
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2026-01-30
 * @version 0.1
 * @brief 字体文本纹理缓存管理器
 *
 * ************************************************************************
 * @copyright Copyright (c) 2026 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */

#pragma once
#include <unordered_map>
#include <string>
#include <chrono>
#include <cstdint>
#include <SDL3/SDL_gpu.h>
#include "DeviceManager.hpp"
#include "FontManager.hpp"
#include "../common/GPUWrappers.hpp"
#include <Eigen/Dense>

namespace ui::managers
{

class TextTextureCache
{
public:
    TextTextureCache(ui::managers::DeviceManager& deviceManager, ui::managers::FontManager& fontManager);

    ~TextTextureCache();

    // 禁止拷贝和移动
    TextTextureCache(const TextTextureCache&) = delete;
    TextTextureCache& operator=(const TextTextureCache&) = delete;
    TextTextureCache(TextTextureCache&&) = delete;
    TextTextureCache& operator=(TextTextureCache&&) = delete;

    void clear();

    /**
     * @brief 获取缓存统计信息
     */
    struct CacheStats
    {
        size_t cacheSize;
        size_t maxSize;
        size_t hitCount;
        size_t missCount;
        float hitRate;
        size_t evictionCount;
    };

    CacheStats getStats() const;

    /**
     * @brief 获取缓存大小
     */
    size_t size() const;

    SDL_GPUTexture* getOrUpload(const std::string& text,
                                const Eigen::Vector4f& color,
                                uint32_t& outWidth,
                                uint32_t& outHeight,
                                float fontSize = 0.0F);

private:
    // 缓存容量配置
    static constexpr size_t MAX_CACHE_SIZE = 256; // 最大缓存条目数
    static constexpr size_t EVICTION_BATCH = 32;  // 每次驱逐数量

    // 缓存条目结构
    struct CacheEntry
    {
        wrappers::UniqueGPUTexture texture;
        uint32_t width = 0;
        uint32_t height = 0;
        std::chrono::steady_clock::time_point lastAccessTime;
        uint32_t accessCount = 0;
    };

    /**
     * @brief 将浮点颜色分量量化为 uint8（四舍五入）
     */
    static uint8_t quantizeColor(float value);

    /**
     * @brief 构建缓存键
     * @note 文本纹理改为 alpha mask 后，颜色在 shader 中着色，缓存键不再包含颜色
     */
    std::string buildCacheKey(const std::string& text, float fontSize) const;

    bool isR8UnormSampledTextureSupported(SDL_GPUDevice* device);

    /**
     * @brief 尝试从缓存获取纹理
     */
    SDL_GPUTexture* tryGetFromCache(const std::string& cacheKey, uint32_t& outWidth, uint32_t& outHeight);

    /**
     * @brief 创建并缓存新纹理
     */
    SDL_GPUTexture* createAndCacheTexture(const std::string& text,
                                          const Eigen::Vector4f& color,
                                          const std::string& cacheKey,
                                          uint32_t& outWidth,
                                          uint32_t& outHeight,
                                          float fontSize);

    /**
     * @brief 创建GPU纹理并上传数据
     */
    wrappers::UniqueGPUTexture
        createAndUploadTexture(SDL_GPUDevice* device, const std::vector<uint8_t>& bitmap, int width, int height);

    /**
     * @brief 上传纹理数据到GPU
     */
    bool uploadTextureData(SDL_GPUDevice* device,
                           SDL_GPUTexture* texture,
                           const std::vector<uint8_t>& bitmap,
                           uint32_t width,
                           uint32_t height);

    /**
     * @brief 驱逐最少使用的缓存条目（LRU策略）
     */
    void evictLRU();

    /**
     * @brief 批量驱逐最少使用的条目
     */
    void evictBatch();

    DeviceManager& m_deviceManager;
    FontManager& m_fontManager;
    std::unordered_map<std::string, CacheEntry> m_cache;

    enum class R8UnormSampledSupportState : std::uint8_t
    {
        UNKNOWN,
        SUPPORTED,
        UNSUPPORTED,
    };

    SDL_GPUDevice* m_r8SupportCheckedDevice = nullptr;
    R8UnormSampledSupportState m_r8UnormSampledSupportState = R8UnormSampledSupportState::UNKNOWN;

    // 统计信息
    mutable size_t m_hitCount = 0;
    mutable size_t m_missCount = 0;
    size_t m_evictionCount = 0;
};

} // namespace ui::managers
