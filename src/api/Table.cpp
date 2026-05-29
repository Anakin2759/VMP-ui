#include "Table.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "common/Policies.hpp"
#include "common/Tags.hpp"
#include "singleton/Registry.hpp"
#include "common/Types.hpp"
#include "common/components/Data.hpp"
#include "common/components/Layout.hpp"
#include "entt/entity/entity.hpp"
#include "entt/entity/fwd.hpp"

namespace ui::table
{

namespace
{

[[nodiscard]] float MinColumnWidthAt(const components::TableInfo& info, int columnIndex)
{
    if (!info.minColumnWidths.empty() && columnIndex < static_cast<int>(info.minColumnWidths.size()))
    {
        return std::max(0.0F, info.minColumnWidths.at(static_cast<size_t>(columnIndex)));
    }
    return 0.0F;
}

[[nodiscard]] std::vector<float> ComputeEqualColumnWidths(const components::TableInfo& info, float visibleWidth)
{
    const int columnCount = info.columnCount;
    std::vector<float> widths(static_cast<size_t>(columnCount), 0.0F);
    const float equalWidth = visibleWidth / static_cast<float>(columnCount);
    for (int columnIndex = 0; columnIndex < columnCount; ++columnIndex)
    {
        widths.at(static_cast<size_t>(columnIndex)) = std::max(equalWidth, MinColumnWidthAt(info, columnIndex));
    }
    return widths;
}

[[nodiscard]] std::vector<float> ComputeFixedColumnWidths(const components::TableInfo& info, float visibleWidth)
{
    const int columnCount = info.columnCount;
    if (info.columnWidths.empty() || !std::cmp_equal(info.columnWidths.size(), columnCount))
    {
        return ComputeEqualColumnWidths(info, visibleWidth);
    }

    std::vector<float> widths(static_cast<size_t>(columnCount), 0.0F);
    for (int columnIndex = 0; columnIndex < columnCount; ++columnIndex)
    {
        widths.at(static_cast<size_t>(columnIndex)) =
            std::max(info.columnWidths.at(static_cast<size_t>(columnIndex)), MinColumnWidthAt(info, columnIndex));
    }
    return widths;
}

[[nodiscard]] float TotalColumnWeight(const components::TableInfo& info)
{
    float totalWeight = 0.0F;
    if (!info.columnWidths.empty() && std::cmp_equal(info.columnWidths.size(), info.columnCount))
    {
        for (float columnWeight : info.columnWidths)
        {
            totalWeight += std::max(0.0F, columnWeight);
        }
    }
    return totalWeight > 0.0F ? totalWeight : static_cast<float>(info.columnCount);
}

[[nodiscard]] std::vector<float> ComputeProportionalColumnWidths(const components::TableInfo& info, float visibleWidth)
{
    const int columnCount = info.columnCount;
    const float totalWeight = TotalColumnWeight(info);
    std::vector<float> widths(static_cast<size_t>(columnCount), 0.0F);
    for (int columnIndex = 0; columnIndex < columnCount; ++columnIndex)
    {
        float columnWeight = 1.0F;
        if (!info.columnWidths.empty() && columnIndex < static_cast<int>(info.columnWidths.size()))
        {
            columnWeight = std::max(0.0F, info.columnWidths.at(static_cast<size_t>(columnIndex)));
        }
        const float proportionalWidth = (columnWeight / totalWeight) * visibleWidth;
        widths.at(static_cast<size_t>(columnIndex)) = std::max(proportionalWidth, MinColumnWidthAt(info, columnIndex));
    }
    return widths;
}

[[nodiscard]] std::vector<float> ComputeAdaptiveColumnWidths(const components::TableInfo& info, float visibleWidth)
{
    const int columnCount = info.columnCount;
    std::vector<float> widths(static_cast<size_t>(columnCount), 0.0F);
    float fixedTotal = 0.0F;
    int flexCount = 0;
    for (int columnIndex = 0; columnIndex < columnCount; ++columnIndex)
    {
        float fixedWidth = 0.0F;
        if (!info.columnWidths.empty() && columnIndex < static_cast<int>(info.columnWidths.size()))
        {
            fixedWidth = info.columnWidths.at(static_cast<size_t>(columnIndex));
        }
        if (fixedWidth > 0.0F)
        {
            widths.at(static_cast<size_t>(columnIndex)) = std::max(fixedWidth, MinColumnWidthAt(info, columnIndex));
            fixedTotal += widths.at(static_cast<size_t>(columnIndex));
        }
        else
        {
            ++flexCount;
        }
    }

    const float remainingWidth = std::max(0.0F, visibleWidth - fixedTotal);
    const float flexWidth = (flexCount > 0) ? (remainingWidth / static_cast<float>(flexCount)) : 0.0F;
    for (int columnIndex = 0; columnIndex < columnCount; ++columnIndex)
    {
        const bool hasFixedWidth = !info.columnWidths.empty()
                                && columnIndex < static_cast<int>(info.columnWidths.size())
                                && info.columnWidths.at(static_cast<size_t>(columnIndex)) > 0.0F;
        if (!hasFixedWidth)
        {
            widths.at(static_cast<size_t>(columnIndex)) = std::max(flexWidth, MinColumnWidthAt(info, columnIndex));
        }
    }
    return widths;
}

} // namespace

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

void SetColumnSizing(entt::entity entity, policies::TableColumnSizing sizing)
{
    Registry::GetOrEmplace<components::TableInfo>(entity).columnSizing = sizing;
}

void SetMinColumnWidths(entt::entity entity, std::vector<float> minWidths)
{
    Registry::GetOrEmplace<components::TableInfo>(entity).minColumnWidths = std::move(minWidths);
}

void SetMinRowHeight(entt::entity entity, float height)
{
    Registry::GetOrEmplace<components::TableInfo>(entity).minRowHeight = std::max(0.0F, height);
}

void SetRowHeight(entt::entity entity, float height)
{
    Registry::GetOrEmplace<components::TableInfo>(entity).rowHeight = std::max(0.0F, height);
}

std::vector<float> ComputeColumnWidths(const components::TableInfo& info, float tableWidth)
{
    const int columnCount = info.columnCount;
    if (columnCount <= 0)
    {
        return {};
    }
    const float visibleWidth = std::max(0.0F, tableWidth);

    switch (info.columnSizing)
    {
        case policies::TableColumnSizing::FIXED:
            return ComputeFixedColumnWidths(info, visibleWidth);
        case policies::TableColumnSizing::PROPORTIONAL:
            return ComputeProportionalColumnWidths(info, visibleWidth);
        case policies::TableColumnSizing::ADAPTIVE:
            return ComputeAdaptiveColumnWidths(info, visibleWidth);
        case policies::TableColumnSizing::EQUAL:
        default:
            return ComputeEqualColumnWidths(info, visibleWidth);
    }
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
    if (cell.cellEntity != entt::null && cell.cellEntity != widgetEntity && Registry::Valid(cell.cellEntity))
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
    const bool alreadyChild =
        std::ranges::find(parentHierarchy.children, widgetEntity) != parentHierarchy.children.end();
    if (!alreadyChild)
    {
        auto& childHierarchy = Registry::GetOrEmplace<components::Hierarchy>(widgetEntity);
        childHierarchy.parent = tableEntity;
        Registry::Remove<components::RootTag>(widgetEntity);
        parentHierarchy.children.push_back(widgetEntity);
    }
}

} // namespace ui::table
