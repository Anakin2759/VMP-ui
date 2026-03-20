#include <gtest/gtest.h>

#include "src/ui/managers/ResourceProvider.hpp"

namespace ui::tests
{

TEST(UiCoverageTest, BinaryResourceDefaultStateIsEmpty)
{
    const managers::BinaryResource resource{};

    EXPECT_TRUE(resource.empty());
    EXPECT_EQ(resource.data(), nullptr);
    EXPECT_EQ(resource.size(), 0U);
    EXPECT_FALSE(static_cast<bool>(resource));
}

TEST(UiCoverageTest, DefaultResourceProviderLoadsEmbeddedFont)
{
    const auto provider = managers::GetDefaultUiResourceProvider();

    ASSERT_NE(provider, nullptr);
    EXPECT_TRUE(provider->exists("assets/fonts/NotoSansSC-VariableFont_wght.ttf"));

    const auto resource = provider->loadBinary("assets/fonts/NotoSansSC-VariableFont_wght.ttf");
    ASSERT_TRUE(resource.has_value()) << resource.error();
    EXPECT_TRUE(static_cast<bool>(resource.value()));
    EXPECT_GT(resource->size(), 0U);
    EXPECT_NE(resource->data(), nullptr);
}

TEST(UiCoverageTest, DefaultResourceProviderLoadsEmbeddedIconCodepoints)
{
    const auto provider = managers::GetDefaultUiResourceProvider();

    ASSERT_NE(provider, nullptr);
    EXPECT_TRUE(provider->exists("assets/icons/MaterialSymbolsRounded[FILL,GRAD,opsz,wght].codepoints"));

    const auto resource = provider->loadBinary("assets/icons/MaterialSymbolsRounded[FILL,GRAD,opsz,wght].codepoints");
    ASSERT_TRUE(resource.has_value()) << resource.error();
    EXPECT_GT(resource->size(), 0U);
}

TEST(UiCoverageTest, DefaultResourceProviderReportsMissingResource)
{
    const auto provider = managers::GetDefaultUiResourceProvider();

    ASSERT_NE(provider, nullptr);
    EXPECT_FALSE(provider->exists("assets/does-not-exist.bin"));

    const auto resource = provider->loadBinary("assets/does-not-exist.bin");
    ASSERT_FALSE(resource.has_value());
    EXPECT_FALSE(resource.error().empty());
}

} // namespace ui::tests