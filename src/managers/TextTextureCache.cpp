#include "TextTextureCache.hpp"

namespace ui::managers
{

TextTextureCache::TextTextureCache(ui::managers::DeviceManager& deviceManager,
                                   ui::managers::FontManager& fontManager)
    : m_deviceManager(deviceManager), m_fontManager(fontManager)
{
    Logger::info("[TextTextureCache] Initialized with max size: {}", MAX_CACHE_SIZE);
}

TextTextureCache::~TextTextureCache() { clear(); }

void TextTextureCache::clear()
{
    m_cache.clear();
    Logger::info("[TextTextureCache] Cleared all cached textures");
}

TextTextureCache::CacheStats TextTextureCache::getStats() const
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

size_t TextTextureCache::size() const { return m_cache.size(); }

SDL_GPUTexture* TextTextureCache::getOrUpload(const std::string& text,
                                              const Eigen::Vector4f& color,
                                              uint32_t& outWidth,
                                              uint32_t& outHeight,
                                              float fontSize)
{
    SDL_GPUDevice* device = m_deviceManager.getDevice();
    if (device == nullptr || !m_fontManager.isLoaded()) return nullptr;
    if (!isR8UnormSampledTextureSupported(device)) return nullptr;

    std::string cacheKey = buildCacheKey(text, fontSize);

    // 尝试从缓存获取
    if (SDL_GPUTexture* cachedTexture = tryGetFromCache(cacheKey, outWidth, outHeight))
    {
        return cachedTexture;
    }

    // 缓存未命中，创建新纹理
    return createAndCacheTexture(text, color, cacheKey, outWidth, outHeight, fontSize);
}

uint8_t TextTextureCache::quantizeColor(float value)
{
    return static_cast<uint8_t>(std::lround(std::clamp(value, 0.0F, 1.0F) * 255.0F));
}

std::string TextTextureCache::buildCacheKey(const std::string& text, float fontSize) const
{
    return text + "_" + std::to_string(static_cast<int>(fontSize * 10.0F));
}

bool TextTextureCache::isR8UnormSampledTextureSupported(SDL_GPUDevice* device)
{
    if (m_r8SupportCheckedDevice != device)
    {
        m_r8SupportCheckedDevice = device;
        m_r8UnormSampledSupportState = R8UnormSampledSupportState::UNKNOWN;
    }

    if (m_r8UnormSampledSupportState == R8UnormSampledSupportState::UNKNOWN)
    {
        const bool supported = SDL_GPUTextureSupportsFormat(
            device, SDL_GPU_TEXTUREFORMAT_R8_UNORM, SDL_GPU_TEXTURETYPE_2D, SDL_GPU_TEXTUREUSAGE_SAMPLER);
        m_r8UnormSampledSupportState =
            supported ? R8UnormSampledSupportState::SUPPORTED : R8UnormSampledSupportState::UNSUPPORTED;

        if (!supported)
        {
            Logger::warn(
                "[TextTextureCache] Device does not support sampled R8_UNORM format, skip text texture upload");
        }
    }

    return m_r8UnormSampledSupportState == R8UnormSampledSupportState::SUPPORTED;
}

SDL_GPUTexture* TextTextureCache::tryGetFromCache(const std::string& cacheKey,
                                                  uint32_t& outWidth,
                                                  uint32_t& outHeight)
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

SDL_GPUTexture* TextTextureCache::createAndCacheTexture(const std::string& text,
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

wrappers::UniqueGPUTexture TextTextureCache::createAndUploadTexture(SDL_GPUDevice* device,
                                                                    const std::vector<uint8_t>& bitmap,
                                                                    int width,
                                                                    int height)
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

bool TextTextureCache::uploadTextureData(SDL_GPUDevice* device,
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

void TextTextureCache::evictLRU()
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

void TextTextureCache::evictBatch()
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

} // namespace ui::managers
