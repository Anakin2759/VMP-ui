/**
 * ************************************************************************
 *
 * @file Components.h
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2025-12-05
 * @brief UI ECS 组件定义 (完整且优化版)
 *
 * 遵循ECS模式：纯数据结构，无行为逻辑，只包含属性,不含状态
 * 确保组件的纯粹性和独立性
 *
 * ************************************************************************
 * @copyright Copyright (c) 2025 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */
#pragma once

#include "Types.hpp"
#include <string>
#include <vector>
#include <functional>
#include <cfloat>
#include <entt/entt.hpp>
#include "Policies.hpp"

namespace ui::components
{

template <typename... Args>
using on_event = std::move_only_function<void(Args...)>;

// ===================== 基本信息组件 =====================
struct BaseInfo
{
    using is_component_tag = void;
    std::string alias; // 组件别名，便于调试和识别
};

/**
 * @brief 缩放组件（渲染时生效，不影响布局）
 */
struct Scale
{
    using is_component_tag = void;
    Vec2 value{1.0F, 1.0F};
};

/**
 * @brief 渲染偏移组件（渲染时生效，不影响布局）
 * 用于实现按下下沉、悬浮上浮等视觉效果
 */
struct RenderOffset
{
    using is_component_tag = void;
    Vec2 value{0.0F, 0.0F};
};

/**
 * @brief 透明度组件（叠加）
 */
struct Alpha
{
    using is_component_tag = void;
    float value = 1.0F;
};

// ===================== 基础尺寸与位置 =====================

/**
 * @brief 尺寸约束组件
 */
struct Size
{
    using is_component_tag = void;
    Vec2 size{0.0F, 0.0F};
    Vec2 minSize{0.0F, 0.0F};
    Vec2 maxSize{FLT_MAX, FLT_MAX};
    policies::Size sizePolicy = policies::Size::Auto; // 宽度策略

    float percentage = 1.0F; // 百分比值 (当策略为 Percentage 时使用，0.0-1.0)
};

/**
 * @brief UI 元素的相对位置组件
 */
struct Position
{
    using is_component_tag = void;
    Vec2 value{0.0F, 0.0F};
    policies::Position positionPolicy = policies::Position::Fixed;
};

/**
 * @brief UI画布/屏幕尺寸（
 *
 * 该组件推荐存放在 registry.ctx() 中，供 LayoutSystem 等逻辑系统读取，
 */
struct CanvasSize
{
    using is_component_tag = void;
    Vec2 value{0.0F, 0.0F};
};

/**
 * @brief 边距组件 (外边距)
 */
struct Margin
{
    using is_component_tag = void;
    Vec4 values{0.0F, 0.0F, 0.0F, 0.0F}; // Top, Right, Bottom, Left
};

/**
 * @brief 内边距组件
 * 定义元素内容与其边框之间的空间。
 */
struct Padding
{
    using is_component_tag = void;
    Vec4 values{0.0F, 0.0F, 0.0F, 0.0F}; // Top, Right, Bottom, Left
};

/**
 * @brief 背景绘制组件
 */
struct Background
{
    using is_component_tag = void;
    Color color{0.0F, 0.0F, 0.0F, 0.0F};
    Vec4 borderRadius{0.0F, 0.0F, 0.0F, 0.0F}; // 圆角半径 (x:TopLeft, y:TopRight, z:BottomRight, w:BottomLeft)
    policies::Feature enabled = policies::Feature::Disabled; // 是否启用背景绘制
};

/**
 * @brief 边框组件
 */
struct Border
{
    using is_component_tag = void;
    Color color{1.0F, 1.0F, 1.0F, 1.0F};
    float thickness = 1.0F;
    Vec4 borderRadius{0.0F, 0.0F, 0.0F, 0.0F}; // 圆角半径
    policies::Feature enabled = policies::Feature::Disabled;
};

/**
 * @brief 阴影组件
 */
struct Shadow
{
    using is_component_tag = void;
    float softness{};                    // 阴影柔和度
    Vec2 offset{0.0F, 0.0F};             // 阴影偏移 (x, y)
    Color color{0.0F, 0.0F, 0.0F, 1.0F}; // 阴影颜色
    policies::Feature enabled = policies::Feature::Disabled;
};

// ===================== 层级与滚动 =====================

/**
 * @brief 层级关系组件 - Parent-Children 结构
 */
struct Hierarchy
{
    using is_component_tag = void;
    entt::entity parent = entt::null;
    std::vector<entt::entity> children; // 存储所有子节点
};
/**
 * @brief Z轴顺序组件 - 控制元素的前后显示顺序
 */
struct ZOrderIndex
{
    using is_component_tag = void;
    int value = 0; // Z 顺序值，值越大越靠前
};

/**
 * @brief 滚动区域组件
 */
struct ScrollArea
{
    using is_component_tag = void;
    static constexpr float DEFAULT_SCROLL_SPEED = 10.0F;
    static constexpr float SCROLLBAR_TRACK_WIDTH = 12.0F;
    static constexpr float SCROLLBAR_TRACK_PADDING = 2.0F;
    static constexpr float SCROLLBAR_THUMB_WIDTH = 10.0F;
    static constexpr float SCROLLBAR_THUMB_MIN_SIZE = 20.0F;
    static constexpr float SCROLLBAR_THUMB_INSET = 2.0F;
    Vec2 scrollOffset{0.0F, 0.0F};                        // 当前滚动位置
    Vec2 contentSize{0.0F, 0.0F};                         // 内容区域大小
    float scrollSpeed{DEFAULT_SCROLL_SPEED};              // 滚动速度
    policies::Scroll scroll = policies::Scroll::Vertical; // 滚动方向
    policies::ScrollBar scrollBar = policies::ScrollBar::Draggable;
    policies::ScrollAnchor anchor = policies::ScrollAnchor::Top; // 滚动锚定策略

