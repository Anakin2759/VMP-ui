#include <gtest/gtest.h>

#include <memory>
#include <vector>

#include <ui.hpp>

#include "common/components/Visual.hpp"
#include "src/systems/ThemeSystem.hpp"

namespace ui::tests
{
namespace
{

class ThemeSystemTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        m_scope = std::make_unique<UiRuntimeScope>(m_runtime);
        m_themeSystem = std::make_unique<systems::ThemeSystem>();
        m_themeSystem->registerHandlers();
    }

    void TearDown() override
    {
        m_themeSystem->unregisterHandlers();
        m_themeSystem.reset();
        m_scope.reset();
    }

    static void triggerThemeUpdate() { RuntimeFacade::current().trigger(events::UpdateEvent{}); }

    static std::vector<entt::entity> popupChildren(entt::entity popupEntity)
    {
        const auto* hierarchy = Registry::TryGet<components::Hierarchy>(popupEntity);
        if (hierarchy == nullptr)
        {
            return {};
        }

        return hierarchy->children;
    }

private:
    UiRuntime m_runtime;
    std::unique_ptr<UiRuntimeScope> m_scope;
    std::unique_ptr<systems::ThemeSystem> m_themeSystem;
};

TEST_F(ThemeSystemTest, AppliesDefaultButtonThemeOnUpdate)
{
    const auto button = factory::CreateButton("Theme", "theme_button");

    triggerThemeUpdate();

    const auto* background = Registry::TryGet<components::Background>(button);
    const auto* border = Registry::TryGet<components::Border>(button);
    const auto* text = Registry::TryGet<components::Text>(button);

    ASSERT_NE(background, nullptr);
    ASSERT_NE(border, nullptr);
    ASSERT_NE(text, nullptr);
    EXPECT_EQ(background->enabled, policies::Feature::ENABLED);
    EXPECT_EQ(border->enabled, policies::Feature::ENABLED);
    EXPECT_FLOAT_EQ(background->color.red, theme::CurrentTheme().primaryButtonBackground.red);
    EXPECT_FLOAT_EQ(background->color.green, theme::CurrentTheme().primaryButtonBackground.green);
    EXPECT_FLOAT_EQ(background->color.blue, theme::CurrentTheme().primaryButtonBackground.blue);
    EXPECT_FLOAT_EQ(text->color.red, theme::CurrentTheme().primaryButtonText.red);
    EXPECT_FLOAT_EQ(text->color.green, theme::CurrentTheme().primaryButtonText.green);
    EXPECT_FLOAT_EQ(text->color.blue, theme::CurrentTheme().primaryButtonText.blue);
    EXPECT_TRUE(Registry::AllOf<components::ThemedTag>(button));
}

TEST_F(ThemeSystemTest, ExplicitStyleIsNotOverriddenOnFirstApply)
{
    const auto button = factory::CreateButton("Theme", "theme_button_explicit");
    visibility::SetBackgroundColor(button, Color::Red());
    text::SetTextColor(button, Color::Yellow());

    triggerThemeUpdate();

    const auto& background = Registry::Get<components::Background>(button);
    const auto& text = Registry::Get<components::Text>(button);

    EXPECT_FLOAT_EQ(background.color.red, 1.0F);
    EXPECT_FLOAT_EQ(background.color.green, 0.0F);
    EXPECT_FLOAT_EQ(background.color.blue, 0.0F);
    EXPECT_FLOAT_EQ(text.color.red, 1.0F);
    EXPECT_FLOAT_EQ(text.color.green, 1.0F);
    EXPECT_FLOAT_EQ(text.color.blue, 0.0F);
}

