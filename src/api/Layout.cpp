#include "Layout.hpp"
#include "singleton/Registry.hpp"
#include "common/Policies.hpp"
#include "Utils.hpp"
#include "entt/entity/fwd.hpp"
#include "common/components/Layout.hpp"
#include <algorithm>
namespace ui::layout
{

void SetLayoutDirection(::entt::entity entity, policies::LayoutDirection direction)
{
    if (!Registry::Valid(entity)) return;
    auto& layout = Registry::GetOrEmplace<components::LayoutInfo>(entity);
    layout.direction = direction;
    utils::MarkLayoutDirty(entity);
}

void SetLayoutSpacing(::entt::entity entity, float spacing)
{
    if (!Registry::Valid(entity)) return;
    if (auto* layout = Registry::TryGet<components::LayoutInfo>(entity))
    {
        layout->spacing = std::max(0.0F, spacing);
        utils::MarkLayoutDirty(entity);
    }
}

void SetPadding(::entt::entity entity, float left, float top, float right, float bottom)
{
    if (!Registry::Valid(entity)) return;
    auto& padding = Registry::GetOrEmplace<components::Padding>(entity);
    padding.values = Vec4(top, right, bottom, left);
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
