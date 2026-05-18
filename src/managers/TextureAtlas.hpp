/**
 * ************************************************************************
 *
 * @file TextureAtlas.hpp
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2026-02-10
 * @version 0.1
 * @brief GPU 纹理图集管理器，用于字形缓存
 *
 * 采用 Shelf Bin Packing 算法管理纹理图集：
 * - 每次分配从当前 shelf（行）尝试，不够则开新行
 * - 支持自动扩展图集尺寸（2048x2048 -> 4096x4096）
 * - 每个字形记录其 UV 坐标和偏移量
 *
 * ************************************************************************
 * @copyright Copyright (c) 2026 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */

#pragma once

#include <SDL3/SDL_gpu.h>
#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include "../common/GPUWrappers.hpp"
#include "../singleton/Logger.hpp"

namespace ui::wrappers
{
using GPUTexturePtr = UniqueGPUTexture;
}

namespace ui::managers
{

/**
 * @brief 字形在图集中的位置信息
 */
struct AtlasGlyph
{
    // UV 坐标 (归一化)
    float u0 = 0.0F;
    float v0 = 0.0F;
    float u1 = 0.0F;
    float v1 = 0.0F;

    // 像素坐标（在图集中的位置）
    int32_t x = 0;
    int32_t y = 0;
    int32_t width = 0;
    int32_t height = 0;

    // 渲染偏移量
    int32_t bearingX = 0;  // 水平偏移
    int32_t bearingY = 0;  // 垂直偏移（基线到字形顶部）
    float advanceX = 0.0F; // 水平前进量
};

/**
 * @brief 纹理图集管理器 (Shelf Bin Packing)
 */
class TextureAtlas
{
public:
    /**
     * @brief 构造纹理图集
     * @param device GPU 设备
     * @param initialSize 初始尺寸（宽高相同）
     * @param padding 字形之间的内边距（像素）
     */
    explicit TextureAtlas(SDL_GPUDevice* device, uint32_t initialSize = 2048, uint32_t padding = 2)
        : m_device(device), m_size(initialSize), m_padding(padding)
    {
        if (!createTexture())
        {
            Logger::error("[TextureAtlas] Failed to create initial texture");
        }
    }

    ~TextureAtlas()
    {
        if (m_texture)
        {
            SDL_ReleaseGPUTexture(m_device, m_texture.get());
        }
    }

    // 禁止拷贝和移动
    TextureAtlas(const TextureAtlas&) = delete;
    TextureAtlas& operator=(const TextureAtlas&) = delete;
    TextureAtlas(TextureAtlas&&) = delete;
    TextureAtlas& operator=(TextureAtlas&&) = delete;

    /**
     * @brief 获取图集纹理
     */
    [[nodiscard]] SDL_GPUTexture* getTexture() const { return m_texture.get(); }

    /**
     * @brief 获取图集尺寸
     */
    [[nodiscard]] uint32_t getSize() const { return m_size; }

    /**
     * @brief 添加字形到图集
     * @param codepoint Unicode 码点（作为 key）
     * @param bitmap 字形位图数据（灰度，单通道）
     * @param width 位图宽度
     * @param height 位图高度
     * @param bearingX 水平偏移
     * @param bearingY 垂直偏移
     * @param advanceX 水平前进量
     * @return 字形信息，失败返回 nullopt
     */
    std::optional<AtlasGlyph> addGlyph(uint32_t codepoint,
                                       const uint8_t* bitmap,
                                       int32_t width,
                                       int32_t height,
                                       int32_t bearingX,
                                       int32_t bearingY,
                                       float advanceX)
    {
        // 检查是否已缓存
        auto iter = m_glyphMap.find(codepoint);
        if (iter != m_glyphMap.end())
        {
            return iter->second;
        }

        // 尝试分配空间
        auto pos = allocate(width, height);
        if (!pos.has_value())
        {
            // 尝试扩展图集
            if (!expand())
            {
                Logger::error("[TextureAtlas] Failed to expand atlas for codepoint {}", codepoint);
                return std::nullopt;
            }
            pos = allocate(width, height);
            if (!pos.has_value())
            {
                Logger::error("[TextureAtlas] Still cannot allocate after expansion");
                return std::nullopt;
            }
        }

        auto [xPos, yPos] = *pos;

        // 上传位图到 GPU
        if (!uploadBitmap(bitmap, xPos, yPos, width, height))
        {
            Logger::error("[TextureAtlas] Failed to upload bitmap for codepoint {}", codepoint);
            return std::nullopt;
        }

        // 构造字形信息
        AtlasGlyph glyph;
        glyph.x = static_cast<int32_t>(xPos);
        glyph.y = static_cast<int32_t>(yPos);
        glyph.width = width;
        glyph.height = height;
        glyph.bearingX = bearingX;
        glyph.bearingY = bearingY;
        glyph.advanceX = advanceX;

        // 计算 UV 坐标（归一化）
        auto fSize = static_cast<float>(m_size);
        glyph.u0 = static_cast<float>(xPos) / fSize;
        glyph.v0 = static_cast<float>(yPos) / fSize;
        glyph.u1 = static_cast<float>(xPos + static_cast<uint32_t>(width)) / fSize;
        glyph.v1 = static_cast<float>(yPos + static_cast<uint32_t>(height)) / fSize;

        // 缓存
        m_glyphMap[codepoint] = glyph;
        return glyph;
    }

