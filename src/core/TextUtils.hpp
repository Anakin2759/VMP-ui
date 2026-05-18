/**
 * ************************************************************************
 *
 * @file TextUtils.hpp
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2026-01-30
 * @version 0.1
 * @brief 文本处理工具函数
 *
 * ************************************************************************
 * @copyright Copyright (c) 2026 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */

#pragma once
#include <string>
#include <vector>
#include <functional>
#include <algorithm>
#include "../common/Components.hpp"

namespace ui::utils
{

/**
 * @brief 检查一个字节是否是 UTF-8 字符的起始字节
 */
inline bool IsUtf8StartByte(unsigned char c)
{
    // 在 UTF-8 中，多字节字符的后续字节都以 10xxxxxx 开头 (0x80 到 0xBF)
    // 起始字节可能是 0xxxxxxx (单字节) 或 11xxxxxx (多字节起始)
    return (c & 0xC0) != 0x80;
}

/**
 * @brief 获取上一个字符的字节偏移量
 */
inline size_t PrevCharPos(const std::string& text, size_t pos)
{
    if (pos == 0) return 0;
    size_t newPos = pos - 1;
    while (newPos > 0 && !IsUtf8StartByte(static_cast<unsigned char>(text[newPos])))
    {
        newPos--;
    }
    return newPos;
}

/**
 * @brief 获取下一个字符的字节偏移量
 */
inline size_t NextCharPos(const std::string& text, size_t pos)
{
    if (pos >= text.size()) return text.size();
    size_t newPos = pos + 1;
    while (newPos < text.size() && !IsUtf8StartByte(static_cast<unsigned char>(text[newPos])))
    {
        newPos++;
    }
    return newPos;
}
/**
 * @brief 换行处理单个段落
 */
template <typename MeasureFunc>
inline std::vector<std::string>
    WrapParagraph(const std::string& paragraph, int maxWidth, policies::TextWrap wrapMode, MeasureFunc&& measureFunc)
{
    std::vector<std::string> lines;

    if (paragraph.empty())
    {
        lines.push_back("");
        return lines;
    }

    if (wrapMode == policies::TextWrap::Char)
    {
        std::string currentLine;
        for (size_t i = 0; i < paragraph.size();)
        {
            size_t next = NextCharPos(paragraph, i);
            std::string ch = paragraph.substr(i, next - i);
            float width = measureFunc(currentLine + ch);
            if (width > maxWidth && !currentLine.empty())
            {
                lines.push_back(currentLine);
                currentLine = ch;
            }
            else
            {
                currentLine += ch;
            }
            i = next;
        }
        if (!currentLine.empty()) lines.push_back(currentLine);
        return lines;
    }

    // Word 模式或默认
    std::string currentLine;
    std::string word;

    auto pushWord = [&](const std::string& w)
    {
        if (w.empty()) return;

        // 如果单个单词就超过了最大宽度，强制对其进行字符级换行
        if (measureFunc(w) > maxWidth)
        {
            // 先把当前行推入
            if (!currentLine.empty())
            {
                lines.push_back(currentLine);
                currentLine.clear();
            }

            // 对这个超长单词进行字符换行
            std::string tempWord;
            for (size_t i = 0; i < w.size();)
            {
                size_t next = NextCharPos(w, i);
                std::string ch = w.substr(i, next - i);
                if (measureFunc(tempWord + ch) > maxWidth && !tempWord.empty())
                {
                    lines.push_back(tempWord);
                    tempWord = ch;
                }
                else
                {
                    tempWord += ch;
                }
                i = next;
            }
            currentLine = tempWord;
            return;
        }

        std::string testLine = currentLine.empty() ? w : currentLine + " " + w;
        if (measureFunc(testLine) > maxWidth && !currentLine.empty())
        {
            lines.push_back(currentLine);
            currentLine = w;
        }
        else
        {
            currentLine = testLine;
        }
    };

    for (size_t i = 0; i < paragraph.size();)
    {
        unsigned char c = static_cast<unsigned char>(paragraph[i]);
        if (c == ' ' || c == '\t')
        {
            pushWord(word);
            word.clear();
            // 在 Word 模式下保留必要的间距（如果不是行首）
            if (!currentLine.empty())
            {
                // 注意：由于 pushWord 可能会推入多行，这里简单处理
                // 实际复杂的渲染可能需要更好的处理
            }
            i++;
        }
        else
        {
            size_t next = NextCharPos(paragraph, i);
            word += paragraph.substr(i, next - i);
            i = next;
        }
    }

    pushWord(word);

    if (!currentLine.empty())
    {
        lines.push_back(currentLine);
    }

    return lines;
}

/**
 * @brief 文本换行处理
 */
template <typename MeasureFunc>
inline std::vector<std::string>
    WrapTextLines(const std::string& text, int maxWidth, policies::TextWrap wrapMode, MeasureFunc&& measureFunc)
{
    std::vector<std::string> lines;

    if (wrapMode == policies::TextWrap::NONE || maxWidth <= 0)
    {
        lines.push_back(text);
        return lines;
    }

    // 按换行符分割
    std::string currentParagraph;
    for (char c : text)
    {
        if (c == '\n')
        {
            if (!currentParagraph.empty())
            {
                // 处理当前段落
                auto wrappedLines = WrapParagraph(currentParagraph, maxWidth, wrapMode, measureFunc);
                lines.insert(lines.end(), wrappedLines.begin(), wrappedLines.end());
                currentParagraph.clear();
            }
            lines.push_back(""); // 空行
        }
        else
        {
            currentParagraph += c;
        }
    }

    // 处理最后一段
    if (!currentParagraph.empty())
    {
        auto wrappedLines = WrapParagraph(currentParagraph, maxWidth, wrapMode, measureFunc);
        lines.insert(lines.end(), wrappedLines.begin(), wrappedLines.end());
    }

    return lines;
}

/**
 * @brief 获取能够显示在指定宽度内的文本尾部
 */
template <typename MeasureFunc>
inline std::string GetTailThatFits(const std::string& text, int maxWidth, MeasureFunc&& measureFunc, float& outWidth)
{
    outWidth = 0.0f;

    if (text.empty() || maxWidth <= 0)
    {
        return "";
    }

    // 从尾部开始截取
    for (size_t i = 0; i < text.size(); ++i)
    {
        std::string substr = text.substr(text.size() - i - 1);
        float width = measureFunc(substr);

        if (width <= maxWidth)
        {
            outWidth = width;
            if (i == text.size() - 1)
            {
                return substr; // 全部都能显示
            }
        }
        else
        {
            // 超出宽度，返回上一次的结果
            if (i > 0)
            {
                return text.substr(text.size() - i);
            }
            return "";
        }
    }

    outWidth = measureFunc(text);
    return text;
}

} // namespace ui::utils
