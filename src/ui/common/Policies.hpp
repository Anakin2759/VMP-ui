/**
 * ************************************************************************
 *
 * @file Policies.h
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2025-12-05
 * @brief UI 相关的全局定义 (优化后)
 *
 * ************************************************************************
 */
#pragma once
#include <cstdint>
#include "../traits/PoliciesTraits.hpp"

namespace ui::policies
{
/**
 * @brief 系统管理器枚举
 */
enum class SystemManager : uint32_t // NOLINT(performance-enum-size)
{
    DISABLE_ALL = 0,
    INTERACTION = 1U << 0U,

    HIT_TEST = 1U << 1U,
    TWEEN = 1U << 2U,
    LAYOUT = 1U << 3U,
    RENDER = 1U << 4U,
    STATE = 1U << 5U,
    ACTION = 1U << 6U,
    TIMER = 1U << 7U,
    THEME = 1U << 8U,
    DEFAULT = INTERACTION | HIT_TEST | TWEEN | LAYOUT | RENDER | STATE | ACTION | TIMER | THEME
};
/**
 * @brief 布局方向枚举
 */
enum class LayoutDirection : uint8_t
{
    HORIZONTAL, // 0
    VERTICAL    // 1
};

/**
 * @brief 对齐方式枚举 (使用完整的位标记实现多重对齐)
 * 例如：CENTER = HCENTER | VCENTER
 */
enum class Alignment : uint8_t
{
    NONE = 0,

    // 水平对齐 (Horizontal Flags)
    LEFT = 1U << 0U,    // 0x01
    HCENTER = 1U << 1U, // 0x02
    RIGHT = 1U << 2U,   // 0x04

    // 垂直对齐 (Vertical Flags)
    TOP = 1U << 3U,     // 0x08
    VCENTER = 1U << 4U, // 0x10
    BOTTOM = 1U << 5U,  // 0x20

    // 组合常用对齐方式 (可选，但非常实用)
    CENTER = HCENTER | VCENTER, // 0x12
    TOP_LEFT = TOP | LEFT,      // 0x09
};

/**
 * @brief 动画播放模式枚举
 */
enum class Play : uint8_t
{
    ONCE,    // 单次播放
    LOOP,    // 循环
    PINGPONG // 往返
};

/**
 * @brief 动画缓动类型枚举 (使用专业名称)
 */
enum class Easing : uint8_t
{
    LINEAR, // 线性

    // 常用缓动曲线
    EASE_IN_SINE,     // 正弦缓入
    EASE_OUT_SINE,    // 正弦缓出
    EASE_IN_OUT_SINE, // 正弦缓入缓出

    EASE_IN_QUAD,     // 二次方缓入
    EASE_OUT_QUAD,    // 二次方缓出
    EASE_IN_OUT_QUAD, // 二次方缓入缓出

    CUSTOM // 自定义 (例如通过函数指针 Component)
};

enum class Focus : uint8_t
{
    NOFOCUS,     // 不接受焦点
    TAB_FOCUS,   // 通过 Tab 键接受焦点
    CLICK_FOCUS, // 通过鼠标点击接受焦点
    STRONG_FOCUS // 通过 Tab 键和鼠标点击均可接受焦点
};

/**
 * @brief 尺寸策略枚举 (按位策略，区分水平和垂直)
 */
enum class Size : uint8_t
{
    NONE = 0,

    // 水平方向策略 (Horizontal)
    HFixed = 1 << 0,      // 水平固定
    HAuto = 1 << 1,       // 水平自动
    HFill = 1 << 2,       // 水平填充 (父容器)
    HPercentage = 1 << 3, // 水平百分比

    // 垂直方向策略 (Vertical)
    VFixed = 1 << 4,      // 垂直固定
    VAuto = 1 << 5,       // 垂直自动
    VFill = 1 << 6,       // 垂直填充 (父容器)
    VPercentage = 1 << 7, // 垂直百分比

    // 双向组合 (Common Combinations)
    Fixed = HFixed | VFixed,
    Auto = HAuto | VAuto,
    FillParent = HFill | VFill,
    Percentage = HPercentage | VPercentage,