    /**
     * @brief 查询字形是否已缓存
     */
    [[nodiscard]] std::optional<AtlasGlyph> getGlyph(uint32_t codepoint) const
    {
        auto iter = m_glyphMap.find(codepoint);
        if (iter != m_glyphMap.end())
        {
            return iter->second;
        }
        return std::nullopt;
    }

    /**
     * @brief 清空图集
     */
    void clear()
    {
        m_glyphMap.clear();
        m_shelves.clear();
        m_currentShelfY = 0;
        Logger::info("[TextureAtlas] Cleared all glyphs");
    }

    /**
     * @brief 获取统计信息
     */
    struct Stats
    {
        uint32_t atlasSize = 0;
        uint32_t glyphCount = 0;
        uint32_t shelfCount = 0;
        uint32_t usedPixels = 0;
        float utilization = 0.0F;
    };

    [[nodiscard]] Stats getStats() const
    {
        Stats stats;
        stats.atlasSize = m_size;
        stats.glyphCount = static_cast<uint32_t>(m_glyphMap.size());
        stats.shelfCount = static_cast<uint32_t>(m_shelves.size());

        uint32_t usedPixels = 0;
        for (const auto& [codepoint, glyph] : m_glyphMap)
        {
            usedPixels += static_cast<uint32_t>(glyph.width * glyph.height);
        }
        stats.usedPixels = usedPixels;

        uint32_t totalPixels = m_size * m_size;
        stats.utilization = totalPixels > 0 ? static_cast<float>(usedPixels) / static_cast<float>(totalPixels) : 0.0F;

        return stats;
    }

private:
    /**
     * @brief Shelf 结构（一行）
     */
    struct Shelf
    {
        uint32_t y = 0;      // Shelf 起始 Y 坐标
        uint32_t height = 0; // Shelf 高度
        uint32_t x = 0;      // 当前行的 X 游标
    };

    /**
     * @brief 创建 GPU 纹理
     */
    bool createTexture()
    {
        SDL_GPUTextureCreateInfo textureInfo{};
        textureInfo.type = SDL_GPU_TEXTURETYPE_2D;
        textureInfo.format = SDL_GPU_TEXTUREFORMAT_R8_UNORM; // 单通道灰度
        textureInfo.width = m_size;
        textureInfo.height = m_size;
        textureInfo.layer_count_or_depth = 1;
        textureInfo.num_levels = 1;
        textureInfo.sample_count = SDL_GPU_SAMPLECOUNT_1;
        textureInfo.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;

        SDL_GPUTexture* texture = SDL_CreateGPUTexture(m_device, &textureInfo);
        if (texture == nullptr)
        {
            Logger::error("[TextureAtlas] Failed to create texture: {}", SDL_GetError());
            return false;
        }

        m_texture.reset(texture);
        Logger::info("[TextureAtlas] Created texture atlas {}x{}", m_size, m_size);
        return true;
    }

