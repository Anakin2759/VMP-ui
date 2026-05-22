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
#include "../common/components/Data.hpp"
#include "../common/Tags.hpp"
#include "../common/RenderTypes.hpp"
#include <SDL3/SDL_gpu.h>
#include <vector>

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

    void collect(entt::entity entity, core::RenderContext& context) override;

    [[nodiscard]] int getPriority() const override { return 5; }

private:
    struct TableRenderState
    {
        float tableX = 0.0F;
        float tableY = 0.0F;
        float totalWidth = 0.0F;
        float totalHeight = 0.0F;
        float bodyY = 0.0F;
        float bodyHeight = 0.0F;
        int rowCount = 0;
        float scrollOffsetY = 0.0F;
        std::vector<float> colWidths;
        render::UiPushConstants pushConstants{};
        Eigen::Vector4f gridColor{};
    };

    static Eigen::Vector4f toVec4(const Color& color, float alpha)
    {
        return {color.red, color.green, color.blue, color.alpha * alpha};
    }

    static TableRenderState makeRenderState(const components::TableInfo& info, const core::RenderContext& context);
    static std::vector<float> computeColWidths(const components::TableInfo& info, float totalWidth);
    static const Color& rowBackgroundColor(const components::TableInfo& info, int row);
    static void updateScrollArea(entt::entity entity, const components::TableInfo& info, TableRenderState& state);
    void renderHeaderBackground(const components::TableInfo& info,
                                core::RenderContext& context,
                                const TableRenderState& state) const;
    void renderHeaderText(const components::TableInfo& info,
                          core::RenderContext& context,
                          const TableRenderState& state) const;
    void renderRowBackgrounds(const components::TableInfo& info,
                              core::RenderContext& context,
                              const TableRenderState& state) const;
    void renderBodyGrid(const components::TableInfo& info,
                        core::RenderContext& context,
                        const TableRenderState& state) const;
    void renderCellText(const components::TableInfo& info,
                        core::RenderContext& context,
                        const TableRenderState& state) const;
    void renderCellTexture(const std::string& text,
                           float cellX,
                           float rowY,
                           float rowHeight,
                           const Eigen::Vector4f& cellColor,
                           core::RenderContext& context) const;
    void renderHeaderSeparators(const components::TableInfo& info,
                                core::RenderContext& context,
                                const TableRenderState& state) const;
};

} // namespace ui::renderers
