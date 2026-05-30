/**
 * ************************************************************************
 *
 * @file ThemeSystem.cpp
 * @brief ThemeSystem 实现 — 辅助方法从头文件移至此处，减少编译依赖。
 *
 * ************************************************************************
 */
#include "ThemeSystem.hpp"

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <vector>

#include "api/Theme.hpp"
#include "api/Utils.hpp"
#include "common/Events.hpp"
#include "common/Tags.hpp"
#include "common/components/Data.hpp"
#include "common/components/Visual.hpp"
#include "core/RuntimeFacade.hpp"

namespace ui::systems
{

namespace
{
// =====================================================================
// 内部辅助函数（原 private static 成员，移至匿名命名空间）
// =====================================================================

constexpr float FLOAT_EPSILON = 0.0001F;

bool NearlyEqual(float lhs, float rhs)
{
    return std::abs(lhs - rhs) <= FLOAT_EPSILON;
}

bool SameColor(const Color& lhs, const Color& rhs)
{
    return NearlyEqual(lhs.red, rhs.red) && NearlyEqual(lhs.green, rhs.green) && NearlyEqual(lhs.blue, rhs.blue)
        && NearlyEqual(lhs.alpha, rhs.alpha);
}

bool SameRadii(const Vec4& lhs, const Vec4& rhs)
{
    return NearlyEqual(lhs.x(), rhs.x()) && NearlyEqual(lhs.y(), rhs.y()) && NearlyEqual(lhs.z(), rhs.z())
        && NearlyEqual(lhs.w(), rhs.w());
}

bool CanUpdateManagedRadius(const Vec4& currentRadius,
                            const Vec4& desiredRadius,
                            const std::optional<Vec4>& managedRadius)
{
    return SameRadii(currentRadius, Vec4{0.0F, 0.0F, 0.0F, 0.0F}) || SameRadii(currentRadius, desiredRadius)
        || (managedRadius.has_value() && SameRadii(currentRadius, *managedRadius));
}

bool ColorMatchesAny(const Color& color, std::initializer_list<Color> candidates)
{
    return std::ranges::any_of(candidates, [&color](const Color& candidate) { return SameColor(color, candidate); });
}

bool IsDefaultTextColor(const Color& color)
{
    return SameColor(color, Color::White());
}

bool IsDefaultCheckColor(const Color& color)
{
    return SameColor(color, Color{0.20F, 0.60F, 1.0F, 1.0F});
}

bool IsDefaultCheckBoxColor(const Color& color)
{
    return SameColor(color, Color{0.25F, 0.25F, 0.30F, 1.0F});
}

bool IsDefaultDropDownArrowColor(const Color& color)
{
    return SameColor(color, Color{0.70F, 0.70F, 0.75F, 1.0F});
}

bool IsDefaultSliderTrackColor(const Color& color)
{
    return SameColor(color, Color{0.28F, 0.28F, 0.28F, 1.0F});
}

bool IsDefaultSliderFillColor(const Color& color)
{
    return SameColor(color, Color{0.20F, 0.60F, 1.00F, 1.0F});
}

bool IsDefaultSliderThumbColor(const Color& color)
{
    return SameColor(color, Color{0.40F, 0.75F, 1.00F, 1.0F});
}

bool IsDefaultProgressFillColor(const Color& color)
{
    return SameColor(color, Color{0.2F, 0.6F, 1.0F, 1.0F});
}

bool IsDefaultProgressBackgroundColor(const Color& color)
{
    return SameColor(color, Color{0.3F, 0.3F, 0.3F, 1.0F});
}

bool ApplyBackground(
    Registry& reg, entt::entity entity, const Color& nextColor, const Color& previousColor, const Vec4& radius)
{
    const auto* styleState = reg.try_get<components::ThemeStyleState>(entity);
    auto* background = reg.try_get<components::Background>(entity);
    if (background == nullptr)
    {
        auto& created = reg.emplace<components::Background>(entity);
        created.color = nextColor;
        created.borderRadius = radius;
        created.enabled = policies::Feature::ENABLED;
        return true;
    }

    if ((background->enabled != policies::Feature::ENABLED && SameColor(background->color, Color::Transparent()))
        || SameColor(background->color, previousColor))
    {
        background->color = nextColor;
        if (CanUpdateManagedRadius(background->borderRadius,
                                   radius,
                                   styleState != nullptr ? styleState->managedBackgroundRadius : std::optional<Vec4>{}))
        {
            background->borderRadius = radius;
        }
        background->enabled = policies::Feature::ENABLED;
        return true;
    }

    return false;
}

bool ApplyBorder(Registry& reg,
                 entt::entity entity,
                 const Color& nextColor,
                 const Color& previousColor,
                 const Vec4& radius,
                 float thickness = 1.0F)
{
    const auto* styleState = reg.try_get<components::ThemeStyleState>(entity);
    auto* border = reg.try_get<components::Border>(entity);
    if (border == nullptr)
    {
        auto& created = reg.emplace<components::Border>(entity);
        created.color = nextColor;
        created.thickness = thickness;
        created.borderRadius = radius;
        created.enabled = policies::Feature::ENABLED;
        return true;
    }

    if ((border->enabled != policies::Feature::ENABLED && SameColor(border->color, Color::White()))
        || SameColor(border->color, previousColor))
    {
        border->color = nextColor;
        border->thickness = thickness;
        if (CanUpdateManagedRadius(border->borderRadius,
                                   radius,
                                   styleState != nullptr ? styleState->managedBorderRadius : std::optional<Vec4>{}))
        {
            border->borderRadius = radius;
        }
        border->enabled = policies::Feature::ENABLED;
        return true;
    }

    return false;
}

bool ApplyTextColor(Registry& reg, entt::entity entity, const Color& nextColor, const Color& previousColor)
{
    auto* text = reg.try_get<components::Text>(entity);
    if (text == nullptr)
    {
        return false;
    }

    if (IsDefaultTextColor(text->color) || SameColor(text->color, previousColor))
    {
        text->color = nextColor;
        return true;
    }

    return false;
}

bool ApplyTextEditColor(Registry& reg, entt::entity entity, const Color& nextColor, const Color& previousColor)
{
    auto* textEdit = reg.try_get<components::TextEdit>(entity);
    if (textEdit == nullptr)
    {
        return false;
    }

    if (IsDefaultTextColor(textEdit->textColor) || SameColor(textEdit->textColor, previousColor))
    {
        textEdit->textColor = nextColor;
        return true;
    }

    return false;
}

bool ApplyDropDownArrowColor(Registry& reg,
                             entt::entity entity,
                             const Color& nextColor,
                             const Color& previousColor)
{
    auto* dropDown = reg.try_get<components::DropDown>(entity);
    if (dropDown == nullptr)
    {
        return false;
    }

    if (IsDefaultDropDownArrowColor(dropDown->arrowColor) || SameColor(dropDown->arrowColor, previousColor))
    {
        dropDown->arrowColor = nextColor;
        return true;
    }

    return false;
}

bool ApplyCheckBoxColors(Registry& reg,
                         entt::entity entity,
                         const theme::ThemePalette& nextTheme,
                         const theme::ThemePalette& previousTheme)
{
    auto* checkBox = reg.try_get<components::CheckBox>(entity);
    if (checkBox == nullptr)
    {
        return false;
    }

    bool changed = false;
    if (IsDefaultCheckColor(checkBox->checkColor) || SameColor(checkBox->checkColor, previousTheme.accent))
    {
        checkBox->checkColor = nextTheme.accent;
        changed = true;
    }
    if (IsDefaultCheckBoxColor(checkBox->boxColor) || SameColor(checkBox->boxColor, previousTheme.surfaceBackground))
    {
        checkBox->boxColor = nextTheme.surfaceBackground;
        changed = true;
    }
    return changed;
}

bool ApplySliderColors(Registry& reg,
                       entt::entity entity,
                       const theme::ThemePalette& nextTheme,
                       const theme::ThemePalette& previousTheme)
{
    auto* slider = reg.try_get<components::SliderInfo>(entity);
    if (slider == nullptr)
    {
        return false;
    }

    bool changed = false;
    if (IsDefaultSliderTrackColor(slider->trackColor) || SameColor(slider->trackColor, previousTheme.surfaceBackground))
    {
        slider->trackColor = nextTheme.surfaceBackground;
        changed = true;
    }
    if (IsDefaultSliderFillColor(slider->fillColor) || SameColor(slider->fillColor, previousTheme.accent))
    {
        slider->fillColor = nextTheme.accent;
        changed = true;
    }
    if (IsDefaultSliderThumbColor(slider->thumbColor) || SameColor(slider->thumbColor, previousTheme.accentMuted))
    {
        slider->thumbColor = nextTheme.accentMuted;
        changed = true;
    }
    return changed;
}

bool ApplyProgressBarColors(Registry& reg,
                            entt::entity entity,
                            const theme::ThemePalette& nextTheme,
                            const theme::ThemePalette& previousTheme)
{
    auto* progressBar = reg.try_get<components::ProgressBar>(entity);
    if (progressBar == nullptr)
    {
        return false;
    }

    bool changed = false;
    if (IsDefaultProgressFillColor(progressBar->fillColor) || SameColor(progressBar->fillColor, previousTheme.accent))
    {
        progressBar->fillColor = nextTheme.accent;
        changed = true;
    }
    if (IsDefaultProgressBackgroundColor(progressBar->backgroundColor)
        || SameColor(progressBar->backgroundColor, previousTheme.surfaceBackground))
    {
        progressBar->backgroundColor = nextTheme.surfaceBackground;
        changed = true;
    }
    return changed;
}

void ClearThemedTags(Registry& reg)
{
    std::vector<entt::entity> themedEntities;
    auto themedView = reg.view<components::ThemedTag>();
    for (const entt::entity entity : themedView)
    {
        themedEntities.push_back(entity);
    }

    for (const entt::entity entity : themedEntities)
    {
        if (reg.valid(entity))
        {
            reg.remove<components::ThemedTag>(entity);
        }
    }
}

Color ResolveFocusedBorderColor(
    bool disabled, bool focused, const Color& normalColor, const Color& focusedColor, const Color& disabledColor)
{
    if (disabled)
    {
        return disabledColor;
    }

    if (focused)
    {
        return focusedColor;
    }

    return normalColor;
}

void InitializeManagedButtonColors(Registry& reg,
                                   entt::entity entity,
                                   const theme::ThemePalette& theme,
                                   components::ThemeStyleState& styleState)
{
    if (const auto* background = reg.try_get<components::Background>(entity);
        background != nullptr
        && ColorMatchesAny(background->color,
                           {theme.primaryButtonBackground,
                            theme.primaryButtonBackgroundHover,
                            theme.primaryButtonBackgroundActive,
                            theme.primaryButtonBackgroundDisabled}))
    {
        styleState.managedBackgroundColor = background->color;
        styleState.managedBackgroundRadius = background->borderRadius;
    }

    if (const auto* border = reg.try_get<components::Border>(entity);
        border != nullptr
        && ColorMatchesAny(border->color, {theme.surfaceBorder, theme.focusBorderColor, theme.disabledBorder}))
    {
        styleState.managedBorderColor = border->color;
        styleState.managedBorderRadius = border->borderRadius;
    }

    if (const auto* text = reg.try_get<components::Text>(entity);
        text != nullptr && ColorMatchesAny(text->color, {theme.primaryButtonText, theme.textDisabled}))
    {
        styleState.managedTextColor = text->color;
    }
}

void InitializeManagedDropDownColors(Registry& reg,
                                     entt::entity entity,
                                     const theme::ThemePalette& theme,
                                     components::ThemeStyleState& styleState)
{
    if (const auto* background = reg.try_get<components::Background>(entity);
        background != nullptr
        && ColorMatchesAny(background->color,
                           {theme.inputBackground,
                            theme.inputBackgroundHover,
                            theme.inputBackgroundActive,
                            theme.inputBackgroundDisabled}))
    {
        styleState.managedBackgroundColor = background->color;
        styleState.managedBackgroundRadius = background->borderRadius;
    }

    if (const auto* border = reg.try_get<components::Border>(entity);
        border != nullptr
        && ColorMatchesAny(border->color, {theme.inputBorder, theme.focusBorderColor, theme.inputBorderDisabled}))
    {
        styleState.managedBorderColor = border->color;
        styleState.managedBorderRadius = border->borderRadius;
    }

    if (const auto* text = reg.try_get<components::Text>(entity);
        text != nullptr && ColorMatchesAny(text->color, {theme.inputText, theme.textDisabled}))
    {
        styleState.managedTextColor = text->color;
    }

    if (const auto* dropDown = reg.try_get<components::DropDown>(entity);
        dropDown != nullptr
        && ColorMatchesAny(
            dropDown->arrowColor,
            {theme.dropDownArrow, theme.dropDownArrowHover, theme.dropDownArrowActive, theme.dropDownArrowDisabled}))
    {
        styleState.managedIndicatorColor = dropDown->arrowColor;
    }
}

void InitializeManagedDropDownPopupPanelColors(Registry& reg,
                                               entt::entity entity,
                                               const theme::ThemePalette& theme,
                                               components::ThemeStyleState& styleState)
{
    if (const auto* background = reg.try_get<components::Background>(entity);
        background != nullptr && SameColor(background->color, theme.popupBackground))
    {
        styleState.managedBackgroundColor = background->color;
        styleState.managedBackgroundRadius = background->borderRadius;
    }

    if (const auto* border = reg.try_get<components::Border>(entity);
        border != nullptr && SameColor(border->color, theme.popupBorder))
    {
        styleState.managedBorderColor = border->color;
        styleState.managedBorderRadius = border->borderRadius;
    }
}

void InitializeManagedDropDownPopupItemColors(Registry& reg,
                                              entt::entity entity,
                                              const theme::ThemePalette& theme,
                                              components::ThemeStyleState& styleState)
{
    if (const auto* background = reg.try_get<components::Background>(entity);
        background != nullptr
        && ColorMatchesAny(background->color,
                           {theme.popupItemBackground,
                            theme.popupItemBackgroundHover,
                            theme.popupItemBackgroundActive,
                            theme.popupItemBackgroundSelected}))
    {
        styleState.managedBackgroundColor = background->color;
        styleState.managedBackgroundRadius = background->borderRadius;
    }

    if (const auto* text = reg.try_get<components::Text>(entity);
        text != nullptr && ColorMatchesAny(text->color, {theme.popupItemText, theme.popupItemTextSelected}))
    {
        styleState.managedTextColor = text->color;
    }
}

void InitializeManagedCheckBoxColors(Registry& reg,
                                     entt::entity entity,
                                     const theme::ThemePalette& theme,
                                     components::ThemeStyleState& styleState)
{
    if (const auto* text = reg.try_get<components::Text>(entity);
        text != nullptr && ColorMatchesAny(text->color, {theme.textPrimary, theme.textDisabled}))
    {
        styleState.managedTextColor = text->color;
    }

    if (const auto* checkBox = reg.try_get<components::CheckBox>(entity); checkBox != nullptr)
    {
        if (ColorMatchesAny(
                checkBox->boxColor,
                {theme.surfaceBackground, theme.checkBoxBoxHover, theme.checkBoxBoxActive, theme.checkBoxBoxDisabled}))
        {
            styleState.managedBackgroundColor = checkBox->boxColor;
        }

        if (ColorMatchesAny(checkBox->checkColor, {theme.accent, theme.accentDisabled}))
        {
            styleState.managedIndicatorColor = checkBox->checkColor;
        }
    }
}

void InitializeManagedInteractiveColors(Registry& reg, entt::entity entity, const theme::ThemePalette& theme)
{
    auto& styleState = reg.get_or_emplace<components::ThemeStyleState>(entity);

    if (reg.any_of<components::ButtonTag>(entity))
    {
        InitializeManagedButtonColors(reg, entity, theme, styleState);
    }

    if (reg.any_of<components::DropDownTag>(entity))
    {
        InitializeManagedDropDownColors(reg, entity, theme, styleState);
    }

    if (reg.any_of<components::TextEditTag>(entity))
    {
        if (const auto* background = reg.try_get<components::Background>(entity);
            background != nullptr
            && ColorMatchesAny(background->color,
                               {theme.inputBackground,
                                theme.inputBackgroundHover,
                                theme.inputBackgroundActive,
                                theme.inputBackgroundDisabled}))
        {
            styleState.managedBackgroundColor = background->color;
            styleState.managedBackgroundRadius = background->borderRadius;
        }

        if (const auto* border = reg.try_get<components::Border>(entity);
            border != nullptr
            && ColorMatchesAny(border->color, {theme.inputBorder, theme.focusBorderColor, theme.inputBorderDisabled}))
        {
            styleState.managedBorderColor = border->color;
            styleState.managedBorderRadius = border->borderRadius;
        }
    }

    if (reg.any_of<components::CheckBoxTag>(entity))
    {
        InitializeManagedCheckBoxColors(reg, entity, theme, styleState);
    }

    if (reg.any_of<components::DropDownPopupPanel>(entity))
    {
        InitializeManagedDropDownPopupPanelColors(reg, entity, theme, styleState);
    }

    if (reg.any_of<components::DropDownPopupItem>(entity))
    {
        InitializeManagedDropDownPopupItemColors(reg, entity, theme, styleState);
    }
}

bool UpdateManagedBackground(Registry& reg, entt::entity entity, const Color& desiredColor)
{
    auto* background = reg.try_get<components::Background>(entity);
    auto* styleState = reg.try_get<components::ThemeStyleState>(entity);
    if (background == nullptr || styleState == nullptr || !styleState->managedBackgroundColor.has_value())
    {
        return false;
    }

    if (!SameColor(background->color, *styleState->managedBackgroundColor)
        || SameColor(background->color, desiredColor))
    {
        return false;
    }

    background->color = desiredColor;
    background->enabled = policies::Feature::ENABLED;
    styleState->managedBackgroundColor = desiredColor;
    return true;
}

bool UpdateManagedBorder(Registry& reg, entt::entity entity, const Color& desiredColor)
{
    auto* border = reg.try_get<components::Border>(entity);
    auto* styleState = reg.try_get<components::ThemeStyleState>(entity);
    if (border == nullptr || styleState == nullptr || !styleState->managedBorderColor.has_value())
    {
        return false;
    }

    if (!SameColor(border->color, *styleState->managedBorderColor) || SameColor(border->color, desiredColor))
    {
        return false;
    }

    border->color = desiredColor;
    border->enabled = policies::Feature::ENABLED;
    styleState->managedBorderColor = desiredColor;
    return true;
}

bool UpdateManagedTextColor(Registry& reg, entt::entity entity, const Color& desiredColor)
{
    auto* text = reg.try_get<components::Text>(entity);
    auto* styleState = reg.try_get<components::ThemeStyleState>(entity);
    if (text == nullptr || styleState == nullptr || !styleState->managedTextColor.has_value())
    {
        return false;
    }

    if (!SameColor(text->color, *styleState->managedTextColor) || SameColor(text->color, desiredColor))
    {
        return false;
    }

    text->color = desiredColor;
    styleState->managedTextColor = desiredColor;
    return true;
}

bool UpdateManagedCheckBoxBoxColor(Registry& reg, entt::entity entity, const Color& desiredColor)
{
    auto* checkBox = reg.try_get<components::CheckBox>(entity);
    auto* styleState = reg.try_get<components::ThemeStyleState>(entity);
    if (checkBox == nullptr || styleState == nullptr || !styleState->managedBackgroundColor.has_value())
    {
        return false;
    }

    if (!SameColor(checkBox->boxColor, *styleState->managedBackgroundColor)
        || SameColor(checkBox->boxColor, desiredColor))
    {
        return false;
    }

    checkBox->boxColor = desiredColor;
    styleState->managedBackgroundColor = desiredColor;
    return true;
}

bool UpdateManagedIndicatorColor(Registry& reg, entt::entity entity, const Color& desiredColor)
{
    auto* styleState = reg.try_get<components::ThemeStyleState>(entity);
    if (styleState == nullptr || !styleState->managedIndicatorColor.has_value())
    {
        return false;
    }

    if (auto* checkBox = reg.try_get<components::CheckBox>(entity); checkBox != nullptr)
    {
        if (!SameColor(checkBox->checkColor, *styleState->managedIndicatorColor)
            || SameColor(checkBox->checkColor, desiredColor))
        {
            return false;
        }

        checkBox->checkColor = desiredColor;
        styleState->managedIndicatorColor = desiredColor;
        return true;
    }

    if (auto* dropDown = reg.try_get<components::DropDown>(entity); dropDown != nullptr)
    {
        if (!SameColor(dropDown->arrowColor, *styleState->managedIndicatorColor)
            || SameColor(dropDown->arrowColor, desiredColor))
        {
            return false;
        }

        dropDown->arrowColor = desiredColor;
        styleState->managedIndicatorColor = desiredColor;
        return true;
    }

    return false;
}

bool IsDropDownPopupItemSelected(Registry& reg, const components::DropDownPopupItem& popupItem)
{
    const auto* ownerDropDown = reg.try_get<components::DropDown>(popupItem.owner);
    if (ownerDropDown == nullptr)
    {
        return false;
    }

    return ownerDropDown->selectedIndex == popupItem.optionIndex;
}

bool ApplyButtonInteractiveTheme(Registry& reg, entt::entity entity, const theme::ThemePalette& theme)
{
    const bool disabled = reg.any_of<components::DisabledTag>(entity);
    bool changed = false;

    Color desiredBackground = disabled ? theme.primaryButtonBackgroundDisabled : theme.primaryButtonBackground;
    if (!disabled)
    {
        if (reg.any_of<components::ActiveTag>(entity))
        {
            desiredBackground = theme.primaryButtonBackgroundActive;
        }
        else if (reg.any_of<components::HoveredTag>(entity))
        {
            desiredBackground = theme.primaryButtonBackgroundHover;
        }
    }

    changed |= UpdateManagedBackground(reg, entity, desiredBackground);
    changed |= UpdateManagedBorder(reg,
                                   entity,
                                   ResolveFocusedBorderColor(disabled,
                                                             reg.any_of<components::FocusedTag>(entity),
                                                             theme.surfaceBorder,
                                                             theme.focusBorderColor,
                                                             theme.disabledBorder));
    changed |= UpdateManagedTextColor(reg, entity, disabled ? theme.textDisabled : theme.primaryButtonText);
    return changed;
}

bool ApplyTextEditInteractiveTheme(Registry& reg, entt::entity entity, const theme::ThemePalette& theme)
{
    return UpdateManagedBorder(reg,
                               entity,
                               ResolveFocusedBorderColor(reg.any_of<components::DisabledTag>(entity),
                                                         reg.any_of<components::FocusedTag>(entity),
                                                         theme.inputBorder,
                                                         theme.focusBorderColor,
                                                         theme.inputBorderDisabled));
}

bool ApplyDropDownInteractiveTheme(Registry& reg, entt::entity entity, const theme::ThemePalette& theme)
{
    const bool disabled = reg.any_of<components::DisabledTag>(entity);
    bool changed = false;

    Color desiredBackground = disabled ? theme.inputBackgroundDisabled : theme.inputBackground;
    Color desiredArrow = disabled ? theme.dropDownArrowDisabled : theme.dropDownArrow;
    if (!disabled)
    {
        if (reg.any_of<components::ActiveTag>(entity))
        {
            desiredBackground = theme.inputBackgroundActive;
            desiredArrow = theme.dropDownArrowActive;
        }
        else if (reg.any_of<components::HoveredTag>(entity))
        {
            desiredBackground = theme.inputBackgroundHover;
            desiredArrow = theme.dropDownArrowHover;
        }
    }

    changed |= UpdateManagedBackground(reg, entity, desiredBackground);
    changed |= UpdateManagedBorder(reg,
                                   entity,
                                   ResolveFocusedBorderColor(disabled,
                                                             reg.any_of<components::FocusedTag>(entity),
                                                             theme.inputBorder,
                                                             theme.focusBorderColor,
                                                             theme.inputBorderDisabled));
    changed |= UpdateManagedTextColor(reg, entity, disabled ? theme.textDisabled : theme.inputText);
    changed |= UpdateManagedIndicatorColor(reg, entity, desiredArrow);
    return changed;
}

bool ApplyCheckBoxInteractiveTheme(Registry& reg, entt::entity entity, const theme::ThemePalette& theme)
{
    const bool disabled = reg.any_of<components::DisabledTag>(entity);
    bool changed = false;

    Color desiredBoxColor = disabled ? theme.checkBoxBoxDisabled : theme.surfaceBackground;
    if (!disabled)
    {
        if (reg.any_of<components::ActiveTag>(entity))
        {
            desiredBoxColor = theme.checkBoxBoxActive;
        }
        else if (reg.any_of<components::HoveredTag>(entity))
        {
            desiredBoxColor = theme.checkBoxBoxHover;
        }
    }

    changed |= UpdateManagedCheckBoxBoxColor(reg, entity, desiredBoxColor);
    changed |= UpdateManagedIndicatorColor(reg, entity, disabled ? theme.accentDisabled : theme.accent);
    changed |= UpdateManagedTextColor(reg, entity, disabled ? theme.textDisabled : theme.textPrimary);
    return changed;
}

bool ApplyDropDownPopupItemInteractiveTheme(Registry& reg, entt::entity entity, const theme::ThemePalette& theme)
{
    const auto* popupItem = reg.try_get<components::DropDownPopupItem>(entity);
    if (popupItem == nullptr)
    {
        return false;
    }

    Color desiredBackground = theme.popupItemBackground;
    if (IsDropDownPopupItemSelected(reg, *popupItem))
    {
        desiredBackground = theme.popupItemBackgroundSelected;
    }
    else if (reg.any_of<components::ActiveTag>(entity))
    {
        desiredBackground = theme.popupItemBackgroundActive;
    }
    else if (reg.any_of<components::HoveredTag>(entity))
    {
        desiredBackground = theme.popupItemBackgroundHover;
    }
    const Color desiredText =
        IsDropDownPopupItemSelected(reg, *popupItem) ? theme.popupItemTextSelected : theme.popupItemText;

    bool changed = false;
    changed |= UpdateManagedBackground(reg, entity, desiredBackground);
    changed |= UpdateManagedTextColor(reg, entity, desiredText);
    return changed;
}

bool ApplyInteractiveThemeToEntity(Registry& reg, entt::entity entity, const theme::ThemePalette& theme)
{
    bool changed = false;

    if (reg.any_of<components::ButtonTag>(entity))
    {
        changed |= ApplyButtonInteractiveTheme(reg, entity, theme);
    }

    if (reg.any_of<components::TextEditTag>(entity))
    {
        changed |= ApplyTextEditInteractiveTheme(reg, entity, theme);
    }

    if (reg.any_of<components::DropDownTag>(entity))
    {
        changed |= ApplyDropDownInteractiveTheme(reg, entity, theme);
    }

    if (reg.any_of<components::CheckBoxTag>(entity))
    {
        changed |= ApplyCheckBoxInteractiveTheme(reg, entity, theme);
    }

    if (reg.any_of<components::DropDownPopupItem>(entity))
    {
        changed |= ApplyDropDownPopupItemInteractiveTheme(reg, entity, theme);
    }

    return changed;
}

bool ApplyButtonTheme(Registry& reg,
                      entt::entity entity,
                      const theme::ThemePalette& nextTheme,
                      const theme::ThemePalette& previousTheme)
{
    bool changed = false;
    changed |= ApplyBackground(reg,
                               entity,
                               nextTheme.primaryButtonBackground,
                               previousTheme.primaryButtonBackground,
                               nextTheme.primaryButtonRadius);
    changed |= ApplyBorder(reg,
                           entity,
                           nextTheme.surfaceBorder,
                           previousTheme.surfaceBorder,
                           nextTheme.primaryButtonRadius,
                           nextTheme.primaryButtonBorderThickness);
    changed |= ApplyTextColor(reg, entity, nextTheme.primaryButtonText, previousTheme.primaryButtonText);
    return changed;
}

bool ApplyLabelTheme(Registry& reg,
                     entt::entity entity,
                     const theme::ThemePalette& nextTheme,
                     const theme::ThemePalette& previousTheme)
{
    return ApplyTextColor(reg, entity, nextTheme.textPrimary, previousTheme.textPrimary);
}

bool ApplyTextEditTheme(Registry& reg,
                        entt::entity entity,
                        const theme::ThemePalette& nextTheme,
                        const theme::ThemePalette& previousTheme)
{
    bool changed = false;
    changed |= ApplyBackground(
        reg, entity, nextTheme.inputBackground, previousTheme.inputBackground, nextTheme.inputControlRadius);
    changed |= ApplyBorder(reg,
                           entity,
                           nextTheme.inputBorder,
                           previousTheme.inputBorder,
                           nextTheme.inputControlRadius,
                           nextTheme.inputBorderThickness);
    changed |= ApplyTextColor(reg, entity, nextTheme.inputText, previousTheme.inputText);
    changed |= ApplyTextEditColor(reg, entity, nextTheme.inputText, previousTheme.inputText);
    return changed;
}

bool ApplyCheckBoxTheme(Registry& reg,
                        entt::entity entity,
                        const theme::ThemePalette& nextTheme,
                        const theme::ThemePalette& previousTheme)
{
    bool changed = false;
    changed |= ApplyTextColor(reg, entity, nextTheme.textPrimary, previousTheme.textPrimary);
    changed |= ApplyCheckBoxColors(reg, entity, nextTheme, previousTheme);
    return changed;
}

bool ApplyDropDownTheme(Registry& reg,
                        entt::entity entity,
                        const theme::ThemePalette& nextTheme,
                        const theme::ThemePalette& previousTheme)
{
    bool changed = false;
    changed |= ApplyBackground(
        reg, entity, nextTheme.inputBackground, previousTheme.inputBackground, nextTheme.inputControlRadius);
    changed |= ApplyBorder(reg,
                           entity,
                           nextTheme.inputBorder,
                           previousTheme.inputBorder,
                           nextTheme.inputControlRadius,
                           nextTheme.inputBorderThickness);
    changed |= ApplyTextColor(reg, entity, nextTheme.inputText, previousTheme.inputText);
    changed |= ApplyDropDownArrowColor(reg, entity, nextTheme.dropDownArrow, previousTheme.dropDownArrow);
    return changed;
}

bool ReapplyManagedBackgroundRadius(Registry& reg,
                                    entt::entity entity,
                                    const Color& expectedColor,
                                    const Vec4& previousRadius,
                                    const Vec4& nextRadius)
{
    auto* background = reg.try_get<components::Background>(entity);
    if (background == nullptr || !SameColor(background->color, expectedColor)
        || !SameRadii(background->borderRadius, previousRadius))
    {
        return false;
    }

    background->borderRadius = nextRadius;
    return true;
}

bool ReapplyManagedBorderRadius(Registry& reg,
                                entt::entity entity,
                                const Color& expectedColor,
                                const Vec4& previousRadius,
                                const Vec4& nextRadius)
{
    auto* border = reg.try_get<components::Border>(entity);
    if (border == nullptr || !SameColor(border->color, expectedColor)
        || !SameRadii(border->borderRadius, previousRadius))
    {
        return false;
    }

    border->borderRadius = nextRadius;
    return true;
}

bool ApplyDropDownPopupPanelTheme(Registry& reg,
                                  entt::entity entity,
                                  const theme::ThemePalette& nextTheme,
                                  const theme::ThemePalette& previousTheme)
{
    bool changed = false;
    changed |= ApplyBackground(
        reg, entity, nextTheme.popupBackground, previousTheme.popupBackground, nextTheme.popupPanelRadius);
    changed |= ApplyBorder(reg,
                           entity,
                           nextTheme.popupBorder,
                           previousTheme.popupBorder,
                           nextTheme.popupPanelRadius,
                           nextTheme.popupBorderThickness);

    changed |= ReapplyManagedBackgroundRadius(
        reg, entity, nextTheme.popupBackground, previousTheme.popupPanelRadius, nextTheme.popupPanelRadius);
    changed |= ReapplyManagedBorderRadius(
        reg, entity, nextTheme.popupBorder, previousTheme.popupPanelRadius, nextTheme.popupPanelRadius);

    return changed;
}

bool ApplyDropDownPopupItemTheme(Registry& reg,
                                 entt::entity entity,
                                 const theme::ThemePalette& nextTheme,
                                 const theme::ThemePalette& previousTheme)
{
    const auto* popupItem = reg.try_get<components::DropDownPopupItem>(entity);
    const Color initialBackground = (popupItem != nullptr && IsDropDownPopupItemSelected(reg, *popupItem))
                                      ? nextTheme.popupItemBackgroundSelected
                                      : nextTheme.popupItemBackground;
    const Color previousBackground = (popupItem != nullptr && IsDropDownPopupItemSelected(reg, *popupItem))
                                       ? previousTheme.popupItemBackgroundSelected
                                       : previousTheme.popupItemBackground;
    const Color initialText = (popupItem != nullptr && IsDropDownPopupItemSelected(reg, *popupItem))
                                ? nextTheme.popupItemTextSelected
                                : nextTheme.popupItemText;
    const Color previousText = (popupItem != nullptr && IsDropDownPopupItemSelected(reg, *popupItem))
                                 ? previousTheme.popupItemTextSelected
                                 : previousTheme.popupItemText;

    bool changed = false;
    changed |= ApplyBackground(reg, entity, initialBackground, previousBackground, nextTheme.popupItemRadius);
    changed |= ApplyTextColor(reg, entity, initialText, previousText);

    changed |= ReapplyManagedBackgroundRadius(
        reg, entity, initialBackground, previousTheme.popupItemRadius, nextTheme.popupItemRadius);

    return changed;
}

bool ApplySliderTheme(Registry& reg,
                      entt::entity entity,
                      const theme::ThemePalette& nextTheme,
                      const theme::ThemePalette& previousTheme)
{
    return ApplySliderColors(reg, entity, nextTheme, previousTheme);
}

bool ApplyProgressBarTheme(Registry& reg,
                           entt::entity entity,
                           const theme::ThemePalette& nextTheme,
                           const theme::ThemePalette& previousTheme)
{
    return ApplyProgressBarColors(reg, entity, nextTheme, previousTheme);
}

bool ApplyWindowTheme(Registry& reg,
                      entt::entity entity,
                      const theme::ThemePalette& nextTheme,
                      const theme::ThemePalette& previousTheme)
{
    bool changed = false;
    changed |= ApplyBackground(
        reg, entity, nextTheme.windowBackground, previousTheme.windowBackground, nextTheme.windowPanelRadius);
    changed |= ApplyBorder(reg,
                           entity,
                           nextTheme.surfaceBorder,
                           previousTheme.surfaceBorder,
                           nextTheme.windowPanelRadius,
                           nextTheme.windowBorderThickness);

    changed |= ReapplyManagedBackgroundRadius(
        reg, entity, nextTheme.windowBackground, previousTheme.windowPanelRadius, nextTheme.windowPanelRadius);
    changed |= ReapplyManagedBorderRadius(
        reg, entity, nextTheme.surfaceBorder, previousTheme.windowPanelRadius, nextTheme.windowPanelRadius);

    return changed;
}

bool ApplyThemeToEntity(Registry& reg,
                        entt::entity entity,
                        const theme::ThemePalette& nextTheme,
                        const theme::ThemePalette& previousTheme)
{
    bool changed = false;

    if (reg.any_of<components::ButtonTag>(entity))
    {
        changed |= ApplyButtonTheme(reg, entity, nextTheme, previousTheme);
    }

    if (reg.any_of<components::LabelTag>(entity))
    {
        changed |= ApplyLabelTheme(reg, entity, nextTheme, previousTheme);
    }

    if (reg.any_of<components::TextEditTag>(entity))
    {
        changed |= ApplyTextEditTheme(reg, entity, nextTheme, previousTheme);
    }

    if (reg.any_of<components::CheckBoxTag>(entity))
    {
        changed |= ApplyCheckBoxTheme(reg, entity, nextTheme, previousTheme);
    }

    if (reg.any_of<components::DropDownTag>(entity))
    {
        changed |= ApplyDropDownTheme(reg, entity, nextTheme, previousTheme);
    }

    if (reg.any_of<components::DropDownPopupPanel>(entity))
    {
        changed |= ApplyDropDownPopupPanelTheme(reg, entity, nextTheme, previousTheme);
    }

    if (reg.any_of<components::DropDownPopupItem>(entity))
    {
        changed |= ApplyDropDownPopupItemTheme(reg, entity, nextTheme, previousTheme);
    }

    if (reg.any_of<components::SliderTag>(entity))
    {
        changed |= ApplySliderTheme(reg, entity, nextTheme, previousTheme);
    }

    if (reg.any_of<components::ProgressBarTag>(entity))
    {
        changed |= ApplyProgressBarTheme(reg, entity, nextTheme, previousTheme);
    }

    if (reg.any_of<components::WindowTag>(entity) || reg.any_of<components::DialogTag>(entity))
    {
        changed |= ApplyWindowTheme(reg, entity, nextTheme, previousTheme);
    }

    InitializeManagedInteractiveColors(reg, entity, nextTheme);
    return changed;
}

} // anonymous namespace

// =====================================================================
// ThemeSystem 方法实现
// =====================================================================

void ThemeSystem::registerHandlersImpl()
{
    m_disp != nullptr
        ? m_disp->sink<events::UpdateEvent>().connect<&ThemeSystem::update>(*this)
        : RuntimeFacade::current().enttDispatcher().sink<events::UpdateEvent>().connect<&ThemeSystem::update>(*this);
}

void ThemeSystem::unregisterHandlersImpl()
{
    m_disp != nullptr
        ? m_disp->sink<events::UpdateEvent>().disconnect<&ThemeSystem::update>(*this)
        : RuntimeFacade::current().enttDispatcher().sink<events::UpdateEvent>().disconnect<&ThemeSystem::update>(*this);
}

ui::interface::SystemPhase ThemeSystem::getPhase()
{
    return ui::interface::SystemPhase::LOGIC;
}

void ThemeSystem::update()
{
    auto& reg = m_reg != nullptr ? *m_reg : RuntimeFacade::current().registry();
    auto& themeContext = RuntimeFacade::current().ensureContext<theme::ThemeContext>();

    if (themeContext.reapplyRequested)
    {
        ClearThemedTags(reg);
    }

    auto unthemedView = m_reg->view<components::BaseInfo>(entt::exclude<components::ThemedTag>);
    for (const entt::entity entity : unthemedView)
    {
        const bool changed = ApplyThemeToEntity(reg, entity, themeContext.palette, themeContext.previousPalette);
        reg.emplace_or_replace<components::ThemedTag>(entity);
        if (changed)
        {
            ui::utils::MarkVisualChanged(entity);
        }
    }

    auto themedView = reg.view<components::BaseInfo, components::ThemedTag>();
    for (const entt::entity entity : themedView)
    {
        if (ApplyInteractiveThemeToEntity(reg, entity, themeContext.palette))
        {
            ui::utils::MarkVisualChanged(entity);
        }
    }

    if (themeContext.reapplyRequested)
    {
        themeContext.previousPalette = themeContext.palette;
        themeContext.reapplyRequested = false;
    }
}

} // namespace ui::systems
