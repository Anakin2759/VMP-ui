/**
 * ************************************************************************
 *
 * @file Image.hpp
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2026-05-18
 * @version 0.1
 * @brief Image 组件 Chain DSL — 设置路径、着色、UV
 *
 * ************************************************************************
 * @copyright Copyright (c) 2026 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */
#pragma once

#include <string_view>
#include <entt/entt.hpp>
#include "../common/components/Data.hpp"
#include "../common/Types.hpp"
#include "../singleton/Registry.hpp"
#include "Chains.hpp"

namespace ui::image
{
/**
 * @brief 设置 Image 组件的图片路径。路径变更会重置加载状态，触发资源系统重新加载。
 * @param entity 实体ID
 * @param path 图片文件路径（相对于资源目录，支持 bmp/png/jpeg 等格式
 */
inline void SetImagePath(entt::entity entity, std::string_view path)
{
    if (!Registry::Valid(entity)) return;
    auto& src = Registry::GetOrEmplace<components::ImageSource>(entity);
    src.path = std::string(path);
    src.loaded = false;
    src.loadFailed = false;
}

inline void SetImageTint(entt::entity entity, Color color)
{
    if (!Registry::Valid(entity)) return;
    auto& img = Registry::GetOrEmplace<components::Image>(entity);
    img.tintColor = color;
}

inline void SetImageUV(entt::entity entity, Vec2 uvMin, Vec2 uvMax)
{
    if (!Registry::Valid(entity)) return;
    auto& img = Registry::GetOrEmplace<components::Image>(entity);
    img.uvMin = uvMin;
    img.uvMax = uvMax;
}

} // namespace ui::image

namespace ui::actions::image
{
inline constexpr EntityAction<&ui::image::SetImagePath> SET_IMAGE_PATH_ACTION{};
inline constexpr EntityAction<&ui::image::SetImageTint> SET_IMAGE_TINT_ACTION{};
inline constexpr EntityAction<&ui::image::SetImageUV> SET_IMAGE_UV_ACTION{};
} // namespace ui::actions::image

namespace ui::chains
{

inline auto ImagePath(std::string_view path)
{
    return ui::actions::image::SET_IMAGE_PATH_ACTION.bind(path);
}

inline auto ImageTint(Color color)
{
    return ui::actions::image::SET_IMAGE_TINT_ACTION.bind(color);
}

inline auto ImageUV(Vec2 uvMin, Vec2 uvMax)
{
    return ui::actions::image::SET_IMAGE_UV_ACTION.bind(uvMin, uvMax);
}

} // namespace ui::chains