    // 滚动条交互状态
    bool scrollbarHovered{false}; // 滚动条滑块是否悬停
    bool scrollbarPressed{false}; // 滚动条滑块是否按下
    bool trackHovered{false};     // 滚动条轨道是否悬停
};

struct VerticalScrollbarGeometry
{
    using is_component_tag = void;
    bool visible = false;
    Rect containerRect;
    Rect viewportRect;
    Rect trackRect;
    Rect thumbRect;
    float viewportHeight = 0.0F;
    float contentHeight = 0.0F;
    float trackHeight = 0.0F;
    float thumbHeight = 0.0F;
    float maxScroll = 0.0F;
};

// ===================== 布局组件 =====================

/**
 * @brief 布局容器配置组件
 */
struct LayoutInfo
{
    using is_component_tag = void;
    static constexpr float DEFAULT_SPACING = 5.0F;
    policies::LayoutDirection direction = policies::LayoutDirection::HORIZONTAL; // 布局方向
    policies::Alignment alignment = policies::Alignment::CENTER;                 // 元素对齐方式
    float spacing = DEFAULT_SPACING;                                             // 元素间距
};

/**
 * @brief 间隔器配置组件
 */
struct Spacer
{
    using is_component_tag = void;
    uint8_t stretchFactor = 1; // 默认拉伸因子
};

// ===================== 文本组件 =====================

/**
 * @brief 文本内容组件
 */
struct Text
{
    using is_component_tag = void;
    std::string content; // 文本内容
    Color color{1.0F, 1.0F, 1.0F, 1.0F};
    float fontSize = 24.0F; // 0 表示使用默认字体大小

    float wrapWidth = 0.0F;                                    // 换行宽度，0 表示不换行
    float lineHeight = 1.2F;                                   // 行高倍数
    float letterSpacing = 0.0F;                                // 字符间距（像素或倍数，取决于渲染实现）
    policies::TextWrap wordWrap = policies::TextWrap::NONE;    // 默认不换行
    policies::Alignment alignment = policies::Alignment::NONE; // 默认居中
    policies::TextFlag flags = policies::TextFlag::Default;    // 其他文本属性
};

/**
 * @brief 文本编辑框数据组件
 */
struct TextEdit
{
    using is_component_tag = void;
    static constexpr size_t MAX_LENGTH = 1024;
    std::string buffer;      // 存储输入文本的缓冲区
    std::string placeholder; // 占位符文本
    Color textColor{1.0F, 1.0F, 1.0F, 1.0F};
    size_t maxLength = MAX_LENGTH;
    policies::TextFlag inputMode = policies::TextFlag::Default;

