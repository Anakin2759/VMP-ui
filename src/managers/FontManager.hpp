/**
 * ************************************************************************
 *
 * @file FontManager.hpp
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2026-01-30
 * @version 0.2
 * @brief 字体管理器（基于 FreeType2）
 *
 * 使用 FreeType2 进行高质量字形渲染，配合纹理图集缓存
 * - 默认加载 ui/assets/fonts/ 下的字体文件
 * - 支持 TTF, OTF, TTC 等多种字体格式
 * - 字形位图采用抗锯齿灰度渲染
 *
 * 2026-02-10 更新说明：
 *  从 stb_truetype 迁移到 FreeType2，支持更好的渲染质量和字体格式
 *
 * ************************************************************************
 * @copyright Copyright (c) 2026 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */

#pragma once

#include <ft2build.h>
#include FT_FREETYPE_H

#include <hb.h>
#include <hb-ft.h>

#include <vector>
#include <string>
#include <string_view>
#include <unordered_map>
#include <memory>
#include <cstdint>
#include <cmath>
#include "../singleton/Logger.hpp"
#include "../common/ErrorCodes.hpp"
#include "../common/Result.hpp"

namespace ui::managers
{

/**
 * @brief 字形信息结构（FreeType 版本）
 */
struct GlyphInfo
{
    int width = 0;               // 位图宽度
    int height = 0;              // 位图高度
    int bearingX = 0;            // 水平偏移（左侧基准）
    int bearingY = 0;            // 垂直偏移（基线到字形顶部）
    float advanceX = 0.0F;       // 水平前进量（像素）
    std::vector<uint8_t> bitmap; // 灰度位图数据
};

/**
 * @brief HarfBuzz 成形后的字形位置信息
 */
struct ShapedGlyph
{
    uint32_t glyphId = 0;  // 字形索引
    float xOffset = 0.0F;  // X 偏移（像素）
    float yOffset = 0.0F;  // Y 偏移（像素）
    float xAdvance = 0.0F; // X 前进量（像素）
    float yAdvance = 0.0F; // Y 前进量（像素）
    uint32_t cluster = 0;  // 对应的字符簇索引
};

/**
 * @brief 字体管理器，封装 FreeType2 功能
 */
class FontManager
{
public:
    FontManager()
    {
        FT_Library lib = nullptr;
        FT_Error error = FT_Init_FreeType(&lib);
        if (error)
        {
            Logger::error("[FontManager] Failed to initialize FreeType: error {}", error);
        }
        else
        {
            m_ftLibrary.reset(lib);
            Logger::info("[FontManager] FreeType initialized successfully");
        }
    }

    ~FontManager() = default;

    // 禁止拷贝和移动
    FontManager(const FontManager&) = delete;
    FontManager& operator=(const FontManager&) = delete;
    FontManager(FontManager&&) = delete;
    FontManager& operator=(FontManager&&) = delete;

    /**
     * @brief 从内存加载字体
     * @param fontData 字体数据指针
     * @param dataSize 字体数据大小
     * @param fontSize 字体大小（像素）
     * @return 加载成功返回 true
     */
    Result<void> loadFromMemory(const uint8_t* fontData, size_t dataSize, float fontSize)
    {
        if (!m_ftLibrary)
        {
            Logger::error("[FontManager] FreeType not initialized");
            return MakeError(ui_errc::device_unavailable);
        }

        if (fontData == nullptr || dataSize == 0)
        {
            Logger::error("[FontManager] Invalid font data");
            return MakeError(ui_errc::invalid_argument);
        }

        // 复制字体数据（FreeType 需要持久内存）
        m_fontData.resize(dataSize);
        std::memcpy(m_fontData.data(), fontData, dataSize);

        FT_Face face = nullptr;
        FT_Error error =
            FT_New_Memory_Face(m_ftLibrary.get(), m_fontData.data(), static_cast<FT_Long>(dataSize), 0, &face);
        if (error)
        {
            Logger::error("[FontManager] Failed to load font face: error {}", error);
            return MakeError(ui_errc::asset_load_failed);
        }
        m_ftFace.reset(face);
        face = nullptr;

        // 设置像素大小
        error = FT_Set_Pixel_Sizes(m_ftFace.get(), 0, static_cast<FT_UInt>(fontSize));
        if (error)
        {
            m_ftFace.reset();
            Logger::error("[FontManager] Failed to set pixel size: error {}", error);
            return MakeError(ui_errc::asset_load_failed);
        }

        m_fontSize = fontSize;
        m_loaded = true;

        // 创建 HarfBuzz font
        createHarfBuzzFont();

        Logger::info("[FontManager] Font loaded: {} at {}px", m_ftFace->family_name, fontSize);
        return Ok();
    }

