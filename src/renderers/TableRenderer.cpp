/**
 * ************************************************************************
 *
 * @file TableRenderer.cpp
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2026-05-20
 * @version 0.1
 * @brief Table 渲染器实现
 *
 * ************************************************************************
 * @copyright Copyright (c) 2026 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */
#include "TableRenderer.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "api/Table.hpp"
#include "SDL3/SDL_gpu.h"
#include "SDL3/SDL_rect.h"
#include "common/RenderTypes.hpp"
#include "common/Types.hpp"
#include "common/components/Data.hpp"
#include "common/components/Interaction.hpp"
#include "core/RenderContext.hpp"
#include "entt/entity/entity.hpp"
#include "entt/entity/fwd.hpp"
#include "managers/BatchManager.hpp"
#include "managers/FontManager.hpp"
#include "managers/TextTextureCache.hpp"
#include "singleton/Registry.hpp"

namespace ui::renderers
{

void TableRenderer::collect(entt::entity entity, core::RenderContext& context)
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

    auto state = makeRenderState(*info, context);
    updateScrollArea(entity, state);

    // 表头背景（不需横向滚动，始终充满可见宽度）
    renderHeaderBackground(*info, context, state);

    // 表头文字需要横向裁切
    const SDL_Rect headerRect{.x = static_cast<int>(state.tableX),
                              .y = static_cast<int>(state.tableY),
                              .w = static_cast<int>(std::max(0.0F, state.totalWidth)),
                              .h = static_cast<int>(std::max(0.0F, info->headerHeight))};
    context.pushScissor(headerRect);
    renderHeaderText(*info, context, state);
    context.popScissor();

    const SDL_Rect bodyRect{.x = static_cast<int>(state.tableX),
                            .y = static_cast<int>(state.bodyY),
                            .w = static_cast<int>(std::max(0.0F, state.totalWidth)),
                            .h = static_cast<int>(std::max(0.0F, state.bodyHeight))};
    context.pushScissor(bodyRect);
    renderRowBackgrounds(*info, context, state);
    renderBodyGrid(*info, context, state);
    renderCellText(*info, context, state);
    context.popScissor();

    renderHeaderSeparators(*info, context, state);
}

TableRenderer::TableRenderState TableRenderer::makeRenderState(const components::TableInfo& info,
                                                               const core::RenderContext& context)
{
    TableRenderState state;
    state.tableX = context.position.x();
    state.tableY = context.position.y();
    state.totalWidth = std::max(0.0F, context.size.x());
    state.totalHeight = std::max(0.0F, context.size.y());
    state.bodyY = state.tableY + info.headerHeight;
    state.bodyHeight = std::max(0.0F, state.totalHeight - info.headerHeight);
    state.rowCount = static_cast<int>(info.cells.size());
    state.effectiveRowHeight = std::max(0.0F, std::max(info.rowHeight, info.minRowHeight));
    state.colWidths = ui::table::ComputeColumnWidths(info, state.totalWidth);

    // 计算 contentWidth（所有列宽之和）
    state.contentWidth = 0.0F;
    for (float columnWidth : state.colWidths)
    {
        state.contentWidth += columnWidth;
    }

    state.pushConstants.screen_size[0] = context.screenWidth;
    state.pushConstants.screen_size[1] = context.screenHeight;
    state.pushConstants.opacity = context.alpha;
    state.gridColor = toVec4(info.gridColor, context.alpha);
    return state;
}

std::vector<float> TableRenderer::computeColWidths(const components::TableInfo& info, float totalWidth)
{
    return ui::table::ComputeColumnWidths(info, totalWidth);
}

void TableRenderer::updateScrollArea(entt::entity entity, TableRenderState& state)
{
    auto* scrollArea = Registry::TryGet<components::ScrollArea>(entity);
    if (scrollArea == nullptr)
    {
        return;
    }

    const float contentHeight = static_cast<float>(state.rowCount) * state.effectiveRowHeight;
    scrollArea->contentSize = {state.contentWidth, contentHeight};

    // 垂直滚动
    const float viewportHeight = state.bodyHeight;
    const float maxScrollY = std::max(0.0F, contentHeight - viewportHeight);
    scrollArea->scrollOffset.y() = std::clamp(scrollArea->scrollOffset.y(), 0.0F, maxScrollY);
    state.scrollOffsetY = scrollArea->scrollOffset.y();

    // 水平滚动
    const float maxScrollX = std::max(0.0F, state.contentWidth - state.totalWidth);
    scrollArea->scrollOffset.x() = std::clamp(scrollArea->scrollOffset.x(), 0.0F, maxScrollX);
    state.scrollOffsetX = scrollArea->scrollOffset.x();
}

