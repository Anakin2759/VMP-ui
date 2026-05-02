#pragma once

#include <cstdint>
#include <utility>

#include <entt/entt.hpp>

#include "../common/GlobalContext.hpp"
#include "../singleton/Dispatcher.hpp"
#include "../singleton/Registry.hpp"

namespace ui
{

class UiRuntime;

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
    struct ActiveRuntimeState
    {
        Registry* registry = nullptr;
        Dispatcher* dispatcher = nullptr;
    };
    /**
     * @brief windowId 和 entity 的双向映射服务
     * 由于窗口实体的生命周期和 SDL 窗口的生命周期不完全绑定（例如，窗口销毁后实体可能短暂存在直到帧结束），因此需要提供一个独立的服务来管理两者的映射关系，并在窗口销毁时及时
     */
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

    [[nodiscard]] ActiveRuntimeState activateRuntime(UiRuntime& runtime) const;

    void restoreRuntime(ActiveRuntimeState previousRuntime) const;

    // Phase 2: raw entt access for explicit ECS / event-bus calls.
    // Use these in new and migrated code instead of static Registry::/Dispatcher:: APIs.
    [[nodiscard]] entt::registry& enttRegistry() const { return registry().raw(); }

    [[nodiscard]] entt::dispatcher& enttDispatcher() const { return dispatcher().raw(); }

    [[nodiscard]] globalcontext::FrameContext& frame() const { return context<globalcontext::FrameContext>(); }

    [[nodiscard]] globalcontext::StateContext& state() const { return context<globalcontext::StateContext>(); }

    [[nodiscard]] globalcontext::FrameContext* tryFrame() const { return tryContext<globalcontext::FrameContext>(); }

    [[nodiscard]] globalcontext::StateContext* tryState() const { return tryContext<globalcontext::StateContext>(); }

    [[nodiscard]] WindowLookupService windowLookup() const { return {}; }

    template <traits::Events Event>
    void trigger(Event&& event = {}) const
    {
        dispatcher().raw().trigger(std::forward<Event>(event));
    }

    template <traits::Events Event>
    void enqueue(Event&& event = {}) const
    {
        dispatcher().raw().enqueue(std::forward<Event>(event));
    }

    void update() const
    {
        dispatcher().raw().update();
    }

    template <traits::Events Event>
    void update() const
    {
        dispatcher().raw().update<Event>();
    }

    template <traits::Events Event>
    [[nodiscard]] auto sink() const
    {
        return dispatcher().raw().sink<Event>();
    }

    template <typename Context>
    [[nodiscard]] Context& context() const
    {
        return registry().raw().ctx().get<Context>();
    }

    template <typename Context>
    [[nodiscard]] Context* tryContext() const
    {
        return registry().raw().ctx().find<Context>();
    }

    template <typename Context, typename... Args>
    [[nodiscard]] Context& ensureContext(Args&&... args) const
    {
        if (auto* existing = tryContext<Context>())
        {
            return *existing;
        }

        return registry().raw().ctx().emplace<Context>(std::forward<Args>(args)...);
    }

private:
    RuntimeFacade() = default;
};

} // namespace ui