    /**
     * @brief 检查字体是否已加载
     */
    [[nodiscard]] bool isLoaded() const { return m_loaded && m_ftFace != nullptr; }

    /**
     * @brief 获取字体大小
     */
    [[nodiscard]] float getFontSize() const { return m_fontSize; }

    /**
     * @brief 获取字体高度（行高）- 像素
     */
    [[nodiscard]] int getFontHeight(float fontSize = 0.0F)
    {
        if (!m_ftFace) return 0;

        const float targetSize = (fontSize > 0.0F) ? fontSize : m_fontSize;
        const bool needRestore = (std::abs(targetSize - m_fontSize) > 0.1F);
        const float oldSize = m_fontSize;
        if (needRestore)
        {
            setPixelSize(targetSize);
        }

        const int fontHeight = static_cast<int>(m_ftFace->size->metrics.height >> 6);

        if (needRestore)
        {
            setPixelSize(oldSize);
        }

        return fontHeight;
    }

    /**
     * @brief 获取基线位置（ascender）- 像素
     */
    [[nodiscard]] int getBaseline(float fontSize = 0.0F)
    {
        if (!m_ftFace) return 0;

        const float targetSize = (fontSize > 0.0F) ? fontSize : m_fontSize;
        const bool needRestore = (std::abs(targetSize - m_fontSize) > 0.1F);
        const float oldSize = m_fontSize;
        if (needRestore)
        {
            setPixelSize(targetSize);
        }

        const int baseline = static_cast<int>(m_ftFace->size->metrics.ascender >> 6);

        if (needRestore)
        {
            setPixelSize(oldSize);
        }

        return baseline;
    }

    /**
     * @brief 测量 UTF-8 文本的宽度
     * @param text UTF-8 编码的文本
     * @param maxWidth 最大宽度（像素），超过此宽度则停止测量
     * @param outMeasuredLength 输出实际测量的字节长度
     * @return 文本的像素宽度
     */
    int measureString(const char* text, size_t textLen, int maxWidth, size_t* outMeasuredLength, float fontSize = 0.0F)
    {
        if (!m_ftFace || text == nullptr || textLen == 0)
        {
            if (outMeasuredLength) *outMeasuredLength = 0;
            return 0;
        }

        const float targetSize = (fontSize > 0.0F) ? fontSize : m_fontSize;
        const bool needRestore = (std::abs(targetSize - m_fontSize) > 0.1F);
        const float oldSize = m_fontSize;
        if (needRestore)
        {
            setPixelSize(targetSize);
        }

        // 使用 HarfBuzz 进行文本成形（包含正确的 kerning、连字等处理）
        auto shapedGlyphs = shapeText(text, textLen);
        if (shapedGlyphs.empty())
        {
            if (needRestore)
            {
                setPixelSize(oldSize);
            }
            if (outMeasuredLength) *outMeasuredLength = 0;
            return 0;
        }

        float totalWidth = 0.0F;
        size_t measuredEnd = textLen; // 默认：测量到文本末尾

        for (size_t idx = 0; idx < shapedGlyphs.size(); ++idx)
        {
            float advance = shapedGlyphs[idx].xAdvance;

            if (maxWidth > 0 && totalWidth + advance > static_cast<float>(maxWidth))
            {
                // 超出最大宽度，停在当前字形的 cluster 起始处（字节偏移）
                measuredEnd = shapedGlyphs[idx].cluster;
                break;
            }

            totalWidth += advance;
        }

        if (outMeasuredLength)
        {
            *outMeasuredLength = measuredEnd;
        }

        if (needRestore)
        {
            setPixelSize(oldSize);
        }

        return static_cast<int>(std::ceil(totalWidth));
    }

    /**
     * @brief 测量文本宽度（简化版本）
     */
    int measureTextWidth(const std::string& text, float fontSize = 0.0F)
    {
        size_t measuredLen = 0;
        return measureString(text.c_str(), text.size(), 0, &measuredLen, fontSize);
    }

