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
#include "../common/Types.hpp"
#include "../common/Components.hpp"
#include "../common/Tags.hpp"
#include "../singleton/Registry.hpp"
#include "../singleton/Dispatcher.hpp"
#include "../common/Events.hpp"
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

        // 监听可能导致缓存失效的事件
        Registry::OnConstruct<components::ZOrderIndex>().connect<&HitTestSystem::onZOrderChanged>(*this);
        Registry::OnUpdate<components::ZOrderIndex>().connect<&HitTestSystem::onZOrderChanged>(*this);
        Registry::OnDestroy<components::ZOrderIndex>().connect<&HitTestSystem::onZOrderChanged>(*this);

        Registry::OnConstruct<components::Hierarchy>().connect<&HitTestSystem::onHierarchyChanged>(*this);
        Registry::OnUpdate<components::Hierarchy>().connect<&HitTestSystem::onHierarchyChanged>(*this);
        Registry::OnDestroy<components::Hierarchy>().connect<&HitTestSystem::onHierarchyChanged>(*this);

        Registry::OnConstruct<components::VisibleTag>().connect<&HitTestSystem::onVisibilityChanged>(*this);
        Registry::OnDestroy<components::VisibleTag>().connect<&HitTestSystem::onVisibilityChanged>(*this);

        // 监听会改变“是否可交互”的组件，避免命中缓存保留过期成员
        Registry::OnConstruct<components::Clickable>().connect<&HitTestSystem::onInteractivityChanged>(*this);
        Registry::OnDestroy<components::Clickable>().connect<&HitTestSystem::onInteractivityChanged>(*this);

        Registry::OnConstruct<components::ScrollArea>().connect<&HitTestSystem::onInteractivityChanged>(*this);
        Registry::OnDestroy<components::ScrollArea>().connect<&HitTestSystem::onInteractivityChanged>(*this);

        Registry::OnConstruct<components::SliderInfo>().connect<&HitTestSystem::onInteractivityChanged>(*this);
        Registry::OnDestroy<components::SliderInfo>().connect<&HitTestSystem::onInteractivityChanged>(*this);

        Registry::OnConstruct<components::TextEditTag>().connect<&HitTestSystem::onInteractivityChanged>(*this);
        Registry::OnDestroy<components::TextEditTag>().connect<&HitTestSystem::onInteractivityChanged>(*this);

        Registry::OnConstruct<components::DisabledTag>().connect<&HitTestSystem::onInteractivityChanged>(*this);
        Registry::OnDestroy<components::DisabledTag>().connect<&HitTestSystem::onInteractivityChanged>(*this);

        Registry::OnConstruct<components::TextEdit>().connect<&HitTestSystem::onTextEditChanged>(*this);
        Registry::OnUpdate<components::TextEdit>().connect<&HitTestSystem::onTextEditChanged>(*this);
        Registry::OnDestroy<components::TextEdit>().connect<&HitTestSystem::onTextEditChanged>(*this);
    }

    void unregisterHandlersImpl()
    {
        Dispatcher::Sink<events::RawPointerMove>().disconnect<&HitTestSystem::onRawPointerMove>(*this);
        Dispatcher::Sink<events::RawPointerButton>().disconnect<&HitTestSystem::onRawPointerButton>(*this);
        Dispatcher::Sink<events::RawPointerWheel>().disconnect<&HitTestSystem::onRawPointerWheel>(*this);

        // 断开缓存失效的信号
        Registry::OnConstruct<components::ZOrderIndex>().disconnect<&HitTestSystem::onZOrderChanged>(*this);
        Registry::OnUpdate<components::ZOrderIndex>().disconnect<&HitTestSystem::onZOrderChanged>(*this);
        Registry::OnDestroy<components::ZOrderIndex>().disconnect<&HitTestSystem::onZOrderChanged>(*this);

        Registry::OnConstruct<components::Hierarchy>().disconnect<&HitTestSystem::onHierarchyChanged>(*this);
        Registry::OnUpdate<components::Hierarchy>().disconnect<&HitTestSystem::onHierarchyChanged>(*this);
        Registry::OnDestroy<components::Hierarchy>().disconnect<&HitTestSystem::onHierarchyChanged>(*this);

        Registry::OnConstruct<components::VisibleTag>().disconnect<&HitTestSystem::onVisibilityChanged>(*this);
        Registry::OnDestroy<components::VisibleTag>().disconnect<&HitTestSystem::onVisibilityChanged>(*this);

        Registry::OnConstruct<components::Clickable>().disconnect<&HitTestSystem::onInteractivityChanged>(*this);
        Registry::OnDestroy<components::Clickable>().disconnect<&HitTestSystem::onInteractivityChanged>(*this);

        Registry::OnConstruct<components::ScrollArea>().disconnect<&HitTestSystem::onInteractivityChanged>(*this);
        Registry::OnDestroy<components::ScrollArea>().disconnect<&HitTestSystem::onInteractivityChanged>(*this);

        Registry::OnConstruct<components::SliderInfo>().disconnect<&HitTestSystem::onInteractivityChanged>(*this);
        Registry::OnDestroy<components::SliderInfo>().disconnect<&HitTestSystem::onInteractivityChanged>(*this);

        Registry::OnConstruct<components::TextEditTag>().disconnect<&HitTestSystem::onInteractivityChanged>(*this);
        Registry::OnDestroy<components::TextEditTag>().disconnect<&HitTestSystem::onInteractivityChanged>(*this);

        Registry::OnConstruct<components::DisabledTag>().disconnect<&HitTestSystem::onInteractivityChanged>(*this);
        Registry::OnDestroy<components::DisabledTag>().disconnect<&HitTestSystem::onInteractivityChanged>(*this);

        Registry::OnConstruct<components::TextEdit>().disconnect<&HitTestSystem::onTextEditChanged>(*this);
        Registry::OnUpdate<components::TextEdit>().disconnect<&HitTestSystem::onTextEditChanged>(*this);
        Registry::OnDestroy<components::TextEdit>().disconnect<&HitTestSystem::onTextEditChanged>(*this);
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
        // 构建从当前实体到根的路径
        std::vector<entt::entity> path;
        entt::entity current = entity;
        while (current != entt::null && Registry::Valid(current))
        {
            path.push_back(current);
            const auto* hierarchy = Registry::TryGet<components::Hierarchy>(current);
            current = hierarchy == nullptr ? entt::null : hierarchy->parent;
        }

        // 从根到当前实体正向遍历，累加位置偏移
        Vec2 pos(0.0F, 0.0F);
        for (auto it = path.rbegin(); it != path.rend(); ++it)
        {
            entt::entity entity = *it;
            // 窗口本身的 Position 是屏幕坐标，在窗口内交互时应视为原点(0,0)，忽略其屏幕位置偏移
            if (Registry::AnyOf<components::WindowTag, components::DialogTag>(entity)) continue;

            const auto* posComp = Registry::TryGet<components::Position>(entity);
            if (posComp != nullptr)
            {
                pos.x() += posComp->value.x();
                pos.y() += posComp->value.y();
            }
        }
        return pos;
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

    /**
     * @brief 使指定窗口的缓存失效
     */
    void invalidateWindowCache(entt::entity window)
    {
        auto iterator = m_zOrderCache.find(window);
        if (iterator != m_zOrderCache.end())
        {
            iterator->second.dirty = true;
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
     * @brief ZOrderIndex 组件变化回调
     */
    void onZOrderChanged(entt::entity entity)
    {
        entt::entity window = findRootWindow(entity);
        invalidateWindowCache(window);
    }

    /**
     * @brief 层级关系变化回调
     */
    void onHierarchyChanged([[maybe_unused]] entt::entity entity)
    {
        // 层级变化可能影响多个窗口，为简化处理，使所有缓存失效
        invalidateAllCaches();
    }

    /**
     * @brief 可见性变化回调
     */
    void onVisibilityChanged([[maybe_unused]] entt::entity entity)
    {
        entt::entity window = findRootWindow(entity);
        invalidateWindowCache(window);
    }

    /**
     * @brief 交互资格变化回调
     *
     * 命中缓存保存的是“当前窗口内哪些实体需要参与命中测试”。
     * Clickable / ScrollArea / SliderInfo / TextEditTag / DisabledTag 的变化
     * 都会改变这个集合，因此必须使对应窗口缓存失效。
     */
    void onInteractivityChanged(entt::entity entity)
    {
        entt::entity window = findRootWindow(entity);
        invalidateWindowCache(window);
    }

    /**
     * @brief TextEdit 组件变化回调
     *
     * 当前命中系统会根据 TextEdit 的 ReadOnly 标志决定输入框是否参与交互。
     * 因此 TextEdit 更新也必须使窗口缓存失效。
     */
    void onTextEditChanged(entt::entity entity)
    {
        entt::entity window = findRootWindow(entity);
        invalidateWindowCache(window);
    }

    /**
     * @brief 解析鼠标位置对应的命中实体
     * @param pos 鼠标绝对位置
     * @param windowID 窗口ID
     * @return 命中的实体，如果未命中返回 entt::null
     */
    entt::entity resolveHitEntity(const Vec2& pos, uint32_t windowID)
    {
        entt::entity topWindow = entt::null;
        auto viewWin = Registry::View<components::Window>();
        for (auto entity : viewWin)
        {
            if (viewWin.get<components::Window>(entity).windowID == windowID)
            {
                topWindow = entity;
                break;
            }
        }

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