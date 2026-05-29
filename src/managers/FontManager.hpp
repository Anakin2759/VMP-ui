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

#include <freetype/freetype.h>
#include <freetype/ftimage.h>
#include <freetype/fttypes.h>

#include <hb.h>
#include <hb-ft.h>

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <cstdlib>
#include <span>
#include <vector>
#include <string>
#include <string_view>
#include <unordered_map>
#include <memory>
#include <cstdint>
#include <cmath>
#include "singleton/Logger.hpp"
#include "common/ErrorCodes.hpp"
#include "common/Result.hpp"

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

struct GlyphLayout
{
    GlyphInfo glyph;
    float xPos = 0.0F;
    float yOffset = 0.0F;
};

struct TextBlendState
{
    int baseline = 0;
    int outWidth = 0;
    int outHeight = 0;
    uint8_t red = 0;
    uint8_t green = 0;
    uint8_t blue = 0;
    uint8_t alpha = 0;
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
        if (error != 0)
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
        if (m_ftLibrary == nullptr)
        {
            Logger::error("[FontManager] FreeType not initialized");
            return MakeError(UiErrc::DEVICE_UNAVAILABLE);
        }

        if (fontData == nullptr || dataSize == 0)
        {
            Logger::error("[FontManager] Invalid font data");
            return MakeError(UiErrc::INVALID_ARGUMENT);
        }

        // 复制字体数据（FreeType 需要持久内存）
        m_fontData.resize(dataSize);
        std::memcpy(m_fontData.data(), fontData, dataSize);

        FT_Face face = nullptr;
        FT_Error error =
            FT_New_Memory_Face(m_ftLibrary.get(), m_fontData.data(), static_cast<FT_Long>(dataSize), 0, &face);
        if (error != 0)
        {
            Logger::error("[FontManager] Failed to load font face: error {}", error);
            return MakeError(UiErrc::ASSET_LOAD_FAILED);
        }
        m_ftFace.reset(face);
        face = nullptr;