    /**
     * @brief 渲染单个字形到灰度位图
     * @param codepoint Unicode 码点
     * @param fontSize 字体大小（像素），0 表示使用默认大小
     * @return 字形信息
     */
    GlyphInfo renderGlyph(int codepoint, float fontSize = 0.0F)
    {
        GlyphInfo info;

        if (!m_ftFace) return info;

        // 使用指定字体大小或默认大小
        float targetSize = (fontSize > 0.0F) ? fontSize : m_fontSize;

        // 检查缓存（包含字体大小）
        uint64_t cacheKey = makeGlyphCacheKey(codepoint, targetSize);
        auto it = m_glyphCache.find(cacheKey);
        if (it != m_glyphCache.end())
        {
            return it->second;
        }

        // 临时设置字体大小
        bool needRestore = (std::abs(targetSize - m_fontSize) > 0.1F);
        float oldSize = m_fontSize;
        if (needRestore)
        {
            setPixelSize(targetSize);
        }

        // 加载字形
        FT_UInt glyphIndex = FT_Get_Char_Index(m_ftFace.get(), static_cast<FT_ULong>(codepoint));
        FT_Error error = FT_Load_Glyph(m_ftFace.get(), glyphIndex, FT_LOAD_DEFAULT | FT_LOAD_TARGET_NORMAL);
        if (error)
        {
            Logger::warn("[FontManager] Failed to load glyph for codepoint {}: error {}", codepoint, error);
            return info;
        }

        // 渲染为灰度位图
        error = FT_Render_Glyph(m_ftFace->glyph, FT_RENDER_MODE_NORMAL);
        if (error)
        {
            Logger::warn("[FontManager] Failed to render glyph for codepoint {}: error {}", codepoint, error);
            return info;
        }

        FT_GlyphSlot slot = m_ftFace->glyph;
        FT_Bitmap& bitmap = slot->bitmap;

        info.width = static_cast<int>(bitmap.width);
        info.height = static_cast<int>(bitmap.rows);
        info.bearingX = slot->bitmap_left;
        info.bearingY = slot->bitmap_top;
        info.advanceX = static_cast<float>(slot->advance.x >> 6);

        // 复制位图数据
        if (bitmap.buffer && bitmap.width > 0 && bitmap.rows > 0)
        {
            size_t dataSize = bitmap.width * bitmap.rows;
            info.bitmap.resize(dataSize);

            if (bitmap.pitch == static_cast<int>(bitmap.width))
            {
                // 连续内存，直接拷贝
                std::memcpy(info.bitmap.data(), bitmap.buffer, dataSize);
            }
            else
            {
                // 不连续，逐行拷贝
                for (unsigned int row = 0; row < bitmap.rows; ++row)
                {
                    std::memcpy(
                        info.bitmap.data() + row * bitmap.width, bitmap.buffer + row * bitmap.pitch, bitmap.width);
                }
            }
        }

        // 恢复原字体大小
        if (needRestore)
        {
            setPixelSize(oldSize);
        }

        // 缓存字形
        m_glyphCache[cacheKey] = info;

        return info;
    }

    /**
     * @brief 获取文本超采样缩放因子
     * @note 保留此接口以兼容旧的渲染管线
     */
    [[nodiscard]] float getOversampleScale() const { return TEXT_OVERSAMPLE_SCALE; }

