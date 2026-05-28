/**
 * ************************************************************************
 *
 * @file ThemeSystem.hpp
 * @brief 最小 ThemeSystem：为尚未主题化的实体补默认样式，并支持最小交互态主题更新。
 *
 * ************************************************************************
 */
#pragma once

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <vector>

#include "../api/Theme.hpp"
#include "../api/Utils.hpp"
#include "../common/Events.hpp"
#include "../common/Tags.hpp"
#include "../common/components/Data.hpp"
#include "../common/components/Visual.hpp"
#include "../core/RuntimeFacade.hpp"
#include "../interface/Isystem.hpp"

namespace ui::systems
{

class ThemeSystem : public ui::interface::EnableRegister<ThemeSystem>
{
public:
    void registerHandlersImpl()
    {
        RuntimeFacade::current().enttDispatcher().sink<events::UpdateEvent>().connect<&ThemeSystem::update>(*this);
    }

    void unregisterHandlersImpl()
    {
        RuntimeFacade::current().enttDispatcher().sink<events::UpdateEvent>().disconnect<&ThemeSystem::update>(*this);
    }

    ui::interface::SystemPhase getPhase() { return ui::interface::SystemPhase::Logic; }

private:
    static constexpr float FLOAT_EPSILON = 0.0001F;
    /**
     * @brief 比较两个浮点数是否近似相等
     * @param lhs 左操作数
     * @param rhs 右操作数
     * @return true 如果两个浮点数近似相等
     * @return false 如果两个浮点数不近似相等
     */
    static bool nearlyEqual(float lhs, float rhs) { return std::abs(lhs - rhs) <= FLOAT_EPSILON; }

    static bool sameColor(const Color& lhs, const Color& rhs)
    {
        return nearlyEqual(lhs.red, rhs.red) && nearlyEqual(lhs.green, rhs.green) && nearlyEqual(lhs.blue, rhs.blue)
            && nearlyEqual(lhs.alpha, rhs.alpha);
    }

    static bool sameRadii(const Vec4& lhs, const Vec4& rhs)
    {
        return nearlyEqual(lhs.x(), rhs.x()) && nearlyEqual(lhs.y(), rhs.y()) && nearlyEqual(lhs.z(), rhs.z())
            && nearlyEqual(lhs.w(), rhs.w());
    }

    static bool canUpdateManagedRadius(const Vec4& currentRadius,
                                       const Vec4& desiredRadius,
                                       const std::optional<Vec4>& managedRadius)
    {
        return sameRadii(currentRadius, Vec4{0.0F, 0.0F, 0.0F, 0.0F}) || sameRadii(currentRadius, desiredRadius)
            || (managedRadius.has_value() && sameRadii(currentRadius, *managedRadius));
    }

    static bool colorMatchesAny(const Color& color, std::initializer_list<Color> candidates)
    {
        return std::ranges::any_of(candidates,
                                   [&color](const Color& candidate) { return sameColor(color, candidate); });
    }

    static bool isDefaultTextColor(const Color& color) { return sameColor(color, Color::White()); }

    static bool isDefaultCheckColor(const Color& color) { return sameColor(color, Color{0.20F, 0.60F, 1.0F, 1.0F}); }

    static bool isDefaultCheckBoxColor(const Color& color)
    {
        return sameColor(color, Color{0.25F, 0.25F, 0.30F, 1.0F});
    }

    static bool isDefaultDropDownArrowColor(const Color& color)
    {
        return sameColor(color, Color{0.70F, 0.70F, 0.75F, 1.0F});
    }

    static bool isDefaultSliderTrackColor(const Color& color)
    {
        return sameColor(color, Color{0.28F, 0.28F, 0.28F, 1.0F});
    }

    static bool isDefaultSliderFillColor(const Color& color)
    {
        return sameColor(color, Color{0.20F, 0.60F, 1.00F, 1.0F});
    }

    static bool isDefaultSliderThumbColor(const Color& color)
    {
        return sameColor(color, Color{0.40F, 0.75F, 1.00F, 1.0F});
    }

    static bool isDefaultProgressFillColor(const Color& color)
    {
        return sameColor(color, Color{0.2F, 0.6F, 1.0F, 1.0F});
    }

    static bool isDefaultProgressBackgroundColor(const Color& color)
    {
        return sameColor(color, Color{0.3F, 0.3F, 0.3F, 1.0F});
    }