    // 混合示例
    HFixedVAuto = HFixed | VAuto,
    HAutoVFixed = HAuto | VFixed,
    HFillVAuto = HFill | VAuto
};

enum class Feature : uint8_t
{
    Disabled,
    Enabled
};

enum class Visibility : uint8_t
{
    Visible,
    Hidden,
    Collapsed
};
/**
 * @brief 文本换行模式枚举
 */
enum class TextWrap : uint8_t
{
    NONE,
    Word, // 按单词换行
    Char  // 按字符换行
};

enum class TextFlag : uint32_t
{
    Default = 0,
    Password = 1 << 0,               // 掩码显示
    ReadOnly = 1 << 1,               // 不可编辑
    Multiline = 1 << 2,              // 支持多行渲染
    Transferable = 1 << 3,           // 支持文本拖拽/复制
    RichText = 1 << 4,               // 启用富文本/标记语言解析
    NoWrap = 1 << 5,                 // 强制不换行
    Ansi = 1 << 6,                   // 启用 ANSI 转义码解析
    Underline = 1 << 7,              // 启用下划线
    WORD_WRAP = 1U << 8U | 0U << 9U, // 启用按单词换行
    CHAR_WRAP = 0U << 8U | 1U << 9U, // 启用按字符换行
    NONE_WRAP = 0U << 8U | 0U << 9U, // 不换行
};

/**
 * @brief  窗口位置策略枚举
 */
enum class AspectRatio : uint8_t
{
    Ignore,  // 忽略
    Maintain // 保持比例
};

enum class CheckState : uint8_t
{
    Unchecked,
    Checked,
    Indeterminate
};

enum class Orientation : uint8_t
{
    Horizontal,
    Vertical
};

enum class Selection : uint8_t
{
    Single,
    Multi
};

enum class SortOrder : uint8_t
{
    NONE,
    Ascending,
    Descending
};

enum class AnimationState : uint8_t
{
    Stopped,
    Playing
};

enum class LabelVisibility : uint8_t
{
    Hidden,
    Visible
};

/**
 * @brief 位置策略 - 用于控制窗口/容器的定位方式
 */
enum class Position : uint8_t
{
    Default = 0,        // 默认行为（由布局系统决定）
    VFixed = 1 << 0,    // 使用 Position 组件中的固定位置
    VCenter = 1 << 1,   // 在屏幕/父容器中居中
    VAuto = 1 << 2,     // 由布局系统自动计算
    VAbsolute = 1 << 3, // 绝对定位
    HFixed = 1 << 4,    // 使用 Position 组件中的固定位置
    HCenter = 1 << 5,   // 在屏幕/父容器中居中
    HAuto = 1 << 6,     // 由布局系统自动计算
    HAbsolute = 1 << 7, // 绝对定位

    Auto = VAuto | HAuto,
    Center = VCenter | HCenter,
    Absolute = VAbsolute | HAbsolute,
    Fixed = VFixed | HFixed
};

enum class Scroll : uint8_t
{
    NONE,       // 不滚动
    Vertical,   // 垂直滚动
    Horizontal, // 水平滚动
    Both,       // 双向滚动
};

enum class ScrollBar : uint8_t
{
    Default = 0,
    NoVisibility = 1 << 0,
    Draggable = 1 << 1,
    AutoHide = 1 << 2

};

/**
 * @brief 滚动内容更新时的锚定策略
 */
enum class ScrollAnchor : uint8_t
{
    Top,    // 锚定顶部：内容尺寸变化时，保持顶部偏移量不变 (适用于普通文档/输入框，尾部追加内容时视口不动)
    Bottom, // 锚定底部：内容尺寸变化时，保持底部偏移量不变 (适用于聊天历史加载/日志跟随)
    Smart   // 智能锚定：如果滚动条在最底部，则锚定底部；否则锚定顶部
};

/**
 * @brief 窗口标志位 - 类似 Qt 的策略
 */
enum class WindowFlag : uint16_t
{
    Default = 0,
    NoTitleBar = 1 << 0,   // 无标题栏
    NoResize = 1 << 1,     // 禁止调整大小
    NoMove = 1 << 2,       // 禁止移动
    NoCollapse = 1 << 3,   // 禁止折叠/最小化
    NoBackground = 1 << 4, // 无背景
    NoClose = 1 << 5,      // 无关闭按钮
    Modal = 1 << 6,        // 模态窗口
    HasToolbar = 1 << 7,   // 有工具栏

    // 组合
    Frameless = NoTitleBar | NoResize | NoMove,
    Dialog = Modal | NoCollapse
};

/**
 * @brief 图标位置枚举 - 图标相对于文本的位置
 */
enum class IconFlag : uint8_t
{
    // --- 渲染类型 (Flags) ---
    Default = 0,
    Texture = 1 << 0, // 是贴图还是矢量字体
    HasText = 1 << 1, // 是否携带文本标签
};

enum class Log : uint16_t
{
    SingleFileR = 1 << 0,  // 只写单个日志文件（覆盖模式）
    SingleFileRW = 1 << 1, // 读写单个日志文件（追加模式）
    Terminal = 1 << 1,
};
} // namespace ui::policies