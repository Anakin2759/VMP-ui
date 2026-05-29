#include "Canvas.hpp"

#include "singleton/Registry.hpp"
#include "entt/entity/fwd.hpp"
#include "common/components/Data.hpp"
#include "common/Types.hpp"
#include <vector>
#include <utility>

namespace ui::canvas
{

void Clear(entt::entity entity)
{
    if (!Registry::Valid(entity)) return;
    auto& list = Registry::GetOrEmplace<components::CanvasDrawList>(entity);
    list.commands.clear();
}

void DrawLine(entt::entity entity, Vec2 from, Vec2 endPos, Color color, float lineWidth)
{
    if (!Registry::Valid(entity)) return;
    auto& list = Registry::GetOrEmplace<components::CanvasDrawList>(entity);
    list.commands.push_back({.type = components::CanvasDrawType::LINE,
                             .p1 = from,
                             .p2 = endPos,
                             .p3 = {},
                             .p4 = {},
                             .color = color,
                             .lineWidth = lineWidth,
                             .points = {}});
}

void DrawRect(entt::entity entity, Vec2 topLeft, Vec2 bottomRight, Color color, float lineWidth)
{
    if (!Registry::Valid(entity)) return;
    auto& list = Registry::GetOrEmplace<components::CanvasDrawList>(entity);
    list.commands.push_back({.type = components::CanvasDrawType::RECT,
                             .p1 = topLeft,
                             .p2 = bottomRight,
                             .p3 = {},
                             .p4 = {},
                             .color = color,
                             .lineWidth = lineWidth,
                             .points = {}});
}

void DrawFilledRect(entt::entity entity, Vec2 topLeft, Vec2 bottomRight, Color color)
{
    if (!Registry::Valid(entity)) return;
    auto& list = Registry::GetOrEmplace<components::CanvasDrawList>(entity);
    list.commands.push_back({.type = components::CanvasDrawType::FILLED_RECT,
                             .p1 = topLeft,
                             .p2 = bottomRight,
                             .p3 = {},
                             .p4 = {},
                             .color = color,
                             .lineWidth = 1.0F,
                             .points = {}});
}

void DrawCircle(entt::entity entity, Vec2 center, float radius, Color color, float lineWidth)
{
    if (!Registry::Valid(entity)) return;
    auto& list = Registry::GetOrEmplace<components::CanvasDrawList>(entity);
    list.commands.push_back({.type = components::CanvasDrawType::CIRCLE,
                             .p1 = center,
                             .p2 = {radius, 0.0F},
                             .p3 = {},
                             .p4 = {},
                             .color = color,
                             .lineWidth = lineWidth,
                             .points = {}});
}

void DrawFilledCircle(entt::entity entity, Vec2 center, float radius, Color color)
{
    if (!Registry::Valid(entity)) return;
    auto& list = Registry::GetOrEmplace<components::CanvasDrawList>(entity);
    list.commands.push_back({.type = components::CanvasDrawType::FILLED_CIRCLE,
                             .p1 = center,
                             .p2 = {radius, 0.0F},
                             .p3 = {},
                             .p4 = {},
                             .color = color,
                             .lineWidth = 1.0F,
                             .points = {}});
}

void DrawPolyline(entt::entity entity, std::vector<Vec2> points, Color color, float lineWidth)
{
    if (!Registry::Valid(entity) || points.size() < 2) return;
    auto& list = Registry::GetOrEmplace<components::CanvasDrawList>(entity);
    components::CanvasDrawCommand cmd;
    cmd.type = components::CanvasDrawType::POLYLINE;
    cmd.color = color;
    cmd.lineWidth = lineWidth;
    cmd.points = std::move(points);
    list.commands.push_back(std::move(cmd));
}

void DrawCubicBezier(entt::entity entity, Vec2 startPos, Vec2 cp1, Vec2 cp2, Vec2 endPos, Color color, float lineWidth)
{
    if (!Registry::Valid(entity)) return;
    auto& list = Registry::GetOrEmplace<components::CanvasDrawList>(entity);
    components::CanvasDrawCommand cmd;
    cmd.type = components::CanvasDrawType::CUBIC_BEZIER;
    cmd.p1 = startPos;
    cmd.p2 = cp1;
    cmd.p3 = cp2;
    cmd.p4 = endPos;
    cmd.color = color;
    cmd.lineWidth = lineWidth;
    list.commands.push_back(std::move(cmd));
}

// ---- Painter 实现 ----

Painter& Painter::moveTo(Vec2 pos)
{
    m_cursor = pos;
    // 移动画笔时，如果路径非空则结束上段路径并开始新段
    m_path.clear();
    m_path.push_back(pos);
    return *this;
}

Painter& Painter::lineTo(Vec2 pos)
{
    if (m_path.empty())
    {
        m_path.push_back(m_cursor);
    }
    m_path.push_back(pos);
    m_cursor = pos;
    return *this;
}

Painter& Painter::cubicTo(Vec2 cp1, Vec2 cp2, Vec2 endPos)
{
    if (m_path.empty())
    {
        m_path.push_back(m_cursor);
    }
    // 自适应细分三次贝塞尔曲线，目标弦误差 ≤ 0.5 px
    constexpr float FLATNESS_SQ = 0.5F * 0.5F;
    constexpr int MAX_DEPTH = 8;

    const Vec2 startPos = m_cursor;

    // 迭代细分（栈实现，避免递归调用开销）
    struct Seg
    {
        Vec2 ptA, ptB, ptC, ptD;
        int segDepth;
    };
    std::vector<Seg> segStack;
    segStack.push_back({.ptA = startPos, .ptB = cp1, .ptC = cp2, .ptD = endPos, .segDepth = 0});

    while (!segStack.empty())
    {
        auto [curA, curB, curC, curD, curDepth] = segStack.back();
        segStack.pop_back();

        if (curDepth >= MAX_DEPTH)
        {
            m_path.push_back(curD);
            continue;
        }

        // 估算弦误差：控制点到端点连线的最大偏差
        // 用 (b-a)×(d-a) 和 (c-a)×(d-a) 的向量叉积近似
        const Vec2 vecDA{curD.x() - curA.x(), curD.y() - curA.y()};
        const Vec2 vecBA{curB.x() - curA.x(), curB.y() - curA.y()};
        const Vec2 vecCA{curC.x() - curA.x(), curC.y() - curA.y()};
        const float cross1 = (vecBA.x() * vecDA.y()) - (vecBA.y() * vecDA.x());
        const float cross2 = (vecCA.x() * vecDA.y()) - (vecCA.y() * vecDA.x());
        const float flatness = (cross1 * cross1) + (cross2 * cross2);

        if (flatness <= FLATNESS_SQ)
        {
            m_path.push_back(curD);
            continue;
        }

        // 中点细分（de Casteljau）
        const Vec2 midAB{(curA.x() + curB.x()) * 0.5F, (curA.y() + curB.y()) * 0.5F};
        const Vec2 midBC{(curB.x() + curC.x()) * 0.5F, (curB.y() + curC.y()) * 0.5F};
        const Vec2 midCD{(curC.x() + curD.x()) * 0.5F, (curC.y() + curD.y()) * 0.5F};
        const Vec2 midABC{(midAB.x() + midBC.x()) * 0.5F, (midAB.y() + midBC.y()) * 0.5F};
        const Vec2 midBCD{(midBC.x() + midCD.x()) * 0.5F, (midBC.y() + midCD.y()) * 0.5F};
        const Vec2 midPt{(midABC.x() + midBCD.x()) * 0.5F, (midABC.y() + midBCD.y()) * 0.5F};

        // 后半段先压栈（LIFO），前半段后压（先弹出）
        segStack.push_back({.ptA = midPt, .ptB = midBCD, .ptC = midCD, .ptD = curD, .segDepth = curDepth + 1});
        segStack.push_back({.ptA = curA, .ptB = midAB, .ptC = midABC, .ptD = midPt, .segDepth = curDepth + 1});
    }

    m_cursor = endPos;
    return *this;
}

Painter& Painter::polyline(std::vector<Vec2> points)
{
    m_path = std::move(points);
    if (!m_path.empty())
    {
        m_cursor = m_path.back();
    }
    return *this;
}

Painter& Painter::commit(Color color, float lineWidth)
{
    if (m_path.size() >= 2)
    {
        DrawPolyline(m_canvas, m_path, color, lineWidth);
    }
    m_path.clear();
    return *this;
}

} // namespace ui::canvas
