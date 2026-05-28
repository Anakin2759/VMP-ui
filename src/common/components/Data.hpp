/**
 * @file Data.hpp
 * @brief 复杂数据/控件组件（文本、图像、表格、控件状态等）
 *
 * 包含：BaseInfo / Text / TextEdit / Image / ImageSource / Icon /
 *       ListArea / TableCell / TableInfo / SliderInfo / ScrollBar /
 *       ProgressBar / Calendar / Caret / Menu / CheckBox / DropDown /
 *       CanvasDrawType / CanvasDrawCommand / CanvasDrawList
 */
#pragma once

#include "../Types.hpp"
#include "../Policies.hpp"
#include "Interaction.hpp" // on_event<>
#include <string>
#include <string_view>
#include <vector>
#include <algorithm>
#include <entt/entt.hpp>

namespace ui::components
{

// ===================== 基本信息组件 =====================

struct BaseInfo
{
    using is_component_tag = void;
    std::string alias; // 组件别名，便于调试和识别
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
    float letterSpacing = 0.0F;                                // 字符间距（像素或倍数）
    policies::TextWrap wordWrap = policies::TextWrap::NONE;    // 默认不换行
    policies::Alignment alignment = policies::Alignment::NONE; // 默认居中
    policies::TextFlag flags = policies::TextFlag::DEFAULT;    // 其他文本属性
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
    policies::TextFlag inputMode = policies::TextFlag::DEFAULT;

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
    policies::AspectRatio maintainAspectRatio = policies::AspectRatio::MAINTAIN;
};

/// 图像文件来源组件——指定图像路径，由 ImageManager 异步/同步加载
struct ImageSource
{
    using is_component_tag = void;
    std::string path;        // 文件路径（bmp/png/jpeg）
    bool loaded = false;     // 是否已加载完毕
    bool loadFailed = false; // 加载是否失败
};

// ===================== 图标组件 =====================

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

    policies::IconFlag type = policies::IconFlag::DEFAULT; // 图标类型

    // 纹理图标相关字段（type == Texture 时使用）
    std::string textureId;  // 图标纹理ID
    Vec2 uvMin{0.0F, 0.0F}; // UV 最小坐标
    Vec2 uvMax{1.0F, 1.0F}; // UV 最大坐标

    // 字体图标相关字段（type == Font 时使用）
    const void* fontHandle = nullptr; // IconFont 字体句柄
    uint32_t codepoint = 0;           // Unicode 码点（如 0xF015 表示 home 图标）

    // 通用字段
    Vec2 size{DEFAULT_SIZE, DEFAULT_SIZE};                     // 图标尺寸
    policies::IconFlag iconflag = policies::IconFlag::DEFAULT; // 相对于文本的位置
    float spacing = DEFAULT_SPACING;                           // 与文本的间距
    Color tintColor{1.0F, 1.0F, 1.0F, 1.0F};                   // 图标颜色
};

// ===================== 列表 / 表格 =====================

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
    policies::Selection multiSelect = policies::Selection::SINGLE;
};

/**
 * @brief 表格单元格数据
 */
struct TableCell
{
    std::string text;
    Color textColor{0.0F, 0.0F, 0.0F, 1.0F};
    Color bgColor{1.0F, 1.0F, 1.0F, 0.0F}; // alpha=0 表示使用默认背景
    /// 可选：嵌入该单元格的控件实体（entt::null 表示无控件）。
    /// 控件的 Position/Size 由 TableRenderer 根据单元格几何自动更新。
    entt::entity cellEntity{entt::null};
};

/**
 * @brief 表格组件（富版本）
 */
struct TableInfo
{
    using is_component_tag = void;
    int columnCount = 1;
    int rowCount = 0;
    std::vector<std::string> headers;          // 表头，size == columnCount
    std::vector<std::vector<TableCell>> cells; // cells[row][col]
    int selectedRow = -1;                      // -1 = 无选中
    Color headerBgColor{0.2F, 0.4F, 0.8F, 1.0F};
    Color headerTextColor{1.0F, 1.0F, 1.0F, 1.0F}; // 表头文字颜色
    Color rowBgColor{1.0F, 1.0F, 1.0F, 1.0F};
    Color altRowBgColor{0.94F, 0.94F, 0.94F, 1.0F};
    Color selectedRowBgColor{0.2F, 0.5F, 1.0F, 0.3F};
    Color gridColor{0.8F, 0.8F, 0.8F, 1.0F};
    Color cellTextColor{0.15F, 0.15F, 0.18F, 1.0F}; // 数据单元格文字颜色
    float rowHeight = 28.0F;
    float headerHeight = 32.0F;
    std::vector<float> columnWidths; ///< 每列宽度（或比例权重），空则均等分
    policies::TableColumnSizing columnSizing = policies::TableColumnSizing::EQUAL; ///< 列宽分配策略
    std::vector<float> minColumnWidths; ///< 各列最小宽度（可为空；不足 columnCount 时后补 0）
    float minRowHeight = 0.0F;          ///< 行最小高度（0 表示不限制）
};

