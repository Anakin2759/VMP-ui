/**
 * ************************************************************************
 *
 * @file Canvas.hpp
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2026-05-18
 * @version 0.1
 * @brief Canvas 绘图 API — 提供命令式绘制和 Chain DSL
 *
 * ************************************************************************
 * @copyright Copyright (c) 2026 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */
#pragma once

#include <entt/entt.hpp>
#include "../common/Types.hpp"
#include "Chains.hpp"

namespace ui::canvas
{

void Clear(entt::entity entity);
void DrawLine(entt::entity entity, Vec2 from, Vec2 endPos, Color color, float lineWidth = 1.0F);
void DrawRect(entt::entity entity, Vec2 topLeft, Vec2 bottomRight, Color color, float lineWidth = 1.0F);
void DrawFilledRect(entt::entity entity, Vec2 topLeft, Vec2 bottomRight, Color color);
void DrawCircle(entt::entity entity, Vec2 center, float radius, Color color, float lineWidth = 1.0F);
void DrawFilledCircle(entt::entity entity, Vec2 center, float radius, Color color);

} // namespace ui::canvas

namespace ui::actions
{
namespace canvas
{
inline constexpr EntityAction<&ui::canvas::Clear> CANVAS_CLEAR_ACTION{};
inline constexpr EntityAction<&ui::canvas::DrawLine> CANVAS_DRAW_LINE_ACTION{};
inline constexpr EntityAction<&ui::canvas::DrawRect> CANVAS_DRAW_RECT_ACTION{};
inline constexpr EntityAction<&ui::canvas::DrawFilledRect> CANVAS_DRAW_FILLED_RECT_ACTION{};
} // namespace canvas
} // namespace ui::actions

namespace ui::chains
{

inline auto CanvasClear()
{
    return ui::actions::canvas::CANVAS_CLEAR_ACTION.bind();
}

inline auto CanvasDrawLine(Vec2 from, Vec2 endPos, Color color, float lineWidth = 1.0F)
{
    return ui::actions::canvas::CANVAS_DRAW_LINE_ACTION.bind(from, endPos, color, lineWidth);
}

inline auto CanvasDrawRect(Vec2 topLeft, Vec2 bottomRight, Color color, float lineWidth = 1.0F)
{
    return ui::actions::canvas::CANVAS_DRAW_RECT_ACTION.bind(topLeft, bottomRight, color, lineWidth);
}

inline auto CanvasDrawFilledRect(Vec2 topLeft, Vec2 bottomRight, Color color)
{
    return ui::actions::canvas::CANVAS_DRAW_FILLED_RECT_ACTION.bind(topLeft, bottomRight, color);
}



} // namespace ui::chains