void TableRenderer::renderHeaderBackground(const components::TableInfo& info,
                                           core::RenderContext& context,
                                           const TableRenderState& state) const
{
    if (info.headers.empty())
    {
        return;
    }

    context.batchManager->beginBatch(context.whiteTexture, context.currentScissor, state.pushConstants);
    context.batchManager->addRect({state.tableX, state.tableY},
                                  {state.totalWidth, info.headerHeight},
                                  toVec4(info.headerBgColor, context.alpha));
}

void TableRenderer::renderHeaderText(const components::TableInfo& info,
                                     core::RenderContext& context,
                                     const TableRenderState& state) const
{
    if (info.headers.empty() || context.fontManager == nullptr || !context.fontManager->isLoaded()
        || context.textTextureCache == nullptr)
    {
        return;
    }

    static constexpr float HEADER_FONT_SIZE = 13.0F;
    const Eigen::Vector4f headerTextColor = toVec4(info.headerTextColor, 1.0F);
    float headerX = state.tableX - state.scrollOffsetX;
    for (int column = 0; column < info.columnCount && std::cmp_less(column, info.headers.size()); ++column)
    {
        const float columnWidth = state.colWidths.at(static_cast<size_t>(column));
        const std::string& header = info.headers.at(static_cast<size_t>(column));
        if (!header.empty())
        {
            uint32_t textWidth = 0;
            uint32_t textHeight = 0;
            SDL_GPUTexture* textTexture = context.textTextureCache->getOrUpload(
                header, headerTextColor, textWidth, textHeight, HEADER_FONT_SIZE);
            if (textTexture != nullptr)
            {
                const float scale = context.fontManager->getOversampleScale();
                const Eigen::Vector2f textSize{static_cast<float>(textWidth) / scale,
                                               static_cast<float>(textHeight) / scale};
                const float drawX = std::round(headerX + ((columnWidth - textSize.x()) * 0.5F));
                const float drawY = std::round(state.tableY + ((info.headerHeight - textSize.y()) * 0.5F));
                render::UiPushConstants textConstants{};
                textConstants.screen_size[0] = context.screenWidth;
                textConstants.screen_size[1] = context.screenHeight;
                textConstants.rect_size[0] = textSize.x();
                textConstants.rect_size[1] = textSize.y();
                textConstants.opacity = context.alpha;
                textConstants.padding = 2.0F;
                context.batchManager->beginBatch(textTexture, context.currentScissor, textConstants);
                context.batchManager->addRect({drawX, drawY}, textSize, headerTextColor);
            }
        }
        headerX += columnWidth;
    }
}

const Color& TableRenderer::rowBackgroundColor(const components::TableInfo& info, int row)
{
    if (row == info.selectedRow)
    {
        return info.selectedRowBgColor;
    }
    if ((row % 2) == 0)
    {
        return info.rowBgColor;
    }
    return info.altRowBgColor;
}

void TableRenderer::renderRowBackgrounds(const components::TableInfo& info,
                                         core::RenderContext& context,
                                         const TableRenderState& state) const
{
    float rowY = state.bodyY - state.scrollOffsetY;
    for (int row = 0; row < state.rowCount; ++row)
    {
        if (rowY + state.effectiveRowHeight > state.bodyY && rowY < state.bodyY + state.bodyHeight)
        {
            const Color& backgroundColor = rowBackgroundColor(info, row);
            context.batchManager->beginBatch(context.whiteTexture, context.currentScissor, state.pushConstants);
            context.batchManager->addRect({state.tableX, rowY},
                                          {state.totalWidth, state.effectiveRowHeight},
                                          toVec4(backgroundColor, context.alpha));
        }
        rowY += state.effectiveRowHeight;
    }
}

void TableRenderer::renderBodyGrid(const components::TableInfo& info,
                                   core::RenderContext& context,
                                   const TableRenderState& state) const
{
    // 竖向分隔线：随内容横向滚动
    float columnX = state.tableX - state.scrollOffsetX;
    for (int column = 0; column < info.columnCount; ++column)
    {
        columnX += state.colWidths.at(static_cast<size_t>(column));
        if (column < info.columnCount - 1)
        {
            // 只绘制在可见区域内的分隔线
            if (columnX > state.tableX && columnX < state.tableX + state.totalWidth)
            {
                context.batchManager->beginBatch(
                    context.whiteTexture, context.currentScissor, state.pushConstants);
                context.batchManager->addRect(
                    {columnX - 0.5F, state.bodyY}, {1.0F, state.bodyHeight}, state.gridColor);
            }
        }
    }

    // 水平分隔线：始终覆盖可见宽度，不横向滚动
    float rowY = state.bodyY - state.scrollOffsetY;
    for (int row = 0; row < state.rowCount; ++row)
    {
        rowY += state.effectiveRowHeight;
        if (rowY > state.bodyY && rowY < state.bodyY + state.bodyHeight)
        {
            context.batchManager->beginBatch(
                context.whiteTexture, context.currentScissor, state.pushConstants);
            context.batchManager->addRect(
                {state.tableX, rowY - 0.5F}, {state.totalWidth, 1.0F}, state.gridColor);
        }
    }
}

