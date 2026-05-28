#include "Theme.hpp"

#include "../core/RuntimeFacade.hpp"

namespace ui::theme
{

ThemePalette DefaultDarkTheme()
{
    return ThemePalette{};
}

void SetTheme(const ThemePalette& palette)
{
    auto& context = RuntimeFacade::current().ensureContext<ThemeContext>();
    context.previousPalette = context.palette;
    context.palette = palette;
    ++context.version;
    context.reapplyRequested = true;
}

void UseDefaultDarkTheme()
{
    SetTheme(DefaultDarkTheme());
}

void RequestThemeReapply()
{
    auto& context = RuntimeFacade::current().ensureContext<ThemeContext>();
    context.previousPalette = context.palette;
    context.reapplyRequested = true;
}

const ThemePalette& CurrentTheme()
{
    return RuntimeFacade::current().ensureContext<ThemeContext>().palette;
}

} // namespace ui::theme