    // Cursor and selection
    size_t cursorPosition = 0; // 光标位置（字节索引）
    size_t selectionStart = 0; // 选择起始位置（字节索引）
    size_t selectionEnd = 0;   // 选择结束位置（字节索引）
    bool hasSelection = false; // 是否有选中内容

    // Callbacks
    on_event<> onSubmit;                        // 按回车键时触发（单行模式）
    on_event<const std::string&> onTextChanged; // 文本内容改变时触发
};

// ===================== 图像组件 =====================

/**
 * @brief 图像组件
 */
struct Image
{
    using is_component_tag = void;
    void* textureId = nullptr;                 // 纹理句柄 (例如 SDL_Texture* 或 OpenGL ID)
    Vec2 uvMin{0.0F, 0.0F};                    // UV 最小坐标
    Vec2 uvMax{1.0F, 1.0F};                    // UV 最大坐标
    Color tintColor{1.0F, 1.0F, 1.0F, 1.0F};   // 颜色叠加
    Color borderColor{0.0F, 0.0F, 0.0F, 0.0F}; // 边框颜色
    policies::AspectRatio maintainAspectRatio = policies::AspectRatio::Maintain;
};

// ===================== 交互与状态 =====================

/**
 * @brief 可点击组件
 */
struct Clickable
{
    using is_component_tag = void;
    on_event<> onClick;
    policies::Feature enabled = policies::Feature::Enabled;
};

/**
 * @brief 可拖拽组件
 * 标记实体可被鼠标拖拽移动
 */
struct Draggable
{
    using is_component_tag = void;
    policies::Feature enabled = policies::Feature::Enabled;
    bool lockX = false; // 锁定X轴
    bool lockY = false; // 锁定Y轴
    bool dragging = false;

    // 拖拽开始/结束的回调
    on_event<> onDragStart;
    on_event<> onDragEnd;
    on_event<Vec2> onDragMove; // 参数为 delta
};

/**
 * @brief 可悬浮组件
 */
struct Hoverable
{
    using is_component_tag = void;
    on_event<> onHover;
    on_event<> onUnhover;
    policies::Feature enabled = policies::Feature::Enabled;
};

/**
 * @brief 可按压组件 - 用于处理长按、拖动等场景
 */
struct Pressable
{
    using is_component_tag = void;
    on_event<> onPress;   // 鼠标按下时触发
    on_event<> onRelease; // 鼠标松开时触发
    policies::Feature enabled = policies::Feature::Enabled;
};

/**
 * @brief 可选中组件
 */
struct Checkable
{
    using is_component_tag = void;
    policies::CheckState checked = policies::CheckState::Unchecked;
};

// ===================== 动画组件 =====================

/**
 * @brief 动画时间状态 (单位: 毫秒 ms)
 */
struct AnimationTime
{
    using is_component_tag = void;
    float duration = 200.0F; // 默认200毫秒
    float elapsed = 0.0F;
    policies::Easing easing = policies::Easing::LINEAR;
    policies::Play mode = policies::Play::ONCE;
    policies::AnimationState state = policies::AnimationState::Playing;
    bool autoCleanup = true;
};

/**
 * @brief 位置动画目标
 */
struct AnimationPosition
{
    using is_component_tag = void;
    Vec2 from;
    Vec2 to;
};

/**
 * @brief 透明度动画目标
 */
struct AnimationAlpha
{
    using is_component_tag = void;
    float from = 1.0F;
    float to = 0.0F;
};

/**
 * @brief 缩放动画目标
 */
struct AnimationScale
{
    using is_component_tag = void;
    Vec2 from{1.0F, 1.0F};
    Vec2 to{1.0F, 1.0F};
};

/**
 * @brief 渲染偏移动画目标
 */
struct AnimationRenderOffset
{
    using is_component_tag = void;
    Vec2 from{0.0F, 0.0F};
    Vec2 to{0.0F, 0.0F};
};

/**
 * @brief 颜色动画目标
 * 支持驱动常见可着色组件（如 Background、Text、Image、Icon 等）
 */
struct AnimationColor
{
    using is_component_tag = void;
    Color from;
    Color to;
};

/**
 * @brief 交互动效配置
 * 当发生交互时，自动触发 Tween 动画
 */
struct InteractiveAnimation
{
    using is_component_tag = void;

