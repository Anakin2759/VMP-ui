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
#include "traits/PoliciesTraits.hpp"

namespace ui::policies
{
/**
 * @brief 系统管理器枚举
 */
enum class SystemManager : uint16_t
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
    H_FIXED = 1 << 0,      // 水平固定
    H_AUTO = 1 << 1,       // 水平自动
    H_FILL = 1 << 2,       // 水平填充 (父容器)
    H_PERCENTAGE = 1 << 3, // 水平百分比

    // 垂直方向策略 (Vertical)
    V_FIXED = 1 << 4,      // 垂直固定
    V_AUTO = 1 << 5,       // 垂直自动
    V_FILL = 1 << 6,       // 垂直填充 (父容器)
    V_PERCENTAGE = 1 << 7, // 垂直百分比

    // 双向组合 (Common Combinations)
    FIXED = H_FIXED | V_FIXED,
    AUTO = H_AUTO | V_AUTO,
    FILL_PARENT = H_FILL | V_FILL,
    PERCENTAGE = H_PERCENTAGE | V_PERCENTAGE,

    // 混合示例
    H_FIXED_V_AUTO = H_FIXED | V_AUTO,
    H_AUTO_V_FIXED = H_AUTO | V_FIXED,
    H_FILL_V_AUTO = H_FILL | V_AUTO
};

enum class Feature : uint8_t
{
    DISABLED,
    ENABLED
};

enum class Visibility : uint8_t
{
    VISIBLE,
    HIDDEN,
    COLLAPSED
};
/**
 * @brief 文本换行模式枚举
 */
enum class TextWrap : uint8_t
{
    NONE,
    WORD, // 按单词换行
    CHAR  // 按字符换行
};

enum class TextFlag : uint16_t
{
    DEFAULT = 0,
    PASSWORD = 1 << 0,  // 掩码显示
    READ_ONLY = 1 << 1, // 不可编辑
    MULTILINE = 1 << 2, // 支持多行渲染
    READ_ONLY_MULTILINE = (1U << 1U) | (1U << 2U),
    TRANSFERABLE = 1 << 3,           // 支持文本拖拽/复制
    RICH_TEXT = 1 << 4,              // 启用富文本/标记语言解析
    NO_WRAP = 1 << 5,                // 强制不换行
    ANSI = 1 << 6,                   // 启用 ANSI 转义码解析
    UNDERLINE = 1 << 7,              // 启用下划线
    WORD_WRAP = 1U << 8U | 0U << 9U, // 启用按单词换行
    CHAR_WRAP = 0U << 8U | 1U << 9U, // 启用按字符换行
    NONE_WRAP = 0U << 8U | 0U << 9U, // 不换行
};

/**
 * @brief  窗口位置策略枚举
 */
enum class AspectRatio : uint8_t
{
    IGNORE_RATIO, // 忽略
    MAINTAIN      // 保持比例
};

enum class CheckState : uint8_t
{
    UNCHECKED,
    CHECKED,
    INDETERMINATE
};

enum class Orientation : uint8_t
{
    HORIZONTAL,
    VERTICAL
};

enum class Selection : uint8_t
{
    SINGLE,
    MULTI
};

enum class SortOrder : uint8_t
{
    NONE,
    ASCENDING,
    DESCENDING
};

/**
 * @brief 表格列宽分配策略
 */
enum class TableColumnSizing : uint8_t
{
    EQUAL,        ///< 均等：所有列等宽，总和 == 可见宽度（默认）
    FIXED,        ///< 固定：columnWidths 直接使用，超出时横向滚动
    PROPORTIONAL, ///< 比例：columnWidths 作为权重按可见宽度分配（应用 minColumnWidths 下界）
    ADAPTIVE,     ///< 自适应：columnWidths>0 的列固定宽度，其余列均分剩余空间
};

enum class AnimationState : uint8_t
{
    STOPPED,
    PLAYING
};

enum class LabelVisibility : uint8_t
{
    HIDDEN,
    VISIBLE
};

/**
 * @brief 位置策略 - 用于控制窗口/容器的定位方式
 */
enum class Position : uint8_t
{
    DEFAULT = 0,         // 默认行为（由布局系统决定）
    V_FIXED = 1 << 0,    // 使用 Position 组件中的固定位置
    V_CENTER = 1 << 1,   // 在屏幕/父容器中居中
    V_AUTO = 1 << 2,     // 由布局系统自动计算
    V_ABSOLUTE = 1 << 3, // 绝对定位
    H_FIXED = 1 << 4,    // 使用 Position 组件中的固定位置
    H_CENTER = 1 << 5,   // 在屏幕/父容器中居中
    H_AUTO = 1 << 6,     // 由布局系统自动计算
    H_ABSOLUTE = 1 << 7, // 绝对定位

    AUTO = V_AUTO | H_AUTO,
    CENTER = V_CENTER | H_CENTER,
    ABSOLUTE_POS = V_ABSOLUTE | H_ABSOLUTE,
    FIXED = V_FIXED | H_FIXED
};

enum class Scroll : uint8_t
{
    NO_SCROLL,  // 不滚动
    VERTICAL,   // 垂直滚动
    HORIZONTAL, // 水平滚动
    BOTH,       // 双向滚动
};

enum class ScrollBar : uint8_t
{
    DEFAULT = 0,
    NO_VISIBILITY = 1 << 0,
    DRAGGABLE = 1 << 1,
    AUTO_HIDE = 1 << 2

};

/**
 * @brief 滚动内容更新时的锚定策略
 */
enum class ScrollAnchor : uint8_t
{
    TOP,    // 锚定顶部：内容尺寸变化时，保持顶部偏移量不变 (适用于普通文档/输入框，尾部追加内容时视口不动)
    BOTTOM, // 锚定底部：内容尺寸变化时，保持底部偏移量不变 (适用于聊天历史加载/日志跟随)
    SMART   // 智能锚定：如果滚动条在最底部，则锚定底部；否则锚定顶部
};

/**
 * @brief 窗口标志位 - 类似 Qt 的策略
 */
enum class WindowFlag : uint8_t
{
    DEFAULT = 0,
    NO_TITLE_BAR = 1 << 0,  // 无标题栏
    NO_RESIZE = 1 << 1,     // 禁止调整大小
    NO_MOVE = 1 << 2,       // 禁止移动
    NO_COLLAPSE = 1 << 3,   // 禁止折叠/最小化
    NO_BACKGROUND = 1 << 4, // 无背景
    NO_CLOSE = 1 << 5,      // 无关闭按钮
    MODAL = 1 << 6,         // 模态窗口
    HAS_TOOLBAR = 1 << 7,   // 有工具栏

    // 组合
    FRAMELESS = NO_TITLE_BAR | NO_RESIZE | NO_MOVE,
    DIALOG = MODAL | NO_COLLAPSE
};

/**
 * @brief 图标位置枚举 - 图标相对于文本的位置
 */
enum class IconFlag : uint8_t
{
    // --- 渲染类型 (Flags) ---
    DEFAULT = 0,
    TEXTURE = 1 << 0,  // 是贴图还是矢量字体
    HAS_TEXT = 1 << 1, // 是否携带文本标签
};

enum class Log : uint8_t
{
    SINGLE_FILE_R = 1 << 0,  // 只写单个日志文件（覆盖模式）
    SINGLE_FILE_RW = 1 << 1, // 读写单个日志文件（追加模式）
    TERMINAL = 1 << 1,
};
} // namespace ui::policies