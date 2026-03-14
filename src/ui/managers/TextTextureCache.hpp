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
#include <cmath>
#include <algorithm>
#include <SDL3/SDL_gpu.h>
#include "DeviceManager.hpp"
#include "FontManager.hpp"
#include "../common/GPUWrappers.hpp"
#include "../singleton/Logger.hpp"
#include <Eigen/Dense>

namespace ui::managers
{

class TextTextureCache
{
public:
    TextTextureCache(ui::managers::DeviceManager& deviceManager, ui::managers::FontManager& fontManager)
        : m_deviceManager(deviceManager), m_fontManager(fontManager)
    {
        Logger::info("[TextTextureCache] Initialized with max size: {}", MAX_CACHE_SIZE);
    }

    ~TextTextureCache() { clear(); }

    // 禁止拷贝和移动
    TextTextureCache(const TextTextureCache&) = delete;
    TextTextureCache& operator=(const TextTextureCache&) = delete;
    TextTextureCache(TextTextureCache&&) = delete;
    TextTextureCache& operator=(TextTextureCache&&) = delete;

    void clear()
    {
        m_cache.clear();
        Logger::info("[TextTextureCache] Cleared all cached textures");
    }

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

    CacheStats getStats() const
    {
        CacheStats stats{};
        stats.cacheSize = m_cache.size();
        stats.maxSize = MAX_CACHE_SIZE;
        stats.hitCount = m_hitCount;
        stats.missCount = m_missCount;
        stats.hitRate = (m_hitCount + m_missCount) > 0
                            ? static_cast<float>(m_hitCount) / static_cast<float>(m_hitCount + m_missCount)
                            : 0.0F;
        stats.evictionCount = m_evictionCount;
        return stats;
    }

    /**
     * @brief 获取缓存大小
     */
    size_t size() const { return m_cache.size(); }

    SDL_GPUTexture* getOrUpload(const std::string& text,
                                const Eigen::Vector4f& color,
                                uint32_t& outWidth,
                                uint32_t& outHeight,
                                float fontSize = 0.0F)
    {
        SDL_GPUDevice* device = m_deviceManager.getDevice();
        if (device == nullptr || !m_fontManager.isLoaded()) return nullptr;

        std::string cacheKey = buildCacheKey(text, fontSize);

        // 尝试从缓存获取
        if (SDL_GPUTexture* cachedTexture = tryGetFromCache(cacheKey, outWidth, outHeight))
        {
            return cachedTexture;
        }

        // 缓存未命中，创建新纹理
        return createAndCacheTexture(text, color, cacheKey, outWidth, outHeight, fontSize);
    }

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
    static uint8_t quantizeColor(float value)
    {
        return static_cast<uint8_t>(std::lround(std::clamp(value, 0.0F, 1.0F) * 255.0F));
    }

    /**
     * @brief 构建缓存键
     * @note 文本纹理改为 alpha mask 后，颜色在 shader 中着色，缓存键不再包含颜色
     */
    std::string buildCacheKey(const std::string& text, float fontSize) const
    {
        return text + "_" + std::to_string(static_cast<int>(fontSize * 10.0F));
    }

    /**
     * @brief 尝试从缓存获取纹理
     */
    SDL_GPUTexture* tryGetFromCache(const std::string& cacheKey, uint32_t& outWidth, uint32_t& outHeight)
    {
        auto iter = m_cache.find(cacheKey);
        if (iter != m_cache.end())
        {
            // 缓存命中：更新访问时间
            iter->second.lastAccessTime = std::chrono::steady_clock::now();
            iter->second.accessCount++;
            m_hitCount++;

            outWidth = iter->second.width;
            outHeight = iter->second.height;
            return iter->second.texture.get();
        }
        return nullptr;
    }

    /**
     * @brief 创建并缓存新纹理
     */
    SDL_GPUTexture* createAndCacheTexture(const std::string& text,
                                          const Eigen::Vector4f& color,
                                          const std::string& cacheKey,
                                          uint32_t& outWidth,
                                          uint32_t& outHeight,
                                          float fontSize)
    {
        SDL_GPUDevice* device = m_deviceManager.getDevice();

        // 缓存未命中
        m_missCount++;

        // 如果达到容量限制，执行LRU驱逐
        if (m_cache.size() >= MAX_CACHE_SIZE)
        {
            evictLRU();
        }

        // 渲染文本位图（传入 fontSize）
        int bitmapWidth = 0;
        int bitmapHeight = 0;
        std::vector<uint8_t> bitmap =
            m_fontManager.renderTextAlphaMask(text, quantizeColor(color.w()), bitmapWidth, bitmapHeight, fontSize);

        if (bitmap.empty() || bitmapWidth == 0 || bitmapHeight == 0)
        {
            return nullptr;
        }

        // 创建GPU纹理并上传数据
        auto texture = createAndUploadTexture(device, bitmap, bitmapWidth, bitmapHeight);
        if (texture == nullptr)
        {
            return nullptr;
        }

        SDL_GPUTexture* rawTexture = texture.get();

        // 创建缓存条目
        CacheEntry entry{};
        entry.texture = std::move(texture);
        entry.width = static_cast<uint32_t>(bitmapWidth);
        entry.height = static_cast<uint32_t>(bitmapHeight);
        entry.lastAccessTime = std::chrono::steady_clock::now();
        entry.accessCount = 1;

        m_cache[cacheKey] = std::move(entry);

        outWidth = static_cast<uint32_t>(bitmapWidth);
        outHeight = static_cast<uint32_t>(bitmapHeight);

        return rawTexture;
    }