TEST_F(ThemeSystemTest, SetThemeReappliesThemeOwnedValues)
{
    const auto button = factory::CreateButton("Theme", "theme_button_reapply");

    triggerThemeUpdate();

    auto newTheme = theme::DefaultDarkTheme();
    newTheme.primaryButtonBackground = Color::Green();
    newTheme.primaryButtonText = Color::Black();
    theme::SetTheme(newTheme);

    triggerThemeUpdate();

    const auto& background = Registry::Get<components::Background>(button);
    const auto& text = Registry::Get<components::Text>(button);

    EXPECT_FLOAT_EQ(background.color.red, 0.0F);
    EXPECT_FLOAT_EQ(background.color.green, 1.0F);
    EXPECT_FLOAT_EQ(background.color.blue, 0.0F);
    EXPECT_FLOAT_EQ(text.color.red, 0.0F);
    EXPECT_FLOAT_EQ(text.color.green, 0.0F);
    EXPECT_FLOAT_EQ(text.color.blue, 0.0F);
}

TEST_F(ThemeSystemTest, SetThemeDoesNotOverwriteExplicitOverrideAfterFirstApply)
{
    const auto button = factory::CreateButton("Theme", "theme_button_override");

    triggerThemeUpdate();
    visibility::SetBackgroundColor(button, Color::Red());

    auto newTheme = theme::DefaultDarkTheme();
    newTheme.primaryButtonBackground = Color::Green();
    theme::SetTheme(newTheme);

    triggerThemeUpdate();

    const auto& background = Registry::Get<components::Background>(button);
    EXPECT_FLOAT_EQ(background.color.red, 1.0F);
    EXPECT_FLOAT_EQ(background.color.green, 0.0F);
    EXPECT_FLOAT_EQ(background.color.blue, 0.0F);
}

TEST_F(ThemeSystemTest, ButtonHoverAndActiveUseThemeStateColors)
{
    const auto button = factory::CreateButton("Theme", "theme_button_states");

    triggerThemeUpdate();
    EXPECT_FLOAT_EQ(Registry::Get<components::Background>(button).color.red,
                    theme::CurrentTheme().primaryButtonBackground.red);

    Registry::EmplaceOrReplace<components::HoveredTag>(button);
    triggerThemeUpdate();
    {
        const auto& background = Registry::Get<components::Background>(button);
        EXPECT_FLOAT_EQ(background.color.red, theme::CurrentTheme().primaryButtonBackgroundHover.red);
        EXPECT_FLOAT_EQ(background.color.green, theme::CurrentTheme().primaryButtonBackgroundHover.green);
        EXPECT_FLOAT_EQ(background.color.blue, theme::CurrentTheme().primaryButtonBackgroundHover.blue);
    }

    Registry::EmplaceOrReplace<components::ActiveTag>(button);
    triggerThemeUpdate();
    {
        const auto& background = Registry::Get<components::Background>(button);
        EXPECT_FLOAT_EQ(background.color.red, theme::CurrentTheme().primaryButtonBackgroundActive.red);
        EXPECT_FLOAT_EQ(background.color.green, theme::CurrentTheme().primaryButtonBackgroundActive.green);
        EXPECT_FLOAT_EQ(background.color.blue, theme::CurrentTheme().primaryButtonBackgroundActive.blue);
    }

    Registry::Remove<components::ActiveTag>(button);
    triggerThemeUpdate();
    {
        const auto& background = Registry::Get<components::Background>(button);
        EXPECT_FLOAT_EQ(background.color.red, theme::CurrentTheme().primaryButtonBackgroundHover.red);
        EXPECT_FLOAT_EQ(background.color.green, theme::CurrentTheme().primaryButtonBackgroundHover.green);
        EXPECT_FLOAT_EQ(background.color.blue, theme::CurrentTheme().primaryButtonBackgroundHover.blue);
    }

    Registry::Remove<components::HoveredTag>(button);
    triggerThemeUpdate();
    {
        const auto& background = Registry::Get<components::Background>(button);
        EXPECT_FLOAT_EQ(background.color.red, theme::CurrentTheme().primaryButtonBackground.red);
        EXPECT_FLOAT_EQ(background.color.green, theme::CurrentTheme().primaryButtonBackground.green);
        EXPECT_FLOAT_EQ(background.color.blue, theme::CurrentTheme().primaryButtonBackground.blue);
    }
}

