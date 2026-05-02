#include "RuntimeFacade.hpp"

#include "UiRuntime.hpp"

#include "../common/WindowEntityLookup.hpp"

namespace ui
{

RuntimeFacade::ActiveRuntimeState RuntimeFacade::activateRuntime(UiRuntime& runtime) const
{
    return {
        Registry::swapActiveInstance(&runtime.m_registry),
        Dispatcher::swapActiveInstance(&runtime.m_dispatcher),
    };
}

void RuntimeFacade::restoreRuntime(ActiveRuntimeState previousRuntime) const
{
    Dispatcher::swapActiveInstance(previousRuntime.dispatcher);
    Registry::swapActiveInstance(previousRuntime.registry);
}

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