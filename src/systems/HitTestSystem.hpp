/**
 * ************************************************************************
 *
 * @file HitTestSystem.hpp
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2026-01-28
 * @version 0.2
 * @brief 处理外部输入事件的碰撞检测系统,z轴排序和命中测试

  - 负责将鼠标位置映射到UI实体上
  - 支持Z轴排序，确保正确的命中检测顺序
  - 发送组件状态切换请求

  功能从interaction system中剥离出来，专注于碰撞检测，发送状态事件给下游处理
 *
 * ************************************************************************
 * @copyright Copyright (c) 2026 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */

#pragma once
#include <entt/entt.hpp>
#include <vector>
#include <algorithm>
#include <unordered_map>
#include "../api/Utils.hpp"
#include "../common/Types.hpp"
#include "../common/Components.hpp"
#include "../common/Tags.hpp"
#include "../singleton/Registry.hpp"
#include "../singleton/Dispatcher.hpp"
#include "../common/Events.hpp"
#include "../core/RuntimeFacade.hpp"
#include "../interface/Isystem.hpp"

namespace ui::systems
{

/**
 * @brief 碰撞检测系统 - 负责鼠标位置到UI实体的映射
 * @note 使用 Z-Order 缓存机制优化性能，按窗口维护排序后的可交互实体列表
 */
class HitTestSystem : public ui::interface::EnableRegister<HitTestSystem>
{
public:
    void registerHandlersImpl()
    {
        Dispatcher::Sink<events::RawPointerMove>().connect<&HitTestSystem::onRawPointerMove>(*this);
        Dispatcher::Sink<events::RawPointerButton>().connect<&HitTestSystem::onRawPointerButton>(*this);
        Dispatcher::Sink<events::RawPointerWheel>().connect<&HitTestSystem::onRawPointerWheel>(*this);

        // 监听可能影响命中缓存成员和排序的组件变化，统一打失效标记。
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

    void unregisterHandlersImpl()
    {
        Dispatcher::Sink<events::RawPointerMove>().disconnect<&HitTestSystem::onRawPointerMove>(*this);
        Dispatcher::Sink<events::RawPointerButton>().disconnect<&HitTestSystem::onRawPointerButton>(*this);
        Dispatcher::Sink<events::RawPointerWheel>().disconnect<&HitTestSystem::onRawPointerWheel>(*this);

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

    /**
     * @brief 执行点碰撞测试 (Hit Test)
     * @param point 鼠标绝对位置
     * @param pos 实体绝对位置
     * @param size 实体尺寸
     * @return bool 是否命中
     */
    static bool isPointInRect(const Vec2& point, const Vec2& pos, const Vec2& size)
    {
        return point.x() >= pos.x() && point.x() < (pos.x() + size.x()) && point.y() >= pos.y() &&
               point.y() < (pos.y() + size.y());
    }

    /**
     * @brief 计算实体的绝对位置（考虑父节点层级）
     * @param entity 当前实体
     * @return Vec2 实体的绝对位置
     */
    static Vec2 getAbsolutePosition(entt::entity entity)
    {
        return ui::utils::GetAbsolutePosition(entity);
    }

    /**
     * @brief 查找实体所属的根窗口/对话框
     * @return 根窗口实体，如果不在任何窗口内则返回 entt::null
     */
    static entt::entity findRootWindow(entt::entity entity)
    {
        entt::entity current = entity;
        entt::entity rootWindow = entt::null;

        while (current != entt::null && Registry::Valid(current))
        {
            if (Registry::AnyOf<components::WindowTag, components::DialogTag>(current))
            {
                rootWindow = current;
            }
            const auto* hierarchy = Registry::TryGet<components::Hierarchy>(current);
            current = hierarchy == nullptr ? entt::null : hierarchy->parent;
        }

        return rootWindow;
    }

    /**
     * @brief 获取按 Z-Order 从前到后排序的可交互实体列表（带缓存）
     * @param topWindow 当前鼠标所在的顶层窗口（entt::null 表示不在任何窗口内）
     * @return 排序后的可交互实体列表（Z-Order 高的在前）
     * @note 使用缓存机制，只在缓存失效时重新计算
     * @note 该缓存只保存“窗口内哪些实体当前可交互以及它们的排序”，
     *       不缓存绝对位置和尺寸；几何命中仍在每次事件解析时实时读取 Position / Size。
     */
    std::vector<entt::entity> getZOrderedInteractables(entt::entity topWindow)
    {
        processPendingCacheInvalidationTags();

        if (auto iterator = m_zOrderCache.find(topWindow); iterator != m_zOrderCache.end() && !iterator->second.dirty)
        {
            return iterator->second.entities;
        }

        // 重建缓存
        std::vector<std::pair<int, entt::entity>> interactables;

        // 遍历所有具有 Position 和 Size 的实体
        auto view = Registry::View<components::Position, components::Size>();

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

        // 排序：Z-Order 值越大（越靠近前端）的排在前面
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

    /**
     * @brief 在指定窗口内查找鼠标位置命中的实体
     * @param mousePos 鼠标绝对位置
     * @param topWindow 当前窗口实体
     * @return 命中的实体，如果未命中返回 entt::null
     */
    entt::entity findHitEntity(const Vec2& mousePos, entt::entity topWindow)
    {
        auto interactables = getZOrderedInteractables(topWindow);

        // 从前到后进行碰撞测试
        for (auto entity : interactables)
        {
            const auto& size = Registry::Get<components::Size>(entity);
            Vec2 absPos = getAbsolutePosition(entity);

            if (isPointInRect(mousePos, absPos, size.size))
            {
                return entity; // 找到最前面的命中实体
            }
        }

        return entt::null;
    }

private:
    template <typename Component>
    void connectInvalidateConstructDestroy()
    {
        Registry::OnConstruct<Component>().template connect<&HitTestSystem::markHitCacheDirty>(*this);
        Registry::OnDestroy<Component>().template connect<&HitTestSystem::markHitCacheDirty>(*this);
    }

    template <typename Component>
    void disconnectInvalidateConstructDestroy()
    {
        Registry::OnConstruct<Component>().template disconnect<&HitTestSystem::markHitCacheDirty>(*this);
        Registry::OnDestroy<Component>().template disconnect<&HitTestSystem::markHitCacheDirty>(*this);
    }

    template <typename Component>
    void connectInvalidateConstructUpdateDestroy()
    {
        Registry::OnConstruct<Component>().template connect<&HitTestSystem::markHitCacheDirty>(*this);
        Registry::OnUpdate<Component>().template connect<&HitTestSystem::markHitCacheDirty>(*this);
        Registry::OnDestroy<Component>().template connect<&HitTestSystem::markHitCacheDirty>(*this);
    }

    template <typename Component>
    void disconnectInvalidateConstructUpdateDestroy()
    {
        Registry::OnConstruct<Component>().template disconnect<&HitTestSystem::markHitCacheDirty>(*this);
        Registry::OnUpdate<Component>().template disconnect<&HitTestSystem::markHitCacheDirty>(*this);
        Registry::OnDestroy<Component>().template disconnect<&HitTestSystem::markHitCacheDirty>(*this);
    }

    /**
     * @brief Z-Order 缓存结构
     */
    struct ZOrderCache
    {
        std::vector<entt::entity> entities; // 排序后的实体列表
        bool dirty = true;                  // 缓存是否失效
    };

    // 按窗口维护的 Z-Order 缓存
    std::unordered_map<entt::entity, ZOrderCache> m_zOrderCache;

    entt::entity m_cacheInvalidationMarker = entt::null;

    /**
     * @brief 使所有窗口的缓存失效
     */
    void invalidateAllCaches()
    {
        for (auto& [window, cache] : m_zOrderCache)
        {
            cache.dirty = true;
        }
    }

    void markGlobalHitCacheDirty()
    {
        if (m_cacheInvalidationMarker == entt::null || !Registry::Valid(m_cacheInvalidationMarker))
        {
            m_cacheInvalidationMarker = Registry::Create();
        }
        Registry::EmplaceOrReplace<components::HitCacheInvalidateTag>(m_cacheInvalidationMarker);
    }

    /**
     * @brief 统一缓存脏标记回调
     */
    void markHitCacheDirty(entt::entity entity)
    {
        if (Registry::Valid(entity))
        {
            Registry::EmplaceOrReplace<components::HitCacheInvalidateTag>(entity);
            return;
        }

        // Destroy 场景下兜底为全局标记，避免漏失效。
        markGlobalHitCacheDirty();
    }

    void processPendingCacheInvalidationTags()
    {
        auto taggedView = Registry::View<components::HitCacheInvalidateTag>();
        if (taggedView.begin() == taggedView.end())
        {
            return;
        }

        invalidateAllCaches();

        std::vector<entt::entity> taggedEntities;
        for (auto entity : taggedView)
        {
            taggedEntities.push_back(entity);
        }

        for (auto entity : taggedEntities)
        {
            if (Registry::Valid(entity) && Registry::AnyOf<components::HitCacheInvalidateTag>(entity))
            {
                Registry::Remove<components::HitCacheInvalidateTag>(entity);
            }
        }
    }

    /**
     * @brief 检查实体是否可交互且可见
     */
    static bool isEntityInteractable(entt::entity entity)
    {
        bool isInteractive =
            Registry::AnyOf<components::Clickable, components::ScrollArea, components::SliderInfo>(entity);

        if (!isInteractive && Registry::AnyOf<components::TextEditTag>(entity))
        {
            if (const auto* edit = Registry::TryGet<components::TextEdit>(entity))
            {
                isInteractive = !policies::HasFlag(edit->inputMode, policies::TextFlag::ReadOnly);
            }
        }

        if (!isInteractive)
        {
            return false;
        }

        // 忽略禁用或不可见的实体
        return !Registry::AnyOf<components::DisabledTag>(entity) && Registry::AnyOf<components::VisibleTag>(entity);
    }

    /**
     * @brief 计算实体的 Z-Order
     */
    static int calculateZOrder(entt::entity entity)
    {
        if (const auto* zOrderComp = Registry::TryGet<components::ZOrderIndex>(entity))
        {
            return zOrderComp->value;
        }

        // 回退到深度排序：层级越深（子元素），Z-Order 越高
        int depth = 0;
        entt::entity current = entity;
        while (current != entt::null)
        {
            const auto* hierarchy = Registry::TryGet<components::Hierarchy>(current);
            current = hierarchy != nullptr ? hierarchy->parent : entt::null;
            depth++;
        }
        return depth;
    }

    /**
     * @brief 解析鼠标位置对应的命中实体
     * @param pos 鼠标绝对位置
     * @param windowID 窗口ID
     * @return 命中的实体，如果未命中返回 entt::null
     */
    entt::entity resolveHitEntity(const Vec2& pos, uint32_t windowID)
    {
        entt::entity topWindow = RuntimeFacade::current().windowLookup().findById(windowID);
        if (topWindow == entt::null) return entt::null;
        return findHitEntity(pos, topWindow);
    }

    void onRawPointerMove(const events::RawPointerMove& event)
    {
        const entt::entity hit = resolveHitEntity(event.position, event.windowID);
        Dispatcher::Enqueue<events::HitPointerMove>(events::HitPointerMove{.raw = event, .hitEntity = hit});
    }

    void onRawPointerButton(const events::RawPointerButton& event)
    {
        const entt::entity hit = resolveHitEntity(event.position, event.windowID);
        Dispatcher::Enqueue<events::HitPointerButton>(events::HitPointerButton{.raw = event, .hitEntity = hit});
    }

    void onRawPointerWheel(const events::RawPointerWheel& event)
    {
        const entt::entity hit = resolveHitEntity(event.position, event.windowID);
        Dispatcher::Enqueue<events::HitPointerWheel>(events::HitPointerWheel{.raw = event, .hitEntity = hit});
    }
};

} // namespace ui::systems