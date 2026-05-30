/**
 * ************************************************************************
 *
 * @file HitTestSystem.hpp
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2026-01-28
 * @version 0.2

    Maps mouse positions to UI entities, respects Z-order, and emits state transition requests.
 *
 * ************************************************************************
 * @copyright Copyright (c) 2026 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */

#pragma once
#include <entt/entt.hpp>
#include <vector>
#include <unordered_map>
#include "common/Types.hpp"
#include "common/Events.hpp"
#include "interface/ISystem.hpp"
#include "singleton/Dispatcher.hpp"
#include "singleton/Registry.hpp"

namespace ui::systems
{

/**
 */
class HitTestSystem : public ui::interface::EnableRegister<HitTestSystem>
{
public:
    HitTestSystem() = default;
    explicit HitTestSystem(Registry& reg, Dispatcher& disp) : m_reg(&reg), m_disp(&disp) {}

    void registerHandlersImpl();

    void unregisterHandlersImpl();

    /**
     * @param point 鼠标绝对位置
     * @param pos 实体绝对位置
     * @param size 实体尺寸
     * @return bool 是否命中
     */
    static bool isPointInRect(const Vec2& point, const Vec2& pos, const Vec2& size);

    /**
     * @brief 计算实体的绝对位置（考虑父节点层级）
     * @param entity 当前实体
     */
    static Vec2 getAbsolutePosition(entt::entity entity);

    /**
     * @return 根窗口实体，如果不在任何窗口内则返回 entt::null
     */
    entt::entity findRootWindow(entt::entity entity) const;

    /**
     * @param topWindow 当前鼠标所在的顶层窗口（entt::null 表示不在任何窗口内）
     * @note 使用缓存机制，只在缓存失效时重新计算
     * @note 该缓存只保存“窗口内哪些实体当前可交互以及它们的排序”，
     */
    std::vector<entt::entity> getZOrderedInteractables(entt::entity topWindow);

    /**
     * @param mousePos 鼠标绝对位置
     * @param topWindow 当前窗口实体
     */
    entt::entity findHitEntity(const Vec2& mousePos, entt::entity topWindow);

private:
    /**
     * @brief 连接组件生命周期事件以自动失效缓存
     */
    template <typename Component>
    void connectInvalidateConstructDestroy()
    {
        m_reg->on_construct<Component>().template connect<&HitTestSystem::markHitCacheDirty>(*this);
        m_reg->on_destroy<Component>().template connect<&HitTestSystem::markHitCacheDirty>(*this);
    }

    template <typename Component>
    void disconnectInvalidateConstructDestroy()
    {
        m_reg->on_construct<Component>().template disconnect<&HitTestSystem::markHitCacheDirty>(*this);
        m_reg->on_destroy<Component>().template disconnect<&HitTestSystem::markHitCacheDirty>(*this);
    }

    template <typename Component>
    void connectInvalidateConstructUpdateDestroy()
    {
        m_reg->on_construct<Component>().template connect<&HitTestSystem::markHitCacheDirty>(*this);
        m_reg->on_update<Component>().template connect<&HitTestSystem::markHitCacheDirty>(*this);
        m_reg->on_destroy<Component>().template connect<&HitTestSystem::markHitCacheDirty>(*this);
    }

    template <typename Component>
    void disconnectInvalidateConstructUpdateDestroy()
    {
        m_reg->on_construct<Component>().template disconnect<&HitTestSystem::markHitCacheDirty>(*this);
        m_reg->on_update<Component>().template disconnect<&HitTestSystem::markHitCacheDirty>(*this);
        m_reg->on_destroy<Component>().template disconnect<&HitTestSystem::markHitCacheDirty>(*this);
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

    bool m_hitCacheDirty = true;
    Registry* m_reg = nullptr;
    Dispatcher* m_disp = nullptr;

    /**
     * @brief 使所有窗口的缓存失效
     */
    void invalidateAllCaches();

    /**
     */
    void markHitCacheDirty(entt::entity entity);
    void processPendingCacheInvalidationTags();

    /**
     */
    bool isEntityInteractable(entt::entity entity) const;

    /**
     */
    int calculateZOrder(entt::entity entity) const;

    /**
     * @param pos 鼠标绝对位置
     * @param windowID 窗口ID
     */
    entt::entity resolveHitEntity(const Vec2& pos, uint32_t windowID);

    void onRawPointerMove(const events::RawPointerMove& event);
    void onRawPointerButton(const events::RawPointerButton& event);
    void onRawPointerWheel(const events::RawPointerWheel& event);
};

} // namespace ui::systems