    // 悬停配置
    std::optional<Vec2> hoverScale;
    std::optional<Vec2> hoverOffset; // 悬停位移
    float hoverDuration = 200.0F;    // 毫秒

    // 按下配置
    std::optional<Vec2> pressScale;
    std::optional<Vec2> pressOffset; // 按下位移
    float pressDuration = 100.0F;    // 毫秒

    // 拖曳配置 (当组件支持拖拽时)
    std::optional<Vec2> dragScale;
    std::optional<Vec2> dragLiftOffset; // 拖起时的视觉偏移
    float dragDuration = 200.0F;

    // 原始状态记录（用于恢复）
    Vec2 normalScale{1.0F, 1.0F};
    Vec2 normalOffset{0.0F, 0.0F};
};
// ===================== 复杂组件数据 =====================

/**
 * @brief 窗口组件
 */
struct Window
{
    using is_component_tag = void;
    static constexpr float MIN_WID = 300.0F;
    static constexpr float MIN_HIG = 200.0F;
    std::string title;
    Vec2 minSize{MIN_WID, MIN_HIG};
    Vec2 maxSize{FLT_MAX, FLT_MAX};
    policies::WindowFlag flags = policies::WindowFlag::Default;
    uint32_t windowID = 0;
};

/**
 * @brief 箭头组件
 */
struct Arrow
{
    using is_component_tag = void;
    static constexpr float DEFAULT_THICKNESS = 2.0F;
    static constexpr float DEFAULT_ARROW_SIZE = 10.0F;
    Vec2 startPoint{0.0F, 0.0F};
    Vec2 endPoint{100.0F, 100.0F};
    Color color{1.0F, 1.0F, 1.0F, 1.0F};
    float thickness = DEFAULT_THICKNESS;
    float arrowSize = DEFAULT_ARROW_SIZE;
};

/**
 * @brief 列表区域组件
 */
struct ListArea
{
    using is_component_tag = void;
    static constexpr float DEFAULT_ITEM_HEIGHT = 30.0F;
    std::vector<entt::entity> items;
    std::vector<int> selectedIndices;
    float itemHeight = DEFAULT_ITEM_HEIGHT;
    int selectedIndex = -1;
    policies::Selection multiSelect = policies::Selection::Single;
};

/**
 * @brief 表格组件
 */
struct TableInfo
{
    using is_component_tag = void;
    std::vector<std::string> headers;
    std::vector<std::vector<std::string>> rows;
    std::vector<float> columnWidths;
    int sortColumn = -1;
    policies::Feature resizable = policies::Feature::Enabled;
    policies::Feature sortable = policies::Feature::Disabled;
    policies::SortOrder sortAscending = policies::SortOrder::Ascending;
};

/**
 * @brief 线条组件
 */
struct LineInfo
{
    using is_component_tag = void;
    static constexpr float DEFAULT_THICKNESS = 2.0F;
    Vec2 startPoint{0.0F, 0.0F};
    Vec2 endPoint{100.0F, 0.0F};
    Vec4 color{1.0F, 1.0F, 1.0F, 1.0F};
    float thickness = DEFAULT_THICKNESS;
};

// ===================== 渲染与状态组件 =====================

/**
 * @brief 标题组件 (通常用于 Window/Dialog)
 */
struct Title
{
    using is_component_tag = void;
    std::string text;
};

/**
 * @brief 可选中/目标化组件
 */
struct Targetable
{
    using is_component_tag = void;
    int priority = 0;
    policies::Feature selectable = policies::Feature::Disabled;
};

/**
 * @brief 滑块组件信息
 */
struct SliderInfo
{
    using is_component_tag = void;
    float minValue = 0.0F; // 最小值
    float maxValue = 1.0F; // 最大值
    float currentValue = 0.0F;