        // 设置像素大小
        error = FT_Set_Pixel_Sizes(m_ftFace.get(), 0, static_cast<FT_UInt>(fontSize));
        if (error != 0)
        {
            m_ftFace.reset();
            Logger::error("[FontManager] Failed to set pixel size: error {}", error);
            return MakeError(UiErrc::ASSET_LOAD_FAILED);
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
        if (m_ftFace == nullptr) return 0;

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
        if (m_ftFace == nullptr) return 0;

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
        if (m_ftFace == nullptr || text == nullptr || textLen == 0)
        {
            if (outMeasuredLength != nullptr) *outMeasuredLength = 0;
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
            if (outMeasuredLength != nullptr) *outMeasuredLength = 0;
            return 0;
        }

        float totalWidth = 0.0F;
        size_t measuredEnd = textLen; // 默认：测量到文本末尾

        for (const auto& shapedGlyph : shapedGlyphs)
        {
            const float advance = shapedGlyph.xAdvance;

            if (maxWidth > 0 && totalWidth + advance > static_cast<float>(maxWidth))
            {
                // 超出最大宽度，停在当前字形的 cluster 起始处（字节偏移）
                measuredEnd = shapedGlyph.cluster;
                break;
            }

            totalWidth += advance;
        }

        if (outMeasuredLength != nullptr)
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

        if (m_ftFace == nullptr) return info;

        // 使用指定字体大小或默认大小
        const float targetSize = (fontSize > 0.0F) ? fontSize : m_fontSize;

        // 检查缓存（包含字体大小）
        const uint64_t cacheKey = makeGlyphCacheKey(codepoint, targetSize);
        auto glyphIterator = m_glyphCache.find(cacheKey);
        if (glyphIterator != m_glyphCache.end())
        {
            return glyphIterator->second;
        }

        // 临时设置字体大小
        const FontSizeGuard sizeGuard(*this, targetSize);

        // 加载字形
        const FT_UInt glyphIndex = FT_Get_Char_Index(m_ftFace.get(), static_cast<FT_ULong>(codepoint));
        FT_Error error = FT_Load_Glyph(m_ftFace.get(), glyphIndex, FT_LOAD_DEFAULT | FT_LOAD_TARGET_NORMAL);
        if (error != 0)
        {
            Logger::warn("[FontManager] Failed to load glyph for codepoint {}: error {}", codepoint, error);
            return info;
        }

        // 渲染为灰度位图
        error = FT_Render_Glyph(m_ftFace->glyph, FT_RENDER_MODE_NORMAL);
        if (error != 0)
        {
            Logger::warn("[FontManager] Failed to render glyph for codepoint {}: error {}", codepoint, error);
            return info;
        }

        FT_GlyphSlot slot = m_ftFace->glyph;
        const FT_Bitmap& bitmap = slot->bitmap;

        info.width = static_cast<int>(bitmap.width);
        info.height = static_cast<int>(bitmap.rows);
        info.bearingX = slot->bitmap_left;
        info.bearingY = slot->bitmap_top;
        info.advanceX = static_cast<float>(slot->advance.x >> 6);

        info.bitmap = copyBitmapRows(bitmap);

        // 缓存字形
        m_glyphCache.insert_or_assign(cacheKey, info);

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
        const FontSizeGuard sizeGuard(*this, rasterSize);

        float cursorX = 0.0F;
        auto layouts = buildGlyphLayouts(text, rasterSize, cursorX);

        if (layouts.empty())
        {
            outWidth = outHeight = 0;
            return result;
        }

        const int baseline = getBaseline();
        const int fontHeight = getFontHeight();
        outWidth = static_cast<int>(std::ceil(cursorX));
        outHeight = fontHeight;

        if (outWidth <= 0 || outHeight <= 0)
        {
            outWidth = outHeight = 0;
            return result;
        }

        result.resize(static_cast<size_t>(outWidth) * static_cast<size_t>(outHeight), 0);
        blendAlphaMask(layouts, baseline, outWidth, outHeight, result);

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
        const float targetSize = (fontSize > 0.0F) ? fontSize : m_fontSize;
        const FontSizeGuard sizeGuard(*this, targetSize);

        float cursorX = 0.0F;
        auto layouts = buildGlyphLayouts(text, targetSize, cursorX);

        if (layouts.empty())
        {
            outWidth = outHeight = 0;
            return result;
        }

        const int baseline = getBaseline();
        const int fontHeight = getFontHeight();
        outWidth = static_cast<int>(std::ceil(cursorX));
        outHeight = fontHeight;

        if (outWidth <= 0 || outHeight <= 0)
        {
            outWidth = outHeight = 0;
            return result;
        }

        // 创建 RGBA 位图（初始化为透明）
        result.resize(static_cast<size_t>(outWidth) * static_cast<size_t>(outHeight) * 4, 0);
        blendTextBitmap(layouts,
                        {.baseline = baseline,
                         .outWidth = outWidth,
                         .outHeight = outHeight,
                         .red = red,
                         .green = green,
                         .blue = blue,
                         .alpha = alpha},
                        result);

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

        if (m_hbFont == nullptr || text == nullptr || textLen == 0)
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
        const std::span<const hb_glyph_info_t> glyphInfoSpan(glyphInfos, glyphCount);
        const std::span<const hb_glyph_position_t> glyphPositionSpan(glyphPositions, glyphCount);

        result.reserve(glyphCount);
        for (size_t glyphIndex = 0; glyphIndex < glyphInfoSpan.size(); ++glyphIndex)
        {
            const auto& glyphInfo = glyphInfoSpan.subspan(glyphIndex, 1U).front();
            const auto& glyphPosition = glyphPositionSpan.subspan(glyphIndex, 1U).front();
            ShapedGlyph glyph;
            glyph.glyphId = glyphInfo.codepoint;
            glyph.cluster = glyphInfo.cluster;
            glyph.xOffset = static_cast<float>(glyphPosition.x_offset) / 64.0F;
            glyph.yOffset = static_cast<float>(glyphPosition.y_offset) / 64.0F;
            glyph.xAdvance = static_cast<float>(glyphPosition.x_advance) / 64.0F;
            glyph.yAdvance = static_cast<float>(glyphPosition.y_advance) / 64.0F;
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

        if (m_ftFace == nullptr) return info;

        // 使用指定字体大小或默认大小
        const float targetSize = (fontSize > 0.0F) ? fontSize : m_fontSize;

        // 检查缓存（高位标记 glyphId 键，避免与 codepoint 键冲突）
        const uint64_t cacheKey = makeGlyphCacheKey(static_cast<int>(glyphId), targetSize) | (1ULL << 63);
        auto glyphIterator = m_glyphCache.find(cacheKey);
        if (glyphIterator != m_glyphCache.end())
        {
            return glyphIterator->second;
        }

        // 临时设置字体大小
        const FontSizeGuard sizeGuard(*this, targetSize);

        // 加载字形
        FT_Error error = FT_Load_Glyph(m_ftFace.get(), glyphId, FT_LOAD_DEFAULT | FT_LOAD_TARGET_NORMAL);
        if (error != 0)
        {
            Logger::warn("[FontManager] Failed to load glyph index {}: error {}", glyphId, error);
            return info;
        }

        // 渲染为灰度位图
        error = FT_Render_Glyph(m_ftFace->glyph, FT_RENDER_MODE_NORMAL);
        if (error != 0)
        {
            Logger::warn("[FontManager] Failed to render glyph index {}: error {}", glyphId, error);
            return info;
        }

        FT_GlyphSlot slot = m_ftFace->glyph;
        const FT_Bitmap& bitmap = slot->bitmap;

        info.width = static_cast<int>(bitmap.width);
        info.height = static_cast<int>(bitmap.rows);
        info.bearingX = slot->bitmap_left;
        info.bearingY = slot->bitmap_top;
        info.advanceX = static_cast<float>(slot->advance.x >> 6);

        info.bitmap = copyBitmapRows(bitmap);

        // 缓存字形
        m_glyphCache.insert_or_assign(cacheKey, info);

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

        if (m_ftFace == nullptr)
        {
            Logger::warn("[FontManager] Cannot create HarfBuzz font: FreeType face not loaded");
            return;
        }

        // 使用 hb-ft 桥接创建 HarfBuzz font
        m_hbFont.reset(hb_ft_font_create(m_ftFace.get(), nullptr));
        if (m_hbFont == nullptr)
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
        if (m_ftFace == nullptr || size <= 0.0F) return;
        const FT_Error error = FT_Set_Pixel_Sizes(m_ftFace.get(), 0, static_cast<FT_UInt>(size));
        if (error == 0)
        {
            m_fontSize = size;
            // 更新 HarfBuzz font 的缩放
            if (m_hbFont != nullptr)
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
        auto sizeKey = static_cast<uint32_t>(fontSize * 10.0F);
        return (static_cast<uint64_t>(sizeKey) << 32) | static_cast<uint32_t>(codepoint);
    }

    class FontSizeGuard
    {
    public:
        FontSizeGuard(FontManager& manager, float targetSize)
            : m_manager(manager), m_oldSize(manager.m_fontSize),
              m_needRestore(std::abs(targetSize - manager.m_fontSize) > 0.1F)
        {
            if (m_needRestore)
            {
                m_manager.setPixelSize(targetSize);
            }
        }

        ~FontSizeGuard()
        {
            if (m_needRestore)
            {
                m_manager.setPixelSize(m_oldSize);
            }
        }

        FontSizeGuard(const FontSizeGuard&) = delete;
        FontSizeGuard& operator=(const FontSizeGuard&) = delete;
        FontSizeGuard(FontSizeGuard&&) = delete;
        FontSizeGuard& operator=(FontSizeGuard&&) = delete;

    private:
        FontManager& m_manager;
        float m_oldSize;
        bool m_needRestore;
    };

    static std::vector<uint8_t> copyBitmapRows(const FT_Bitmap& bitmap)
    {
        if (bitmap.buffer == nullptr || bitmap.width == 0 || bitmap.rows == 0)
        {
            return {};
        }

        const size_t width = bitmap.width;
        const size_t rows = bitmap.rows;
        const auto pitch = static_cast<size_t>(std::abs(bitmap.pitch));
        const size_t dataSize = width * rows;
        std::vector<uint8_t> bitmapCopy(dataSize);

        const std::span<const uint8_t> source(bitmap.buffer, pitch * rows);
        for (size_t row = 0; row < rows; ++row)
        {
            const auto sourceRow = source.subspan(row * pitch, width);
            auto destinationRow = std::span<uint8_t>(bitmapCopy).subspan(row * width, width);
            std::ranges::copy(sourceRow, destinationRow.begin());
        }
        return bitmapCopy;
    }

    std::vector<GlyphLayout> buildGlyphLayouts(const std::string& text, float fontSize, float& cursorX)
    {
        std::vector<GlyphLayout> layouts;
        auto shapedGlyphs = shapeText(text.c_str(), text.size());
        layouts.reserve(shapedGlyphs.size());
        for (const auto& shaped : shapedGlyphs)
        {
            layouts.push_back({.glyph = renderGlyphByIndex(shaped.glyphId, fontSize),
                               .xPos = cursorX + shaped.xOffset,
                               .yOffset = shaped.yOffset});
            cursorX += shaped.xAdvance;
        }
        return layouts;
    }

    static bool isInsideBitmap(int xPos, int yPos, int outWidth, int outHeight)
    {
        return xPos >= 0 && xPos < outWidth && yPos >= 0 && yPos < outHeight;
    }

    static uint8_t glyphAlphaAt(const GlyphInfo& glyph, int xOffset, int yOffset)
    {
        const auto pixelIndex =
            (static_cast<size_t>(yOffset) * static_cast<size_t>(glyph.width)) + static_cast<size_t>(xOffset);
        return glyph.bitmap.at(pixelIndex);
    }

    static void blendAlphaMask(const std::vector<GlyphLayout>& layouts,
                               int baseline,
                               int outWidth,
                               int outHeight,
                               std::vector<uint8_t>& result)
    {
        for (const auto& layout : layouts)
        {
            blendAlphaGlyph(layout, baseline, outWidth, outHeight, result);
        }
    }

    static void blendAlphaGlyph(
        const GlyphLayout& layout, int baseline, int outWidth, int outHeight, std::vector<uint8_t>& result)
    {
        const auto& glyph = layout.glyph;
        const int xPos = static_cast<int>(std::floor(layout.xPos)) + glyph.bearingX;
        const int yPos = baseline - glyph.bearingY - static_cast<int>(std::round(layout.yOffset));

        for (int yOffset = 0; yOffset < glyph.height; ++yOffset)
        {
            for (int xOffset = 0; xOffset < glyph.width; ++xOffset)
            {
                const int bitmapX = xPos + xOffset;
                const int bitmapY = yPos + yOffset;
                if (!isInsideBitmap(bitmapX, bitmapY, outWidth, outHeight)) continue;

                const auto pixelIndex =
                    (static_cast<size_t>(bitmapY) * static_cast<size_t>(outWidth)) + static_cast<size_t>(bitmapX);
                const uint8_t finalAlpha = glyphAlphaAt(glyph, xOffset, yOffset);
                result.at(pixelIndex) = std::max(result.at(pixelIndex), finalAlpha);
            }
        }
    }

    static void blendTextBitmap(const std::vector<GlyphLayout>& layouts,
                                const TextBlendState& blendState,
                                std::vector<uint8_t>& result)
    {
        for (const auto& layout : layouts)
        {
            blendTextGlyph(layout, blendState, result);
        }
    }

    static uint8_t premultiplyColor(uint8_t color, uint8_t alpha)
    {
        return static_cast<uint8_t>(((static_cast<uint16_t>(color) * alpha) + 127U) / 255U);
    }

    static void writeSourcePixel(
        std::vector<uint8_t>& result, size_t pixelIndex, uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha)
    {
        result.at(pixelIndex) = red;
        result.at(pixelIndex + 1U) = green;
        result.at(pixelIndex + 2U) = blue;
        result.at(pixelIndex + 3U) = alpha;
    }

    static void blendSourceOver(
        std::vector<uint8_t>& result, size_t pixelIndex, uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha)
    {
        const float sourceAlpha = static_cast<float>(alpha) / 255.0F;
        const float oneMinusSourceAlpha = 1.0F - sourceAlpha;
        result.at(pixelIndex) = static_cast<uint8_t>(std::lround(std::min(
            255.0F, static_cast<float>(red) + (static_cast<float>(result.at(pixelIndex)) * oneMinusSourceAlpha))));
        result.at(pixelIndex + 1U) = static_cast<uint8_t>(std::lround(std::min(
            255.0F,
            static_cast<float>(green) + (static_cast<float>(result.at(pixelIndex + 1U)) * oneMinusSourceAlpha))));
        result.at(pixelIndex + 2U) = static_cast<uint8_t>(std::lround(std::min(
            255.0F,
            static_cast<float>(blue) + (static_cast<float>(result.at(pixelIndex + 2U)) * oneMinusSourceAlpha))));
        result.at(pixelIndex + 3U) = static_cast<uint8_t>(std::lround(std::min(
            255.0F,
            static_cast<float>(alpha) + (static_cast<float>(result.at(pixelIndex + 3U)) * oneMinusSourceAlpha))));
    }

    static void
        blendTextGlyph(const GlyphLayout& layout, const TextBlendState& blendState, std::vector<uint8_t>& result)
    {
        const auto& glyph = layout.glyph;
        const int xPos = static_cast<int>(std::floor(layout.xPos)) + glyph.bearingX;
        const int yPos = blendState.baseline - glyph.bearingY - static_cast<int>(std::round(layout.yOffset));

        for (int yOffset = 0; yOffset < glyph.height; ++yOffset)
        {
            for (int xOffset = 0; xOffset < glyph.width; ++xOffset)
            {
                const int bitmapX = xPos + xOffset;
                const int bitmapY = yPos + yOffset;
                if (!isInsideBitmap(bitmapX, bitmapY, blendState.outWidth, blendState.outHeight)) continue;

                const auto pixelIndex = ((static_cast<size_t>(bitmapY) * static_cast<size_t>(blendState.outWidth))
                                         + static_cast<size_t>(bitmapX))
                                      * 4U;
                const auto finalAlpha = premultiplyColor(glyphAlphaAt(glyph, xOffset, yOffset), blendState.alpha);
                if (finalAlpha == 0) continue;

                const auto sourceRed = premultiplyColor(blendState.red, finalAlpha);
                const auto sourceGreen = premultiplyColor(blendState.green, finalAlpha);
                const auto sourceBlue = premultiplyColor(blendState.blue, finalAlpha);
                if (result.at(pixelIndex + 3U) == 0)
                {
                    writeSourcePixel(result, pixelIndex, sourceRed, sourceGreen, sourceBlue, finalAlpha);
                }
                else
                {
                    blendSourceOver(result, pixelIndex, sourceRed, sourceGreen, sourceBlue, finalAlpha);
                }
            }
        }
    }

    bool m_loaded = false;
    float m_fontSize = 16.0F;

    std::vector<uint8_t> m_fontData;

    // RAII 包装器：负责在析构时自动释放 FreeType/HarfBuzz 资源
    struct FtLibraryDeleter
    {
        void operator()(std::remove_pointer_t<FT_Library>* library) const noexcept { FT_Done_FreeType(library); }
    };
    struct FtFaceDeleter
    {
        void operator()(std::remove_pointer_t<FT_Face>* face) const noexcept { FT_Done_Face(face); }
    };
    struct HbFontDeleter
    {
        void operator()(hb_font_t* font) const noexcept { hb_font_destroy(font); }
    };

    // 成员销毁顺序为声明逆序：m_hbFont → m_ftFace → m_ftLibrary（正确清理顺序）
    std::unique_ptr<std::remove_pointer_t<FT_Library>, FtLibraryDeleter> m_ftLibrary;
    std::unique_ptr<std::remove_pointer_t<FT_Face>, FtFaceDeleter> m_ftFace;
    std::unique_ptr<hb_font_t, HbFontDeleter> m_hbFont;

    // 字形缓存（key = (fontSize << 32) | codepoint）
    std::unordered_map<uint64_t, GlyphInfo> m_glyphCache;
};

} // namespace ui::managers