    /**
     * @brief 渲染整个文本到 alpha mask 位图
     * @param text UTF-8 文本
     * @param alpha 整体 alpha (0-255)
     * @param outWidth 输出位图宽度
     * @param outHeight 输出位图高度
     * @param fontSize 字体大小（像素），0 表示使用默认大小
     * @return 单通道（R8）位图数据，每像素 1 字节，值为字形 coverage
     */
    std::vector<uint8_t> renderTextAlphaMask(
        const std::string& text, uint8_t alpha, int& outWidth, int& outHeight, float fontSize = 0.0F)
    {
        std::vector<uint8_t> result;

        static_cast<void>(alpha);

        if (!m_loaded || text.empty())
        {
            outWidth = outHeight = 0;
            return result;
        }

        const float logicalSize = (fontSize > 0.0F) ? fontSize : m_fontSize;
        const float rasterSize = logicalSize * TEXT_OVERSAMPLE_SCALE;
        bool needRestore = (std::abs(rasterSize - m_fontSize) > 0.1F);
        float oldSize = m_fontSize;
        if (needRestore)
        {
            setPixelSize(rasterSize);
        }

        struct GlyphLayout
        {
            GlyphInfo glyph;
            float xPos = 0.0F;
            float yOffset = 0.0F;
        };
        std::vector<GlyphLayout> layouts;
        float cursorX = 0.0F;

        auto shapedGlyphs = shapeText(text.c_str(), text.size());
        for (const auto& shaped : shapedGlyphs)
        {
            GlyphInfo glyph = renderGlyphByIndex(shaped.glyphId, rasterSize);
            layouts.push_back({glyph, cursorX + shaped.xOffset, shaped.yOffset});
            cursorX += shaped.xAdvance;
        }

        if (layouts.empty())
        {
            if (needRestore) setPixelSize(oldSize);
            outWidth = outHeight = 0;
            return result;
        }

        int baseline = getBaseline();
        int fontHeight = getFontHeight();
        outWidth = static_cast<int>(std::ceil(cursorX));
        outHeight = fontHeight;

        if (outWidth <= 0 || outHeight <= 0)
        {
            outWidth = outHeight = 0;
            if (needRestore) setPixelSize(oldSize);
            return result;
        }

        result.resize(static_cast<size_t>(outWidth) * static_cast<size_t>(outHeight), 0);

        for (const auto& layout : layouts)
        {
            const auto& glyph = layout.glyph;
            int xPos = static_cast<int>(std::floor(layout.xPos)) + glyph.bearingX;
            int yPos = baseline - glyph.bearingY - static_cast<int>(std::round(layout.yOffset));

            for (int yOff = 0; yOff < glyph.height; ++yOff)
            {
                for (int xOff = 0; xOff < glyph.width; ++xOff)
                {
                    int bx = xPos + xOff;
                    int by = yPos + yOff;
                    if (bx < 0 || bx >= outWidth || by < 0 || by >= outHeight) continue;

                    int pixelIndex = by * outWidth + bx;
                    uint8_t finalAlpha = glyph.bitmap[(yOff * glyph.width) + xOff];

                    if (finalAlpha <= result[pixelIndex])
                    {
                        continue;
                    }

                    result[pixelIndex] = finalAlpha;
                }
            }
        }

        if (needRestore)
        {
            setPixelSize(oldSize);
        }

        return result;
    }