    // 当前值
    float step = 0.0F;                                                  // 步长，0 表示连续滑动
    policies::Orientation vertical = policies::Orientation::Horizontal; // 是否为垂直滑块
    on_event<float> onValueChanged;                                     // 值变化回调
    policies::Alignment labelAlignment = policies::Alignment::NONE;     // 标签对齐方式
};

/**
 * @brief 滚动条组件
 */
struct ScrollBar
{
    using is_component_tag = void;
    static constexpr float MIN_THUMB_SIZE = 20.0F;
    static constexpr float DEFAULT_WIDTH = 12.0F;
    float scrollPosition = 0.0F;                                      // 当前滚动位置 (0.0 - 1.0)
    float viewportSize = 1.0F;                                        // 可见区域占总内容的比例 (0.0 - 1.0)
    float thumbSize = MIN_THUMB_SIZE;                                 // 滑块大小（像素）
    float trackSize = 0.0F;                                           // 轨道总长度（像素）
    policies::Orientation vertical = policies::Orientation::Vertical; // 是否为垂直滚动条
    policies::Visibility autoHide = policies::Visibility::Visible;    // 无需滚动时自动隐藏
    bool dragging = false;                                            // 是否正在拖动滑块
    Color thumbColor{0.5F, 0.5F, 0.5F, 0.8F};                         // 滑块颜色
    Color trackColor{0.2F, 0.2F, 0.2F, 0.5F};                         // 轨道颜色
    on_event<float> onScroll;                                         // 滚动回调
};

/**
 * @brief 进度条组件
 */
struct ProgressBar
{
    using is_component_tag = void;
    float progress = 0.0F;                                                    // 进度值 (0.0 - 1.0)
    Color fillColor{0.2F, 0.6F, 1.0F, 1.0F};                                  // 填充颜色
    Color backgroundColor{0.3F, 0.3F, 0.3F, 1.0F};                            // 背景颜色
    policies::LabelVisibility showLabel = policies::LabelVisibility::Visible; // 是否显示百分比标签
    policies::AnimationState animated = policies::AnimationState::Stopped;    // 是否启用动画效果
};

/**
 * @brief 图标组件 - 用于为控件添加图标装饰
 * 通常附加到 Button、Label 等控件上
 * 支持两种类型：纹理图标（Image）和字体图标（IconFont）
 */
struct Icon
{
    using is_component_tag = void;
    static constexpr float DEFAULT_SIZE = 16.0F;
    static constexpr float DEFAULT_SPACING = 4.0F;

    policies::IconFlag type = policies::IconFlag::Default; // 图标类型

    // 纹理图标相关字段（type == Texture 时使用）
    std::string textureId;  // 图标纹理ID
    Vec2 uvMin{0.0F, 0.0F}; // UV 最小坐标
    Vec2 uvMax{1.0F, 1.0F}; // UV 最大坐标

    // 字体图标相关字段（type == Font 时使用）
    void* fontHandle = nullptr; // IconFont 字体句柄
    uint32_t codepoint = 0;     // Unicode 码点（如 0xF015 表示 home 图标）

    // 通用字段
    Vec2 size{DEFAULT_SIZE, DEFAULT_SIZE};                     // 图标尺寸
    policies::IconFlag iconflag = policies::IconFlag::Default; // 相对于文本的位置
    float spacing = DEFAULT_SPACING;                           // 与文本的间距
    Color tintColor{1.0F, 1.0F, 1.0F, 1.0F};                   // 图标颜色
};

/**
 * @brief 日历组件
 */
struct Calendar
{
    using is_component_tag = void;
    int year = 1970;
    int month = 1; // 1-12
    int day = 1;   // 1-31
};

/**
 * @brief 插入符组件
 */
struct Caret
{
    using is_component_tag = void;
    float blinkInterval = 0.5F; // 闪烁间隔（秒）
    float elapsedTime = 0.0F;   // 用于闪烁动画的累计时间
    bool visible = true;
};

/**
 * @brief 菜单组件
 */
struct Menu
{
    using is_component_tag = void;
};

/**
 * @brief 复选框组件
 */
struct CheckBox
{
    using is_component_tag = void;
};

} // namespace ui::components