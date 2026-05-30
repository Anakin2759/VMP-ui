#include "Layout.hpp"
#include "Scale.hpp"
#include "core/RuntimeFacade.hpp"
#include "common/Policies.hpp"
#include "Utils.hpp"
#include "entt/entity/fwd.hpp"
#include "common/components/Layout.hpp"
#include <algorithm>
namespace ui::layout
{
namespace
{
[[nodiscard]] entt::registry& CurrentRegistry()
{
    return RuntimeFacade::current().enttRegistry();
}
} // namespace

void SetLayoutDirection(::entt::entity entity, policies::LayoutDirection direction)
{
    auto& reg = CurrentRegistry();
    if (!reg.valid(entity)) return;
    auto& layout = reg.get_or_emplace<components::LayoutInfo>(entity);
    layout.direction = direction;
    utils::MarkLayoutDirty(entity);
}

void SetLayoutSpacing(::entt::entity entity, float spacing)
{
    auto& reg = CurrentRegistry();
    if (!reg.valid(entity)) return;
    if (auto* layout = reg.try_get<components::LayoutInfo>(entity))
    {
        layout->spacing = std::max(0.0F, scale::Metric(spacing));
        utils::MarkLayoutDirty(entity);
    }
}

void SetPadding(::entt::entity entity, float left, float top, float right, float bottom)
{
    auto& reg = CurrentRegistry();
    if (!reg.valid(entity)) return;
    auto& padding = reg.get_or_emplace<components::Padding>(entity);
    padding.values = Vec4(scale::Metric(top), scale::Metric(right), scale::Metric(bottom), scale::Metric(left));
    utils::MarkLayoutDirty(entity);
}

void SetPadding(::entt::entity entity, float padding)
{
    SetPadding(entity, padding, padding, padding, padding);
}

void CenterInParent(::entt::entity entity)
{
    utils::MarkLayoutDirty(entity);
}

} // namespace ui::layout