    /**
     * @brief 渲染整个文本到 RGBA 位图（兼容旧接口）
     * @param text UTF-8 文本
     * @param red R 通道 (0-255)
     * @param green G 通道 (0-255)
     * @param blue B 通道 (0-255)
     * @param alpha A 通道 (0-255)
     * @param outWidth 输出位图宽度
     * @param outHeight 输出位图高度
     * @param fontSize 字体大小（像素），0 表示使用默认大小
     * @return RGBA 位图数据
     */
    std::vector<uint8_t> renderTextBitmap(const std::string& text,
                                          uint8_t red,
                                          uint8_t green,
                                          uint8_t blue,
                                          uint8_t alpha,
                                          int& outWidth,
                                          int& outHeight,
                                          float fontSize = 0.0F)
    {
        std::vector<uint8_t> result;

        if (!m_loaded || text.empty())
        {
            outWidth = outHeight = 0;
            return result;
        }

        // 使用指定字体大小或默认大小
        float targetSize = (fontSize > 0.0F) ? fontSize : m_fontSize;
        bool needRestore = (std::abs(targetSize - m_fontSize) > 0.1F);
        float oldSize = m_fontSize;
        if (needRestore)
        {
            setPixelSize(targetSize);
        }

        // 使用 HarfBuzz 进行文本成形，获取正确的字形 ID、位置和前进量
        struct GlyphLayout
        {
            GlyphInfo glyph;
            float xPos = 0.0F;
            float yOffset = 0.0F;
        };
        std::vector<GlyphLayout> layouts;
        float cursorX = 0.0F;

        auto shapedGlyphs = shapeText(text.c_str(), text.size());
        for (const auto& shaped : shapedGlyphs)
        {
            GlyphInfo glyph = renderGlyphByIndex(shaped.glyphId, targetSize);
            layouts.push_back({glyph, cursorX + shaped.xOffset, shaped.yOffset});
            cursorX += shaped.xAdvance;
        }

        if (layouts.empty())
        {
            if (needRestore) setPixelSize(oldSize);
            outWidth = outHeight = 0;
            return result;
        }

        int baseline = getBaseline();
        int fontHeight = getFontHeight();
        outWidth = static_cast<int>(std::ceil(cursorX));
        outHeight = fontHeight;

        if (outWidth <= 0 || outHeight <= 0)
        {
            outWidth = outHeight = 0;
            return result;
        }

        // 创建 RGBA 位图（初始化为透明）
        result.resize(static_cast<size_t>(outWidth) * static_cast<size_t>(outHeight) * 4, 0);

        // 混合字形
        for (const auto& layout : layouts)
        {
            const auto& glyph = layout.glyph;
            int xPos = static_cast<int>(std::floor(layout.xPos)) + glyph.bearingX;
            int yPos = baseline - glyph.bearingY - static_cast<int>(std::round(layout.yOffset));

            for (int yOff = 0; yOff < glyph.height; ++yOff)
            {
                for (int xOff = 0; xOff < glyph.width; ++xOff)
                {
                    int bx = xPos + xOff;
                    int by = yPos + yOff;
                    if (bx < 0 || bx >= outWidth || by < 0 || by >= outHeight) continue;

                    int pixelIndex = (by * outWidth + bx) * 4;
                    uint8_t srcAlpha = glyph.bitmap[(yOff * glyph.width) + xOff];
                    auto finalAlpha = static_cast<uint8_t>((srcAlpha * alpha + 127) / 255);

                    if (finalAlpha == 0) continue;

                    // 预乘 Alpha Over 合成（适用于重叠字形）
                    // C_out = C_src + C_dst × (1 - A_src)
                    uint8_t dstA = result[pixelIndex + 3];
                    auto srcR = static_cast<uint8_t>((red * finalAlpha + 127) / 255);
                    auto srcG = static_cast<uint8_t>((green * finalAlpha + 127) / 255);
                    auto srcB = static_cast<uint8_t>((blue * finalAlpha + 127) / 255);

                    if (dstA == 0)
                    {
                        // 目标像素为空，直接写入
                        result[pixelIndex + 0] = srcR;
                        result[pixelIndex + 1] = srcG;
                        result[pixelIndex + 2] = srcB;
                        result[pixelIndex + 3] = finalAlpha;
                    }
                    else
                    {
                        // 预乘 Alpha Over 合成
                        float srcAf = static_cast<float>(finalAlpha) / 255.0F;
                        float oneMinusSrcA = 1.0F - srcAf;
                        result[pixelIndex + 0] = static_cast<uint8_t>(std::lround(std::min(
                            255.0F,
                            static_cast<float>(srcR) + (static_cast<float>(result[pixelIndex + 0]) * oneMinusSrcA))));
                        result[pixelIndex + 1] = static_cast<uint8_t>(std::lround(std::min(
                            255.0F,
                            static_cast<float>(srcG) + (static_cast<float>(result[pixelIndex + 1]) * oneMinusSrcA))));
                        result[pixelIndex + 2] = static_cast<uint8_t>(std::lround(std::min(
                            255.0F,
                            static_cast<float>(srcB) + (static_cast<float>(result[pixelIndex + 2]) * oneMinusSrcA))));
                        result[pixelIndex + 3] = static_cast<uint8_t>(std::lround(std::min(
                            255.0F, static_cast<float>(finalAlpha) + (static_cast<float>(dstA) * oneMinusSrcA))));
                    }
                }
            }
        }

        // 恢复原字体大小
        if (needRestore)
        {
            setPixelSize(oldSize);
        }

        return result;
    }

    /**
     * @brief 使用 HarfBuzz 对文本进行成形（shaping）
     * @param text UTF-8 文本
     * @param textLen 文本长度
     * @return 成形后的字形位置数组
     */
    std::vector<ShapedGlyph> shapeText(const char* text, size_t textLen)
    {
        std::vector<ShapedGlyph> result;

        if (!m_hbFont || !text || textLen == 0)
        {
            return result;
        }

        // 创建 HarfBuzz buffer
        hb_buffer_t* buf = hb_buffer_create();
        hb_buffer_add_utf8(buf, text, static_cast<int>(textLen), 0, static_cast<int>(textLen));
        // 强制 LTR 方向（CJK + 拉丁均为 LTR），自动检测脚本和语言
        hb_buffer_set_direction(buf, HB_DIRECTION_LTR);
        hb_buffer_guess_segment_properties(buf);

        // 执行成形
        hb_shape(m_hbFont.get(), buf, nullptr, 0);

        // 获取结果
        unsigned int glyphCount = 0;
        hb_glyph_info_t* glyphInfos = hb_buffer_get_glyph_infos(buf, &glyphCount);
        hb_glyph_position_t* glyphPositions = hb_buffer_get_glyph_positions(buf, &glyphCount);

        result.reserve(glyphCount);
        for (unsigned int i = 0; i < glyphCount; ++i)
        {
            ShapedGlyph glyph;
            glyph.glyphId = glyphInfos[i].codepoint;
            glyph.cluster = glyphInfos[i].cluster;
            glyph.xOffset = static_cast<float>(glyphPositions[i].x_offset) / 64.0F;
            glyph.yOffset = static_cast<float>(glyphPositions[i].y_offset) / 64.0F;
            glyph.xAdvance = static_cast<float>(glyphPositions[i].x_advance) / 64.0F;
            glyph.yAdvance = static_cast<float>(glyphPositions[i].y_advance) / 64.0F;
            result.push_back(glyph);
        }

        hb_buffer_destroy(buf);
        return result;
    }

