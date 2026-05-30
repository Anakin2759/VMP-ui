/**
 * ************************************************************************
 *
 * @file WindowSync.hpp
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2026-05-30
 * @version 0.1
 * @brief 处理窗口相关组件与 SDL_Window 之间的同步逻辑
 *
 * ************************************************************************
 * @copyright Copyright (c) 2026 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */
#pragma once

#include <SDL3/SDL.h>

#include <cfloat>
#include <cmath>
#include <string>

#include "components/Layout.hpp"
#include "components/Visual.hpp"
#include "components/Window.hpp"
#include "Policies.hpp"
#include "Tags.hpp"
#include "Types.hpp"
#include "singleton/Registry.hpp"

namespace ui::window_sync
{

inline void SyncWindowTitle(entt::entity entity, const components::Window& windowComp, SDL_Window* sdlWindow)
{
    std::string newTitle;

    const auto* titleComp = Registry::TryGet<components::Title>(entity);
    if (titleComp != nullptr && !titleComp->text.empty())
    {
        newTitle = titleComp->text;
    }
    else if (!windowComp.title.empty())
    {
        newTitle = windowComp.title;
    }

    if (!newTitle.empty())
    {
        const char* currentTitle = SDL_GetWindowTitle(sdlWindow);
        if (currentTitle == nullptr || newTitle != currentTitle)
        {
            SDL_SetWindowTitle(sdlWindow, newTitle.c_str());
        }
    }
}

inline void SyncWindowPosition(entt::entity entity, SDL_Window* sdlWindow)
{
    auto* posComp = Registry::TryGet<components::Position>(entity);
    if (posComp == nullptr) return;

    int currentX = 0;
    int currentY = 0;
    SDL_GetWindowPosition(sdlWindow, &currentX, &currentY);

    constexpr float POSITION_EPSILON = 0.01F;
    if (std::abs(posComp->value.x()) < POSITION_EPSILON && std::abs(posComp->value.y()) < POSITION_EPSILON)
    {
        posComp->value = Eigen::Vector2f{static_cast<float>(currentX), static_cast<float>(currentY)};
        return;
    }

    const int targetX = static_cast<int>(posComp->value.x());
    const int targetY = static_cast<int>(posComp->value.y());
    if (std::abs(currentX - targetX) > 1 || std::abs(currentY - targetY) > 1)
    {
        SDL_SetWindowPosition(sdlWindow, targetX, targetY);
    }
}

inline bool TryGetWindowSizeTarget(const components::Size& sizeComp, int& width, int& height)
{
    const float widthValue = sizeComp.size.x();
    const float heightValue = sizeComp.size.y();
    if (!std::isfinite(widthValue) || !std::isfinite(heightValue) || widthValue <= 0.0F || heightValue <= 0.0F)
    {
        return false;
    }

    width = static_cast<int>(std::round(widthValue));
    height = static_cast<int>(std::round(heightValue));
    return width > 0 && height > 0;
}

inline void SyncWindowSize(entt::entity entity, SDL_Window* sdlWindow)
{
    auto* sizeComp = Registry::TryGet<components::Size>(entity);
    if (sizeComp == nullptr) return;

    int currentWidth = 0;
    int currentHeight = 0;
    SDL_GetWindowSize(sdlWindow, &currentWidth, &currentHeight);

    int targetWidth = 0;
    int targetHeight = 0;
    if (!TryGetWindowSizeTarget(*sizeComp, targetWidth, targetHeight))
    {
        if (currentWidth > 0 && currentHeight > 0)
        {
            sizeComp->size = Eigen::Vector2f{static_cast<float>(currentWidth), static_cast<float>(currentHeight)};
        }
        return;
    }

    if (currentWidth != targetWidth || currentHeight != targetHeight)
    {
        SDL_SetWindowSize(sdlWindow, targetWidth, targetHeight);
    }
}

inline void SyncWindowSizeConstraints(const components::Window& windowComp, SDL_Window* sdlWindow)
{
    int currentMinW = 0;
    int currentMinH = 0;
    int currentMaxW = 0;
    int currentMaxH = 0;
    SDL_GetWindowMinimumSize(sdlWindow, &currentMinW, &currentMinH);
    SDL_GetWindowMaximumSize(sdlWindow, &currentMaxW, &currentMaxH);

    const int newMinW = static_cast<int>(windowComp.minSize.x());
    const int newMinH = static_cast<int>(windowComp.minSize.y());
    const int newMaxW = (windowComp.maxSize.x() < FLT_MAX) ? static_cast<int>(windowComp.maxSize.x()) : 0;
    const int newMaxH = (windowComp.maxSize.y() < FLT_MAX) ? static_cast<int>(windowComp.maxSize.y()) : 0;

    if (newMinW != currentMinW || newMinH != currentMinH)
    {
        SDL_SetWindowMinimumSize(sdlWindow, newMinW, newMinH);
    }

    if (newMaxW != currentMaxW || newMaxH != currentMaxH)
    {
        SDL_SetWindowMaximumSize(sdlWindow, newMaxW, newMaxH);
    }
}

inline void SyncWindowFrameless(const components::Window& windowComp, SDL_Window* sdlWindow)
{
    const SDL_WindowFlags flags = SDL_GetWindowFlags(sdlWindow);
    const bool currentlyBordered = (flags & SDL_WINDOW_BORDERLESS) == 0;
    const bool shouldBeBordered = !policies::HasFlag(windowComp.flags, policies::WindowFlag::NO_TITLE_BAR);

    if (currentlyBordered != shouldBeBordered)
    {
        SDL_SetWindowBordered(sdlWindow, shouldBeBordered);
    }
}

inline void SyncWindowResizable(const components::Window& windowComp, SDL_Window* sdlWindow)
{
    const SDL_WindowFlags flags = SDL_GetWindowFlags(sdlWindow);
    const bool currentlyResizable = (flags & SDL_WINDOW_RESIZABLE) != 0;
    const bool shouldBeResizable = !policies::HasFlag(windowComp.flags, policies::WindowFlag::NO_RESIZE);

    if (currentlyResizable != shouldBeResizable)
    {
        SDL_SetWindowResizable(sdlWindow, shouldBeResizable);
    }
}

inline void SyncWindowOpacity(entt::entity entity, SDL_Window* sdlWindow)
{
    const auto* alphaComp = Registry::TryGet<components::Alpha>(entity);
    if (alphaComp == nullptr) return;

    const float currentOpacity = SDL_GetWindowOpacity(sdlWindow);
    constexpr float OPACITY_THRESHOLD = 0.01F;
    if (std::abs(currentOpacity - alphaComp->value) > OPACITY_THRESHOLD)
    {
        SDL_SetWindowOpacity(sdlWindow, alphaComp->value);
    }
}

inline void SyncWindowVisibility(entt::entity entity, SDL_Window* sdlWindow)
{
    const bool shouldBeVisible = Registry::AnyOf<components::VisibleTag>(entity);
    const SDL_WindowFlags flags = SDL_GetWindowFlags(sdlWindow);
    const bool currentlyVisible = (flags & SDL_WINDOW_HIDDEN) == 0;

    if (shouldBeVisible && !currentlyVisible)
    {
        SDL_ShowWindow(sdlWindow);
    }
    else if (!shouldBeVisible && currentlyVisible)
    {
        SDL_HideWindow(sdlWindow);
    }
}

inline void SyncWindowModal(entt::entity entity, const components::Window& windowComp, SDL_Window* sdlWindow)
{
    if (!Registry::AnyOf<components::DialogTag>(entity)) return;

    const SDL_WindowFlags flags = SDL_GetWindowFlags(sdlWindow);
    const bool currentlyModal = (flags & SDL_WINDOW_MODAL) != 0;
    const bool isModal = policies::HasFlag(windowComp.flags, policies::WindowFlag::MODAL);

    if (isModal && !currentlyModal)
    {
        SDL_SetWindowModal(sdlWindow, true);
    }
    else if (!isModal && currentlyModal)
    {
        SDL_SetWindowModal(sdlWindow, false);
    }
}

inline void SyncWindowProperties(entt::entity entity, components::Window& windowComp, SDL_Window* sdlWindow)
{
    if (sdlWindow == nullptr) return;

    SyncWindowTitle(entity, windowComp, sdlWindow);
    SyncWindowPosition(entity, sdlWindow);
    SyncWindowSizeConstraints(windowComp, sdlWindow);
    SyncWindowSize(entity, sdlWindow);
    SyncWindowResizable(windowComp, sdlWindow);
    SyncWindowFrameless(windowComp, sdlWindow);
    SyncWindowOpacity(entity, sdlWindow);
    SyncWindowVisibility(entity, sdlWindow);
    SyncWindowModal(entity, windowComp, sdlWindow);
}

} // namespace ui::window_sync