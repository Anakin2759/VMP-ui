/**
 * ************************************************************************
 *
 * @file FontAtlasManager.hpp
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2026-02-10
 * @version 0.1
 * @brief 字体图集管理器（FreeType + TextureAtlas）
 *
 * 集成 FreeType 字体管理和纹理图集，实现高效的字形缓存：
 * - 使用 FreeType2 渲染高质量字形
 * - 字形位图统一缓存到 GPU 纹理图集
 * - 提供字形 UV 坐标用于文本渲染
 *
 * ************************************************************************
 * @copyright Copyright (c) 2026 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */

#pragma once

#include "FontManager.hpp"
#include "TextureAtlas.hpp"
#include "DeviceManager.hpp"
#include "common/ErrorCodes.hpp"
#include "common/Result.hpp"
#include <memory>
#include <optional>

namespace ui::managers
{

/**
 * @brief 字体图集管理器（集成 FontManager + TextureAtlas）
 */
class FontAtlasManager
{
public:
    explicit FontAtlasManager(DeviceManager& deviceManager)
        : m_deviceManager(deviceManager), m_fontManager(std::make_unique<FontManager>())
    {
        Logger::info("[FontAtlasManager] Initialized");
    }

    ~FontAtlasManager() = default;

    // 禁止拷贝和移动
    FontAtlasManager(const FontAtlasManager&) = delete;
    FontAtlasManager& operator=(const FontAtlasManager&) = delete;
    FontAtlasManager(FontAtlasManager&&) = delete;
    FontAtlasManager& operator=(FontAtlasManager&&) = delete;

    /**
     * @brief 从内存加载字体
     * @param fontData 字体数据指针
     * @param dataSize 字体数据大小
     * @param fontSize 字体大小（像素）
     * @return 加载成功返回 true
     */
    Result<void> loadFromMemory(const uint8_t* fontData, size_t dataSize, float fontSize)
    {
        if (auto loadResult = m_fontManager->loadFromMemory(fontData, dataSize, fontSize); !loadResult.has_value())
        {
            return MakeError(loadResult.error());
        }

        // 创建纹理图集
        SDL_GPUDevice* device = m_deviceManager.getDevice();
        if (device == nullptr)
        {
            Logger::error("[FontAtlasManager] No GPU device available");
            return MakeError(UiErrc::DEVICE_UNAVAILABLE);
        }

        m_atlas = std::make_unique<TextureAtlas>(device, 2048, 2);
        Logger::info("[FontAtlasManager] Font loaded and atlas created");
        return Ok();
    }

    /**
     * @brief 检查字体是否已加载
     */
    [[nodiscard]] bool isLoaded() const { return m_fontManager && m_fontManager->isLoaded() && m_atlas; }

    /**
     * @brief 获取字体高度（行高）
     */
    [[nodiscard]] int getFontHeight() const { return m_fontManager ? m_fontManager->getFontHeight() : 0; }

    /**
     * @brief 获取基线位置
     */
    [[nodiscard]] int getBaseline() const { return m_fontManager ? m_fontManager->getBaseline() : 0; }

    /**
     * @brief 测量文本宽度
     */
    int measureTextWidth(const std::string& text) { return m_fontManager ? m_fontManager->measureTextWidth(text) : 0; }

    /**
     * @brief 测量文本（带最大宽度限制）
     */
    int measureString(const char* text, size_t textLen, int maxWidth, size_t* outMeasuredLength)
    {
        return m_fontManager ? m_fontManager->measureString(text, textLen, maxWidth, outMeasuredLength) : 0;
    }

    /**
     * @brief 使用 HarfBuzz 对文本进行成形
     * @param text UTF-8 文本
     * @param textLen 文本长度
     * @return 成形后的字形位置数组
     */
    std::vector<ShapedGlyph> shapeText(const char* text, size_t textLen)
    {
        return m_fontManager ? m_fontManager->shapeText(text, textLen) : std::vector<ShapedGlyph>{};
    }

    /**
     * @brief 获取字形（自动添加到图集）
     * @param codepoint Unicode 码点
     * @return 图集中的字形信息，失败返回 nullopt
     */
    std::optional<AtlasGlyph> getOrAddGlyph(uint32_t codepoint)
    {
        if (!isLoaded()) return std::nullopt;

        // 检查图集中是否已存在
        auto existing = m_atlas->getGlyph(codepoint);
        if (existing.has_value())
        {
            return existing;
        }

        // 渲染字形位图
        GlyphInfo glyph = m_fontManager->renderGlyph(static_cast<int>(codepoint));
        if (glyph.bitmap.empty())
        {
            Logger::warn("[FontAtlasManager] Empty bitmap for codepoint {}", codepoint);
            return std::nullopt;
        }

        // 添加到图集
        return m_atlas->addGlyph(
            codepoint, glyph.bitmap.data(), glyph.width, glyph.height, glyph.bearingX, glyph.bearingY, glyph.advanceX);
    }

    /**
     * @brief 通过字形索引获取字形（用于 HarfBuzz 成形结果）
     * @param glyphId 字形索引（由 HarfBuzz shapeText 返回）
     * @return 图集中的字形信息，失败返回 nullopt
     */
    std::optional<AtlasGlyph> getOrAddGlyphByIndex(uint32_t glyphId)
    {
        if (!isLoaded()) return std::nullopt;

        // 使用高位标记区分 glyphId 与 codepoint，避免图集缓存冲突
        static constexpr uint32_t GLYPH_ID_FLAG = 0x80000000U;
        uint32_t key = glyphId | GLYPH_ID_FLAG;

        // 检查图集中是否已存在
        auto existing = m_atlas->getGlyph(key);
        if (existing.has_value())
        {
            return existing;
        }

        // 按字形索引渲染位图
        GlyphInfo glyph = m_fontManager->renderGlyphByIndex(glyphId);
        if (glyph.bitmap.empty())
        {
            return std::nullopt;
        }

        // 添加到图集
        return m_atlas->addGlyph(
            key, glyph.bitmap.data(), glyph.width, glyph.height, glyph.bearingX, glyph.bearingY, glyph.advanceX);
    }

    /**
     * @brief 获取图集纹理
     */
    [[nodiscard]] SDL_GPUTexture* getAtlasTexture() const { return m_atlas ? m_atlas->getTexture() : nullptr; }

    /**
     * @brief 获取图集统计信息
     */
    [[nodiscard]] TextureAtlas::Stats getAtlasStats() const
    {
        return m_atlas ? m_atlas->getStats() : TextureAtlas::Stats{};
    }

    /**
     * @brief 清空字形缓存和图集
     */
    void clear()
    {
        if (m_fontManager)
        {
            m_fontManager->clearCache();
        }
        if (m_atlas)
        {
            m_atlas->clear();
        }
        Logger::info("[FontAtlasManager] Cleared all caches");
    }

    /**
     * @brief 解码 UTF-8 字符（工具方法）
     */
    static size_t decodeUTF8(std::string_view text, int& outCodepoint)
    {
        return FontManager::decodeUTF8(text, outCodepoint);
    }

private:
    DeviceManager& m_deviceManager;
    std::unique_ptr<FontManager> m_fontManager;
    std::unique_ptr<TextureAtlas> m_atlas;
};

} // namespace ui::managers