    /**
     * @brief 根据字形索引渲染字形
     * @param glyphId 字形索引（由 HarfBuzz 提供）
     * @param fontSize 字体大小（像素），0 表示使用默认大小
     * @return 字形信息
     */
    GlyphInfo renderGlyphByIndex(uint32_t glyphId, float fontSize = 0.0F)
    {
        GlyphInfo info;

        if (!m_ftFace) return info;

        // 使用指定字体大小或默认大小
        float targetSize = (fontSize > 0.0F) ? fontSize : m_fontSize;

        // 检查缓存（高位标记 glyphId 键，避免与 codepoint 键冲突）
        uint64_t cacheKey = makeGlyphCacheKey(static_cast<int>(glyphId), targetSize) | (1ULL << 63);
        auto it = m_glyphCache.find(cacheKey);
        if (it != m_glyphCache.end())
        {
            return it->second;
        }

        // 临时设置字体大小
        bool needRestore = (std::abs(targetSize - m_fontSize) > 0.1F);
        float oldSize = m_fontSize;
        if (needRestore)
        {
            setPixelSize(targetSize);
        }

        // 加载字形
        FT_Error error = FT_Load_Glyph(m_ftFace.get(), glyphId, FT_LOAD_DEFAULT | FT_LOAD_TARGET_NORMAL);
        if (error)
        {
            Logger::warn("[FontManager] Failed to load glyph index {}: error {}", glyphId, error);
            return info;
        }

        // 渲染为灰度位图
        error = FT_Render_Glyph(m_ftFace->glyph, FT_RENDER_MODE_NORMAL);
        if (error)
        {
            Logger::warn("[FontManager] Failed to render glyph index {}: error {}", glyphId, error);
            return info;
        }

        FT_GlyphSlot slot = m_ftFace->glyph;
        FT_Bitmap& bitmap = slot->bitmap;

        info.width = static_cast<int>(bitmap.width);
        info.height = static_cast<int>(bitmap.rows);
        info.bearingX = slot->bitmap_left;
        info.bearingY = slot->bitmap_top;
        info.advanceX = static_cast<float>(slot->advance.x >> 6);

        // 复制位图数据
        if (bitmap.buffer && bitmap.width > 0 && bitmap.rows > 0)
        {
            size_t dataSize = bitmap.width * bitmap.rows;
            info.bitmap.resize(dataSize);

            if (bitmap.pitch == static_cast<int>(bitmap.width))
            {
                std::memcpy(info.bitmap.data(), bitmap.buffer, dataSize);
            }
            else
            {
                for (unsigned int row = 0; row < bitmap.rows; ++row)
                {
                    std::memcpy(
                        info.bitmap.data() + row * bitmap.width, bitmap.buffer + row * bitmap.pitch, bitmap.width);
                }
            }
        }

        // 恢复原字体大小
        if (needRestore)
        {
            setPixelSize(oldSize);
        }

        // 缓存字形
        m_glyphCache[cacheKey] = info;

        return info;
    }

    /**
     * @brief 清空字形缓存
     */
    void clearCache()
    {
        m_glyphCache.clear();
        Logger::info("[FontManager] Glyph cache cleared");
    }

