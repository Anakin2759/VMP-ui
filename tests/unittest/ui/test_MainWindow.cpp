/**
 * ************************************************************************
 *
 * @file test_MainWindow.cpp
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2026-03-20
 * @version 0.3
 * @brief UI 模块集成测试
 *
 * 直接组合 UI 模块公开 API，验证工厂、链式 DSL、层级和文本编辑状态的联动。
 * 避免依赖 client 视图层，确保测试边界停留在 ui 模块内部。
 *
 * ************************************************************************
 */
#include <gtest/gtest.h>

#include <ui.hpp>

#include "src/ui/singleton/Registry.hpp"

namespace ui::tests
{
namespace
{

class UiIntegrationTest : public ::testing::Test
{
protected:
    void SetUp() override { Registry::Clear(); }

    void TearDown() override { Registry::Clear(); }
};

bool ContainsChild(const components::Hierarchy& hierarchy, entt::entity child)
{
    return std::find(hierarchy.children.begin(), hierarchy.children.end(), child) != hierarchy.children.end();
}

} // namespace

TEST_F(UiIntegrationTest, DslBuildsWidgetTreeWithinUiModule)
{
    using namespace ui::chains;

    const auto root = factory::CreateVBoxLayout("root_layout");
    const auto button = factory::CreateButton("Start", "start_button");
    const auto editor = factory::CreateLineEdit("seed", "placeholder", "name_input");

    root | Spacing(12.0F) | Padding(8.0F) | AddChild(button) | AddChild(editor);
    button | FixedSize(180.0F, 44.0F) | BackgroundColor(Color::Blue()) | BorderRadius(6.0F) |
        BorderColor(Color::White()) | BorderThickness(2.0F) | Text("Ready") | FontSize(18.0F) |
        TextAlignment(policies::Alignment::CENTER) | Show();

    const auto& rootHierarchy = Registry::Get<components::Hierarchy>(root);
    const auto& rootLayout = Registry::Get<components::LayoutInfo>(root);
    const auto& rootPadding = Registry::Get<components::Padding>(root);
    const auto& buttonHierarchy = Registry::Get<components::Hierarchy>(button);
    const auto& buttonSize = Registry::Get<components::Size>(button);
    const auto& buttonText = Registry::Get<components::Text>(button);
    const auto& buttonBackground = Registry::Get<components::Background>(button);
    const auto& buttonBorder = Registry::Get<components::Border>(button);

    ASSERT_EQ(rootHierarchy.children.size(), 2U);
    EXPECT_EQ(rootHierarchy.children[0], button);
    EXPECT_EQ(rootHierarchy.children[1], editor);
    EXPECT_EQ(buttonHierarchy.parent, root);
    EXPECT_EQ(Registry::Get<components::BaseInfo>(root).alias, "root_layout");
    EXPECT_EQ(Registry::Get<components::BaseInfo>(button).alias, "start_button");
    EXPECT_EQ(rootLayout.direction, policies::LayoutDirection::VERTICAL);
    EXPECT_FLOAT_EQ(rootLayout.spacing, 12.0F);
    EXPECT_FLOAT_EQ(rootPadding.values.x(), 8.0F);
    EXPECT_FLOAT_EQ(rootPadding.values.y(), 8.0F);
    EXPECT_FLOAT_EQ(rootPadding.values.z(), 8.0F);
    EXPECT_FLOAT_EQ(rootPadding.values.w(), 8.0F);
    EXPECT_EQ(buttonSize.sizePolicy, policies::Size::Fixed);
    EXPECT_FLOAT_EQ(buttonSize.size.x(), 180.0F);
    EXPECT_FLOAT_EQ(buttonSize.size.y(), 44.0F);
    EXPECT_EQ(buttonText.content, "Ready");
    EXPECT_FLOAT_EQ(buttonText.fontSize, 18.0F);
    EXPECT_EQ(buttonText.alignment, policies::Alignment::CENTER);
    EXPECT_EQ(buttonBackground.enabled, policies::Feature::Enabled);
    EXPECT_FLOAT_EQ(buttonBackground.color.blue, 1.0F);
    EXPECT_FLOAT_EQ(buttonBackground.borderRadius.x(), 6.0F);
    EXPECT_EQ(buttonBorder.enabled, policies::Feature::Enabled);
    EXPECT_FLOAT_EQ(buttonBorder.thickness, 2.0F);
    EXPECT_TRUE(Registry::AllOf<components::VisibleTag>(button));
    EXPECT_FALSE(Registry::AllOf<components::RootTag>(button));
    EXPECT_FALSE(Registry::AllOf<components::RootTag>(editor));
    EXPECT_TRUE(Registry::AllOf<components::LayoutDirtyTag>(root));
}

TEST_F(UiIntegrationTest, ReparentAndRemoveChildKeepHierarchyConsistent)
{
    const auto firstParent = factory::CreateHBoxLayout("first_parent");
    const auto secondParent = factory::CreateVBoxLayout("second_parent");
    const auto child = factory::CreateLabel("Status", "status_label");

    hierarchy::AddChild(firstParent, child);

    auto& firstHierarchy = Registry::Get<components::Hierarchy>(firstParent);
    ASSERT_EQ(firstHierarchy.children.size(), 1U);
    EXPECT_TRUE(ContainsChild(firstHierarchy, child));
    EXPECT_EQ(Registry::Get<components::Hierarchy>(child).parent, firstParent);

    hierarchy::AddChild(secondParent, child);

    const auto& secondHierarchy = Registry::Get<components::Hierarchy>(secondParent);
    EXPECT_TRUE(firstHierarchy.children.empty());
    ASSERT_EQ(secondHierarchy.children.size(), 1U);
    EXPECT_TRUE(ContainsChild(secondHierarchy, child));
    EXPECT_EQ(Registry::Get<components::Hierarchy>(child).parent, secondParent);
    EXPECT_FALSE(Registry::AllOf<components::RootTag>(child));
    EXPECT_TRUE(Registry::AllOf<components::LayoutDirtyTag>(firstParent));
    EXPECT_TRUE(Registry::AllOf<components::LayoutDirtyTag>(secondParent));

    hierarchy::RemoveChild(secondParent, child);

    EXPECT_TRUE(Registry::Get<components::Hierarchy>(secondParent).children.empty());
    EXPECT_TRUE(Registry::Get<components::Hierarchy>(child).parent == entt::null);
    EXPECT_TRUE(Registry::AllOf<components::RootTag>(child));
}

TEST_F(UiIntegrationTest, TextBrowserFactoryCombinesScrollableReadOnlyEditorState)
{
    using namespace ui::chains;

    const auto browser = factory::CreateTextBrowser("hello world", "unused", "log_browser");

    bool submitCalled = false;
    std::string changedText;

    browser | TextEditContent("ok") | TextColor(Color::Red()) | PasswordMode(policies::TextFlag::Password) |
        OnSubmit([&submitCalled]() { submitCalled = true; }) |
        OnTextChanged([&changedText](const std::string& value) { changedText = value; });

    auto& textEdit = Registry::Get<components::TextEdit>(browser);
    auto& text = Registry::Get<components::Text>(browser);
    const auto& scrollArea = Registry::Get<components::ScrollArea>(browser);
    const auto& size = Registry::Get<components::Size>(browser);

    ASSERT_TRUE(static_cast<bool>(textEdit.onSubmit));
    ASSERT_TRUE(static_cast<bool>(textEdit.onTextChanged));

    textEdit.onSubmit();
    textEdit.onTextChanged(textEdit.buffer);

    EXPECT_TRUE(submitCalled);
    EXPECT_EQ(changedText, "ok");
    EXPECT_EQ(textEdit.buffer, "ok");
    EXPECT_EQ(textEdit.cursorPosition, 0U);
    EXPECT_FALSE(textEdit.hasSelection);
    EXPECT_EQ(textEdit.selectionStart, 0U);
    EXPECT_EQ(textEdit.selectionEnd, 0U);
    EXPECT_EQ(text.alignment, policies::Alignment::TOP | policies::Alignment::LEFT);
    EXPECT_EQ(text.wordWrap, policies::TextWrap::Word);
    EXPECT_FLOAT_EQ(textEdit.textColor.red, 1.0F);
    EXPECT_FLOAT_EQ(textEdit.textColor.green, 0.0F);
    EXPECT_FLOAT_EQ(textEdit.textColor.blue, 0.0F);
    EXPECT_EQ(scrollArea.scroll, policies::Scroll::Vertical);
    EXPECT_EQ(size.sizePolicy, policies::Size::FillParent);
    EXPECT_NE(textEdit.inputMode, policies::TextFlag::ReadOnly | policies::TextFlag::Multiline);
    EXPECT_NE(textEdit.inputMode, policies::TextFlag::Default);
}

} // namespace ui::tests
