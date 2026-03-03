#pragma once

#include <entt/entt.hpp>
#include "../common/Policies.hpp"
#include "Chains.hpp" // Changed: Include Chains.hpp for DSL

namespace ui::size
{
void SetFixedSize(::entt::entity entity, float width, float height);
void SetSizePolicy(::entt::entity entity, policies::Size policy);
void SetSize(::entt::entity entity, float width, float height);
void SetPosition(::entt::entity entity, float positionX, float positionY);

} // namespace ui::size

namespace ui::chains
{
inline auto FixedSize(float width, float height)
{
    return Call<ui::size::SetFixedSize>(width, height);
}
inline auto SizePolicy(ui::policies::Size policy)
{
    return Call<ui::size::SetSizePolicy>(policy);
}
inline auto Size(float width, float height)
{
    return Call<ui::size::SetSize>(width, height);
}
inline auto Position(float positionX, float positionY)
{
    return Call<ui::size::SetPosition>(positionX, positionY);
}
} // namespace ui::chains
