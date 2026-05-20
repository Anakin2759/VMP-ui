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

    void collect(entt::entity entity, core::RenderContext& context) override;

    [[nodiscard]] int getPriority() const override { return 5; }

private:
    static Eigen::Vector4f toVec4(const Color& color, float alpha)
    {
        return {color.red, color.green, color.blue, color.alpha * alpha};
    }

    static std::vector<float> computeColWidths(const components::TableInfo& info, float totalWidth);
};

} // namespace ui::renderers