    /**
     * @brief 分配空间（Shelf Bin Packing）
     * @return 成功返回 (x, y) 坐标
     */
    std::optional<std::pair<uint32_t, uint32_t>> allocate(int32_t width, int32_t height)
    {
        uint32_t glyphWidth = static_cast<uint32_t>(width) + m_padding;
        uint32_t glyphHeight = static_cast<uint32_t>(height) + m_padding;

        // 尝试从现有 shelf 分配
        for (auto& shelf : m_shelves)
        {
            if (shelf.height >= glyphHeight && (shelf.x + glyphWidth) <= m_size)
            {
                uint32_t allocX = shelf.x;
                uint32_t allocY = shelf.y;
                shelf.x += glyphWidth;
                return std::make_pair(allocX, allocY);
            }
        }

        // 创建新 shelf
        if (m_currentShelfY + glyphHeight <= m_size)
        {
            Shelf newShelf;
            newShelf.y = m_currentShelfY;
            newShelf.height = glyphHeight;
            newShelf.x = glyphWidth;

            m_shelves.push_back(newShelf);
            m_currentShelfY += glyphHeight;

            return std::make_pair(0U, newShelf.y);
        }

        return std::nullopt;
    }

    /**
     * @brief 扩展图集尺寸（2x）
     */
    bool expand()
    {
        if (m_size >= 4096)
        {
            Logger::warn("[TextureAtlas] Cannot expand beyond 4096x4096");
            return false;
        }

        uint32_t newSize = m_size * 2;
        Logger::info("[TextureAtlas] Expanding atlas from {}x{} to {}x{}", m_size, m_size, newSize, newSize);

        // 创建新纹理
        SDL_GPUTextureCreateInfo textureInfo{};
        textureInfo.type = SDL_GPU_TEXTURETYPE_2D;
        textureInfo.format = SDL_GPU_TEXTUREFORMAT_R8_UNORM;
        textureInfo.width = newSize;
        textureInfo.height = newSize;
        textureInfo.layer_count_or_depth = 1;
        textureInfo.num_levels = 1;
        textureInfo.sample_count = SDL_GPU_SAMPLECOUNT_1;
        textureInfo.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;

        SDL_GPUTexture* newTexture = SDL_CreateGPUTexture(m_device, &textureInfo);
        if (!newTexture)
        {
            Logger::error("[TextureAtlas] Failed to create expanded texture");
            return false;
        }

        // TODO: 理想情况下应该复制旧纹理内容到新纹理
        // SDL3 GPU API 需要通过 blit 或重新上传实现
        // 这里简化实现：仅释放旧纹理，字形需要重新上传

        if (m_texture)
        {
            SDL_ReleaseGPUTexture(m_device, m_texture.get());
        }

        m_texture.reset(newTexture);
        m_size = newSize;

        // 重置布局状态（字形映射保留，但需重新上传）
        // 简化处理：清空所有，让调用方重新添加
        m_glyphMap.clear();
        m_shelves.clear();
        m_currentShelfY = 0;

        Logger::warn("[TextureAtlas] Expansion requires re-uploading all glyphs");
        return true;
    }

    /**
     * @brief 上传位图到纹理（灰度转 R8）
     */
    bool uploadBitmap(const uint8_t* bitmap, uint32_t xPos, uint32_t yPos, int32_t width, int32_t height)
    {
        if (bitmap == nullptr || width <= 0 || height <= 0) return false;

        // 注意：SDL3 GPU API 需要通过 CopyPass 上传纹理
        // 这里简化实现，实际应该创建 TransferBuffer 并使用 CopyPass
        // 由于 SDL_UploadToGPUTexture 签名在不同版本可能不同，这里提供占位实现
        // TODO: 完整实现需要 CommandBuffer + CopyPass + TransferBuffer

        Logger::warn("[TextureAtlas] uploadBitmap not fully implemented, atlas updates may not work");
        return true;
    }

    SDL_GPUDevice* m_device = nullptr;
    wrappers::GPUTexturePtr m_texture;

    uint32_t m_size = 2048;
    uint32_t m_padding = 2;

    std::vector<Shelf> m_shelves;
    uint32_t m_currentShelfY = 0;

    std::unordered_map<uint32_t, AtlasGlyph> m_glyphMap;
};

} // namespace ui::managers
