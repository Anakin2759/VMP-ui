/**
 * ************************************************************************
 *
 * @file ErrorCodes.cpp
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2026-05-19
 * @version 0.1
 *
 *
 * ************************************************************************
 * @copyright Copyright (c) 2026 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */
#include "ErrorCodes.hpp"
#include <string>
#include <system_error>
#include <string_view>

namespace ui
{

const char* UiErrorCategory::name() const noexcept
{
    return "ui";
}

std::string UiErrorCategory::message(int condition) const
{
    switch (static_cast<ui_errc>(condition))
    {
        case ui_errc::invalid_entity:
            return "invalid entity";
        case ui_errc::invalid_argument:
            return "invalid argument";
        case ui_errc::registry_unavailable:
            return "registry unavailable";
        case ui_errc::not_implemented:
            return "not implemented";

        case ui_errc::hierarchy_cycle:
            return "hierarchy cycle detected";
        case ui_errc::hierarchy_detached:
            return "entity detached from hierarchy";
        case ui_errc::entity_already_exists:
            return "entity already exists";

        case ui_errc::asset_not_found:
            return "asset not found";
        case ui_errc::asset_load_failed:
            return "asset load failed";
        case ui_errc::asset_decode_failed:
            return "asset decode failed";
        case ui_errc::asset_upload_failed:
            return "asset upload failed";
        case ui_errc::atlas_full:
            return "texture atlas is full and cannot be expanded";
        case ui_errc::glyph_render_failed:
            return "freetype glyph render failed";
        case ui_errc::file_open_failed:
            return "failed to open resource file from disk";

        case ui_errc::device_unavailable:
            return "gpu device unavailable";
        case ui_errc::pipeline_unavailable:
            return "gpu pipeline unavailable";
        case ui_errc::shader_compile_failed:
            return "shader compile failed";
        case ui_errc::swapchain_unavailable:
            return "gpu swapchain texture is not ready";
        case ui_errc::backend_unavailable:
            return "no gpu/sdl backend available (all candidates exhausted)";
        case ui_errc::window_claim_failed:
            return "SDL_ClaimWindowForGPUDevice failed";
        case ui_errc::buffer_map_failed:
            return "SDL_MapGPUTransferBuffer failed";

        case ui_errc::theme_not_found:
            return "theme item not found";
        case ui_errc::theme_type_mismatch:
            return "theme item type mismatch";

        case ui_errc::script_parse_error:
            return "script parse error";
        case ui_errc::script_runtime_error:
            return "script runtime error";

        case ui_errc::unknown:
            return "unknown ui error";
    }
    return "unrecognized ui error";
}

const std::error_category& GetUiErrorCategory() noexcept
{
    static const UiErrorCategory kCategory{};
    return kCategory;
}

std::error_code make_error_code(ui_errc error) noexcept
{
    return {static_cast<int>(error), GetUiErrorCategory()};
}

std::string_view ToStringView(ui_errc error) noexcept
{
    switch (error)
    {
        case ui_errc::invalid_entity:
            return "invalid_entity";
        case ui_errc::invalid_argument:
            return "invalid_argument";
        case ui_errc::registry_unavailable:
            return "registry_unavailable";
        case ui_errc::not_implemented:
            return "not_implemented";

        case ui_errc::hierarchy_cycle:
            return "hierarchy_cycle";
        case ui_errc::hierarchy_detached:
            return "hierarchy_detached";
        case ui_errc::entity_already_exists:
            return "entity_already_exists";

        case ui_errc::asset_not_found:
            return "asset_not_found";
        case ui_errc::asset_load_failed:
            return "asset_load_failed";
        case ui_errc::asset_decode_failed:
            return "asset_decode_failed";
        case ui_errc::asset_upload_failed:
            return "asset_upload_failed";
        case ui_errc::atlas_full:
            return "atlas_full";
        case ui_errc::glyph_render_failed:
            return "glyph_render_failed";
        case ui_errc::file_open_failed:
            return "file_open_failed";

        case ui_errc::device_unavailable:
            return "device_unavailable";
        case ui_errc::pipeline_unavailable:
            return "pipeline_unavailable";
        case ui_errc::shader_compile_failed:
            return "shader_compile_failed";
        case ui_errc::swapchain_unavailable:
            return "swapchain_unavailable";
        case ui_errc::backend_unavailable:
            return "backend_unavailable";
        case ui_errc::window_claim_failed:
            return "window_claim_failed";
        case ui_errc::buffer_map_failed:
            return "buffer_map_failed";

        case ui_errc::theme_not_found:
            return "theme_not_found";
        case ui_errc::theme_type_mismatch:
            return "theme_type_mismatch";

        case ui_errc::script_parse_error:
            return "script_parse_error";
        case ui_errc::script_runtime_error:
            return "script_runtime_error";

        case ui_errc::unknown:
            return "unknown";
    }
    return "unrecognized";
}

} // namespace ui