// ===================== 控件状态 =====================

/**
 * @brief 滑块组件信息
 */
struct SliderInfo
{
    using is_component_tag = void;
    float minValue = 0.0F; // 最小值
    float maxValue = 1.0F; // 最大值
    float currentValue = 0.0F;

    float step = 0.0F;                                                  // 步长，0 表示连续滑动
    policies::Orientation vertical = policies::Orientation::HORIZONTAL; // 是否为垂直滑块
    on_event<float> onValueChanged;                                     // 值变化回调
    policies::Alignment labelAlignment = policies::Alignment::NONE;     // 标签对齐方式

    // ---- 外观配置 ----
    Color trackColor{0.28F, 0.28F, 0.28F, 1.0F}; // 轨道背景色
    Color fillColor{0.20F, 0.60F, 1.00F, 1.0F};  // 填充/进度色
    Color thumbColor{0.40F, 0.75F, 1.00F, 1.0F}; // 滑块圆形色
    float thumbSize = 14.0F;                     // 滑块直径（px）
    float thumbRadius = 7.0F;                    // 滑块圆角（通常 = thumbSize/2 为圆形）
    float trackThickness = 6.0F;                 // 轨道细轴厚度（px）
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
    policies::Orientation vertical = policies::Orientation::VERTICAL; // 是否为垂直滚动条
    policies::Visibility autoHide = policies::Visibility::VISIBLE;    // 无需滚动时自动隐藏
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
    policies::LabelVisibility showLabel = policies::LabelVisibility::VISIBLE; // 是否显示百分比标签
    policies::AnimationState animated = policies::AnimationState::STOPPED;    // 是否启用动画效果
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
    bool checked = false;
    std::string label;
    Color checkColor{0.20F, 0.60F, 1.0F, 1.0F};
    Color boxColor{0.25F, 0.25F, 0.30F, 1.0F};
    on_event<bool> onChanged;
};

/**
 * @brief 下拉框组件
 */
struct DropDown
{
    using is_component_tag = void;
    std::vector<std::string> options;
    int selectedIndex = 0;
    bool open = false;
    entt::entity popupEntity = entt::null; // 当前弹出列表实体（entt::null 表示已关闭）
    Color arrowColor{0.70F, 0.70F, 0.75F, 1.0F};
    on_event<int> onChanged;

    [[nodiscard]] std::string_view selectedText() const
    {
        if (options.empty()) return {};
        int idx = std::clamp(selectedIndex, 0, static_cast<int>(options.size()) - 1);
        return options[static_cast<std::size_t>(idx)];
    }
};

struct DropDownPopupPanel
{
    using is_component_tag = void;
    entt::entity owner = entt::null;
};

struct DropDownPopupItem
{
    using is_component_tag = void;
    entt::entity owner = entt::null;
    int optionIndex = -1;
};

// ===================== Canvas 绘图组件 =====================

enum class CanvasDrawType : uint8_t
{
    LINE,
    RECT,
    FILLED_RECT,
    CIRCLE,
    FILLED_CIRCLE,
    POLYLINE,    // 折线：points 存储顶点序列，依次连接
    CUBIC_BEZIER // 三次贝塞尔曲线：p1=起点, p2=控制点1, p3=控制点2, p4=终点
};

struct CanvasDrawCommand
{
    CanvasDrawType type = CanvasDrawType::LINE;
    Vec2 p1{0.0F, 0.0F}; // 起点 / 矩形左上 / 圆心 / 贝塞尔起点
    Vec2 p2{0.0F, 0.0F}; // 终点 / 矩形右下 / (p2.x=半径) / 贝塞尔控制点1
    Vec2 p3{0.0F, 0.0F}; // 贝塞尔控制点2（仅 CUBIC_BEZIER）
    Vec2 p4{0.0F, 0.0F}; // 贝塞尔终点（仅 CUBIC_BEZIER）
    Color color{1.0F, 1.0F, 1.0F, 1.0F};
    float lineWidth = 1.0F;
    std::vector<Vec2> points; // 顶点列表（仅 POLYLINE）
};

struct CanvasDrawList
{
    using is_component_tag = void;
    std::vector<CanvasDrawCommand> commands;
};

} // namespace ui::components
