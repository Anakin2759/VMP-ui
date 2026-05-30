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

#include "Entity.hpp"
#include <string>
#include <vector>
#include "common/Types.hpp"
#include "common/Policies.hpp"
#include "common/components/Data.hpp"
#include "Chains.hpp"

namespace ui::table
{

/**
 * @brief 设置列数和可选表头
 * @param entity    表格实体
 * @param count     列数
 * @param headers   表头文字（可为空）
 */
void SetColumns(ui::entity entity, int count, std::vector<std::string> headers = {});

/**
 * @brief 设置每列固定宽度
 * @param widths 宽度列表，size 必须与 columnCount 一致
 */
void SetColumnWidths(ui::entity entity, std::vector<float> widths);

/**
 * @brief 在末尾追加一行
 * @param texts 每列文字，数量不足时自动补空
 */
void AddRow(ui::entity entity, std::vector<std::string> texts);

/**
 * @brief 设置指定单元格文字
 */
void SetCell(ui::entity entity, int row, int col, std::string text);

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
void SetCellWidget(ui::entity tableEntity, int row, int col, ui::entity widgetEntity);

/**
 * @brief 设置指定单元格颜色
 * @param textColor  文字颜色
 * @param bgColor    背景颜色（alpha=0 表示使用行默认背景）
 */
void SetCellColor(ui::entity entity, int row, int col, Color textColor, Color bgColor);

/**
 * @brief 清空所有行数据（保留列定义和表头）
 */
void ClearRows(ui::entity entity);

/**
 * @brief 设置选中行，-1 为无选中
 */
void SetSelectedRow(ui::entity entity, int row);

/**
 * @brief 设置表头文字颜色
 * @param color 文字颜色
 */
void SetHeaderTextColor(ui::entity entity, Color color);

/**
 * @brief 设置列宽分配策略
 */
void SetColumnSizing(ui::entity entity, policies::TableColumnSizing sizing);

/**
 * @brief 设置各列最小宽度
 * @param minWidths 最小宽度列表，不足 columnCount 时后补 0
 */
void SetMinColumnWidths(ui::entity entity, std::vector<float> minWidths);

/**
 * @brief 设置行最小高度（rowHeight 实际生效高度 = max(rowHeight, minRowHeight)）
 */
void SetMinRowHeight(ui::entity entity, float height);

/**
 * @brief 设置行高
 */
void SetRowHeight(ui::entity entity, float height);

/**
 * @brief 计算实际列宽（供 Renderer 和 HitTest 共用）
 * @param info       表格组件
 * @param tableWidth 可见宽度（用于 EQUAL/PROPORTIONAL/ADAPTIVE 模式）
 * @return 各列实际宽度列表（size == columnCount）
 */
[[nodiscard]] std::vector<float> ComputeColumnWidths(const components::TableInfo& info, float tableWidth);

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
    return Chain{[color](ui::entity entity) { ui::table::SetHeaderTextColor(entity, color); }};
}

/**
 * @brief Chain DSL：设置选中行
 */
inline auto TableSelectedRow(int row)
{
    return ui::actions::table::SET_SELECTED_ROW_ACTION.bind(row);
}

/**
 * @brief Chain DSL：在指定单元格嵌入任意控件
 *
 * 示例：table | TableSetCellWidget(0, 2, btnEntity);
 */
inline auto TableSetCellWidget(int row, int col, ui::entity widgetEntity)
{
    return ui::actions::table::SET_CELL_WIDGET_ACTION.bind(row, col, widgetEntity);
}

/**
 * @brief Chain DSL：设置列宽分配策略
 *
 * 示例：entity | TableColumnSizingMode(policies::TableColumnSizing::FIXED);
 */
inline auto TableColumnSizingMode(policies::TableColumnSizing sizing)
{
    return Chain{[sizing](ui::entity entity) { ui::table::SetColumnSizing(entity, sizing); }};
}

/**
 * @brief Chain DSL：设置各列最小宽度
 *
 * 示例：entity | TableMinColumnWidths({60.0F, 80.0F, 40.0F});
 */
inline auto TableMinColumnWidths(std::vector<float> minWidths)
{
    return Chain{[minWidths = std::move(minWidths)](ui::entity entity) mutable
                 { ui::table::SetMinColumnWidths(entity, std::move(minWidths)); }};
}

/**
 * @brief Chain DSL：设置行最小高度
 */
inline auto TableMinRowHeight(float height)
{
    return Chain{[height](ui::entity entity) { ui::table::SetMinRowHeight(entity, height); }};
}

/**
 * @brief Chain DSL：设置行高
 */
inline auto TableRowHeight(float height)
{
    return Chain{[height](ui::entity entity) { ui::table::SetRowHeight(entity, height); }};
}

} // namespace ui::chains
