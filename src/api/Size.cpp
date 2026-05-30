#include "Size.hpp"
#include "core/RuntimeFacade.hpp"
#include "Utils.hpp"
#include "entt/entity/fwd.hpp"
#include "common/components/Layout.hpp"
#include "common/Policies.hpp"

namespace ui::size
{
namespace
{
[[nodiscard]] entt::registry& CurrentRegistry()
{
    return RuntimeFacade::current().enttRegistry();
}
} // namespace

void SetFixedSize(::entt::entity entity, float width, float height)
{
    auto& reg = CurrentRegistry();
    if (!reg.valid(entity)) return;

    auto& size = reg.get_or_emplace<components::Size>(entity);
    size.sizePolicy = policies::Size::FIXED;
    size.size = {width, height};
    utils::MarkLayoutDirty(entity);
}

void SetSizePolicy(::entt::entity entity, policies::Size policy)
{
    auto& reg = CurrentRegistry();
    if (!reg.valid(entity)) return;
    auto& size = reg.get_or_emplace<components::Size>(entity);
    size.sizePolicy = policy;
    utils::MarkLayoutDirty(entity);
}

void SetSize(::entt::entity entity, float width, float height)
{
    auto& reg = CurrentRegistry();
    if (!reg.valid(entity)) return;
    auto& size = reg.get_or_emplace<components::Size>(entity);
    size.size = {width, height};
    utils::MarkLayoutDirty(entity);
}

void SetPosition(::entt::entity entity, float positionX, float positionY)
{
    auto& reg = CurrentRegistry();
    if (!reg.valid(entity)) return;
    auto& pos = reg.get_or_emplace<components::Position>(entity);
    pos.value = {positionX, positionY};
    utils::MarkLayoutDirty(entity);
}

} // namespace ui::size
