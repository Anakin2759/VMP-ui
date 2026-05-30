#include "Visibility.hpp"
#include "Scale.hpp"
#include "core/RuntimeFacade.hpp"
#include "common/Tags.hpp"
#include "common/Policies.hpp"
#include "common/WindowSync.hpp"
#include "SDL3/SDL_video.h"
#include "Utils.hpp"
#include "entt/entity/fwd.hpp"
#include "common/components/Window.hpp"
#include "common/components/Layout.hpp"
#include "common/components/Visual.hpp"
#include "common/Types.hpp"
#include <algorithm>

namespace ui::visibility
{
namespace
{
[[nodiscard]] entt::registry& CurrentRegistry()
{
    return RuntimeFacade::current().enttRegistry();
}
} // namespace

void SetVisible(::entt::entity entity, bool visible)
{
    auto& reg = CurrentRegistry();
    if (!reg.valid(entity)) return;
    if (visible)
    {
        reg.emplace_or_replace<components::VisibleTag>(entity);
    }
    else
    {
        reg.remove<components::VisibleTag>(entity);
    }

    ui::utils::MarkLayoutAndVisualChanged(entity);
}

void Show(::entt::entity entity)
{
    auto& reg = CurrentRegistry();
    if (!reg.valid(entity)) return;
    // 先标记可见，避免窗口事件同步时被误判为隐藏
    reg.emplace_or_replace<components::VisibleTag>(entity);
    auto* windowComp = reg.try_get<components::Window>(entity);
    if (windowComp != nullptr && windowComp->windowID != 0)
    {
        SDL_Window* sdlWindow = SDL_GetWindowFromID(windowComp->windowID);
        if (sdlWindow != nullptr)
        {
            window_sync::SyncWindowProperties(entity, *windowComp, sdlWindow);
            SDL_SetWindowPosition(sdlWindow, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
            SDL_ShowWindow(sdlWindow);
            int posX = 0;
            int posY = 0;
            SDL_GetWindowPosition(sdlWindow, &posX, &posY);
            auto* posComp = reg.try_get<components::Position>(entity);
            if (posComp != nullptr)
            {
                posComp->value = Eigen::Vector2f{static_cast<float>(posX), static_cast<float>(posY)};
            }
        }
    }
    ui::utils::MarkLayoutAndVisualChanged(entity);
}

void Hide(::entt::entity entity)
{
    auto& reg = CurrentRegistry();
    if (!reg.valid(entity)) return;
    reg.remove<components::VisibleTag>(entity);
    auto* windowComp = reg.try_get<components::Window>(entity);
    if (windowComp != nullptr && windowComp->windowID != 0)
    {
        SDL_Window* sdlWindow = SDL_GetWindowFromID(windowComp->windowID);
        if (sdlWindow != nullptr)
        {
            SDL_HideWindow(sdlWindow);
        }
    }

    ui::utils::MarkLayoutAndVisualChanged(entity);
}

void SetAlpha(::entt::entity entity, float alpha)
{
    auto& reg = CurrentRegistry();
    if (!reg.valid(entity)) return;
    auto& alphaComp = reg.get_or_emplace<components::Alpha>(entity);
    alphaComp.value = std::clamp(alpha, 0.0F, 1.0F);
    ui::utils::MarkVisualChanged(entity);
}

void SetBackgroundColor(::entt::entity entity, const Color& color)
{
    auto& reg = CurrentRegistry();
    if (!reg.valid(entity)) return;
    auto& background = reg.get_or_emplace<components::Background>(entity);
    background.color = color;
    background.enabled = policies::Feature::ENABLED;
    ui::utils::MarkVisualChanged(entity);
}

void SetBorderRadius(::entt::entity entity, float radius)
{
    auto& reg = CurrentRegistry();
    if (!reg.valid(entity)) return;
    auto& background = reg.get_or_emplace<components::Background>(entity);
    const auto radiusClamped = std::max(0.0F, scale::Metric(radius));
    background.borderRadius = {radiusClamped, radiusClamped, radiusClamped, radiusClamped};
    background.enabled = policies::Feature::ENABLED;
    if (auto* border = reg.try_get<components::Border>(entity))
    {
        border->borderRadius = {radiusClamped, radiusClamped, radiusClamped, radiusClamped};
    }
    ui::utils::MarkVisualChanged(entity);
}

void SetBorderColor(::entt::entity entity, const Color& color)
{
    auto& reg = CurrentRegistry();
    if (!reg.valid(entity)) return;
    auto& border = reg.get_or_emplace<components::Border>(entity);
    border.color = color;
    border.enabled = policies::Feature::ENABLED;
    ui::utils::MarkVisualChanged(entity);
}

void SetBorderThickness(::entt::entity entity, float thickness)
{
    auto& reg = CurrentRegistry();
    if (!reg.valid(entity)) return;
    auto& border = reg.get_or_emplace<components::Border>(entity);
    border.thickness = scale::Metric(thickness);
    border.enabled = policies::Feature::ENABLED;
    ui::utils::MarkVisualChanged(entity);
}

} // namespace ui::visibility
