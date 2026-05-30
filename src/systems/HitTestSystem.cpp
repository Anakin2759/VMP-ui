#include "HitTestSystem.hpp"
#include <algorithm>
#include "api/Utils.hpp"
#include "common/components/Data.hpp"
#include "common/components/Interaction.hpp"
#include "common/components/Layout.hpp"
#include "common/Tags.hpp"
#include "core/RuntimeFacade.hpp"

namespace ui::systems
{

void HitTestSystem::registerHandlersImpl()
{
    m_disp->sink<events::RawPointerMove>().connect<&HitTestSystem::onRawPointerMove>(*this);
    m_disp->sink<events::RawPointerButton>().connect<&HitTestSystem::onRawPointerButton>(*this);
    m_disp->sink<events::RawPointerWheel>().connect<&HitTestSystem::onRawPointerWheel>(*this);

    connectInvalidateConstructUpdateDestroy<components::ZOrderIndex>();
    connectInvalidateConstructUpdateDestroy<components::Hierarchy>();
    connectInvalidateConstructDestroy<components::VisibleTag>();

    connectInvalidateConstructDestroy<components::Clickable>();
    connectInvalidateConstructDestroy<components::ScrollArea>();
    connectInvalidateConstructDestroy<components::SliderInfo>();
    connectInvalidateConstructDestroy<components::TextEditTag>();
    connectInvalidateConstructDestroy<components::DisabledTag>();

    connectInvalidateConstructUpdateDestroy<components::TextEdit>();
}

void HitTestSystem::unregisterHandlersImpl()
{
    m_disp->sink<events::RawPointerMove>().disconnect<&HitTestSystem::onRawPointerMove>(*this);
    m_disp->sink<events::RawPointerButton>().disconnect<&HitTestSystem::onRawPointerButton>(*this);
    m_disp->sink<events::RawPointerWheel>().disconnect<&HitTestSystem::onRawPointerWheel>(*this);

    disconnectInvalidateConstructUpdateDestroy<components::ZOrderIndex>();
    disconnectInvalidateConstructUpdateDestroy<components::Hierarchy>();
    disconnectInvalidateConstructDestroy<components::VisibleTag>();

    disconnectInvalidateConstructDestroy<components::Clickable>();
    disconnectInvalidateConstructDestroy<components::ScrollArea>();
    disconnectInvalidateConstructDestroy<components::SliderInfo>();
    disconnectInvalidateConstructDestroy<components::TextEditTag>();
    disconnectInvalidateConstructDestroy<components::DisabledTag>();

    disconnectInvalidateConstructUpdateDestroy<components::TextEdit>();
}

bool HitTestSystem::isPointInRect(const Vec2& point, const Vec2& pos, const Vec2& size)
{
    return point.x() >= pos.x() && point.x() < (pos.x() + size.x()) && point.y() >= pos.y()
        && point.y() < (pos.y() + size.y());
}

Vec2 HitTestSystem::getAbsolutePosition(entt::entity entity)
{
    return ui::utils::GetAbsolutePosition(entity);
}

entt::entity HitTestSystem::findRootWindow(entt::entity entity) const
{
    entt::entity current = entity;
    entt::entity rootWindow = entt::null;

    while (current != entt::null && m_reg->valid(current))
    {
        if (m_reg->any_of<components::WindowTag, components::DialogTag>(current))
        {
            rootWindow = current;
        }
        const auto* hierarchy = m_reg->try_get<components::Hierarchy>(current);
        current = hierarchy == nullptr ? entt::null : hierarchy->parent;
    }

    return rootWindow;
}

std::vector<entt::entity> HitTestSystem::getZOrderedInteractables(entt::entity topWindow)
{
    processPendingCacheInvalidationTags();

    if (auto iterator = m_zOrderCache.find(topWindow); iterator != m_zOrderCache.end() && !iterator->second.dirty)
    {
        return iterator->second.entities;
    }

    // 重建缓存
    std::vector<std::pair<int, entt::entity>> interactables;

    auto view = m_reg->view<components::Position, components::Size>();

    for (auto entity : view)
    {
        if (!isEntityInteractable(entity))
        {
            continue;
        }

        // 检查该实体是否属于 topWindow
        if (findRootWindow(entity) != topWindow)
        {
            continue;
        }

        interactables.emplace_back(calculateZOrder(entity), entity);
    }

    std::ranges::sort(interactables,
                      [](const auto& interactable1, const auto& interactable2)
                      { return interactable1.first > interactable2.first; });

    std::vector<entt::entity> result;
    result.reserve(interactables.size());
    for (const auto& pair : interactables)
    {
        result.push_back(pair.second);
    }

    // 更新缓存
    m_zOrderCache[topWindow] = {.entities = result, .dirty = false};

    return result;
}

entt::entity HitTestSystem::findHitEntity(const Vec2& mousePos, entt::entity topWindow)
{
    auto interactables = getZOrderedInteractables(topWindow);

    // 从前到后进行碰撞测试
    for (auto entity : interactables)
    {
        const auto& size = m_reg->get<components::Size>(entity);
        Vec2 absPos = getAbsolutePosition(entity);

        if (isPointInRect(mousePos, absPos, size.size))
        {
            return entity;
        }
    }

    return entt::null;
}

void HitTestSystem::invalidateAllCaches()
{
    for (auto& [window, cache] : m_zOrderCache)
    {
        cache.dirty = true;
    }
}

void HitTestSystem::markHitCacheDirty(entt::entity entity)
{
    (void)entity;
    m_hitCacheDirty = true;
}

void HitTestSystem::processPendingCacheInvalidationTags()
{
    if (!m_hitCacheDirty)
    {
        return;
    }

    invalidateAllCaches();
    m_hitCacheDirty = false;
}

bool HitTestSystem::isEntityInteractable(entt::entity entity) const
{
    bool isInteractive = m_reg->any_of<components::Clickable, components::ScrollArea, components::SliderInfo>(entity);

    if (!isInteractive && m_reg->any_of<components::TextEditTag>(entity))
    {
        if (const auto* edit = m_reg->try_get<components::TextEdit>(entity))
        {
            isInteractive = !policies::HasFlag(edit->inputMode, policies::TextFlag::READ_ONLY);
        }
    }

    if (!isInteractive)
    {
        return false;
    }

    return !m_reg->any_of<components::DisabledTag>(entity) && m_reg->any_of<components::VisibleTag>(entity);
}

int HitTestSystem::calculateZOrder(entt::entity entity) const
{
    if (const auto* zOrderComp = m_reg->try_get<components::ZOrderIndex>(entity))
    {
        return zOrderComp->value;
    }

    // 回退到深度排序：层级越深（子元素），Z-Order 越高
    int depth = 0;
    entt::entity current = entity;
    while (current != entt::null)
    {
        const auto* hierarchy = m_reg->try_get<components::Hierarchy>(current);
        current = hierarchy != nullptr ? hierarchy->parent : entt::null;
        depth++;
    }
    return depth;
}

entt::entity HitTestSystem::resolveHitEntity(const Vec2& pos, uint32_t windowID)
{
    entt::entity topWindow = RuntimeFacade::current().windowLookup().findById(windowID);
    if (topWindow == entt::null) return entt::null;
    return findHitEntity(pos, topWindow);
}

void HitTestSystem::onRawPointerMove(const events::RawPointerMove& event)
{
    const entt::entity hit = resolveHitEntity(event.position, event.windowID);
    m_disp->enqueue<events::HitPointerMove>(events::HitPointerMove{.raw = event, .hitEntity = hit});
}

void HitTestSystem::onRawPointerButton(const events::RawPointerButton& event)
{
    const entt::entity hit = resolveHitEntity(event.position, event.windowID);
    m_disp->enqueue<events::HitPointerButton>(events::HitPointerButton{.raw = event, .hitEntity = hit});
}

void HitTestSystem::onRawPointerWheel(const events::RawPointerWheel& event)
{
    const entt::entity hit = resolveHitEntity(event.position, event.windowID);
    m_disp->enqueue<events::HitPointerWheel>(events::HitPointerWheel{.raw = event, .hitEntity = hit});
}

} // namespace ui::systems