    /**
     * @brief 解码 UTF-8 字符
     * @param text UTF-8 字符串视图
     * @param outCodepoint 输出码点
     * @return 字符占用的字节数，失败返回 0
     */
    static size_t decodeUTF8(std::string_view text, int& outCodepoint)
    {
        if (text.empty()) return 0;

        const auto byte0 = static_cast<uint8_t>(text[0]);

        // 单字节 ASCII
        if (byte0 < 0x80)
        {
            outCodepoint = byte0;
            return 1;
        }

        // 2 字节
        if ((byte0 & 0xE0U) == 0xC0)
        {
            if (text.size() < 2) return 0;
            outCodepoint = static_cast<int>(((byte0 & 0x1FU) << 6U) | (static_cast<uint8_t>(text[1]) & 0x3FU));
            return 2;
        }

        // 3 字节
        if ((byte0 & 0xF0U) == 0xE0)
        {
            if (text.size() < 3) return 0;
            outCodepoint = static_cast<int>(((byte0 & 0x0FU) << 12U) | ((static_cast<uint8_t>(text[1]) & 0x3FU) << 6U)
                                            | (static_cast<uint8_t>(text[2]) & 0x3FU));
            return 3;
        }

        // 4 字节
        if ((byte0 & 0xF8U) == 0xF0)
        {
            if (text.size() < 4) return 0;
            outCodepoint = static_cast<int>(((byte0 & 0x07U) << 18U) | ((static_cast<uint8_t>(text[1]) & 0x3FU) << 12U)
                                            | ((static_cast<uint8_t>(text[2]) & 0x3FU) << 6U)
                                            | (static_cast<uint8_t>(text[3]) & 0x3FU));
            return 4;
        }

        return 0;
    }

private:
    static constexpr float TEXT_OVERSAMPLE_SCALE = 2.0F;

    /**
     * @brief 创建 HarfBuzz font 对象
     */
    void createHarfBuzzFont()
    {
        m_hbFont.reset(); // 释放旧实例（如有）

        if (!m_ftFace)
        {
            Logger::warn("[FontManager] Cannot create HarfBuzz font: FreeType face not loaded");
            return;
        }

        // 使用 hb-ft 桥接创建 HarfBuzz font
        m_hbFont.reset(hb_ft_font_create(m_ftFace.get(), nullptr));
        if (!m_hbFont)
        {
            Logger::error("[FontManager] Failed to create HarfBuzz font");
            return;
        }

        Logger::info("[FontManager] HarfBuzz font created successfully");
    }

    /**
     * @brief 设置字体像素大小
     */
    void setPixelSize(float size)
    {
        if (!m_ftFace || size <= 0.0F) return;
        FT_Error error = FT_Set_Pixel_Sizes(m_ftFace.get(), 0, static_cast<FT_UInt>(size));
        if (error == 0)
        {
            m_fontSize = size;
            // 更新 HarfBuzz font 的缩放
            if (m_hbFont)
            {
                hb_ft_font_changed(m_hbFont.get());
            }
        }
    }

    /**
     * @brief 生成字形缓存键（包含字体大小）
     */
    static uint64_t makeGlyphCacheKey(int codepoint, float fontSize)
    {
        // 将字体大小转换为整数（精度 0.1px）
        uint32_t sizeKey = static_cast<uint32_t>(fontSize * 10.0F);
        return (static_cast<uint64_t>(sizeKey) << 32) | static_cast<uint32_t>(codepoint);
    }

    bool m_loaded = false;
    float m_fontSize = 16.0F;

    std::vector<uint8_t> m_fontData;

    // RAII 包装器：负责在析构时自动释放 FreeType/HarfBuzz 资源
    struct FtLibraryDeleter
    {
        void operator()(std::remove_pointer_t<FT_Library>* p) const noexcept { FT_Done_FreeType(p); }
    };
    struct FtFaceDeleter
    {
        void operator()(std::remove_pointer_t<FT_Face>* p) const noexcept { FT_Done_Face(p); }
    };
    struct HbFontDeleter
    {
        void operator()(hb_font_t* p) const noexcept { hb_font_destroy(p); }
    };

    // 成员销毁顺序为声明逆序：m_hbFont → m_ftFace → m_ftLibrary（正确清理顺序）
    std::unique_ptr<std::remove_pointer_t<FT_Library>, FtLibraryDeleter> m_ftLibrary;
    std::unique_ptr<std::remove_pointer_t<FT_Face>, FtFaceDeleter> m_ftFace;
    std::unique_ptr<hb_font_t, HbFontDeleter> m_hbFont;

    // 字形缓存（key = (fontSize << 32) | codepoint）
    std::unordered_map<uint64_t, GlyphInfo> m_glyphCache;
};

} // namespace ui::managers
