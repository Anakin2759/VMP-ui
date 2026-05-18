/**
 * ************************************************************************
 *
 * @file Hierarchy.hpp
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2026-01-27
 * @version 0.1
 * @brief 层级关系API封装
  - 提供添加/移除子元素的接口
  - 支持遍历子元素的功能
  - 基于ECS组件实现层级关系管理
  - 简化UI元素的层级操作逻辑
 *
 * ************************************************************************
 * @copyright Copyright (c) 2026 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */
#pragma once

#include <entt/entt.hpp>
#include "../singleton/Registry.hpp"
#include "../common/Components.hpp"
#include "Chains.hpp" // Changed: Include Chains.hpp for DSL

namespace ui::hierarchy
{
/**
 * @brief 从父节点移除子节点
 * @param parent 父节点实体
 * @param child 子节点实体
 */
void RemoveChild(::entt::entity parent, ::entt::entity child);
/**
 * @brief 向父节点添加子节点
 * @param parent 父节点实体
 * @param child 子节点实体
 */
void AddChild(::entt::entity parent, ::entt::entity child);
/**
 * @brief 遍历子元素
 * @param parent 父实体
 * @param visitor 访问函数，接受子实体作为参数
 */
template <typename Func>
void TraverseChildren(::entt::entity parent, Func visitor)
{
    if (!Registry::Valid(parent)) return;

    const auto* hierarchy = Registry::TryGet<components::Hierarchy>(parent);
    if (!hierarchy || hierarchy->children.empty()) return;

    const auto childrenCopy = hierarchy->children;
    for (const ::entt::entity child : childrenCopy)
    {
        if (!Registry::Valid(child)) continue;
        TraverseChildren(child, visitor);
        visitor(child);
    }
}

} // namespace ui::hierarchy

namespace ui::actions
{
namespace hierarchy
{
inline constexpr EntityAction<&ui::hierarchy::AddChild> ADD_CHILD_ACTION{};
inline constexpr EntityAction<&ui::hierarchy::RemoveChild> REMOVE_CHILD_ACTION{};
} // namespace hierarchy
} // namespace ui::actions

namespace ui::chains
{
inline auto AddChild(::entt::entity child)
{
    return ui::actions::hierarchy::ADD_CHILD_ACTION.bind(child);
}
inline auto RemoveChild(::entt::entity child)
{
    return ui::actions::hierarchy::REMOVE_CHILD_ACTION.bind(child);
}
} // namespace ui::chains
