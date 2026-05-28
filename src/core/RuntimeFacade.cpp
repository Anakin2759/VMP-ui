#include "RuntimeFacade.hpp"

#include "UiRuntime.hpp"

#include "../common/WindowEntityLookup.hpp"
#include "singleton/Registry.hpp"
#include "singleton/Dispatcher.hpp"
#include "entt/entity/fwd.hpp"
#include <cstdint>

namespace ui
{

RuntimeFacade::ActiveRuntimeState RuntimeFacade::activateRuntime(UiRuntime& runtime) const
{
    WorkerMailbox*& mailboxSlot = activeMailbox();
    return {
        .registry = Registry::swapActiveInstance(&runtime.m_registry),
        .dispatcher = Dispatcher::swapActiveInstance(&runtime.m_dispatcher),
        .mailbox = std::exchange(mailboxSlot, &runtime.m_mailbox),
    };
}

void RuntimeFacade::restoreRuntime(ActiveRuntimeState previousRuntime) const
{
    Dispatcher::swapActiveInstance(previousRuntime.dispatcher);
    Registry::swapActiveInstance(previousRuntime.registry);
    activeMailbox() = previousRuntime.mailbox;
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