TEST_F(ThemeSystemTest, TextEditFocusedUsesThemeFocusBorderColor)
{
    const auto textEdit = factory::CreateTextEdit("placeholder", false, "theme_text_edit_focus");

    triggerThemeUpdate();
    {
        const auto& border = Registry::Get<components::Border>(textEdit);
        EXPECT_FLOAT_EQ(border.color.red, theme::CurrentTheme().inputBorder.red);
        EXPECT_FLOAT_EQ(border.color.green, theme::CurrentTheme().inputBorder.green);
        EXPECT_FLOAT_EQ(border.color.blue, theme::CurrentTheme().inputBorder.blue);
    }

    Registry::EmplaceOrReplace<components::FocusedTag>(textEdit);
    triggerThemeUpdate();
    {
        const auto& border = Registry::Get<components::Border>(textEdit);
        EXPECT_FLOAT_EQ(border.color.red, theme::CurrentTheme().focusBorderColor.red);
        EXPECT_FLOAT_EQ(border.color.green, theme::CurrentTheme().focusBorderColor.green);
        EXPECT_FLOAT_EQ(border.color.blue, theme::CurrentTheme().focusBorderColor.blue);
    }

    Registry::Remove<components::FocusedTag>(textEdit);
    triggerThemeUpdate();
    {
        const auto& border = Registry::Get<components::Border>(textEdit);
        EXPECT_FLOAT_EQ(border.color.red, theme::CurrentTheme().inputBorder.red);
        EXPECT_FLOAT_EQ(border.color.green, theme::CurrentTheme().inputBorder.green);
        EXPECT_FLOAT_EQ(border.color.blue, theme::CurrentTheme().inputBorder.blue);
    }
}

TEST_F(ThemeSystemTest, SetThemeReappliesThemeOwnedControlRadii)
{
    const auto button = factory::CreateButton("Theme", "theme_button_radius_reapply");
    const auto textEdit = factory::CreateTextEdit("placeholder", false, "theme_text_edit_radius_reapply");
    const auto dropDown = factory::CreateDropDown({"A", "B"}, 0, "theme_dropdown_radius_reapply");

    triggerThemeUpdate();

    {
        const auto& buttonBackground = Registry::Get<components::Background>(button);
        const auto& buttonBorder = Registry::Get<components::Border>(button);
        const auto& textEditBorder = Registry::Get<components::Border>(textEdit);
        const auto& dropDownBackground = Registry::Get<components::Background>(dropDown);
        const auto& dropDownBorder = Registry::Get<components::Border>(dropDown);
        EXPECT_FLOAT_EQ(buttonBackground.borderRadius.x(), theme::CurrentTheme().primaryButtonRadius.x());
        EXPECT_FLOAT_EQ(buttonBorder.thickness, theme::CurrentTheme().primaryButtonBorderThickness);
        EXPECT_FLOAT_EQ(textEditBorder.borderRadius.x(), theme::CurrentTheme().inputControlRadius.x());
        EXPECT_FLOAT_EQ(textEditBorder.thickness, theme::CurrentTheme().inputBorderThickness);
        EXPECT_FLOAT_EQ(dropDownBackground.borderRadius.x(), theme::CurrentTheme().inputControlRadius.x());
        EXPECT_FLOAT_EQ(dropDownBorder.thickness, theme::CurrentTheme().inputBorderThickness);
    }

    auto newTheme = theme::DefaultDarkTheme();
    newTheme.primaryButtonRadius = Vec4{12.0F, 12.0F, 12.0F, 12.0F};
    newTheme.primaryButtonBorderThickness = 1.75F;
    newTheme.inputControlRadius = Vec4{9.0F, 9.0F, 9.0F, 9.0F};
    newTheme.inputBorderThickness = 2.25F;
    theme::SetTheme(newTheme);

    triggerThemeUpdate();

    {
        const auto& buttonBackground = Registry::Get<components::Background>(button);
        const auto& buttonBorder = Registry::Get<components::Border>(button);
        const auto& textEditBackground = Registry::Get<components::Background>(textEdit);
        const auto& textEditBorder = Registry::Get<components::Border>(textEdit);
        const auto& dropDownBackground = Registry::Get<components::Background>(dropDown);
        const auto& dropDownBorder = Registry::Get<components::Border>(dropDown);
        EXPECT_FLOAT_EQ(buttonBackground.borderRadius.x(), 12.0F);
        EXPECT_FLOAT_EQ(buttonBorder.borderRadius.x(), 12.0F);
        EXPECT_FLOAT_EQ(buttonBorder.thickness, 1.75F);
        EXPECT_FLOAT_EQ(textEditBackground.borderRadius.x(), 9.0F);
        EXPECT_FLOAT_EQ(textEditBorder.borderRadius.x(), 9.0F);
        EXPECT_FLOAT_EQ(textEditBorder.thickness, 2.25F);
        EXPECT_FLOAT_EQ(dropDownBackground.borderRadius.x(), 9.0F);
        EXPECT_FLOAT_EQ(dropDownBorder.borderRadius.x(), 9.0F);
        EXPECT_FLOAT_EQ(dropDownBorder.thickness, 2.25F);
    }
}

