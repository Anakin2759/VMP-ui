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
#include "../managers/BatchManager.hpp"
#include "../managers/FontManager.hpp"
#include "../managers/TextTextureCache.hpp"

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

    const float x = context.position.x();
    const float y = context.position.y();
    const float totalWidth = context.size.x();
    const float totalHeight = context.size.y();

    // 计算每列实际宽度
    std::vector<float> colWidths = computeColWidths(*info, totalWidth);

    // ---- 更新 ScrollArea 内容高度并获取滚动偏移 ----
    const int rowCount = static_cast<int>(info->cells.size());
    // 内容高度仅为所有行的总高度（表头固定，不参与滚动）
    const float contentHeight = static_cast<float>(rowCount) * info->rowHeight;
    float scrollOffsetY = 0.0F;
    if (auto* scrollArea = Registry::TryGet<components::ScrollArea>(entity))
    {
        scrollArea->contentSize = {totalWidth, contentHeight};
        const float viewportH = totalHeight - info->headerHeight;
        const float maxScroll = std::max(0.0F, contentHeight - viewportH);
        scrollArea->scrollOffset.y() = std::clamp(scrollArea->scrollOffset.y(), 0.0F, maxScroll);
        scrollOffsetY = scrollArea->scrollOffset.y();
    }

    render::UiPushConstants pc{};
    pc.screen_size[0] = context.screenWidth;
    pc.screen_size[1] = context.screenHeight;
    pc.opacity = context.alpha;

    const float bodyY = y + info->headerHeight;
    const float bodyH = totalHeight - info->headerHeight;

    // ---- 表头背景（固定，不随滚动偏移）----
    if (!info->headers.empty())
    {
        context.batchManager->beginBatch(context.whiteTexture, context.currentScissor, pc);
        context.batchManager->addRect(
            {x, y},
            {totalWidth, info->headerHeight},
            toVec4(info->headerBgColor, context.alpha));
    }

    // ---- 表头文字（列标题，居中对齐）----
    if (!info->headers.empty() && context.fontManager != nullptr && context.fontManager->isLoaded()
        && context.textTextureCache != nullptr)
    {
        static constexpr float HEADER_FONT_SIZE = 13.0F;
        const Eigen::Vector4f headerTextColor = toVec4(info->headerTextColor, 1.0F);
        float hdrX = x;
        for (int col = 0; col < info->columnCount && col < static_cast<int>(info->headers.size()); ++col)
        {
            const float colW          = colWidths[static_cast<size_t>(col)];
            const std::string& header = info->headers[static_cast<size_t>(col)];
            if (!header.empty())
            {
                uint32_t textWidth  = 0;
                uint32_t textHeight = 0;
                SDL_GPUTexture* textTexture = context.textTextureCache->getOrUpload(
                    header, headerTextColor, textWidth, textHeight, HEADER_FONT_SIZE);
                if (textTexture != nullptr)
                {
                    const float scale = context.fontManager->getOversampleScale();
                    const Eigen::Vector2f textSize{static_cast<float>(textWidth) / scale,
                                                   static_cast<float>(textHeight) / scale};
                    const float drawX = std::round(hdrX + ((colW - textSize.x()) * 0.5F));
                    const float drawY = std::round(y + ((info->headerHeight - textSize.y()) * 0.5F));
                    render::UiPushConstants textPc{};
                    textPc.screen_size[0] = context.screenWidth;
                    textPc.screen_size[1] = context.screenHeight;
                    textPc.rect_size[0]   = textSize.x();
                    textPc.rect_size[1]   = textSize.y();
                    textPc.opacity        = context.alpha;
                    textPc.padding        = 2.0F;
                    context.batchManager->beginBatch(textTexture, context.currentScissor, textPc);
                    context.batchManager->addRect({drawX, drawY}, textSize, headerTextColor);
                }
            }
            hdrX += colW;
        }
    }

    // ---- 推入行区域裁剪（body scissor），防止行内容溢出表格边界 ----
    const SDL_Rect bodyRect{
        static_cast<int>(x),
        static_cast<int>(bodyY),
        static_cast<int>(std::max(0.0F, totalWidth)),
        static_cast<int>(std::max(0.0F, bodyH))};
    context.pushScissor(bodyRect);

    // ---- 行背景（应用垂直滚动偏移）----
    float rowY = bodyY - scrollOffsetY;
    for (int row = 0; row < rowCount; ++row)
    {
        // 跳过完全不可见的行（性能优化）
        if (rowY + info->rowHeight > bodyY && rowY < bodyY + bodyH)
        {
            const Color& bg = (row == info->selectedRow)   ? info->selectedRowBgColor
                              : (row % 2 == 0)             ? info->rowBgColor
                                                           : info->altRowBgColor;
            context.batchManager->beginBatch(context.whiteTexture, context.currentScissor, pc);
            context.batchManager->addRect(
                {x, rowY},
                {totalWidth, info->rowHeight},
                toVec4(bg, context.alpha));
        }
        rowY += info->rowHeight;
    }

    // ---- 网格线（列分隔线，覆盖整个 body 区域，固定不滚动）----
    const Eigen::Vector4f gridCol = toVec4(info->gridColor, context.alpha);
    float colX = x;
    for (int col = 0; col < info->columnCount; ++col)
    {
        colX += colWidths[static_cast<size_t>(col)];
        if (col < info->columnCount - 1)
        {
            context.batchManager->beginBatch(context.whiteTexture, context.currentScissor, pc);
            context.batchManager->addRect(
                {colX - 0.5F, bodyY},
                {1.0F, bodyH},
                gridCol);
        }
    }

    // ---- 行分隔线（随滚动偏移）----
    rowY = bodyY - scrollOffsetY;
    for (int row = 0; row < rowCount; ++row)
    {
        rowY += info->rowHeight;
        if (rowY > bodyY && rowY < bodyY + bodyH)
        {
            context.batchManager->beginBatch(context.whiteTexture, context.currentScissor, pc);
            context.batchManager->addRect({x, rowY - 0.5F}, {totalWidth, 1.0F}, gridCol);
        }
    }

    // ---- 数据单元格文字（body scissor 内，随滚动偏移）----
    if (context.fontManager != nullptr && context.fontManager->isLoaded()
        && context.textTextureCache != nullptr)
    {
        static constexpr float CELL_FONT_SIZE     = 12.0F;
        static constexpr float CELL_TEXT_PADDING  = 4.0F;
        const Eigen::Vector4f cellColor = toVec4(info->cellTextColor, 1.0F);
        rowY = bodyY - scrollOffsetY;
        for (int row = 0; row < rowCount; ++row)
        {
            if (rowY + info->rowHeight > bodyY && rowY < bodyY + bodyH)
            {
                float cellX = x;
                for (int col = 0; col < info->columnCount; ++col)
                {
                    const float colW   = colWidths[static_cast<size_t>(col)];
                    const auto& cell   = info->cells[static_cast<size_t>(row)][static_cast<size_t>(col)];
                    if (cell.cellEntity == entt::null && !cell.text.empty())
                    {
                        uint32_t textWidth  = 0;
                        uint32_t textHeight = 0;
                        SDL_GPUTexture* textTexture = context.textTextureCache->getOrUpload(
                            cell.text, cellColor, textWidth, textHeight, CELL_FONT_SIZE);
                        if (textTexture != nullptr)
                        {
                            const float scale = context.fontManager->getOversampleScale();
                            const Eigen::Vector2f textSize{static_cast<float>(textWidth) / scale,
                                                           static_cast<float>(textHeight) / scale};
                            const float drawX = std::round(cellX + CELL_TEXT_PADDING);
                            const float drawY = std::round(rowY + ((info->rowHeight - textSize.y()) * 0.5F));
                            render::UiPushConstants textPc{};
                            textPc.screen_size[0] = context.screenWidth;
                            textPc.screen_size[1] = context.screenHeight;
                            textPc.rect_size[0]   = textSize.x();
                            textPc.rect_size[1]   = textSize.y();
                            textPc.opacity        = context.alpha;
                            textPc.padding        = 2.0F;
                            context.batchManager->beginBatch(textTexture, context.currentScissor, textPc);
                            context.batchManager->addRect({drawX, drawY}, textSize, cellColor);
                        }
                    }
                    cellX += colW;
                }
            }
            rowY += info->rowHeight;
        }
    }

    // ---- 弹出行区域裁剪 ----
    context.popScissor();

    // ---- 表头 / 行体分隔线（固定，使用原始裁剪）----
    context.batchManager->beginBatch(context.whiteTexture, context.currentScissor, pc);
    context.batchManager->addRect({x, bodyY - 0.5F}, {totalWidth, 1.0F}, gridCol);

    // ---- 表头列分隔线（仅在表头区域，固定不滚动）----
    colX = x;
    for (int col = 0; col < info->columnCount; ++col)
    {
        colX += colWidths[static_cast<size_t>(col)];
        if (col < info->columnCount - 1)
        {
            context.batchManager->beginBatch(context.whiteTexture, context.currentScissor, pc);
            context.batchManager->addRect(
                {colX - 0.5F, y},
                {1.0F, info->headerHeight},
                gridCol);
        }
    }
}

std::vector<float> TableRenderer::computeColWidths(const components::TableInfo& info, float totalWidth)
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

} // namespace ui::renderers
