/**
 * ************************************************************************
 *
 * @file Table.hpp
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2026-05-18
 * @version 0.1
 * @brief Table 组件 API — 行列操作和 Chain DSL
 *
 * ************************************************************************
 * @copyright Copyright (c) 2026 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */
#pragma once

#include <entt/entt.hpp>
#include <string>
#include <vector>
#include <functional>
#include "../common/Components.hpp"
#include "../common/Types.hpp"
#include "Chains.hpp"

namespace ui::table
{

/**
 * @brief 设置列数和可选表头
 * @param entity    表格实体
 * @param count     列数
 * @param headers   表头文字（可为空）
 */
void SetColumns(entt::entity entity, int count, std::vector<std::string> headers = {});

/**
 * @brief 设置每列固定宽度
 * @param widths 宽度列表，size 必须与 columnCount 一致
 */
void SetColumnWidths(entt::entity entity, std::vector<float> widths);

/**
 * @brief 在末尾追加一行
 * @param texts 每列文字，数量不足时自动补空
 */
void AddRow(entt::entity entity, std::vector<std::string> texts);

/**
 * @brief 设置指定单元格文字
 */
void SetCell(entt::entity entity, int row, int col, std::string text);

/**
 * @brief 在指定单元格中嵌入任意控件实体
 *
 * 控件实体将成为表格的层级子节点，其 Position/Size 由 TableRenderer
 * 在每帧渲染时自动设置为对应单元格区域。
 * 同一单元格再次调用会替换旧实体。
 *
 * @param tableEntity  表格实体
 * @param row          行索引（0-based）
 * @param col          列索引（0-based）
 * @param widgetEntity 要嵌入的控件实体
 */
void SetCellWidget(entt::entity tableEntity, int row, int col, entt::entity widgetEntity);

/**
 * @brief 设置指定单元格颜色
 * @param textColor  文字颜色
 * @param bgColor    背景颜色（alpha=0 表示使用行默认背景）
 */
void SetCellColor(entt::entity entity, int row, int col, Color textColor, Color bgColor);

/**
 * @brief 清空所有行数据（保留列定义和表头）
 */
void ClearRows(entt::entity entity);

/**
 * @brief 设置选中行，-1 为无选中
 */
void SetSelectedRow(entt::entity entity, int row);

/**
 * @brief 注册单元格点击回调
 * @param callback 回调参数：(row, col)
 */
void SetOnCellClicked(entt::entity entity, std::move_only_function<void(int, int)> callback);

/**
 * @brief 设置表头文字颜色
 * @param color 文字颜色
 */
void SetHeaderTextColor(entt::entity entity, Color color);

} // namespace ui::table

// ===================== EntityAction 常量 =====================

namespace ui::actions::table
{

inline constexpr EntityAction<&ui::table::SetColumns> SET_COLUMNS_ACTION{};
inline constexpr EntityAction<&ui::table::SetColumnWidths> SET_COLUMN_WIDTHS_ACTION{};
inline constexpr EntityAction<&ui::table::AddRow> ADD_ROW_ACTION{};
inline constexpr EntityAction<&ui::table::ClearRows> CLEAR_ROWS_ACTION{};
inline constexpr EntityAction<&ui::table::SetSelectedRow> SET_SELECTED_ROW_ACTION{};
inline constexpr EntityAction<&ui::table::SetCellWidget> SET_CELL_WIDGET_ACTION{};

} // namespace ui::actions::table

// ===================== Chain DSL =====================

namespace ui::chains
{

/**
 * @brief Chain DSL：设置表格列数和可选表头
 *
 * 示例：entity | TableColumns(3, {"姓名", "年龄", "城市"});
 */
inline auto TableColumns(int count, std::vector<std::string> headers = {})
{
    return ui::actions::table::SET_COLUMNS_ACTION.bind(count, std::move(headers));
}

/**
 * @brief Chain DSL：设置每列宽度
 */
inline auto TableColumnWidths(std::vector<float> widths)
{
    return ui::actions::table::SET_COLUMN_WIDTHS_ACTION.bind(std::move(widths));
}

/**
 * @brief Chain DSL：追加一行数据
 *
 * 示例：entity | TableAddRow({"张三", "25", "北京"});
 */
inline auto TableAddRow(std::vector<std::string> texts)
{
    return ui::actions::table::ADD_ROW_ACTION.bind(std::move(texts));
}

/**
 * @brief Chain DSL：清空所有行
 */
inline auto TableClearRows()
{
    return ui::actions::table::CLEAR_ROWS_ACTION.bind();
}

/**
 * @brief Chain DSL：设置表头文字颜色
 */
inline auto TableHeaderTextColor(Color color)
{
    return Chain{[color](entt::entity entity) { ui::table::SetHeaderTextColor(entity, color); }};
}

/**
 * @brief Chain DSL：设置选中行
 */
inline auto TableSelectedRow(int row)
{
    return ui::actions::table::SET_SELECTED_ROW_ACTION.bind(row);
}

/**
 * @brief Chain DSL：注册单元格点击回调
 *
 * 示例：entity | TableOnCellClicked([](int row, int col){ ... });
 */
inline auto TableOnCellClicked(std::move_only_function<void(int, int)> callback)
{
    auto cb = std::make_shared<std::move_only_function<void(int, int)>>(std::move(callback));
    return Chain{[cb](entt::entity entity) mutable
    {
        ui::table::SetOnCellClicked(entity, [cb](int r, int c) { (*cb)(r, c); });
    }};
}

/**
 * @brief Chain DSL：在指定单元格嵌入任意控件
 *
 * 示例：table | TableSetCellWidget(0, 2, btnEntity);
 */
inline auto TableSetCellWidget(int row, int col, entt::entity widgetEntity)
{
    return ui::actions::table::SET_CELL_WIDGET_ACTION.bind(row, col, widgetEntity);
}

} // namespace ui::chains
