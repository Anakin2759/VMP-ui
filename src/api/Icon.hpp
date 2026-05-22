/**
 * ************************************************************************
 *
 * @file Icon.hpp
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2026-02-04
 * @version 0.1
 * @brief 图标组件接口
  - 提供设置和移除图标组件的功能
  - 支持纹理图标和字体图标
  - 可配置图标位置、大小和间距
  - 简化图标组件的使用流程
 *
 * ************************************************************************
 * @copyright Copyright (c) 2026 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */
#pragma once
#include <entt/entt.hpp>
#include <string>
#include "../common/Policies.hpp"
#include "Chains.hpp"
namespace ui::icon
{
/**
 * @brief 设置图标组件
 * @param entity  实体
 * @param textureId 纹理ID
 * @param position 图标位置
 * @param iconSize 图标大小
 * @param spacing 图标与文本间距
 */
void SetIcon(entt::entity entity,
             const std::string& textureId,
             policies::IconFlag iconflag = policies::IconFlag::DEFAULT,
             float iconSize = 16.0F,
             float spacing = 4.0F);
/**
 * @brief 设置字体图标组件
 * @param entity 实体
 * @param fontName 字体库名称
 * @param codepoint Unicode 码点
 * @param position 图标位置
 * @param iconSize 图标大小
 * @param spacing 图标与文本间距
 */
void SetIcon(entt::entity entity,
             const std::string& fontName,
             uint32_t codepoint,
             policies::IconFlag iconflag = policies::IconFlag::DEFAULT,
             float iconSize = 16.0F,
             float spacing = 4.0F);

void RemoveIcon(entt::entity entity);
} // namespace ui::icon

namespace ui::actions::icon
{
inline constexpr EntityAction<static_cast<void (*)(
    entt::entity, const std::string&, uint32_t, policies::IconFlag, float, float)>(&ui::icon::SetIcon)>
    SET_FONT_ICON_ACTION{};
inline constexpr EntityAction<static_cast<void (*)(entt::entity, const std::string&, policies::IconFlag, float, float)>(
    &ui::icon::SetIcon)>
    SET_TEXTURE_ICON_ACTION{};
inline constexpr EntityAction<&ui::icon::RemoveIcon> REMOVE_ICON_ACTION{};
} // namespace ui::actions::icon

namespace ui::chains
{
inline auto Icon(const std::string& fontName,
                 uint32_t codepoint,
                 policies::IconFlag iconflag = policies::IconFlag::DEFAULT,
                 float iconSize = 16.0F,
                 float spacing = 4.0F)
{
    return ui::actions::icon::SET_FONT_ICON_ACTION.bind(fontName, codepoint, iconflag, iconSize, spacing);
}
inline auto Icon(const std::string& textureId,
                 policies::IconFlag iconflag = policies::IconFlag::DEFAULT,
                 float iconSize = 16.0F,
                 float spacing = 4.0F)
{
    return ui::actions::icon::SET_TEXTURE_ICON_ACTION.bind(textureId, iconflag, iconSize, spacing);
}

inline auto RemoveIcon()
{
    return ui::actions::icon::REMOVE_ICON_ACTION.bind();
}

} // namespace ui::chains