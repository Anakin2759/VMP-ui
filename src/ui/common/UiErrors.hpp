#pragma once

#include <system_error>

namespace ui::errors
{

enum class UiErrc
{
    SdlInitFailed = 1,
    UnknownInitializationFailure,
};

enum class FontErrc
{
    FreeTypeNotInitialized = 1,
    InvalidFontData,
    LoadFaceFailed,
    SetPixelSizeFailed,
    GpuDeviceUnavailable,
};

enum class IconErrc
{
    FreeTypeNotInitialized = 1,
    InvalidFontData,
    InvalidCodepointsData,
    LoadFaceFailed,
    SetPixelSizeFailed,
    ParseCodepointsFailed,
};

class UiErrorCategory final : public std::error_category
{
public:
    const char* name() const noexcept override { return "ui.init"; }

    std::string message(int condition) const override
    {
        switch (static_cast<UiErrc>(condition))
        {
            case UiErrc::SdlInitFailed:
                return "SDL initialization failed";
            case UiErrc::UnknownInitializationFailure:
                return "Unknown UI initialization failure";
        }
        return "Unknown UI initialization error";
    }
};

class FontErrorCategory final : public std::error_category
{
public:
    const char* name() const noexcept override { return "ui.font"; }

    std::string message(int condition) const override
    {
        switch (static_cast<FontErrc>(condition))
        {
            case FontErrc::FreeTypeNotInitialized:
                return "FreeType not initialized";
            case FontErrc::InvalidFontData:
                return "Invalid font data";
            case FontErrc::LoadFaceFailed:
                return "Failed to load font face";
            case FontErrc::SetPixelSizeFailed:
                return "Failed to set font pixel size";
            case FontErrc::GpuDeviceUnavailable:
                return "GPU device unavailable for font atlas";
        }
        return "Unknown font loading error";
    }
};

class IconErrorCategory final : public std::error_category
{
public:
    const char* name() const noexcept override { return "ui.icon"; }

    std::string message(int condition) const override
    {
        switch (static_cast<IconErrc>(condition))
        {
            case IconErrc::FreeTypeNotInitialized:
                return "FreeType not initialized";
            case IconErrc::InvalidFontData:
                return "Invalid icon font data";
            case IconErrc::InvalidCodepointsData:
                return "Invalid icon codepoints data";
            case IconErrc::LoadFaceFailed:
                return "Failed to load icon font face";
            case IconErrc::SetPixelSizeFailed:
                return "Failed to set icon font pixel size";
            case IconErrc::ParseCodepointsFailed:
                return "Failed to parse icon codepoints";
        }
        return "Unknown icon loading error";
    }
};

inline const std::error_category& uiErrorCategory()
{
    static UiErrorCategory category;
    return category;
}

inline const std::error_category& fontErrorCategory()
{
    static FontErrorCategory category;
    return category;
}

inline const std::error_category& iconErrorCategory()
{
    static IconErrorCategory category;
    return category;
}

inline std::error_code make_error_code(UiErrc error)
{
    return {static_cast<int>(error), uiErrorCategory()};
}

inline std::error_code make_error_code(FontErrc error)
{
    return {static_cast<int>(error), fontErrorCategory()};
}

inline std::error_code make_error_code(IconErrc error)
{
    return {static_cast<int>(error), iconErrorCategory()};
}

} // namespace ui::errors

namespace std
{

template <>
struct is_error_code_enum<ui::errors::UiErrc> : true_type
{
};

template <>
struct is_error_code_enum<ui::errors::FontErrc> : true_type
{
};

template <>
struct is_error_code_enum<ui::errors::IconErrc> : true_type
{
};

} // namespace std