TEST_F(ThemeSystemTest, DropDownHoverActiveAndDisabledUseThemeStateColors)
{
    const auto dropDown = factory::CreateDropDown({"A", "B"}, 0, "theme_dropdown_states");

    triggerThemeUpdate();
    {
        const auto& background = Registry::Get<components::Background>(dropDown);
        const auto& dropDownData = Registry::Get<components::DropDown>(dropDown);
        EXPECT_FLOAT_EQ(background.color.red, theme::CurrentTheme().inputBackground.red);
        EXPECT_FLOAT_EQ(dropDownData.arrowColor.red, theme::CurrentTheme().dropDownArrow.red);
    }

    Registry::EmplaceOrReplace<components::HoveredTag>(dropDown);
    triggerThemeUpdate();
    {
        const auto& background = Registry::Get<components::Background>(dropDown);
        const auto& dropDownData = Registry::Get<components::DropDown>(dropDown);
        EXPECT_FLOAT_EQ(background.color.red, theme::CurrentTheme().inputBackgroundHover.red);
        EXPECT_FLOAT_EQ(dropDownData.arrowColor.red, theme::CurrentTheme().dropDownArrowHover.red);
    }

    Registry::EmplaceOrReplace<components::ActiveTag>(dropDown);
    triggerThemeUpdate();
    {
        const auto& background = Registry::Get<components::Background>(dropDown);
        const auto& dropDownData = Registry::Get<components::DropDown>(dropDown);
        EXPECT_FLOAT_EQ(background.color.red, theme::CurrentTheme().inputBackgroundActive.red);
        EXPECT_FLOAT_EQ(dropDownData.arrowColor.red, theme::CurrentTheme().dropDownArrowActive.red);
    }

    Registry::Remove<components::HoveredTag>(dropDown);
    Registry::Remove<components::ActiveTag>(dropDown);
    Registry::EmplaceOrReplace<components::DisabledTag>(dropDown);
    triggerThemeUpdate();
    {
        const auto& background = Registry::Get<components::Background>(dropDown);
        const auto& border = Registry::Get<components::Border>(dropDown);
        const auto& text = Registry::Get<components::Text>(dropDown);
        const auto& dropDownData = Registry::Get<components::DropDown>(dropDown);
        EXPECT_FLOAT_EQ(background.color.red, theme::CurrentTheme().inputBackgroundDisabled.red);
        EXPECT_FLOAT_EQ(border.color.red, theme::CurrentTheme().inputBorderDisabled.red);
        EXPECT_FLOAT_EQ(text.color.red, theme::CurrentTheme().textDisabled.red);
        EXPECT_FLOAT_EQ(dropDownData.arrowColor.red, theme::CurrentTheme().dropDownArrowDisabled.red);
    }
}

