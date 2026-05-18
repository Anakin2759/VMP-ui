/**
 * ************************************************************************
 *
 * @file CompenentsTraits.h
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2026-01-23
 * @version 0.1
 * @brief 利用模板元编程实现组件特性检测
 *
 * ************************************************************************
 * @copyright Copyright (c) 2026 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */

#pragma once

#include "../common/Components.hpp"
#include "../common/Tags.hpp"

namespace ui::traits
{

template <typename T>
concept Component = requires { typename T::is_component_tag; };

template <typename T>
concept UiTag = requires { typename T::is_tags_tag; };

template <typename T>
concept ComponentOrUiTag = Component<T> || UiTag<T>;

} // namespace ui::traits
