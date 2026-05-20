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
#include <vector>
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

// ---- 新增：折线 & 三次贝塞尔 ----
void DrawPolyline(entt::entity entity,
                  std::vector<Vec2> points,
                  Color color,
                  float lineWidth = 1.0F);

void DrawCubicBezier(entt::entity entity,
                     Vec2 startPos, Vec2 cp1, Vec2 cp2, Vec2 endPos,
                     Color color,
                     float lineWidth = 1.0F);

// ---- Painter：路径构建器，调用 commit() 将路径写入 Canvas ----
class Painter
{
public:
    explicit Painter(entt::entity canvas) : m_canvas(canvas) {}

    // 移动画笔（不绘制）
    Painter& moveTo(Vec2 pos);
    // 连线到目标点（添加线段）
    Painter& lineTo(Vec2 pos);
    // 三次贝塞尔曲线（控制点1 + 控制点2 + 终点）
    Painter& cubicTo(Vec2 cp1, Vec2 cp2, Vec2 endPos);
    // 将当前路径直接设置为折线顶点（替换内部点列表）
    Painter& polyline(std::vector<Vec2> points);

    // 提交当前路径为一条折线到 Canvas（清空内部路径）
    Painter& commit(Color color, float lineWidth = 1.0F);

private:
    entt::entity m_canvas;
    std::vector<Vec2> m_path;
    Vec2 m_cursor{0.0F, 0.0F};
};

} // namespace ui::canvas


namespace ui::actions::canvas
{
inline constexpr EntityAction<&ui::canvas::Clear> CANVAS_CLEAR_ACTION{};
inline constexpr EntityAction<&ui::canvas::DrawLine> CANVAS_DRAW_LINE_ACTION{};
inline constexpr EntityAction<&ui::canvas::DrawRect> CANVAS_DRAW_RECT_ACTION{};
inline constexpr EntityAction<&ui::canvas::DrawFilledRect> CANVAS_DRAW_FILLED_RECT_ACTION{};
} // namespace ui::actions::canvas


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