void TableRenderer::renderCellText(const components::TableInfo& info,
                                   core::RenderContext& context,
                                   const TableRenderState& state) const
{
    if (context.fontManager == nullptr || !context.fontManager->isLoaded() || context.textTextureCache == nullptr)
    {
        return;
    }

    static constexpr float CELL_FONT_SIZE = 12.0F;
    static constexpr float CELL_TEXT_PADDING = 4.0F;
    const Eigen::Vector4f cellColor = toVec4(info.cellTextColor, 1.0F);
    float rowY = state.bodyY - state.scrollOffsetY;
    for (int row = 0; row < state.rowCount; ++row)
    {
        if (rowY + state.effectiveRowHeight > state.bodyY && rowY < state.bodyY + state.bodyHeight)
        {
            float cellX = state.tableX - state.scrollOffsetX;
            for (int column = 0; column < info.columnCount; ++column)
            {
                const float columnWidth = state.colWidths.at(static_cast<size_t>(column));
                const auto& cell = info.cells.at(static_cast<size_t>(row)).at(static_cast<size_t>(column));
                if (cell.cellEntity == entt::null && !cell.text.empty())
                {
                    renderCellTexture(cell.text, cellX, rowY, state.effectiveRowHeight, cellColor, context);
                }
                cellX += columnWidth;
            }
        }
        rowY += state.effectiveRowHeight;
    }
}

void TableRenderer::renderCellTexture(const std::string& text,
                                      float cellX,
                                      float rowY,
                                      float rowHeight,
                                      const Eigen::Vector4f& cellColor,
                                      core::RenderContext& context) const
{
    static constexpr float CELL_FONT_SIZE = 12.0F;
    static constexpr float CELL_TEXT_PADDING = 4.0F;

    uint32_t textWidth = 0;
    uint32_t textHeight = 0;
    SDL_GPUTexture* textTexture = context.textTextureCache->getOrUpload(
        text, cellColor, textWidth, textHeight, CELL_FONT_SIZE);
    if (textTexture == nullptr)
    {
        return;
    }

    const float scale = context.fontManager->getOversampleScale();
    const Eigen::Vector2f textSize{static_cast<float>(textWidth) / scale, static_cast<float>(textHeight) / scale};
    const float drawX = std::round(cellX + CELL_TEXT_PADDING);
    const float drawY = std::round(rowY + ((rowHeight - textSize.y()) * 0.5F));
    render::UiPushConstants textConstants{};
    textConstants.screen_size[0] = context.screenWidth;
    textConstants.screen_size[1] = context.screenHeight;
    textConstants.rect_size[0] = textSize.x();
    textConstants.rect_size[1] = textSize.y();
    textConstants.opacity = context.alpha;
    textConstants.padding = 2.0F;
    context.batchManager->beginBatch(textTexture, context.currentScissor, textConstants);
    context.batchManager->addRect({drawX, drawY}, textSize, cellColor);
}

void TableRenderer::renderHeaderSeparators(const components::TableInfo& info,
                                           core::RenderContext& context,
                                           const TableRenderState& state) const
{
    // 表头与表体分隔线：始终覆盖可见宽度
    context.batchManager->beginBatch(context.whiteTexture, context.currentScissor, state.pushConstants);
    context.batchManager->addRect({state.tableX, state.bodyY - 0.5F}, {state.totalWidth, 1.0F}, state.gridColor);

    // 表头列分隔线：随内容横向滚动
    float columnX = state.tableX - state.scrollOffsetX;
    for (int column = 0; column < info.columnCount; ++column)
    {
        columnX += state.colWidths.at(static_cast<size_t>(column));
        if (column < info.columnCount - 1)
        {
            if (columnX > state.tableX && columnX < state.tableX + state.totalWidth)
            {
                context.batchManager->beginBatch(
                    context.whiteTexture, context.currentScissor, state.pushConstants);
                context.batchManager->addRect(
                    {columnX - 0.5F, state.tableY}, {1.0F, info.headerHeight}, state.gridColor);
            }
        }
    }
}

} // namespace ui::renderers
