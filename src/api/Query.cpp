#include "Query.hpp"

#include "common/ErrorCodes.hpp"
#include "common/components/Data.hpp"
#include "core/RuntimeFacade.hpp"

namespace ui::query
{

bool IsValid(entity ent) noexcept
{
    return RuntimeFacade::current().enttRegistry().valid(ent);
}

Result<entity> FindByAlias(std::string_view alias)
{
    if (alias.empty()) return MakeError(UiErrc::INVALID_ARGUMENT);

    auto& reg = RuntimeFacade::current().enttRegistry();
    for (auto ent : reg.view<components::BaseInfo>())
    {
        if (reg.get<components::BaseInfo>(ent).alias == alias) return ent;
    }
    return MakeError(UiErrc::INVALID_ENTITY);
}

std::string GetAlias(entity ent)
{
    auto& reg = RuntimeFacade::current().enttRegistry();
    if (!reg.valid(ent)) return {};
    const auto* info = reg.try_get<components::BaseInfo>(ent);
    return info != nullptr ? info->alias : std::string{};
}

} // namespace ui::query
