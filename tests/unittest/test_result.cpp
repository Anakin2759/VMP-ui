// Tests for ui::Result<T> + ui::ui_errc (WP-A1).
// Reference: docs/architecture/result-type-design.md §2/§3/§8.1.

#include <format>
#include <string>
#include <system_error>
#include <type_traits>

#include <gtest/gtest.h>

#include "src/common/ErrorCodes.hpp"
#include "src/common/Result.hpp"

namespace
{

// ---------- Result<T> 成功路径 ----------
TEST(ResultTest, ValueSuccess)
{
    ui::Result<int> result = 42;
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(static_cast<bool>(result));
    EXPECT_EQ(*result, 42);
    EXPECT_EQ(result.value(), 42);
}

TEST(ResultTest, OkFactoryDeducesType)
{
    auto okResult = ui::Ok(7);
    static_assert(std::is_same_v<decltype(okResult), ui::Result<int>>);
    ASSERT_TRUE(okResult.has_value());
    EXPECT_EQ(*okResult, 7);
}

// ---------- Result<T> 失败路径 ----------
TEST(ResultTest, ValueFailure)
{
    ui::Result<int> result = ui::MakeError(ui::UiErrc::INVALID_ARGUMENT);
    ASSERT_FALSE(result.has_value());
    EXPECT_FALSE(static_cast<bool>(result));
    EXPECT_EQ(result.error(), ui::UiErrc::INVALID_ARGUMENT);
    EXPECT_EQ(result.error().category(), ui::GetUiErrorCategory());
}

// ---------- Result<void> 成功 + 失败 ----------
TEST(ResultVoidTest, OkSuccess)
{
    ui::Result<void> const result = ui::Ok();
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(static_cast<bool>(result));
}

TEST(ResultVoidTest, Failure)
{
    ui::Result<void> result = ui::MakeError(ui::UiErrc::THEME_NOT_FOUND);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ui::UiErrc::THEME_NOT_FOUND);
}

// ---------- std::error_code 比较语义 ----------
TEST(ErrorCodeTest, MakeErrorCodeRoutesToUiCategory)
{
    std::error_code const errorCode = ui::MakeErrorCode(ui::UiErrc::ASSET_DECODE_FAILED);
    EXPECT_EQ(errorCode.value(), static_cast<int>(ui::UiErrc::ASSET_DECODE_FAILED));
    EXPECT_EQ(&errorCode.category(), &ui::GetUiErrorCategory());
    EXPECT_STREQ(errorCode.category().name(), "ui");
}

TEST(ErrorCodeTest, EnumEqualityViaMakeErrorCode)
{
    // is_error_code_enum 特化生效后，enum 值可直接与 error_code 比较。
    std::error_code const errorCode = ui::MakeErrorCode(ui::UiErrc::DEVICE_UNAVAILABLE);
    EXPECT_TRUE(errorCode == ui::UiErrc::DEVICE_UNAVAILABLE);
    EXPECT_FALSE(errorCode == ui::UiErrc::ASSET_NOT_FOUND);
}

TEST(ErrorCodeTest, MakeErrorTemplateAcceptsEnum)
{
    auto unexpectedError = ui::MakeError(ui::UiErrc::ASSET_NOT_FOUND);
    std::error_code const errorCode = unexpectedError.error();
    EXPECT_EQ(errorCode, ui::UiErrc::ASSET_NOT_FOUND);
}

TEST(ErrorCodeTest, MessageNonEmpty)
{
    std::error_code const errorCode = ui::MakeErrorCode(ui::UiErrc::ASSET_LOAD_FAILED);
    EXPECT_FALSE(errorCode.message().empty());
}

// ---------- std::formatter<ui_errc> ----------
TEST(ErrorCodeTest, FormatterPrintsEnumName)
{
    const std::string text = std::format("{}", ui::UiErrc::ASSET_DECODE_FAILED);
    EXPECT_EQ(text, "asset_decode_failed");

    const std::string prefixedText = std::format("err={}", ui::UiErrc::INVALID_ARGUMENT);
    EXPECT_EQ(prefixedText, "err=invalid_argument");
}

} // namespace
