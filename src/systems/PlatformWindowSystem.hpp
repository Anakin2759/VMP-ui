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

#include <cmath>

#include <SDL3/SDL.h>

#include "common/components/Window.hpp"
#include "common/Events.hpp"
#include "core/RuntimeFacade.hpp"
#include "interface/ISystem.hpp"
#include "singleton/Dispatcher.hpp"
#include "singleton/Registry.hpp"
#include "common/WindowSync.hpp"
#include "api/Utils.hpp"

namespace ui::systems
{

class PlatformWindowSystem : public ui::interface::EnableRegister<PlatformWindowSystem>
{
public:
    PlatformWindowSystem() = default;
    explicit PlatformWindowSystem(Registry& /*reg*/, Dispatcher& /*disp*/) {}

    void registerHandlersImpl() { SDL_AddEventWatch(&PlatformWindowSystem::platformEventWatch, nullptr); }

    void unregisterHandlersImpl() { SDL_RemoveEventWatch(&PlatformWindowSystem::platformEventWatch, nullptr); }

    ui::interface::SystemPhase getPhase() { return ui::interface::SystemPhase::INPUT; }

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
        return eventType == SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED || eventType == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED
            || eventType == SDL_EVENT_WINDOW_RESIZED || eventType == SDL_EVENT_WINDOW_MOVED
            || eventType == SDL_EVENT_WINDOW_EXPOSED || eventType == SDL_EVENT_WINDOW_SHOWN
            || eventType == SDL_EVENT_WINDOW_HIDDEN;
    }

    static bool shouldSyncWindowPropertiesImmediately(Uint32 eventType)
    {
        return eventType == SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED || eventType == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED
            || eventType == SDL_EVENT_WINDOW_RESIZED || eventType == SDL_EVENT_WINDOW_MOVED
            || eventType == SDL_EVENT_WINDOW_EXPOSED || eventType == SDL_EVENT_WINDOW_SHOWN
            || eventType == SDL_EVENT_WINDOW_HIDDEN;
    }

    static bool requiresImmediatePlatformRefresh(Uint32 eventType)
    {
        return eventType == SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED || eventType == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED
            || eventType == SDL_EVENT_WINDOW_RESIZED || eventType == SDL_EVENT_WINDOW_EXPOSED;
    }

    static void dispatchImmediateWindowEvent(const SDL_WindowEvent& windowEvent)
    {
        if (windowEvent.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED)
        {
            RuntimeFacade::current().trigger<ui::events::WindowPixelSizeChanged>(
                ui::events::WindowPixelSizeChanged{windowEvent.windowID, windowEvent.data1, windowEvent.data2});
            return;
        }

        if (windowEvent.type == SDL_EVENT_WINDOW_MOVED)
        {
            RuntimeFacade::current().trigger<ui::events::WindowMoved>(
                ui::events::WindowMoved{windowEvent.windowID, windowEvent.data1, windowEvent.data2});
        }
    }

    static void triggerImmediateLayoutAndRenderRefresh()
    {
        RuntimeFacade::current().trigger<ui::events::UpdateLayout>(ui::events::UpdateLayout{});
        RuntimeFacade::current().trigger<ui::events::UpdateRendering>(ui::events::UpdateRendering{});
    }

    static void syncWindowMetricsImmediately(uint32_t windowId)
    {
        SDL_Window* sdlWindow = SDL_GetWindowFromID(windowId);
        if (sdlWindow == nullptr) return;

        const auto windowEntity = RuntimeFacade::current().windowLookup().findById(windowId);
        if (!RuntimeFacade::current().registry().valid(windowEntity)) return;

        auto& windowComp = RuntimeFacade::current().registry().get<components::Window>(windowEntity);
        const float oldScale = windowComp.displayScale;
        window_sync::SyncWindowDisplayMetrics(windowComp, sdlWindow);

        constexpr float SCALE_EPSILON = 0.01F;
        if (std::abs(oldScale - windowComp.displayScale) > SCALE_EPSILON)
        {
            RuntimeFacade::current().trigger<ui::events::WindowDisplayScaleChanged>(
                ui::events::WindowDisplayScaleChanged{windowId, oldScale, windowComp.displayScale});
        }

        ui::utils::MarkLayoutAndVisualChanged(windowEntity);
    }

    static void handlePlatformWindowEvent(const SDL_Event& event)
    {
        const auto eventType = event.type;
        if (!isRelevantPlatformWindowEvent(eventType)) return;

        if (shouldSyncWindowPropertiesImmediately(eventType))
        {
            syncWindowMetricsImmediately(event.window.windowID);
        }

        dispatchImmediateWindowEvent(event.window);

        if (requiresImmediatePlatformRefresh(eventType))
        {
            triggerImmediateLayoutAndRenderRefresh();
        }
    }
};

} // namespace ui::systems