/**
 * ************************************************************************
 *
 * @file TableRenderer.hpp
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2026-05-18
 * @version 0.1
 * @brief Table 渲染器 - 渲染表头、行、网格线
 *
 * ************************************************************************
 * @copyright Copyright (c) 2026 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */
#pragma once

#include "../interface/IRenderer.hpp"
#include "../singleton/Registry.hpp"
#include "../common/Components.hpp"
#include "../common/Tags.hpp"
#include "../managers/BatchManager.hpp"
#include "../managers/DeviceManager.hpp"
#include <SDL3/SDL_gpu.h>

namespace ui::renderers
{

/**
 * @brief Table 渲染器
 *
 * 渲染 TableTag + TableInfo 实体：
 * - 表头背景
 * - 交替行背景（含选中行高亮）
 * - 网格线
 * （文字由 TextRenderer 负责，这里只渲染背景和网格）
 */
class TableRenderer : public core::IRenderer
{
public:
    TableRenderer() = default;

    [[nodiscard]] bool canHandle(entt::entity entity) const override
    {
        return Registry::AnyOf<components::TableTag>(entity);
    }

    void collect(entt::entity entity, core::RenderContext& context) override
    {
        if (context.batchManager == nullptr || context.whiteTexture == nullptr)
        {
            return;
        }

        const auto* info = Registry::TryGet<components::TableInfo>(entity);
        if (info == nullptr || info->columnCount <= 0)
        {
            return;
        }

        const float x = context.position.x();
        const float y = context.position.y();
        const float totalWidth = context.size.x();
        const float totalHeight = context.size.y();

        // 计算每列实际宽度
        std::vector<float> colWidths = computeColWidths(*info, totalWidth);

        render::UiPushConstants pc{};
        pc.screen_size[0] = context.screenWidth;
        pc.screen_size[1] = context.screenHeight;
        pc.opacity = context.alpha;

        // ---- 表头背景 ----
        if (!info->headers.empty())
        {
            context.batchManager->beginBatch(context.whiteTexture, context.currentScissor, pc);
            context.batchManager->addRect(
                {x, y},
                {totalWidth, info->headerHeight},
                toVec4(info->headerBgColor, context.alpha));
        }

        // ---- 行背景 ----
        const int rowCount = static_cast<int>(info->cells.size());
        float rowY = y + info->headerHeight;
        for (int row = 0; row < rowCount; ++row)
        {
            const Color& bg = (row == info->selectedRow)    ? info->selectedRowBgColor
                              : (row % 2 == 0)               ? info->rowBgColor
                                                             : info->altRowBgColor;
            context.batchManager->beginBatch(context.whiteTexture, context.currentScissor, pc);
            context.batchManager->addRect(
                {x, rowY},
                {totalWidth, info->rowHeight},
                toVec4(bg, context.alpha));
            rowY += info->rowHeight;
        }

        // ---- 网格线（列分隔线）----
        const Eigen::Vector4f gridCol = toVec4(info->gridColor, context.alpha);
        float colX = x;
        for (int col = 0; col < info->columnCount; ++col)
        {
            colX += colWidths[static_cast<size_t>(col)];
            if (col < info->columnCount - 1)
            {
                context.batchManager->beginBatch(context.whiteTexture, context.currentScissor, pc);
                context.batchManager->addRect(
                    {colX - 0.5F, y},
                    {1.0F, totalHeight},
                    gridCol);
            }
        }

        // ---- 行分隔线 ----
        rowY = y + info->headerHeight;
        context.batchManager->beginBatch(context.whiteTexture, context.currentScissor, pc);
        context.batchManager->addRect({x, rowY - 0.5F}, {totalWidth, 1.0F}, gridCol);

        for (int row = 0; row < rowCount; ++row)
        {
            rowY += info->rowHeight;
            context.batchManager->beginBatch(context.whiteTexture, context.currentScissor, pc);
            context.batchManager->addRect({x, rowY - 0.5F}, {totalWidth, 1.0F}, gridCol);
        }

        // ---- 更新单元格控件的 Position / Size ----
        // （在 RenderSystem 遍历 Hierarchy 子节点之前执行，保证同帧生效）
        updateCellWidgetLayouts(*info, colWidths);
    }

    [[nodiscard]] int getPriority() const override { return 5; }

private:
    static Eigen::Vector4f toVec4(const Color& c, float alpha)
    {
        return {c.red, c.green, c.blue, c.alpha * alpha};
    }

    /// 将每个含子控件的单元格的 Position/Size 写入对应实体，
    /// 使其在本帧 RenderSystem 子节点遍历时使用正确的位置。
    static void updateCellWidgetLayouts(const components::TableInfo& info,
                                        const std::vector<float>& colWidths)
    {
        const int rowCount = static_cast<int>(info.cells.size());
        for (int row = 0; row < rowCount; ++row)
        {
            const float cellY = info.headerHeight + (static_cast<float>(row) * info.rowHeight);
            float cellX = 0.0F;
            for (int col = 0; col < info.columnCount; ++col)
            {
                const float colW = colWidths[static_cast<size_t>(col)];
                const auto& cell = info.cells[static_cast<size_t>(row)][static_cast<size_t>(col)];
                if (cell.cellEntity != entt::null && Registry::Valid(cell.cellEntity))
                {
                    if (auto* pos = Registry::TryGet<components::Position>(cell.cellEntity))
                    {
                        pos->value.x() = cellX;
                        pos->value.y() = cellY;
                    }
                    if (auto* sizeComp = Registry::TryGet<components::Size>(cell.cellEntity))
                    {
                        sizeComp->size.x() = colW;
                        sizeComp->size.y() = info.rowHeight;
                    }
                }
                cellX += colW;
            }
        }
    }

    static std::vector<float> computeColWidths(const components::TableInfo& info, float totalWidth)
    {
        const int n = info.columnCount;
        std::vector<float> widths(static_cast<size_t>(n));

        if (!info.columnWidths.empty() &&
            static_cast<int>(info.columnWidths.size()) == n)
        {
            for (int i = 0; i < n; ++i)
            {
                widths[static_cast<size_t>(i)] = info.columnWidths[static_cast<size_t>(i)];
            }
        }
        else
        {
            const float colW = (n > 0) ? (totalWidth / static_cast<float>(n)) : totalWidth;
            for (int i = 0; i < n; ++i)
            {
                widths[static_cast<size_t>(i)] = colW;
            }
        }
        return widths;
    }
};

} // namespace ui::renderers
