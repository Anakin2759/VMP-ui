/**
 * ************************************************************************
 *
 * @file Tags.h
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2025-12-11
 * @brief UI ECS 标记组件定义 (纯空结构体 Tag)
 *
 * 用于标记UI元素的类型。
    -ButtonTag: 按钮标记
    -LabelTag: 文本标签标记
    -TextTag: 文本通用标记
    -TextEditTag: 文本输入框标记
    -ImageTag: 图像显示标记
    -WindowTag: 窗口标记
    -DialogTag: 对话框标记
    -SpacerTag: 间隔器标记
    -ArrowTag: 几何图形：箭头
    -LineTag: 几何图形：直线
    -ListAreaTag: 列表区域容器标记
    -TableTag: 表格容器标记
 *
 * ************************************************************************
 * @copyright Copyright (c) 2025 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */
#pragma once
#include <entt/entt.hpp>

namespace ui::components
{

// ===================== I. UI 类型标记 (用于 RenderSystem 视图查询) =====================

struct RootTag
{
    using is_tags_tag = void;
};

/**
 * @brief 按钮标记
 */
struct ButtonTag
{
    using is_tags_tag = void;
};

/**
 * @brief 文本标签标记
 */
struct LabelTag
{
    using is_tags_tag = void;
};

/**
 * @brief 文本通用标记（兼容旧渲染逻辑）
 */
struct TextTag
{
    using is_tags_tag = void;
};

/**
 * @brief 文本输入框标记
 */
struct TextEditTag
{
    using is_tags_tag = void;
};

/**
 * @brief 图像显示标记
 */
struct ImageTag
{
    using is_tags_tag = void;
};

/**
 * @brief 窗口标记 (通常是可移动/可关闭的主容器)
 */
struct WindowTag
{
    using is_tags_tag = void;
};

/**
 * @brief 对话框标记 (通常是模态/不可移动的浮动窗口)
 */
struct DialogTag
{
    using is_tags_tag = void;
};

/**
 * @brief 自绘标题栏标记 (用于无边框窗口的自定义标题栏)
 */
struct TitleBarTag
{
    using is_tags_tag = void;
};

/**
 * @brief 间隔器标记 (用于布局系统，无可见内容)
 */
struct SpacerTag
{
    using is_tags_tag = void;
};

/**
 * @brief 几何图形：箭头
 */
struct ArrowTag
{
    using is_tags_tag = void;
};

/**
 * @brief 几何图形：直线
 */
struct LineTag
{
    using is_tags_tag = void;
};

/**
 * @brief 列表区域容器标记
 */
struct ListAreaTag
{
    using is_tags_tag = void;
};

/**
 * @brief 表格容器标记
 */
struct TableTag
{
    using is_tags_tag = void;
};

// ===================== II. 行为与状态标记 (用于 Interaction/Animation/Layout System) =====================

/**
 * @brief 标记元素是可点击的 (逻辑标记，与 Clickable 组件配合)
 */
struct ClickableTag
{
    using is_tags_tag = void;
};

/**
 * @brief 标记元素是可拖动的
 */
struct DraggableTag
{
    using is_tags_tag = void;
};

/**
 * @brief 运行时状态：鼠标悬停标记
 * InteractionSystem 添加此 Tag，RenderSystem 监听此 Tag 来应用 Hover 样式
 */
struct HoveredTag
{
    using is_tags_tag = void;
};

/**
 * @brief 运行时状态：鼠标激活/按下标记
 */
struct ActiveTag
{
    using is_tags_tag = void;
};

/**
 * @brief 运行时状态：元素被禁用标记
 */
struct DisabledTag
{
    using is_tags_tag = void;
};

/**
 * @brief 运行时状态：输入焦点标记
 */
struct FocusedTag
{
    using is_tags_tag = void;
};

/**
 * @brief 运行时状态：元素可见标记
 * 默认情况下元素是可见的，只有明确需要隐藏时才移除此 Tag
 */
struct VisibleTag
{
    using is_tags_tag = void;
};

/**
 * @brief 布局脏标记：标记此容器或其子元素的位置/尺寸需要重新计算
 * InteractionSystem 或 SizeSystem 触发，LayoutSystem 监听
 */
struct LayoutDirtyTag
{
    using is_tags_tag = void;
};

struct RenderDirtyTag
{
    using is_tags_tag = void;
};

struct AnimatingTag
{
    using is_tags_tag = void;
};

/**
 * @brief Slider 标记
 */
struct SliderTag
{
    using is_tags_tag = void;
};

/**
 * @brief ProgressBar 标记
 */
struct ProgressBarTag
{
    using is_tags_tag = void;
};

} // namespace ui::components