    static bool applyBackground(entt::registry& reg,
                                entt::entity entity,
                                const Color& nextColor,
                                const Color& previousColor,
                                const Vec4& radius)
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

        if ((background->enabled != policies::Feature::ENABLED && sameColor(background->color, Color::Transparent()))
            || sameColor(background->color, previousColor))
        {
            background->color = nextColor;
            if (canUpdateManagedRadius(background->borderRadius,
                                       radius,
                                       styleState != nullptr ? styleState->managedBackgroundRadius
                                                             : std::optional<Vec4>{}))
            {
                background->borderRadius = radius;
            }
            background->enabled = policies::Feature::ENABLED;
            return true;
        }

        return false;
    }

    static bool applyBorder(entt::registry& reg,
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

        if ((border->enabled != policies::Feature::ENABLED && sameColor(border->color, Color::White()))
            || sameColor(border->color, previousColor))
        {
            border->color = nextColor;
            border->thickness = thickness;
            if (canUpdateManagedRadius(border->borderRadius,
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

    static bool
        applyTextColor(entt::registry& reg, entt::entity entity, const Color& nextColor, const Color& previousColor)
    {
        auto* text = reg.try_get<components::Text>(entity);
        if (text == nullptr)
        {
            return false;
        }

        if (isDefaultTextColor(text->color) || sameColor(text->color, previousColor))
        {
            text->color = nextColor;
            return true;
        }

        return false;
    }

    static bool
        applyTextEditColor(entt::registry& reg, entt::entity entity, const Color& nextColor, const Color& previousColor)
    {
        auto* textEdit = reg.try_get<components::TextEdit>(entity);
        if (textEdit == nullptr)
        {
            return false;
        }

        if (isDefaultTextColor(textEdit->textColor) || sameColor(textEdit->textColor, previousColor))
        {
            textEdit->textColor = nextColor;
            return true;
        }

        return false;
    }

    static bool applyDropDownArrowColor(entt::registry& reg,
                                        entt::entity entity,
                                        const Color& nextColor,
                                        const Color& previousColor)
    {
        auto* dropDown = reg.try_get<components::DropDown>(entity);
        if (dropDown == nullptr)
        {
            return false;
        }

        if (isDefaultDropDownArrowColor(dropDown->arrowColor) || sameColor(dropDown->arrowColor, previousColor))
        {
            dropDown->arrowColor = nextColor;
            return true;
        }

        return false;
    }

    static bool applyCheckBoxColors(entt::registry& reg,
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
        if (isDefaultCheckColor(checkBox->checkColor) || sameColor(checkBox->checkColor, previousTheme.accent))
        {
            checkBox->checkColor = nextTheme.accent;
            changed = true;
        }
        if (isDefaultCheckBoxColor(checkBox->boxColor)
            || sameColor(checkBox->boxColor, previousTheme.surfaceBackground))
        {
            checkBox->boxColor = nextTheme.surfaceBackground;
            changed = true;
        }
        return changed;
    }

    static bool applySliderColors(entt::registry& reg,
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
        if (isDefaultSliderTrackColor(slider->trackColor)
            || sameColor(slider->trackColor, previousTheme.surfaceBackground))
        {
            slider->trackColor = nextTheme.surfaceBackground;
            changed = true;
        }
        if (isDefaultSliderFillColor(slider->fillColor) || sameColor(slider->fillColor, previousTheme.accent))
        {
            slider->fillColor = nextTheme.accent;
            changed = true;
        }
        if (isDefaultSliderThumbColor(slider->thumbColor) || sameColor(slider->thumbColor, previousTheme.accentMuted))
        {
            slider->thumbColor = nextTheme.accentMuted;
            changed = true;
        }
        return changed;
    }

    static bool applyProgressBarColors(entt::registry& reg,
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
        if (isDefaultProgressFillColor(progressBar->fillColor)
            || sameColor(progressBar->fillColor, previousTheme.accent))
        {
            progressBar->fillColor = nextTheme.accent;
            changed = true;
        }
        if (isDefaultProgressBackgroundColor(progressBar->backgroundColor)
            || sameColor(progressBar->backgroundColor, previousTheme.surfaceBackground))
        {
            progressBar->backgroundColor = nextTheme.surfaceBackground;
            changed = true;
        }
        return changed;
    }

    static void clearThemedTags(entt::registry& reg)
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

    static Color resolveFocusedBorderColor(
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

    static void initializeManagedButtonColors(entt::registry& reg,
                                              entt::entity entity,
                                              const theme::ThemePalette& theme,
                                              components::ThemeStyleState& styleState)
    {
        if (const auto* background = reg.try_get<components::Background>(entity);
            background != nullptr
            && colorMatchesAny(background->color,
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
            && colorMatchesAny(border->color, {theme.surfaceBorder, theme.focusBorderColor, theme.disabledBorder}))
        {
            styleState.managedBorderColor = border->color;
            styleState.managedBorderRadius = border->borderRadius;
        }

        if (const auto* text = reg.try_get<components::Text>(entity);
            text != nullptr && colorMatchesAny(text->color, {theme.primaryButtonText, theme.textDisabled}))
        {
            styleState.managedTextColor = text->color;
        }
    }

    static void initializeManagedDropDownColors(entt::registry& reg,
                                                entt::entity entity,
                                                const theme::ThemePalette& theme,
                                                components::ThemeStyleState& styleState)
    {
        if (const auto* background = reg.try_get<components::Background>(entity);
            background != nullptr
            && colorMatchesAny(background->color,
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
            && colorMatchesAny(border->color, {theme.inputBorder, theme.focusBorderColor, theme.inputBorderDisabled}))
        {
            styleState.managedBorderColor = border->color;
            styleState.managedBorderRadius = border->borderRadius;
        }

        if (const auto* text = reg.try_get<components::Text>(entity);
            text != nullptr && colorMatchesAny(text->color, {theme.inputText, theme.textDisabled}))
        {
            styleState.managedTextColor = text->color;
        }

        if (const auto* dropDown = reg.try_get<components::DropDown>(entity);
            dropDown != nullptr
            && colorMatchesAny(dropDown->arrowColor,
                               {theme.dropDownArrow,
                                theme.dropDownArrowHover,
                                theme.dropDownArrowActive,
                                theme.dropDownArrowDisabled}))
        {
            styleState.managedIndicatorColor = dropDown->arrowColor;
        }
    }

    static void initializeManagedDropDownPopupPanelColors(entt::registry& reg,
                                                          entt::entity entity,
                                                          const theme::ThemePalette& theme,
                                                          components::ThemeStyleState& styleState)
    {
        if (const auto* background = reg.try_get<components::Background>(entity);
            background != nullptr && sameColor(background->color, theme.popupBackground))
        {
            styleState.managedBackgroundColor = background->color;
            styleState.managedBackgroundRadius = background->borderRadius;
        }

        if (const auto* border = reg.try_get<components::Border>(entity);
            border != nullptr && sameColor(border->color, theme.popupBorder))
        {
            styleState.managedBorderColor = border->color;
            styleState.managedBorderRadius = border->borderRadius;
        }
    }

    static void initializeManagedDropDownPopupItemColors(entt::registry& reg,
                                                         entt::entity entity,
                                                         const theme::ThemePalette& theme,
                                                         components::ThemeStyleState& styleState)
    {
        if (const auto* background = reg.try_get<components::Background>(entity);
            background != nullptr
            && colorMatchesAny(background->color,
                               {theme.popupItemBackground,
                                theme.popupItemBackgroundHover,
                                theme.popupItemBackgroundActive,
                                theme.popupItemBackgroundSelected}))
        {
            styleState.managedBackgroundColor = background->color;
            styleState.managedBackgroundRadius = background->borderRadius;
        }

        if (const auto* text = reg.try_get<components::Text>(entity);
            text != nullptr && colorMatchesAny(text->color, {theme.popupItemText, theme.popupItemTextSelected}))
        {
            styleState.managedTextColor = text->color;
        }
    }
    /**
     * @brief 初始化受 ThemeSystem 管理的 CheckBox 颜色状态
     * @param reg 实体注册表
     * @param entity 实体
     * @param theme 主题调色板
     * @param styleState 样式状态组件
     * @details 该函数检查 CheckBox
     * 实体的文本颜色、框颜色和勾选颜色是否与主题中的默认颜色匹配。如果匹配，则将这些颜色记录在 ThemeStyleState
     * 组件中，以便 ThemeSystem 在主题更新时能够正确地更新这些颜色。
     */
    static void initializeManagedCheckBoxColors(entt::registry& reg,
                                                entt::entity entity,
                                                const theme::ThemePalette& theme,
                                                components::ThemeStyleState& styleState)
    {
        if (const auto* text = reg.try_get<components::Text>(entity);
            text != nullptr && colorMatchesAny(text->color, {theme.textPrimary, theme.textDisabled}))
        {
            styleState.managedTextColor = text->color;
        }

        if (const auto* checkBox = reg.try_get<components::CheckBox>(entity); checkBox != nullptr)
        {
            if (colorMatchesAny(checkBox->boxColor,
                                {theme.surfaceBackground,
                                 theme.checkBoxBoxHover,
                                 theme.checkBoxBoxActive,
                                 theme.checkBoxBoxDisabled}))
            {
                styleState.managedBackgroundColor = checkBox->boxColor;
            }

            if (colorMatchesAny(checkBox->checkColor, {theme.accent, theme.accentDisabled}))
            {
                styleState.managedIndicatorColor = checkBox->checkColor;
            }
        }
    }
    /**
     * @brief 初始化受 ThemeSystem 管理的交互态颜色状态
     * @param reg 实体注册表
     * @param entity 实体
     * @param theme 主题调色板
     * @details 该函数根据实体的标签和当前颜色状态，确定哪些颜色属性应该由 ThemeSystem 管理，并将这些颜色记录在
     * ThemeStyleState 组件中。这使得 ThemeSystem
     * 能够在主题更新时正确地更新这些颜色属性，确保交互态颜色始终与当前主题保持一致。
     */
    static void
        initializeManagedInteractiveColors(entt::registry& reg, entt::entity entity, const theme::ThemePalette& theme)
    {
        auto& styleState = reg.get_or_emplace<components::ThemeStyleState>(entity);

        if (reg.any_of<components::ButtonTag>(entity))
        {
            initializeManagedButtonColors(reg, entity, theme, styleState);
        }

        if (reg.any_of<components::DropDownTag>(entity))
        {
            initializeManagedDropDownColors(reg, entity, theme, styleState);
        }

        if (reg.any_of<components::TextEditTag>(entity))
        {
            if (const auto* background = reg.try_get<components::Background>(entity);
                background != nullptr
                && colorMatchesAny(background->color,
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
                && colorMatchesAny(border->color,
                                   {theme.inputBorder, theme.focusBorderColor, theme.inputBorderDisabled}))
            {
                styleState.managedBorderColor = border->color;
                styleState.managedBorderRadius = border->borderRadius;
            }
        }

        if (reg.any_of<components::CheckBoxTag>(entity))
        {
            initializeManagedCheckBoxColors(reg, entity, theme, styleState);
        }

        if (reg.any_of<components::DropDownPopupPanel>(entity))
        {
            initializeManagedDropDownPopupPanelColors(reg, entity, theme, styleState);
        }

        if (reg.any_of<components::DropDownPopupItem>(entity))
        {
            initializeManagedDropDownPopupItemColors(reg, entity, theme, styleState);
        }
    }

    static bool updateManagedBackground(entt::registry& reg, entt::entity entity, const Color& desiredColor)
    {
        auto* background = reg.try_get<components::Background>(entity);
        auto* styleState = reg.try_get<components::ThemeStyleState>(entity);
        if (background == nullptr || styleState == nullptr || !styleState->managedBackgroundColor.has_value())
        {
            return false;
        }

        if (!sameColor(background->color, *styleState->managedBackgroundColor)
            || sameColor(background->color, desiredColor))
        {
            return false;
        }

        background->color = desiredColor;
        background->enabled = policies::Feature::ENABLED;
        styleState->managedBackgroundColor = desiredColor;
        return true;
    }

    static bool updateManagedBorder(entt::registry& reg, entt::entity entity, const Color& desiredColor)
    {
        auto* border = reg.try_get<components::Border>(entity);
        auto* styleState = reg.try_get<components::ThemeStyleState>(entity);
        if (border == nullptr || styleState == nullptr || !styleState->managedBorderColor.has_value())
        {
            return false;
        }

        if (!sameColor(border->color, *styleState->managedBorderColor) || sameColor(border->color, desiredColor))
        {
            return false;
        }

        border->color = desiredColor;
        border->enabled = policies::Feature::ENABLED;
        styleState->managedBorderColor = desiredColor;
        return true;
    }

    static bool updateManagedTextColor(entt::registry& reg, entt::entity entity, const Color& desiredColor)
    {
        auto* text = reg.try_get<components::Text>(entity);
        auto* styleState = reg.try_get<components::ThemeStyleState>(entity);
        if (text == nullptr || styleState == nullptr || !styleState->managedTextColor.has_value())
        {
            return false;
        }

        if (!sameColor(text->color, *styleState->managedTextColor) || sameColor(text->color, desiredColor))
        {
            return false;
        }

        text->color = desiredColor;
        styleState->managedTextColor = desiredColor;
        return true;
    }

    static bool updateManagedCheckBoxBoxColor(entt::registry& reg, entt::entity entity, const Color& desiredColor)
    {
        auto* checkBox = reg.try_get<components::CheckBox>(entity);
        auto* styleState = reg.try_get<components::ThemeStyleState>(entity);
        if (checkBox == nullptr || styleState == nullptr || !styleState->managedBackgroundColor.has_value())
        {
            return false;
        }

        if (!sameColor(checkBox->boxColor, *styleState->managedBackgroundColor)
            || sameColor(checkBox->boxColor, desiredColor))
        {
            return false;
        }

        checkBox->boxColor = desiredColor;
        styleState->managedBackgroundColor = desiredColor;
        return true;
    }

    static bool updateManagedIndicatorColor(entt::registry& reg, entt::entity entity, const Color& desiredColor)
    {
        auto* styleState = reg.try_get<components::ThemeStyleState>(entity);
        if (styleState == nullptr || !styleState->managedIndicatorColor.has_value())
        {
            return false;
        }

        if (auto* checkBox = reg.try_get<components::CheckBox>(entity); checkBox != nullptr)
        {
            if (!sameColor(checkBox->checkColor, *styleState->managedIndicatorColor)
                || sameColor(checkBox->checkColor, desiredColor))
            {
                return false;
            }

            checkBox->checkColor = desiredColor;
            styleState->managedIndicatorColor = desiredColor;
            return true;
        }

        if (auto* dropDown = reg.try_get<components::DropDown>(entity); dropDown != nullptr)
        {
            if (!sameColor(dropDown->arrowColor, *styleState->managedIndicatorColor)
                || sameColor(dropDown->arrowColor, desiredColor))
            {
                return false;
            }

            dropDown->arrowColor = desiredColor;
            styleState->managedIndicatorColor = desiredColor;
            return true;
        }

        return false;
    }

    static bool isDropDownPopupItemSelected(entt::registry& reg, const components::DropDownPopupItem& popupItem)
    {
        const auto* ownerDropDown = reg.try_get<components::DropDown>(popupItem.owner);
        if (ownerDropDown == nullptr)
        {
            return false;
        }

        return ownerDropDown->selectedIndex == popupItem.optionIndex;
    }

    static bool applyButtonInteractiveTheme(entt::registry& reg, entt::entity entity, const theme::ThemePalette& theme)
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

        changed |= updateManagedBackground(reg, entity, desiredBackground);
        changed |= updateManagedBorder(reg,
                                       entity,
                                       resolveFocusedBorderColor(disabled,
                                                                 reg.any_of<components::FocusedTag>(entity),
                                                                 theme.surfaceBorder,
                                                                 theme.focusBorderColor,
                                                                 theme.disabledBorder));
        changed |= updateManagedTextColor(reg, entity, disabled ? theme.textDisabled : theme.primaryButtonText);
        return changed;
    }

    static bool
        applyTextEditInteractiveTheme(entt::registry& reg, entt::entity entity, const theme::ThemePalette& theme)
    {
        return updateManagedBorder(reg,
                                   entity,
                                   resolveFocusedBorderColor(reg.any_of<components::DisabledTag>(entity),
                                                             reg.any_of<components::FocusedTag>(entity),
                                                             theme.inputBorder,
                                                             theme.focusBorderColor,
                                                             theme.inputBorderDisabled));
    }

    static bool
        applyDropDownInteractiveTheme(entt::registry& reg, entt::entity entity, const theme::ThemePalette& theme)
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

        changed |= updateManagedBackground(reg, entity, desiredBackground);
        changed |= updateManagedBorder(reg,
                                       entity,
                                       resolveFocusedBorderColor(disabled,
                                                                 reg.any_of<components::FocusedTag>(entity),
                                                                 theme.inputBorder,
                                                                 theme.focusBorderColor,
                                                                 theme.inputBorderDisabled));
        changed |= updateManagedTextColor(reg, entity, disabled ? theme.textDisabled : theme.inputText);
        changed |= updateManagedIndicatorColor(reg, entity, desiredArrow);
        return changed;
    }

    static bool
        applyCheckBoxInteractiveTheme(entt::registry& reg, entt::entity entity, const theme::ThemePalette& theme)
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

        changed |= updateManagedCheckBoxBoxColor(reg, entity, desiredBoxColor);
        changed |= updateManagedIndicatorColor(reg, entity, disabled ? theme.accentDisabled : theme.accent);
        changed |= updateManagedTextColor(reg, entity, disabled ? theme.textDisabled : theme.textPrimary);
        return changed;
    }

    static bool applyDropDownPopupItemInteractiveTheme(entt::registry& reg,
                                                       entt::entity entity,
                                                       const theme::ThemePalette& theme)
    {
        const auto* popupItem = reg.try_get<components::DropDownPopupItem>(entity);
        if (popupItem == nullptr)
        {
            return false;
        }

        Color desiredBackground = theme.popupItemBackground;
        if (isDropDownPopupItemSelected(reg, *popupItem))
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
            isDropDownPopupItemSelected(reg, *popupItem) ? theme.popupItemTextSelected : theme.popupItemText;

        bool changed = false;
        changed |= updateManagedBackground(reg, entity, desiredBackground);
        changed |= updateManagedTextColor(reg, entity, desiredText);
        return changed;
    }

    static bool
        applyInteractiveThemeToEntity(entt::registry& reg, entt::entity entity, const theme::ThemePalette& theme)
    {
        bool changed = false;

        if (reg.any_of<components::ButtonTag>(entity))
        {
            changed |= applyButtonInteractiveTheme(reg, entity, theme);
        }

        if (reg.any_of<components::TextEditTag>(entity))
        {
            changed |= applyTextEditInteractiveTheme(reg, entity, theme);
        }

        if (reg.any_of<components::DropDownTag>(entity))
        {
            changed |= applyDropDownInteractiveTheme(reg, entity, theme);
        }

        if (reg.any_of<components::CheckBoxTag>(entity))
        {
            changed |= applyCheckBoxInteractiveTheme(reg, entity, theme);
        }

        if (reg.any_of<components::DropDownPopupItem>(entity))
        {
            changed |= applyDropDownPopupItemInteractiveTheme(reg, entity, theme);
        }

        return changed;
    }

    static bool applyButtonTheme(entt::registry& reg,
                                 entt::entity entity,
                                 const theme::ThemePalette& nextTheme,
                                 const theme::ThemePalette& previousTheme)
    {
        bool changed = false;
        changed |= applyBackground(reg,
                                   entity,
                                   nextTheme.primaryButtonBackground,
                                   previousTheme.primaryButtonBackground,
                                   nextTheme.primaryButtonRadius);
        changed |= applyBorder(reg,
                               entity,
                               nextTheme.surfaceBorder,
                               previousTheme.surfaceBorder,
                               nextTheme.primaryButtonRadius,
                               nextTheme.primaryButtonBorderThickness);
        changed |= applyTextColor(reg, entity, nextTheme.primaryButtonText, previousTheme.primaryButtonText);
        return changed;
    }

    static bool applyLabelTheme(entt::registry& reg,
                                entt::entity entity,
                                const theme::ThemePalette& nextTheme,
                                const theme::ThemePalette& previousTheme)
    {
        return applyTextColor(reg, entity, nextTheme.textPrimary, previousTheme.textPrimary);
    }

    static bool applyTextEditTheme(entt::registry& reg,
                                   entt::entity entity,
                                   const theme::ThemePalette& nextTheme,
                                   const theme::ThemePalette& previousTheme)
    {
        bool changed = false;
        changed |= applyBackground(
            reg, entity, nextTheme.inputBackground, previousTheme.inputBackground, nextTheme.inputControlRadius);
        changed |= applyBorder(reg,
                               entity,
                               nextTheme.inputBorder,
                               previousTheme.inputBorder,
                               nextTheme.inputControlRadius,
                               nextTheme.inputBorderThickness);
        changed |= applyTextColor(reg, entity, nextTheme.inputText, previousTheme.inputText);
        changed |= applyTextEditColor(reg, entity, nextTheme.inputText, previousTheme.inputText);
        return changed;
    }

    static bool applyCheckBoxTheme(entt::registry& reg,
                                   entt::entity entity,
                                   const theme::ThemePalette& nextTheme,
                                   const theme::ThemePalette& previousTheme)
    {
        bool changed = false;
        changed |= applyTextColor(reg, entity, nextTheme.textPrimary, previousTheme.textPrimary);
        changed |= applyCheckBoxColors(reg, entity, nextTheme, previousTheme);
        return changed;
    }

    static bool applyDropDownTheme(entt::registry& reg,
                                   entt::entity entity,
                                   const theme::ThemePalette& nextTheme,
                                   const theme::ThemePalette& previousTheme)
    {
        bool changed = false;
        changed |= applyBackground(
            reg, entity, nextTheme.inputBackground, previousTheme.inputBackground, nextTheme.inputControlRadius);
        changed |= applyBorder(reg,
                               entity,
                               nextTheme.inputBorder,
                               previousTheme.inputBorder,
                               nextTheme.inputControlRadius,
                               nextTheme.inputBorderThickness);
        changed |= applyTextColor(reg, entity, nextTheme.inputText, previousTheme.inputText);
        changed |= applyDropDownArrowColor(reg, entity, nextTheme.dropDownArrow, previousTheme.dropDownArrow);
        return changed;
    }

    static bool reapplyManagedBackgroundRadius(entt::registry& reg,
                                               entt::entity entity,
                                               const Color& expectedColor,
                                               const Vec4& previousRadius,
                                               const Vec4& nextRadius)
    {
        auto* background = reg.try_get<components::Background>(entity);
        if (background == nullptr || !sameColor(background->color, expectedColor)
            || !sameRadii(background->borderRadius, previousRadius))
        {
            return false;
        }

        background->borderRadius = nextRadius;
        return true;
    }

    static bool reapplyManagedBorderRadius(entt::registry& reg,
                                           entt::entity entity,
                                           const Color& expectedColor,
                                           const Vec4& previousRadius,
                                           const Vec4& nextRadius)
    {
        auto* border = reg.try_get<components::Border>(entity);
        if (border == nullptr || !sameColor(border->color, expectedColor)
            || !sameRadii(border->borderRadius, previousRadius))
        {
            return false;
        }

        border->borderRadius = nextRadius;
        return true;
    }

    static bool applyDropDownPopupPanelTheme(entt::registry& reg,
                                             entt::entity entity,
                                             const theme::ThemePalette& nextTheme,
                                             const theme::ThemePalette& previousTheme)
    {
        bool changed = false;
        changed |= applyBackground(
            reg, entity, nextTheme.popupBackground, previousTheme.popupBackground, nextTheme.popupPanelRadius);
        changed |= applyBorder(reg,
                               entity,
                               nextTheme.popupBorder,
                               previousTheme.popupBorder,
                               nextTheme.popupPanelRadius,
                               nextTheme.popupBorderThickness);

        changed |= reapplyManagedBackgroundRadius(
            reg, entity, nextTheme.popupBackground, previousTheme.popupPanelRadius, nextTheme.popupPanelRadius);
        changed |= reapplyManagedBorderRadius(
            reg, entity, nextTheme.popupBorder, previousTheme.popupPanelRadius, nextTheme.popupPanelRadius);

        return changed;
    }

    static bool applyDropDownPopupItemTheme(entt::registry& reg,
                                            entt::entity entity,
                                            const theme::ThemePalette& nextTheme,
                                            const theme::ThemePalette& previousTheme)
    {
        const auto* popupItem = reg.try_get<components::DropDownPopupItem>(entity);
        const Color initialBackground = (popupItem != nullptr && isDropDownPopupItemSelected(reg, *popupItem))
                                          ? nextTheme.popupItemBackgroundSelected
                                          : nextTheme.popupItemBackground;
        const Color previousBackground = (popupItem != nullptr && isDropDownPopupItemSelected(reg, *popupItem))
                                           ? previousTheme.popupItemBackgroundSelected
                                           : previousTheme.popupItemBackground;
        const Color initialText = (popupItem != nullptr && isDropDownPopupItemSelected(reg, *popupItem))
                                    ? nextTheme.popupItemTextSelected
                                    : nextTheme.popupItemText;
        const Color previousText = (popupItem != nullptr && isDropDownPopupItemSelected(reg, *popupItem))
                                     ? previousTheme.popupItemTextSelected
                                     : previousTheme.popupItemText;

        bool changed = false;
        changed |= applyBackground(reg, entity, initialBackground, previousBackground, nextTheme.popupItemRadius);
        changed |= applyTextColor(reg, entity, initialText, previousText);

        changed |= reapplyManagedBackgroundRadius(
            reg, entity, initialBackground, previousTheme.popupItemRadius, nextTheme.popupItemRadius);

        return changed;
    }

    static bool applySliderTheme(entt::registry& reg,
                                 entt::entity entity,
                                 const theme::ThemePalette& nextTheme,
                                 const theme::ThemePalette& previousTheme)
    {
        return applySliderColors(reg, entity, nextTheme, previousTheme);
    }

    static bool applyProgressBarTheme(entt::registry& reg,
                                      entt::entity entity,
                                      const theme::ThemePalette& nextTheme,
                                      const theme::ThemePalette& previousTheme)
    {
        return applyProgressBarColors(reg, entity, nextTheme, previousTheme);
    }

    static bool applyWindowTheme(entt::registry& reg,
                                 entt::entity entity,
                                 const theme::ThemePalette& nextTheme,
                                 const theme::ThemePalette& previousTheme)
    {
        bool changed = false;
        changed |= applyBackground(
            reg, entity, nextTheme.windowBackground, previousTheme.windowBackground, nextTheme.windowPanelRadius);
        changed |= applyBorder(reg,
                               entity,
                               nextTheme.surfaceBorder,
                               previousTheme.surfaceBorder,
                               nextTheme.windowPanelRadius,
                               nextTheme.windowBorderThickness);

        changed |= reapplyManagedBackgroundRadius(
            reg, entity, nextTheme.windowBackground, previousTheme.windowPanelRadius, nextTheme.windowPanelRadius);
        changed |= reapplyManagedBorderRadius(
            reg, entity, nextTheme.surfaceBorder, previousTheme.windowPanelRadius, nextTheme.windowPanelRadius);

        return changed;
    }

    static bool applyThemeToEntity(entt::registry& reg,
                                   entt::entity entity,
                                   const theme::ThemePalette& nextTheme,
                                   const theme::ThemePalette& previousTheme)
    {
        bool changed = false;

        if (reg.any_of<components::ButtonTag>(entity))
        {
            changed |= applyButtonTheme(reg, entity, nextTheme, previousTheme);
        }

        if (reg.any_of<components::LabelTag>(entity))
        {
            changed |= applyLabelTheme(reg, entity, nextTheme, previousTheme);
        }

        if (reg.any_of<components::TextEditTag>(entity))
        {
            changed |= applyTextEditTheme(reg, entity, nextTheme, previousTheme);
        }

        if (reg.any_of<components::CheckBoxTag>(entity))
        {
            changed |= applyCheckBoxTheme(reg, entity, nextTheme, previousTheme);
        }

        if (reg.any_of<components::DropDownTag>(entity))
        {
            changed |= applyDropDownTheme(reg, entity, nextTheme, previousTheme);
        }

        if (reg.any_of<components::DropDownPopupPanel>(entity))
        {
            changed |= applyDropDownPopupPanelTheme(reg, entity, nextTheme, previousTheme);
        }

        if (reg.any_of<components::DropDownPopupItem>(entity))
        {
            changed |= applyDropDownPopupItemTheme(reg, entity, nextTheme, previousTheme);
        }

        if (reg.any_of<components::SliderTag>(entity))
        {
            changed |= applySliderTheme(reg, entity, nextTheme, previousTheme);
        }

        if (reg.any_of<components::ProgressBarTag>(entity))
        {
            changed |= applyProgressBarTheme(reg, entity, nextTheme, previousTheme);
        }

        if (reg.any_of<components::WindowTag>(entity) || reg.any_of<components::DialogTag>(entity))
        {
            changed |= applyWindowTheme(reg, entity, nextTheme, previousTheme);
        }

        initializeManagedInteractiveColors(reg, entity, nextTheme);
        return changed;
    }

    void update()
    {
        auto& runtime = RuntimeFacade::current();
        auto& reg = runtime.enttRegistry();
        auto& themeContext = runtime.ensureContext<theme::ThemeContext>();

        if (themeContext.reapplyRequested)
        {
            clearThemedTags(reg);
        }

        auto unthemedView = reg.view<components::BaseInfo>(entt::exclude<components::ThemedTag>);
        for (const entt::entity entity : unthemedView)
        {
            const bool changed = applyThemeToEntity(reg, entity, themeContext.palette, themeContext.previousPalette);
            reg.emplace_or_replace<components::ThemedTag>(entity);
            if (changed)
            {
                ui::utils::MarkVisualChanged(entity);
            }
        }

        auto themedView = reg.view<components::BaseInfo, components::ThemedTag>();
        for (const entt::entity entity : themedView)
        {
            if (applyInteractiveThemeToEntity(reg, entity, themeContext.palette))
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
};

} // namespace ui::systems