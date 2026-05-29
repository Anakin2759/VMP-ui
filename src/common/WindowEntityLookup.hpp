/**
 * ************************************************************************
 *
 * @file WindowEntityLookup.hpp
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2026-03-26
 * @version 0.1
 * @brief 窗口控件查找
 *
 * ************************************************************************
 * @copyright Copyright (c) 2026 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */
#pragma once

#include <cstdint>
#include <unordered_map>

#include <entt/entt.hpp>

#include "components/Window.hpp"
#include "core/RuntimeFacade.hpp"
#include "singleton/Registry.hpp"

namespace ui::window_lookup
{

struct WindowEntityLookupCache
{
    using is_component_tag = void;
    std::unordered_map<uint32_t, entt::entity> entitiesByWindowId;
};

inline WindowEntityLookupCache& Cache()
{
    auto& runtime = RuntimeFacade::current();
    if (auto* cache = runtime.tryContext<WindowEntityLookupCache>())
    {
        return *cache;
    }

    return runtime.ensureContext<WindowEntityLookupCache>();
}

inline bool MatchesWindowId(entt::entity entity, uint32_t windowId)
{
    if (!Registry::Valid(entity) || !Registry::AllOf<components::Window>(entity))
    {
        return false;
    }

    return Registry::Get<components::Window>(entity).windowID == windowId;
}

inline void RememberWindowEntity(entt::entity entity)
{
    if (!Registry::Valid(entity) || !Registry::AllOf<components::Window>(entity))
    {
        return;
    }

    const auto windowId = Registry::Get<components::Window>(entity).windowID;
    if (windowId == 0) return;

    Cache().entitiesByWindowId[windowId] = entity;
}

inline void InvalidateWindowId(uint32_t windowId)
{
    if (windowId == 0) return;

    if (auto* cache = RuntimeFacade::current().tryContext<WindowEntityLookupCache>())
    {
        cache->entitiesByWindowId.erase(windowId);
    }
}

inline void InvalidateWindowEntity(entt::entity entity)
{
    if (!Registry::Valid(entity) || !Registry::AllOf<components::Window>(entity))
    {
        return;
    }

    InvalidateWindowId(Registry::Get<components::Window>(entity).windowID);
}

inline entt::entity FindWindowEntityById(uint32_t windowId)
{
    if (windowId == 0) return entt::null;

    auto& cache = Cache();
    if (const auto cacheEntry = cache.entitiesByWindowId.find(windowId); cacheEntry != cache.entitiesByWindowId.end())
    {
        if (MatchesWindowId(cacheEntry->second, windowId))
        {
            return cacheEntry->second;
        }

        cache.entitiesByWindowId.erase(cacheEntry);
    }

    auto view = Registry::View<components::Window>();
    for (auto entity : view)
    {
        if (view.get<components::Window>(entity).windowID == windowId)
        {
            cache.entitiesByWindowId[windowId] = entity;
            return entity;
        }
    }

    return entt::null;
}

} // namespace ui::window_lookup