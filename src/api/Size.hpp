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

namespace ui::actions::size
{
inline constexpr EntityAction<&ui::size::SetFixedSize> SET_FIXED_SIZE_ACTION{};
inline constexpr EntityAction<&ui::size::SetSizePolicy> SET_SIZE_POLICY_ACTION{};
inline constexpr EntityAction<&ui::size::SetSize> SET_SIZE_ACTION{};
inline constexpr EntityAction<&ui::size::SetPosition> SET_POSITION_ACTION{};
} // namespace ui::actions::size

namespace ui::chains
{
inline auto FixedSize(float width, float height)
{
    return ui::actions::size::SET_FIXED_SIZE_ACTION.bind(width, height);
}
inline auto SizePolicy(ui::policies::Size policy)
{
    return ui::actions::size::SET_SIZE_POLICY_ACTION.bind(policy);
}
inline auto Size(float width, float height)
{
    return ui::actions::size::SET_SIZE_ACTION.bind(width, height);
}
inline auto Position(float positionX, float positionY)
{
    return ui::actions::size::SET_POSITION_ACTION.bind(positionX, positionY);
}
} // namespace ui::chains
