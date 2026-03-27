#include "RuntimeFacade.hpp"

#include "../common/WindowEntityLookup.hpp"

namespace ui
{

entt::entity RuntimeFacade::WindowLookupService::findById(uint32_t windowId) const
{
    return window_lookup::FindWindowEntityById(windowId);
}

void RuntimeFacade::WindowLookupService::remember(entt::entity entity) const
{
    window_lookup::RememberWindowEntity(entity);
}

void RuntimeFacade::WindowLookupService::invalidateId(uint32_t windowId) const
{
    window_lookup::InvalidateWindowId(windowId);
}

void RuntimeFacade::WindowLookupService::invalidateEntity(entt::entity entity) const
{
    window_lookup::InvalidateWindowEntity(entity);
}

} // namespace ui