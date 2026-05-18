/**
 * ************************************************************************
 *
 * @file TextRenderHelper.hpp
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2026-02-10
 * @version 0.1
 * @brief 文本渲染辅助类（基于纹理图集）
 *
 * 提供基于纹理图集的文本渲染功能：
 * - 从 FontAtlasManager 获取字形
 * - 生成文本渲染所需的顶点数据（位置 + UV）
 * - 支持逐字符拼接，无需为每个字符串生成独立纹理
 *
 * 使用示例：
 *   TextRenderHelper helper(fontAtlasManager);
 *   auto renderData = helper.prepareTextRender("Hello", x, y, color);
 *   // 使用 renderData.vertices 和 atlasTexture 进行批量渲染
 *
 * ************************************************************************
 * @copyright Copyright (c) 2026 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */

#pragma once

#include "FontAtlasManager.hpp"
#include <Eigen/Dense>
#include <vector>
#include <string>

namespace ui::managers
{

/**
 * @brief 文本渲染顶点（位置 + UV + 颜色）
 */
struct TextVertex
{
    float x = 0.0F;
    float y = 0.0F;
    float u = 0.0F;
    float v = 0.0F;
    float r = 1.0F;
    float g = 1.0F;
    float b = 1.0F;
    float a = 1.0F;
};

/**
 * @brief 文本渲染数据
 */
struct TextRenderData
{
    std::vector<TextVertex> vertices; // 顶点数据（6个顶点/字符，两个三角形）
    int width = 0;                    // 文本总宽度
    int height = 0;                   // 文本总高度
};

/**
 * @brief 文本渲染辅助类
 */
class TextRenderHelper
{
public:
    explicit TextRenderHelper(FontAtlasManager& fontAtlasManager) : m_fontAtlasManager(fontAtlasManager) {}

    /**
     * @brief 准备文本渲染数据
     * @param text UTF-8 文本
     * @param x 起始 X 坐标
     * @param y 起始 Y 坐标（基线）
     * @param color RGBA 颜色
     * @return 文本渲染数据
     */
    TextRenderData prepareTextRender(const std::string& text, float x, float y, const Eigen::Vector4f& color)
    {
        TextRenderData data;

        if (!m_fontAtlasManager.isLoaded() || text.empty())
        {
            return data;
        }

        float cursorX = x;
        float cursorY = y;
        int maxHeight = m_fontAtlasManager.getFontHeight();

        // 使用 HarfBuzz 进行文本成形，获取正确的字形 ID 和位置
        auto shapedGlyphs = m_fontAtlasManager.shapeText(text.c_str(), text.size());

        for (const auto& shaped : shapedGlyphs)
        {
            // 通过字形索引获取图集字形
            auto glyphOpt = m_fontAtlasManager.getOrAddGlyphByIndex(shaped.glyphId);
            if (!glyphOpt.has_value())
            {
                cursorX += shaped.xAdvance;
                continue;
            }

            const AtlasGlyph& glyph = *glyphOpt;

            // 计算字形渲染位置（应用 HarfBuzz 偏移）
            float glyphX = cursorX + shaped.xOffset + static_cast<float>(glyph.bearingX);
            float glyphY = cursorY - shaped.yOffset - static_cast<float>(glyph.bearingY);
            float glyphW = static_cast<float>(glyph.width);
            float glyphH = static_cast<float>(glyph.height);

            // 生成两个三角形（6个顶点）
            if (glyphW > 0.0F && glyphH > 0.0F)
            {
                // 三角形 1: 左上, 左下, 右下
                data.vertices.push_back(
                    {glyphX, glyphY, glyph.u0, glyph.v0, color.x(), color.y(), color.z(), color.w()});
                data.vertices.push_back(
                    {glyphX, glyphY + glyphH, glyph.u0, glyph.v1, color.x(), color.y(), color.z(), color.w()});
                data.vertices.push_back(
                    {glyphX + glyphW, glyphY + glyphH, glyph.u1, glyph.v1, color.x(), color.y(), color.z(), color.w()});

                // 三角形 2: 左上, 右下, 右上
                data.vertices.push_back(
                    {glyphX, glyphY, glyph.u0, glyph.v0, color.x(), color.y(), color.z(), color.w()});
                data.vertices.push_back(
                    {glyphX + glyphW, glyphY + glyphH, glyph.u1, glyph.v1, color.x(), color.y(), color.z(), color.w()});
                data.vertices.push_back(
                    {glyphX + glyphW, glyphY, glyph.u1, glyph.v0, color.x(), color.y(), color.z(), color.w()});
            }

            // 前进游标
            cursorX += shaped.xAdvance;
            cursorY += shaped.yAdvance;
        }

        data.width = static_cast<int>(cursorX - x);
        data.height = maxHeight;

        return data;
    }

    /**
     * @brief 测量文本宽度
     */
    int measureTextWidth(const std::string& text) { return m_fontAtlasManager.measureTextWidth(text); }

private:
    FontAtlasManager& m_fontAtlasManager;
};

} // namespace ui::managers