TEST_F(ThemeSystemTest, CheckBoxHoverActiveAndDisabledUseThemeStateColors)
{
    const auto checkBox = factory::CreateCheckBox("Theme", true, "theme_checkbox_states");

    triggerThemeUpdate();
    {
        const auto& checkBoxData = Registry::Get<components::CheckBox>(checkBox);
        EXPECT_FLOAT_EQ(checkBoxData.boxColor.red, theme::CurrentTheme().surfaceBackground.red);
        EXPECT_FLOAT_EQ(checkBoxData.checkColor.red, theme::CurrentTheme().accent.red);
    }

    Registry::EmplaceOrReplace<components::HoveredTag>(checkBox);
    triggerThemeUpdate();
    {
        const auto& checkBoxData = Registry::Get<components::CheckBox>(checkBox);
        EXPECT_FLOAT_EQ(checkBoxData.boxColor.red, theme::CurrentTheme().checkBoxBoxHover.red);
    }

    Registry::EmplaceOrReplace<components::ActiveTag>(checkBox);
    triggerThemeUpdate();
    {
        const auto& checkBoxData = Registry::Get<components::CheckBox>(checkBox);
        EXPECT_FLOAT_EQ(checkBoxData.boxColor.red, theme::CurrentTheme().checkBoxBoxActive.red);
    }

    Registry::Remove<components::HoveredTag>(checkBox);
    Registry::Remove<components::ActiveTag>(checkBox);
    Registry::EmplaceOrReplace<components::DisabledTag>(checkBox);
    triggerThemeUpdate();
    {
        const auto& checkBoxData = Registry::Get<components::CheckBox>(checkBox);
        const auto& text = Registry::Get<components::Text>(checkBox);
        EXPECT_FLOAT_EQ(checkBoxData.boxColor.red, theme::CurrentTheme().checkBoxBoxDisabled.red);
        EXPECT_FLOAT_EQ(checkBoxData.checkColor.red, theme::CurrentTheme().accentDisabled.red);
        EXPECT_FLOAT_EQ(text.color.red, theme::CurrentTheme().textDisabled.red);
    }
}

TEST_F(ThemeSystemTest, ButtonDisabledUsesDisabledThemeColors)
{
    const auto button = factory::CreateButton("Theme", "theme_button_disabled");

    triggerThemeUpdate();
    Registry::EmplaceOrReplace<components::DisabledTag>(button);
    triggerThemeUpdate();

    const auto& background = Registry::Get<components::Background>(button);
    const auto& border = Registry::Get<components::Border>(button);
    const auto& text = Registry::Get<components::Text>(button);

    EXPECT_FLOAT_EQ(background.color.red, theme::CurrentTheme().primaryButtonBackgroundDisabled.red);
    EXPECT_FLOAT_EQ(border.color.red, theme::CurrentTheme().disabledBorder.red);
    EXPECT_FLOAT_EQ(text.color.red, theme::CurrentTheme().textDisabled.red);
}

