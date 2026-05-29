/**
 * ************************************************************************
 *
 * @file PlatformWindowSystem.hpp
 * @brief 平台窗口事件桥接系统
 *
 * 从 InteractionSystem 中拆出平台窗口事件监听、窗口同步和补救刷新逻辑。
 *
 * ************************************************************************
 */

#pragma once

#include <SDL3/SDL.h>

#include "common/components/Window.hpp"
#include "common/Events.hpp"
#include "core/RuntimeFacade.hpp"
#include "interface/ISystem.hpp"
#include "singleton/Dispatcher.hpp"
#include "singleton/Registry.hpp"
#include "common/WindowSync.hpp"

namespace ui::systems
{

class PlatformWindowSystem : public ui::interface::EnableRegister<PlatformWindowSystem>
{
public:
    PlatformWindowSystem() = default;
    explicit PlatformWindowSystem(entt::registry& /*reg*/, entt::dispatcher& /*disp*/) {}

    void registerHandlersImpl() { SDL_AddEventWatch(&PlatformWindowSystem::platformEventWatch, nullptr); }

    void unregisterHandlersImpl() { SDL_RemoveEventWatch(&PlatformWindowSystem::platformEventWatch, nullptr); }

    ui::interface::SystemPhase getPhase() { return ui::interface::SystemPhase::Input; }

private:
    static bool SDLCALL platformEventWatch(void* userdata, SDL_Event* event)
    {
        (void)userdata;
        if (event == nullptr) return true;

        handlePlatformWindowEvent(*event);
        return true;
    }

    static bool isRelevantPlatformWindowEvent(Uint32 eventType)
    {
        return eventType == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED || eventType == SDL_EVENT_WINDOW_MOVED
            || eventType == SDL_EVENT_WINDOW_EXPOSED || eventType == SDL_EVENT_WINDOW_SHOWN
            || eventType == SDL_EVENT_WINDOW_HIDDEN;
    }

    static bool shouldSyncWindowPropertiesImmediately(Uint32 eventType)
    {
        return eventType == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED || eventType == SDL_EVENT_WINDOW_MOVED
            || eventType == SDL_EVENT_WINDOW_EXPOSED || eventType == SDL_EVENT_WINDOW_SHOWN
            || eventType == SDL_EVENT_WINDOW_HIDDEN;
    }

    static bool requiresImmediatePlatformRefresh(Uint32 eventType)
    {
        return eventType == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED || eventType == SDL_EVENT_WINDOW_EXPOSED;
    }

    static void dispatchImmediateWindowEvent(const SDL_WindowEvent& windowEvent)
    {
        if (windowEvent.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED)
        {
            Dispatcher::Trigger<ui::events::WindowPixelSizeChanged>(
                ui::events::WindowPixelSizeChanged{windowEvent.windowID, windowEvent.data1, windowEvent.data2});
            return;
        }

        if (windowEvent.type == SDL_EVENT_WINDOW_MOVED)
        {
            Dispatcher::Trigger<ui::events::WindowMoved>(
                ui::events::WindowMoved{windowEvent.windowID, windowEvent.data1, windowEvent.data2});
        }
    }

    static void triggerImmediateLayoutAndRenderRefresh()
    {
        Dispatcher::Trigger<ui::events::UpdateLayout>(ui::events::UpdateLayout{});
        Dispatcher::Trigger<ui::events::UpdateRendering>(ui::events::UpdateRendering{});
    }

    static void syncWindowPropertiesImmediately(uint32_t windowId)
    {
        SDL_Window* sdlWindow = SDL_GetWindowFromID(windowId);
        if (sdlWindow == nullptr) return;

        const auto windowEntity = RuntimeFacade::current().windowLookup().findById(windowId);
        if (!Registry::Valid(windowEntity)) return;

        auto& windowComp = Registry::Get<components::Window>(windowEntity);
        window_sync::SyncWindowProperties(windowEntity, windowComp, sdlWindow);
    }

    static void handlePlatformWindowEvent(const SDL_Event& event)
    {
        const auto eventType = event.type;
        if (!isRelevantPlatformWindowEvent(eventType)) return;

        dispatchImmediateWindowEvent(event.window);

        if (shouldSyncWindowPropertiesImmediately(eventType))
        {
            syncWindowPropertiesImmediately(event.window.windowID);
        }

        if (requiresImmediatePlatformRefresh(eventType))
        {
            triggerImmediateLayoutAndRenderRefresh();
        }
    }
};

} // namespace ui::systems