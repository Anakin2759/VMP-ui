#pragma once

#include <type_traits>

namespace ui::traits
{

// ===================== 元编程基础工具 =====================

/**
 * @brief 类型列表容器
 */
template <typename... Ts>
struct TypeList
{
};

/**
 * @brief 检查类型是否存在于列表中 (变量模板)
 */
template <typename T, typename List>
inline constexpr bool contains_v = false;

template <typename T, typename... Ts>
inline constexpr bool contains_v<T, TypeList<Ts...>> = (std::is_same_v<T, Ts> || ...);

} // namespace ui::traits
