#pragma once

#include <cstdint>
#include <utility>

#include <entt/entt.hpp>

#include "../common/GlobalContext.hpp"
#include "../singleton/Dispatcher.hpp"
#include "../singleton/Registry.hpp"

namespace ui
{

namespace window_lookup
{
entt::entity FindWindowEntityById(uint32_t windowId);
void RememberWindowEntity(entt::entity entity);
void InvalidateWindowId(uint32_t windowId);
void InvalidateWindowEntity(entt::entity entity);
} // namespace window_lookup

class RuntimeFacade
{
public:
    class WindowLookupService
    {
    public:
        [[nodiscard]] entt::entity findById(uint32_t windowId) const;

        void remember(entt::entity entity) const;

        void invalidateId(uint32_t windowId) const;

        void invalidateEntity(entt::entity entity) const;
    };

    static RuntimeFacade& current()
    {
        static RuntimeFacade facade;
        return facade;
    }

    [[nodiscard]] Registry& registry() const { return Registry::current(); }

    [[nodiscard]] Dispatcher& dispatcher() const { return Dispatcher::current(); }

    [[nodiscard]] globalcontext::FrameContext& frame() const { return context<globalcontext::FrameContext>(); }

    [[nodiscard]] globalcontext::StateContext& state() const { return context<globalcontext::StateContext>(); }

    [[nodiscard]] globalcontext::FrameContext* tryFrame() const { return tryContext<globalcontext::FrameContext>(); }

    [[nodiscard]] globalcontext::StateContext* tryState() const { return tryContext<globalcontext::StateContext>(); }

    [[nodiscard]] WindowLookupService windowLookup() const { return {}; }

    template <typename Context>
    [[nodiscard]] Context& context() const
    {
        return Registry::ctx().get<Context>();
    }

    template <typename Context>
    [[nodiscard]] Context* tryContext() const
    {
        return Registry::ctx().find<Context>();
    }

    template <typename Context, typename... Args>
    [[nodiscard]] Context& ensureContext(Args&&... args) const
    {
        if (auto* existing = tryContext<Context>())
        {
            return *existing;
        }

        return Registry::ctx().emplace<Context>(std::forward<Args>(args)...);
    }

private:
    RuntimeFacade() = default;
};

} // namespace ui