TEST_F(ThemeSystemTest, DropDownPopupItemsUseThemeSelectedAndHoverStates)
{
    const auto owner = factory::CreateDropDown({"A", "B", "C"}, 0, "theme_dropdown_popup_owner");
    const auto popupPanel = factory::CreateBaseWidget("theme_dropdown_popup_panel");
    Registry::Emplace<components::DropDownPopupPanel>(popupPanel).owner = owner;
    Registry::Emplace<components::LayoutInfo>(popupPanel).direction = policies::LayoutDirection::VERTICAL;

    const auto selectedItem = factory::CreateBaseWidget("theme_dropdown_popup_item_selected");
    Registry::Emplace<components::DropDownPopupItem>(selectedItem, owner, 0);
    Registry::Emplace<components::Text>(selectedItem).content = "A";

    const auto hoveredItem = factory::CreateBaseWidget("theme_dropdown_popup_item_hovered");
    Registry::Emplace<components::DropDownPopupItem>(hoveredItem, owner, 1);
    Registry::Emplace<components::Text>(hoveredItem).content = "B";

    const auto idleItem = factory::CreateBaseWidget("theme_dropdown_popup_item_idle");
    Registry::Emplace<components::DropDownPopupItem>(idleItem, owner, 2);
    Registry::Emplace<components::Text>(idleItem).content = "C";

    hierarchy::AddChild(popupPanel, selectedItem);
    hierarchy::AddChild(popupPanel, hoveredItem);
    hierarchy::AddChild(popupPanel, idleItem);

    triggerThemeUpdate();

    const auto& popupBackground = Registry::Get<components::Background>(popupPanel);
    const auto& popupBorder = Registry::Get<components::Border>(popupPanel);
    EXPECT_FLOAT_EQ(popupBackground.color.red, theme::CurrentTheme().popupBackground.red);
    EXPECT_FLOAT_EQ(popupBorder.color.red, theme::CurrentTheme().popupBorder.red);

    const auto children = popupChildren(popupPanel);
    ASSERT_EQ(children.size(), 3U);

    {
        const entt::entity selectedChild = children.front();
        ASSERT_TRUE(Registry::AllOf<components::DropDownPopupItem>(selectedChild));
        const auto& background = Registry::Get<components::Background>(selectedChild);
        const auto& text = Registry::Get<components::Text>(selectedChild);
        EXPECT_FLOAT_EQ(background.color.red, theme::CurrentTheme().popupItemBackgroundSelected.red);
        EXPECT_FLOAT_EQ(text.color.red, theme::CurrentTheme().popupItemTextSelected.red);
    }

    Registry::EmplaceOrReplace<components::HoveredTag>(hoveredItem);
    triggerThemeUpdate();
    {
        const auto& background = Registry::Get<components::Background>(children.at(1));
        const auto& text = Registry::Get<components::Text>(children.at(1));
        EXPECT_FLOAT_EQ(background.color.red, theme::CurrentTheme().popupItemBackgroundHover.red);
        EXPECT_FLOAT_EQ(text.color.red, theme::CurrentTheme().popupItemText.red);
    }

    Registry::Remove<components::HoveredTag>(hoveredItem);
    Registry::EmplaceOrReplace<components::ActiveTag>(hoveredItem);
    triggerThemeUpdate();
    {
        const auto& background = Registry::Get<components::Background>(children.at(1));
        const auto& text = Registry::Get<components::Text>(children.at(1));
        EXPECT_FLOAT_EQ(background.color.red, theme::CurrentTheme().popupItemBackgroundActive.red);
        EXPECT_FLOAT_EQ(text.color.red, theme::CurrentTheme().popupItemText.red);
    }

    Registry::Remove<components::ActiveTag>(hoveredItem);
    Registry::Get<components::DropDown>(owner).selectedIndex = 1;
    triggerThemeUpdate();
    {
        const auto& oldSelectedBackground = Registry::Get<components::Background>(children.front());
        const auto& oldSelectedText = Registry::Get<components::Text>(children.front());
        EXPECT_FLOAT_EQ(oldSelectedBackground.color.red, theme::CurrentTheme().popupItemBackground.red);
        EXPECT_FLOAT_EQ(oldSelectedText.color.red, theme::CurrentTheme().popupItemText.red);

        const auto& newSelectedBackground = Registry::Get<components::Background>(children.at(1));
        const auto& newSelectedText = Registry::Get<components::Text>(children.at(1));
        EXPECT_FLOAT_EQ(newSelectedBackground.color.red, theme::CurrentTheme().popupItemBackgroundSelected.red);
        EXPECT_FLOAT_EQ(newSelectedText.color.red, theme::CurrentTheme().popupItemTextSelected.red);
    }
}

