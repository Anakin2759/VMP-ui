/**
 * ************************************************************************
 *
 * @file test_MainWindow.cpp
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2026-03-20
 * @version 0.3
 *
 *
 * ************************************************************************
 */
#include <gtest/gtest.h>

#include <memory>
#include <algorithm>
#include <string>
#include <ui.hpp>

#include "common/components/Layout.hpp"
#include "entt/entity/fwd.hpp"
#include "common/components/Data.hpp"
#include "common/components/Visual.hpp"
#include "entt/entity/entity.hpp"
#include "common/components/Interaction.hpp"
#include "src/singleton/Registry.hpp"

namespace ui::tests
{
namespace
{

class UiIntegrationTest : public ::testing::Test
{
protected:
    void SetUp() override { m_scope = std::make_unique<UiRuntimeScope>(m_runtime); }
    void TearDown() override { m_scope.reset(); }

private:
    UiRuntime m_runtime;
    std::unique_ptr<UiRuntimeScope> m_scope;
};

bool ContainsChild(const components::Hierarchy& hierarchy, entt::entity child)
{
    return std::ranges::find(hierarchy.children, child) != hierarchy.children.end();
}

void ConfigureTextBrowser(entt::entity browser, bool& submitCalled, std::string& changedText)
{
    using namespace ui::chains;

    browser | TextEditContent("ok") | TextColor(Color::Red()) | PasswordMode(policies::TextFlag::PASSWORD) |
        OnSubmit([&submitCalled]() { submitCalled = true; }) |
        OnTextChanged([&changedText](const std::string& value) { changedText = value; });
}

void ExpectTextBrowserCallbacks(components::TextEdit& textEdit, bool& submitCalled, std::string& changedText)
{
    ASSERT_TRUE(static_cast<bool>(textEdit.onSubmit));
    ASSERT_TRUE(static_cast<bool>(textEdit.onTextChanged));

    textEdit.onSubmit();
    textEdit.onTextChanged(textEdit.buffer);

    EXPECT_TRUE(submitCalled);
    EXPECT_EQ(changedText, "ok");
}

void ExpectTextBrowserBufferState(const components::TextEdit& textEdit)
{
    EXPECT_EQ(textEdit.buffer, "ok");
    EXPECT_EQ(textEdit.cursorPosition, 0U);
    EXPECT_FALSE(textEdit.hasSelection);
    EXPECT_EQ(textEdit.selectionStart, 0U);
    EXPECT_EQ(textEdit.selectionEnd, 0U);
}

void ExpectTextBrowserColorAndMode(const components::TextEdit& textEdit)
{
    EXPECT_FLOAT_EQ(textEdit.textColor.red, 1.0F);
    EXPECT_FLOAT_EQ(textEdit.textColor.green, 0.0F);
    EXPECT_FLOAT_EQ(textEdit.textColor.blue, 0.0F);
    EXPECT_NE(textEdit.inputMode, policies::TextFlag::READ_ONLY_MULTILINE);
    EXPECT_NE(textEdit.inputMode, policies::TextFlag::DEFAULT);
}

void ExpectTextBrowserLayoutState(entt::entity browser)
{
    const auto& text = Registry::Get<components::Text>(browser);
    const auto& scrollArea = Registry::Get<components::ScrollArea>(browser);
    const auto& size = Registry::Get<components::Size>(browser);

    EXPECT_EQ(text.alignment, policies::Alignment::TOP | policies::Alignment::LEFT);
    EXPECT_EQ(text.wordWrap, policies::TextWrap::WORD);
    EXPECT_EQ(scrollArea.scroll, policies::Scroll::VERTICAL);
    EXPECT_EQ(size.sizePolicy, policies::Size::FILL_PARENT);
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
    EXPECT_EQ(rootHierarchy.children.at(0), button);
    EXPECT_EQ(rootHierarchy.children.at(1), editor);
    EXPECT_EQ(buttonHierarchy.parent, root);
    EXPECT_EQ(Registry::Get<components::BaseInfo>(root).alias, "root_layout");
    EXPECT_EQ(Registry::Get<components::BaseInfo>(button).alias, "start_button");
    EXPECT_EQ(rootLayout.direction, policies::LayoutDirection::VERTICAL);
    EXPECT_FLOAT_EQ(rootLayout.spacing, 12.0F);
    EXPECT_FLOAT_EQ(rootPadding.values.x(), 8.0F);
    EXPECT_FLOAT_EQ(rootPadding.values.y(), 8.0F);
    EXPECT_FLOAT_EQ(rootPadding.values.z(), 8.0F);
    EXPECT_FLOAT_EQ(rootPadding.values.w(), 8.0F);
    EXPECT_EQ(buttonSize.sizePolicy, policies::Size::FIXED);
    EXPECT_FLOAT_EQ(buttonSize.size.x(), 180.0F);
    EXPECT_FLOAT_EQ(buttonSize.size.y(), 44.0F);
    EXPECT_EQ(buttonText.content, "Ready");
    EXPECT_FLOAT_EQ(buttonText.fontSize, 18.0F);
    EXPECT_EQ(buttonText.alignment, policies::Alignment::CENTER);
    EXPECT_EQ(buttonBackground.enabled, policies::Feature::ENABLED);
    EXPECT_FLOAT_EQ(buttonBackground.color.blue, 1.0F);
    EXPECT_FLOAT_EQ(buttonBackground.borderRadius.x(), 6.0F);
    EXPECT_EQ(buttonBorder.enabled, policies::Feature::ENABLED);
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
    const auto browser = factory::CreateTextBrowser("hello world", "unused", "log_browser");

    bool submitCalled = false;
    std::string changedText;
    ConfigureTextBrowser(browser, submitCalled, changedText);

    auto& textEdit = Registry::Get<components::TextEdit>(browser);
    ExpectTextBrowserCallbacks(textEdit, submitCalled, changedText);
    ExpectTextBrowserBufferState(textEdit);
    ExpectTextBrowserColorAndMode(textEdit);
    ExpectTextBrowserLayoutState(browser);
}

TEST_F(UiIntegrationTest, ContainerFactoriesUseSensibleDefaultAlignment)
{
    const auto vbox = factory::CreateVBoxLayout("vbox_layout");
    const auto hbox = factory::CreateHBoxLayout("hbox_layout");
    const auto scrollArea = factory::CreateScrollArea("scroll_area");

    const auto& vboxLayout = Registry::Get<components::LayoutInfo>(vbox);
    const auto& hboxLayout = Registry::Get<components::LayoutInfo>(hbox);
    const auto& scrollLayout = Registry::Get<components::LayoutInfo>(scrollArea);

    EXPECT_EQ(vboxLayout.direction, policies::LayoutDirection::VERTICAL);
    EXPECT_EQ(vboxLayout.alignment, policies::Alignment::TOP_LEFT);

    EXPECT_EQ(hboxLayout.direction, policies::LayoutDirection::HORIZONTAL);
    EXPECT_EQ(hboxLayout.alignment, policies::Alignment::LEFT | policies::Alignment::VCENTER);

    EXPECT_EQ(scrollLayout.direction, policies::LayoutDirection::VERTICAL);
    EXPECT_EQ(scrollLayout.alignment, policies::Alignment::TOP_LEFT);
}

} // namespace ui::tests
