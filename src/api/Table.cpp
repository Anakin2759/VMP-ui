#include "Table.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "../common/Tags.hpp"
#include "../singleton/Registry.hpp"
#include "common/Types.hpp"
#include "common/components/Data.hpp"
#include "common/components/Layout.hpp"
#include "entt/entity/entity.hpp"
#include "entt/entity/fwd.hpp"

namespace ui::table
{

void SetColumns(entt::entity entity, int count, std::vector<std::string> headers)
{
    auto& info = Registry::GetOrEmplace<components::TableInfo>(entity);
    info.columnCount = count;
    info.headers = std::move(headers);
    // 调整已有行的列数
    for (auto& row : info.cells)
    {
        row.resize(static_cast<size_t>(count));
    }
}

void SetColumnWidths(entt::entity entity, std::vector<float> widths)
{
    auto& info = Registry::GetOrEmplace<components::TableInfo>(entity);
    info.columnWidths = std::move(widths);
}

void AddRow(entt::entity entity, std::vector<std::string> texts)
{
    auto& info = Registry::GetOrEmplace<components::TableInfo>(entity);
    std::vector<components::TableCell> row;
    row.resize(static_cast<size_t>(info.columnCount));
    for (int columnIndex = 0; columnIndex < info.columnCount && std::cmp_less(columnIndex, texts.size()); ++columnIndex)
    {
        row.at(static_cast<size_t>(columnIndex)).text = std::move(texts.at(static_cast<size_t>(columnIndex)));
    }
    info.cells.push_back(std::move(row));
}

void SetCell(entt::entity entity, int row, int col, std::string text)
{
    auto* info = Registry::TryGet<components::TableInfo>(entity);
    if (info == nullptr) return;
    if (row < 0 || std::cmp_greater_equal(row, info->cells.size())) return;
    if (col < 0 || col >= info->columnCount) return;
    info->cells.at(static_cast<size_t>(row)).at(static_cast<size_t>(col)).text = std::move(text);
}

void SetCellColor(entt::entity entity, int row, int col, Color textColor, Color bgColor)
{
    auto* info = Registry::TryGet<components::TableInfo>(entity);
    if (info == nullptr) return;
    if (row < 0 || std::cmp_greater_equal(row, info->cells.size())) return;
    if (col < 0 || col >= info->columnCount) return;
    auto& cell = info->cells.at(static_cast<size_t>(row)).at(static_cast<size_t>(col));
    cell.textColor = textColor;
    cell.bgColor = bgColor;
}

void ClearRows(entt::entity entity)
{
    auto* info = Registry::TryGet<components::TableInfo>(entity);
    if (info != nullptr)
    {
        info->cells.clear();
        info->selectedRow = -1;
    }
}

void SetSelectedRow(entt::entity entity, int row)
{
    auto& info = Registry::GetOrEmplace<components::TableInfo>(entity);
    info.selectedRow = row;
}

void SetHeaderTextColor(entt::entity entity, Color color)
{
    auto& info = Registry::GetOrEmplace<components::TableInfo>(entity);
    info.headerTextColor = color;
}

void SetCellWidget(entt::entity tableEntity, int row, int col, entt::entity widgetEntity)
{
    auto* info = Registry::TryGet<components::TableInfo>(tableEntity);
    if (info == nullptr) return;
    if (row < 0 || std::cmp_greater_equal(row, info->cells.size())) return;
    if (col < 0 || col >= info->columnCount) return;
    if (!Registry::Valid(widgetEntity)) return;

    auto& cell = info->cells.at(static_cast<size_t>(row)).at(static_cast<size_t>(col));

    // 替换旧实体：从 Hierarchy 中移除旧 widget
    if (cell.cellEntity != entt::null && cell.cellEntity != widgetEntity
        && Registry::Valid(cell.cellEntity))
    {
        auto* parentHierarchy = Registry::TryGet<components::Hierarchy>(tableEntity);
        if (parentHierarchy != nullptr)
        {
            auto& children = parentHierarchy->children;
            std::erase(children, cell.cellEntity);
        }
        if (auto* childHierarchy = Registry::TryGet<components::Hierarchy>(cell.cellEntity))
        {
            childHierarchy->parent = entt::null;
        }
    }

    cell.cellEntity = widgetEntity;

    // 标记为 TableCellWidget，使其跳过 Yoga 布局
    Registry::EmplaceOrReplace<components::TableCellWidgetTag>(widgetEntity);

    // 加入表格的 Hierarchy（若不已存在）
    auto& parentHierarchy = Registry::GetOrEmplace<components::Hierarchy>(tableEntity);
    const bool alreadyChild = std::ranges::find(parentHierarchy.children, widgetEntity)
                              != parentHierarchy.children.end();
    if (!alreadyChild)
    {
        auto& childHierarchy = Registry::GetOrEmplace<components::Hierarchy>(widgetEntity);
        childHierarchy.parent = tableEntity;
        Registry::Remove<components::RootTag>(widgetEntity);
        parentHierarchy.children.push_back(widgetEntity);
    }
}

} // namespace ui::table
