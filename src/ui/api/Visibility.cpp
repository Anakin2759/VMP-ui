#include "Visibility.hpp"
#include "../singleton/Registry.hpp"
#include "../common/Components.hpp"
#include "../common/Tags.hpp"
#include "../common/Policies.hpp"
#include "Utils.hpp"
#include <SDL3/SDL.h>

namespace ui::visibility
{
void SetVisible(::entt::entity entity, bool visible)
{
    if (!Registry::Valid(entity)) return;
    if (visible)
    {
        Registry::EmplaceOrReplace<components::VisibleTag>(entity);
    }
    else
    {
        Registry::Remove<components::VisibleTag>(entity);
    }
}

void Show(::entt::entity entity)
{
    if (!Registry::Valid(entity)) return;
    // 先标记可见，避免窗口事件同步时被误判为隐藏
    Registry::EmplaceOrReplace<components::VisibleTag>(entity);
    auto* windowComp = Registry::TryGet<components::Window>(entity);
    if (windowComp != nullptr && windowComp->windowID != 0)
    {
        auto* sizeComp = Registry::TryGet<components::Size>(entity);
        if (sizeComp != nullptr)
        {
            int targetW = static_cast<int>(sizeComp->size.x());
            int targetH = static_cast<int>(sizeComp->size.y());
            if (targetW > 0 && targetH > 0)
            {
                SDL_Window* sdlWindow = SDL_GetWindowFromID(windowComp->windowID);
                if (sdlWindow != nullptr)
                {
                    SDL_SetWindowSize(sdlWindow, targetW, targetH);
                }
            }
        }
        SDL_Window* sdlWindow = SDL_GetWindowFromID(windowComp->windowID);
        if (sdlWindow != nullptr)
        {
            SDL_SetWindowPosition(sdlWindow, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
            SDL_ShowWindow(sdlWindow);
            int posX = 0;
            int posY = 0;
            SDL_GetWindowPosition(sdlWindow, &posX, &posY);
            auto* posComp = Registry::TryGet<components::Position>(entity);
            if (posComp != nullptr)
            {
                posComp->value = Eigen::Vector2f{static_cast<float>(posX), static_cast<float>(posY)};
            }
        }
    }
    ui::utils::MarkLayoutDirty(entity);
    ui::utils::MarkRenderDirty(entity);
}

void Hide(::entt::entity entity)
{
    if (!Registry::Valid(entity)) return;
    Registry::Remove<components::VisibleTag>(entity);
    auto* windowComp = Registry::TryGet<components::Window>(entity);
    if (windowComp != nullptr && windowComp->windowID != 0)
    {
        SDL_Window* sdlWindow = SDL_GetWindowFromID(windowComp->windowID);
        if (sdlWindow != nullptr)
        {
            SDL_HideWindow(sdlWindow);
        }
    }
}

void SetAlpha(::entt::entity entity, float alpha)
{
    if (!Registry::Valid(entity)) return;
    auto& alphaComp = Registry::GetOrEmplace<components::Alpha>(entity);
    alphaComp.value = std::clamp(alpha, 0.0F, 1.0F);
}

void SetBackgroundColor(::entt::entity entity, const Color& color)
{
    if (!Registry::Valid(entity)) return;
    auto& background = Registry::GetOrEmplace<components::Background>(entity);
    background.color = color;
    background.enabled = policies::Feature::Enabled;
}

void SetBorderRadius(::entt::entity entity, float radius)
{
    if (!Registry::Valid(entity)) return;
    auto& background = Registry::GetOrEmplace<components::Background>(entity);
    const auto radiusClamped = std::max(0.0F, radius);
    background.borderRadius = {radiusClamped, radiusClamped, radiusClamped, radiusClamped};
    background.enabled = policies::Feature::Enabled;
    if (auto* border = Registry::TryGet<components::Border>(entity))
    {
        border->borderRadius = {radiusClamped, radiusClamped, radiusClamped, radiusClamped};
    }
}

void SetBorderColor(::entt::entity entity, const Color& color)
{
    if (!Registry::Valid(entity)) return;
    auto& border = Registry::GetOrEmplace<components::Border>(entity);
    border.color = color;
    border.enabled = policies::Feature::Enabled;
}

void SetBorderThickness(::entt::entity entity, float thickness)
{
    if (!Registry::Valid(entity)) return;
    auto& border = Registry::GetOrEmplace<components::Border>(entity);
    border.thickness = thickness;
    border.enabled = policies::Feature::Enabled;
}

} // namespace ui::visibility