TEST_F(ThemeSystemTest, DropDownPopupGeometryReappliesThemeOwnedRadii)
{
    const auto owner = factory::CreateDropDown({"A", "B"}, 0, "theme_dropdown_popup_geometry_owner");
    const auto popupPanel = factory::CreateBaseWidget("theme_dropdown_popup_geometry_panel");
    Registry::Emplace<components::DropDownPopupPanel>(popupPanel).owner = owner;

    const auto popupItem = factory::CreateBaseWidget("theme_dropdown_popup_geometry_item");
    Registry::Emplace<components::DropDownPopupItem>(popupItem, owner, 0);
    Registry::Emplace<components::Text>(popupItem).content = "A";
    hierarchy::AddChild(popupPanel, popupItem);

    triggerThemeUpdate();

    {
        const auto& panelBackground = Registry::Get<components::Background>(popupPanel);
        const auto& panelBorder = Registry::Get<components::Border>(popupPanel);
        const auto& itemBackground = Registry::Get<components::Background>(popupItem);
        EXPECT_FLOAT_EQ(panelBackground.borderRadius.x(), theme::CurrentTheme().popupPanelRadius.x());
        EXPECT_FLOAT_EQ(panelBorder.borderRadius.x(), theme::CurrentTheme().popupPanelRadius.x());
        EXPECT_FLOAT_EQ(panelBorder.thickness, theme::CurrentTheme().popupBorderThickness);
        EXPECT_FLOAT_EQ(itemBackground.borderRadius.x(), theme::CurrentTheme().popupItemRadius.x());
    }

    auto newTheme = theme::DefaultDarkTheme();
    newTheme.popupPanelRadius = Vec4{10.0F, 10.0F, 10.0F, 10.0F};
    newTheme.popupBorderThickness = 2.5F;
    newTheme.popupItemRadius = Vec4{4.0F, 4.0F, 4.0F, 4.0F};
    theme::SetTheme(newTheme);

    triggerThemeUpdate();

    {
        const auto& panelBackground = Registry::Get<components::Background>(popupPanel);
        const auto& panelBorder = Registry::Get<components::Border>(popupPanel);
        const auto& itemBackground = Registry::Get<components::Background>(popupItem);
        EXPECT_FLOAT_EQ(panelBackground.borderRadius.x(), 10.0F);
        EXPECT_FLOAT_EQ(panelBorder.borderRadius.x(), 10.0F);
        EXPECT_FLOAT_EQ(panelBorder.thickness, 2.5F);
        EXPECT_FLOAT_EQ(itemBackground.borderRadius.x(), 4.0F);
    }
}

TEST_F(ThemeSystemTest, WindowGeometryReappliesThemeOwnedRadius)
{
    const auto window = factory::CreateBaseWidget("theme_window_radius_reapply");
    Registry::Emplace<components::WindowTag>(window);

    const auto dialog = factory::CreateBaseWidget("theme_dialog_radius_reapply");
    Registry::Emplace<components::DialogTag>(dialog);

    triggerThemeUpdate();

    {
        const auto& windowBackground = Registry::Get<components::Background>(window);
        const auto& windowBorder = Registry::Get<components::Border>(window);
        const auto& dialogBorder = Registry::Get<components::Border>(dialog);
        EXPECT_FLOAT_EQ(windowBackground.borderRadius.x(), theme::CurrentTheme().windowPanelRadius.x());
        EXPECT_FLOAT_EQ(windowBorder.thickness, theme::CurrentTheme().windowBorderThickness);
        EXPECT_FLOAT_EQ(dialogBorder.borderRadius.x(), theme::CurrentTheme().windowPanelRadius.x());
        EXPECT_FLOAT_EQ(dialogBorder.thickness, theme::CurrentTheme().windowBorderThickness);
    }

    auto newTheme = theme::DefaultDarkTheme();
    newTheme.windowPanelRadius = Vec4{14.0F, 14.0F, 14.0F, 14.0F};
    newTheme.windowBorderThickness = 3.0F;
    theme::SetTheme(newTheme);

    triggerThemeUpdate();

    {
        const auto& windowBackground = Registry::Get<components::Background>(window);
        const auto& windowBorder = Registry::Get<components::Border>(window);
        const auto& dialogBackground = Registry::Get<components::Background>(dialog);
        const auto& dialogBorder = Registry::Get<components::Border>(dialog);
        EXPECT_FLOAT_EQ(windowBackground.borderRadius.x(), 14.0F);
        EXPECT_FLOAT_EQ(windowBorder.borderRadius.x(), 14.0F);
        EXPECT_FLOAT_EQ(windowBorder.thickness, 3.0F);
        EXPECT_FLOAT_EQ(dialogBackground.borderRadius.x(), 14.0F);
        EXPECT_FLOAT_EQ(dialogBorder.borderRadius.x(), 14.0F);
        EXPECT_FLOAT_EQ(dialogBorder.thickness, 3.0F);
    }
}

} // namespace
} // namespace ui::tests