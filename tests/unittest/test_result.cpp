// Tests for ui::Result<T> + ui::ui_errc (WP-A1).
// Reference: docs/architecture/result-type-design.md §2/§3/§8.1.

#include <format>
#include <string>
#include <string_view>
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
    ui::Result<int> r = 42;
    ASSERT_TRUE(r.has_value());
    EXPECT_TRUE(static_cast<bool>(r));
    EXPECT_EQ(*r, 42);
    EXPECT_EQ(r.value(), 42);
}

TEST(ResultTest, OkFactoryDeducesType)
{
    auto r = ui::Ok(7);
    static_assert(std::is_same_v<decltype(r), ui::Result<int>>);
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(*r, 7);
}

// ---------- Result<T> 失败路径 ----------
TEST(ResultTest, ValueFailure)
{
    ui::Result<int> r = ui::MakeError(ui::ui_errc::invalid_argument);
    ASSERT_FALSE(r.has_value());
    EXPECT_FALSE(static_cast<bool>(r));
    EXPECT_EQ(r.error(), ui::ui_errc::invalid_argument);
    EXPECT_EQ(r.error().category(), ui::GetUiErrorCategory());
}

// ---------- Result<void> 成功 + 失败 ----------
TEST(ResultVoidTest, OkSuccess)
{
    ui::Result<void> r = ui::Ok();
    ASSERT_TRUE(r.has_value());
    EXPECT_TRUE(static_cast<bool>(r));
}

TEST(ResultVoidTest, Failure)
{
    ui::Result<void> r = ui::MakeError(ui::ui_errc::theme_not_found);
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error(), ui::ui_errc::theme_not_found);
}

// ---------- std::error_code 比较语义 ----------
TEST(ErrorCodeTest, MakeErrorCodeRoutesToUiCategory)
{
    std::error_code ec = ui::make_error_code(ui::ui_errc::asset_decode_failed);
    EXPECT_EQ(ec.value(), static_cast<int>(ui::ui_errc::asset_decode_failed));
    EXPECT_EQ(&ec.category(), &ui::GetUiErrorCategory());
    EXPECT_STREQ(ec.category().name(), "ui");
}

TEST(ErrorCodeTest, EnumEqualityViaMakeErrorCode)
{
    // is_error_code_enum 特化生效后，enum 值可直接与 error_code 比较。
    std::error_code ec = ui::make_error_code(ui::ui_errc::device_unavailable);
    EXPECT_TRUE(ec == ui::ui_errc::device_unavailable);
    EXPECT_FALSE(ec == ui::ui_errc::asset_not_found);
}

TEST(ErrorCodeTest, MakeErrorTemplateAcceptsEnum)
{
    auto unx = ui::MakeError(ui::ui_errc::asset_not_found);
    std::error_code ec = unx.error();
    EXPECT_EQ(ec, ui::ui_errc::asset_not_found);
}

TEST(ErrorCodeTest, MessageNonEmpty)
{
    std::error_code ec = ui::make_error_code(ui::ui_errc::asset_load_failed);
    EXPECT_FALSE(ec.message().empty());
}

// ---------- std::formatter<ui_errc> ----------
TEST(ErrorCodeTest, FormatterPrintsEnumName)
{
    const std::string s = std::format("{}", ui::ui_errc::asset_decode_failed);
    EXPECT_EQ(s, "asset_decode_failed");

    const std::string s2 = std::format("err={}", ui::ui_errc::invalid_argument);
    EXPECT_EQ(s2, "err=invalid_argument");
}

} // namespace
