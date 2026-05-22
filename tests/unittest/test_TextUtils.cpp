/**
 * ************************************************************************
 * 
 * @file test_TextUtils.cpp
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2026-03-16
 * @version 0.1
 * @brief 测试 TextUtils 的文本处理功能，确保文本换行和尾部截断逻辑正确
 * 
 * ************************************************************************
 * @copyright Copyright (c) 2026 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */
#include <gtest/gtest.h>
#include <string>

#include "common/Policies.hpp"
#include "src/core/TextUtils.hpp"

namespace ui::tests
{
namespace
{

int MeasureByByteLength(const std::string& text)
{
    return static_cast<int>(text.size());
}

} // namespace

TEST(TextUtilsTest, PrevAndNextCharRespectUtf8Boundaries)
{
    const std::string text = "A你B";

    EXPECT_EQ(ui::utils::NextCharPos(text, 0), 1U);
    EXPECT_EQ(ui::utils::NextCharPos(text, 1), 4U);
    EXPECT_EQ(ui::utils::NextCharPos(text, 4), 5U);

    EXPECT_EQ(ui::utils::PrevCharPos(text, 5), 4U);
    EXPECT_EQ(ui::utils::PrevCharPos(text, 4), 1U);
    EXPECT_EQ(ui::utils::PrevCharPos(text, 1), 0U);
}

TEST(TextUtilsTest, WrapTextLinesReturnsOriginalTextWhenWrapDisabled)
{
    const auto lines = ui::utils::WrapTextLines("alpha beta", 3, ui::policies::TextWrap::NONE, MeasureByByteLength);

    ASSERT_EQ(lines.size(), 1U);
    EXPECT_EQ(lines.at(0), "alpha beta");
}

TEST(TextUtilsTest, WrapTextLinesPreservesBlankLinesBetweenParagraphs)
{
    const auto lines = ui::utils::WrapTextLines("alpha\n\nbeta", 10, ui::policies::TextWrap::WORD, MeasureByByteLength);

    ASSERT_EQ(lines.size(), 4U);
    EXPECT_EQ(lines.at(0), "alpha");
    EXPECT_EQ(lines.at(1), "");
    EXPECT_EQ(lines.at(2), "");
    EXPECT_EQ(lines.at(3), "beta");
}

TEST(TextUtilsTest, WordWrapFallsBackToCharacterWrapForLongWord)
{
    const auto lines = ui::utils::WrapTextLines("abcdef", 2, ui::policies::TextWrap::WORD, MeasureByByteLength);

    ASSERT_EQ(lines.size(), 3U);
    EXPECT_EQ(lines.at(0), "ab");
    EXPECT_EQ(lines.at(1), "cd");
    EXPECT_EQ(lines.at(2), "ef");
}

TEST(TextUtilsTest, CharWrapSplitsAtCharacterGranularity)
{
    const auto lines = ui::utils::WrapTextLines("abcd", 2, ui::policies::TextWrap::CHAR, MeasureByByteLength);

    ASSERT_EQ(lines.size(), 2U);
    EXPECT_EQ(lines.at(0), "ab");
    EXPECT_EQ(lines.at(1), "cd");
}

TEST(TextUtilsTest, GetTailThatFitsReturnsLongestSuffixWithinWidth)
{
    float measuredWidth = 0.0F;
    const auto tail = ui::utils::GetTailThatFits("abcdef", 3, MeasureByByteLength, measuredWidth);

    EXPECT_EQ(tail, "def");
    EXPECT_FLOAT_EQ(measuredWidth, 3.0F);
}

TEST(TextUtilsTest, GetTailThatFitsHandlesEmptyAndZeroWidth)
{
    float measuredWidth = 12.0F;

    EXPECT_EQ(ui::utils::GetTailThatFits("", 4, MeasureByByteLength, measuredWidth), "");
    EXPECT_FLOAT_EQ(measuredWidth, 0.0F);

    measuredWidth = 12.0F;
    EXPECT_EQ(ui::utils::GetTailThatFits("abcdef", 0, MeasureByByteLength, measuredWidth), "");
    EXPECT_FLOAT_EQ(measuredWidth, 0.0F);
}

} // namespace ui::tests