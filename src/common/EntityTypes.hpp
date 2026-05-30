/**
 * ************************************************************************
 *
 * @file EntityTypes.hpp
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2026-05-30
 * @version 0.1
 * @brief ui::entity 别名的单一权威定义。
 *
 * 定义放在 common 层，使 Events.hpp 等公共头文件可以用 ui::entity
 * 而不直接暴露 entt::entity 给外部消费者。
 *
 * ************************************************************************
 * @copyright Copyright (c) 2026 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */
#pragma once

#include <entt/entt.hpp>

namespace ui
{

/// @brief 实体句柄，与 entt::entity 完全等价。
/// 外部代码应使用此别名，避免直接依赖 EnTT 类型。
using entity = entt::entity;

/// @brief 空实体常量，等价于 entt::null。
inline constexpr entity null_entity = entt::null;

} // namespace ui
