#include "Size.hpp"
#include "singleton/Registry.hpp"
#include "Utils.hpp"
#include "entt/entity/fwd.hpp"
#include "common/components/Layout.hpp"
#include "common/Policies.hpp"

namespace ui::size
{
void SetFixedSize(::entt::entity entity, float width, float height)
{
    if (!Registry::Valid(entity)) return;

    auto& size = Registry::GetOrEmplace<components::Size>(entity);
    size.sizePolicy = policies::Size::FIXED;
    size.size = {width, height};
    utils::MarkLayoutDirty(entity);
}

void SetSizePolicy(::entt::entity entity, policies::Size policy)
{
    if (!Registry::Valid(entity)) return;
    auto& size = Registry::GetOrEmplace<components::Size>(entity);
    size.sizePolicy = policy;
    utils::MarkLayoutDirty(entity);
}

void SetSize(::entt::entity entity, float width, float height)
{
    if (!Registry::Valid(entity)) return;
    auto& size = Registry::GetOrEmplace<components::Size>(entity);
    size.size = {width, height};
    utils::MarkLayoutDirty(entity);
}

void SetPosition(::entt::entity entity, float positionX, float positionY)
{
    if (!Registry::Valid(entity)) return;
    auto& pos = Registry::GetOrEmplace<components::Position>(entity);
    pos.value = {positionX, positionY};
    utils::MarkLayoutDirty(entity);
}

} // namespace ui::size
