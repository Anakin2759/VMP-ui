#include "Canvas.hpp"

#include "../singleton/Registry.hpp"

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
    list.commands.push_back(
        {components::CanvasDrawType::LINE, from, endPos, color, lineWidth});
}

void DrawRect(entt::entity entity, Vec2 topLeft, Vec2 bottomRight, Color color, float lineWidth)
{
    if (!Registry::Valid(entity)) return;
    auto& list = Registry::GetOrEmplace<components::CanvasDrawList>(entity);
    list.commands.push_back(
        {components::CanvasDrawType::RECT, topLeft, bottomRight, color, lineWidth});
}

void DrawFilledRect(entt::entity entity, Vec2 topLeft, Vec2 bottomRight, Color color)
{
    if (!Registry::Valid(entity)) return;
    auto& list = Registry::GetOrEmplace<components::CanvasDrawList>(entity);
    list.commands.push_back(
        {components::CanvasDrawType::FILLED_RECT, topLeft, bottomRight, color, 1.0F});
}

void DrawCircle(entt::entity entity, Vec2 center, float radius, Color color, float lineWidth)
{
    if (!Registry::Valid(entity)) return;
    auto& list = Registry::GetOrEmplace<components::CanvasDrawList>(entity);
    list.commands.push_back(
        {components::CanvasDrawType::CIRCLE, center, {radius, 0.0F}, color, lineWidth});
}

void DrawFilledCircle(entt::entity entity, Vec2 center, float radius, Color color)
{
    if (!Registry::Valid(entity)) return;
    auto& list = Registry::GetOrEmplace<components::CanvasDrawList>(entity);
    list.commands.push_back(
        {components::CanvasDrawType::FILLED_CIRCLE, center, {radius, 0.0F}, color, 1.0F});
}

} // namespace ui::canvas
