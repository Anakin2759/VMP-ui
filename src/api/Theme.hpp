#pragma once

#include <cstdint>

#include "../common/Types.hpp"

namespace ui::theme
{

struct ThemePalette
{
    Color windowBackground{0.11F, 0.12F, 0.15F, 1.0F};
    Vec4 windowPanelRadius{8.0F, 8.0F, 8.0F, 8.0F};
    float windowBorderThickness = 1.0F;
    Color surfaceBackground{0.18F, 0.19F, 0.24F, 1.0F};
    Color surfaceBorder{0.33F, 0.36F, 0.46F, 1.0F};
    Color disabledBorder{0.25F, 0.27F, 0.33F, 1.0F};
    Color primaryButtonBackground{0.21F, 0.45F, 0.86F, 1.0F};
    Color primaryButtonBackgroundHover{0.27F, 0.52F, 0.92F, 1.0F};
    Color primaryButtonBackgroundActive{0.16F, 0.38F, 0.74F, 1.0F};
    Color primaryButtonBackgroundDisabled{0.24F, 0.28F, 0.36F, 1.0F};
    Color primaryButtonText{0.97F, 0.98F, 1.0F, 1.0F};
    Vec4 primaryButtonRadius{6.0F, 6.0F, 6.0F, 6.0F};
    float primaryButtonBorderThickness = 1.0F;
    Color textPrimary{0.92F, 0.94F, 0.98F, 1.0F};
    Color textMuted{0.72F, 0.75F, 0.82F, 1.0F};
    Color textDisabled{0.50F, 0.53F, 0.60F, 1.0F};
    Color inputBackground{0.14F, 0.15F, 0.19F, 1.0F};
    Color inputBackgroundHover{0.17F, 0.18F, 0.23F, 1.0F};
    Color inputBackgroundActive{0.12F, 0.13F, 0.17F, 1.0F};
    Color inputBackgroundDisabled{0.12F, 0.13F, 0.16F, 1.0F};
    Color inputBorder{0.31F, 0.34F, 0.42F, 1.0F};
    Color inputBorderDisabled{0.22F, 0.24F, 0.30F, 1.0F};
    Color focusBorderColor{0.31F, 0.67F, 0.98F, 1.0F};
    float focusBorderMinThickness = 2.0F;
    Color inputText{0.92F, 0.94F, 0.98F, 1.0F};
    Vec4 inputControlRadius{6.0F, 6.0F, 6.0F, 6.0F};
    float inputBorderThickness = 1.0F;
    Color popupBackground{0.13F, 0.13F, 0.18F, 0.97F};
    Color popupBorder{0.35F, 0.35F, 0.50F, 1.0F};
    Vec4 popupPanelRadius{6.0F, 6.0F, 6.0F, 6.0F};
    float popupBorderThickness = 1.0F;
    Color popupItemBackground{0.0F, 0.0F, 0.0F, 0.0F};
    Color popupItemBackgroundHover{0.20F, 0.23F, 0.31F, 0.92F};
    Color popupItemBackgroundActive{0.16F, 0.20F, 0.28F, 0.96F};
    Color popupItemBackgroundSelected{0.25F, 0.45F, 0.80F, 0.40F};
    Color popupItemText{0.92F, 0.94F, 0.98F, 1.0F};
    Color popupItemTextSelected{0.97F, 0.98F, 1.0F, 1.0F};
    Vec4 popupItemRadius{0.0F, 0.0F, 0.0F, 0.0F};
    Color dropDownArrow{0.70F, 0.70F, 0.75F, 1.0F};
    Color dropDownArrowHover{0.82F, 0.84F, 0.90F, 1.0F};
    Color dropDownArrowActive{0.56F, 0.73F, 0.94F, 1.0F};
    Color dropDownArrowDisabled{0.44F, 0.46F, 0.52F, 1.0F};
    Color checkBoxBoxHover{0.30F, 0.31F, 0.38F, 1.0F};
    Color checkBoxBoxActive{0.22F, 0.23F, 0.29F, 1.0F};
    Color checkBoxBoxDisabled{0.18F, 0.19F, 0.23F, 1.0F};
    Color accent{0.31F, 0.67F, 0.98F, 1.0F};
    Color accentDisabled{0.24F, 0.38F, 0.50F, 1.0F};
    Color accentMuted{0.52F, 0.79F, 1.0F, 1.0F};
};

struct ThemeContext
{
    ThemePalette palette{};
    ThemePalette previousPalette{};
    uint32_t version = 1;
    bool reapplyRequested = false;
};

[[nodiscard]] ThemePalette DefaultDarkTheme();
void SetTheme(const ThemePalette& palette);
void UseDefaultDarkTheme();
void RequestThemeReapply();
[[nodiscard]] const ThemePalette& CurrentTheme();

} // namespace ui::theme