    /**
     * @brief 创建GPU纹理并上传数据
     */
    wrappers::UniqueGPUTexture
        createAndUploadTexture(SDL_GPUDevice* device, const std::vector<uint8_t>& bitmap, int width, int height)
    {
        SDL_GPUTextureCreateInfo textureInfo = {};
        textureInfo.type = SDL_GPU_TEXTURETYPE_2D;
        textureInfo.format = SDL_GPU_TEXTUREFORMAT_R8_UNORM;
        textureInfo.width = static_cast<uint32_t>(width);
        textureInfo.height = static_cast<uint32_t>(height);
        textureInfo.layer_count_or_depth = 1;
        textureInfo.num_levels = 1;
        textureInfo.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;

        auto texture =
            wrappers::MakeGpuResource<wrappers::UniqueGPUTexture>(device, SDL_CreateGPUTexture, &textureInfo);
        if (!texture)
        {
            Logger::error("[TextTextureCache] Failed to create texture");
            return nullptr;
        }

        // 上传纹理数据
        if (!uploadTextureData(device, texture.get(), bitmap, textureInfo.width, textureInfo.height))
        {
            return nullptr;
        }

        return texture;
    }

    /**
     * @brief 上传纹理数据到GPU
     */
    bool uploadTextureData(SDL_GPUDevice* device,
                           SDL_GPUTexture* texture,
                           const std::vector<uint8_t>& bitmap,
                           uint32_t width,
                           uint32_t height)
    {
        SDL_GPUTransferBufferCreateInfo transferInfo = {};
        transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        transferInfo.size = static_cast<uint32_t>(bitmap.size());

        auto transferBuffer = wrappers::MakeGpuResource<wrappers::UniqueGPUTransferBuffer>(
            device, SDL_CreateGPUTransferBuffer, &transferInfo);
        if (!transferBuffer)
        {
            Logger::error("[TextTextureCache] Failed to create transfer buffer");
            return false;
        }

        void* data = SDL_MapGPUTransferBuffer(device, transferBuffer.get(), false);
        if (data == nullptr)
        {
            Logger::error("[TextTextureCache] Failed to map transfer buffer");
            return false;
        }

        SDL_memcpy(data, bitmap.data(), bitmap.size());
        SDL_UnmapGPUTransferBuffer(device, transferBuffer.get());

        SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(device);
        SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmd);

        SDL_GPUTextureTransferInfo srcInfo = {};
        srcInfo.transfer_buffer = transferBuffer.get();
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

        return true;
    }

    /**
     * @brief 驱逐最少使用的缓存条目（LRU策略）
     */
    void evictLRU()
    {
        if (m_cache.empty()) return;

        SDL_GPUDevice* device = m_deviceManager.getDevice();
        if (device == nullptr) return;

        // 找到最旧的条目
        auto lru = m_cache.begin();
        for (auto it = m_cache.begin(); it != m_cache.end(); ++it)
        {
            if (it->second.lastAccessTime < lru->second.lastAccessTime)
            {
                lru = it;
            }
        }

        // 释放纹理资源 (Auto managed by UniqueGPUTexture)
        /*
        if (lru->second.cachedTexture.texture != nullptr)
        {
            SDL_ReleaseGPUTexture(device, lru->second.cachedTexture.texture);
        }
        */

        Logger::debug("[TextTextureCache] Evicted LRU entry: {} (access count: {})",
                      lru->first.substr(0, 50),
                      lru->second.accessCount);

        m_cache.erase(lru);
        m_evictionCount++;

        // 如果缓存仍然过大，批量驱逐更多条目
        if (m_cache.size() >= MAX_CACHE_SIZE)
        {
            evictBatch();
        }
    }

    /**
     * @brief 批量驱逐最少使用的条目
     */
    void evictBatch()
    {
        if (m_cache.size() <= EVICTION_BATCH) return;

        SDL_GPUDevice* device = m_deviceManager.getDevice();
        if (device == nullptr) return;

        // 创建按访问时间排序的列表
        std::vector<std::pair<std::string, std::chrono::steady_clock::time_point>> entries;
        entries.reserve(m_cache.size());

        for (const auto& [key, entry] : m_cache)
        {
            entries.emplace_back(key, entry.lastAccessTime);
        }

        // 按访问时间排序（最旧的在前）
        std::sort(entries.begin(),
                  entries.end(),
                  [](const auto& first, const auto& second) { return first.second < second.second; });

        // 驱逐前 EVICTION_BATCH 个条目
        size_t evicted = 0;
        for (size_t i = 0; i < std::min(EVICTION_BATCH, entries.size()); ++i)
        {
            auto iter = m_cache.find(entries[i].first);
            if (iter != m_cache.end())
            {
                m_cache.erase(iter);
                evicted++;
            }
        }

        m_evictionCount += evicted;
        Logger::info("[TextTextureCache] Batch evicted {} entries, cache size: {}", evicted, m_cache.size());
    }

    DeviceManager& m_deviceManager;
    FontManager& m_fontManager;
    std::unordered_map<std::string, CacheEntry> m_cache;

    // 统计信息
    mutable size_t m_hitCount = 0;
    mutable size_t m_missCount = 0;
    size_t m_evictionCount = 0;
};

} // namespace ui::managers
