/**
 * ************************************************************************
 *
 * @file ActionSystem.h
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2026-01-28 (Updated)
 * @version 0.2
 * @brief 控件动作系统 - 处理抽象交互事件
 *
 * 职责：
 * - 处理高级抽象事件：点击(Click)、悬停(Hover)、长按(LongPress)等
 * - 触发组件上注册的回调函数
 *
 * 设计原则：
 * - 基于ECS事件驱动
 * - 易于扩展新的抽象动作类型
 *
 * ************************************************************************
 * @copyright Copyright (c) 2026 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */

#pragma once

#include <entt/entt.hpp>
#include "common/Events.hpp"
#include "singleton/Logger.hpp"
#include "interface/ISystem.hpp"
#include "api/Animation.hpp"
#include "api/Hierarchy.hpp"
#include "common/components/Animation.hpp"
#include "common/components/Interaction.hpp"
#include "common/components/Layout.hpp"
#include "common/Tags.hpp"
#include "core/RuntimeFacade.hpp"
#include "common/GlobalContext.hpp"
namespace ui::systems
{
class ActionSystem : public ui::interface::EnableRegister<ActionSystem>
{
public:
    ActionSystem() = default;
    explicit ActionSystem(entt::registry& reg, entt::dispatcher& disp) : m_reg(&reg), m_disp(&disp) {}

    void registerHandlersImpl()
    {
        auto& disp = m_disp != nullptr ? *m_disp : RuntimeFacade::current().enttDispatcher();
        // 只监听抽象事件
        disp.sink<ui::events::ClickEvent>().connect<&ActionSystem::onClickEvent>(*this);
        disp.sink<ui::events::HoverEvent>().connect<&ActionSystem::onHoverEvent>(*this);
        disp.sink<ui::events::UnhoverEvent>().connect<&ActionSystem::onUnhoverEvent>(*this);

        // 监听底层事件以驱动交互动效
        disp.sink<ui::events::MousePressEvent>().connect<&ActionSystem::onMousePress>(*this);
        disp.sink<ui::events::MouseReleaseEvent>().connect<&ActionSystem::onMouseRelease>(*this);
        disp.sink<ui::events::HitPointerMove>().connect<&ActionSystem::onHitPointerMove>(*this);

        // 默认拖放处理路径：监听拖放事件并更新层级
        disp.sink<ui::events::DragDroppedEvent>().connect<&ActionSystem::onDragDroppedDefault>(*this);
    }
    void unregisterHandlersImpl()
    {
        auto& disp = m_disp != nullptr ? *m_disp : RuntimeFacade::current().enttDispatcher();
        disp.sink<ui::events::ClickEvent>().disconnect<&ActionSystem::onClickEvent>(*this);
        disp.sink<ui::events::HoverEvent>().disconnect<&ActionSystem::onHoverEvent>(*this);
        disp.sink<ui::events::UnhoverEvent>().disconnect<&ActionSystem::onUnhoverEvent>(*this);

        disp.sink<ui::events::MousePressEvent>().disconnect<&ActionSystem::onMousePress>(*this);
        disp.sink<ui::events::MouseReleaseEvent>().disconnect<&ActionSystem::onMouseRelease>(*this);
        disp.sink<ui::events::HitPointerMove>().disconnect<&ActionSystem::onHitPointerMove>(*this);

        disp.sink<ui::events::DragDroppedEvent>().disconnect<&ActionSystem::onDragDroppedDefault>(*this);
    }

private:
    entt::registry* m_reg = nullptr;
    entt::dispatcher* m_disp = nullptr;
    [[nodiscard]] entt::registry& effectiveReg() noexcept
    {
        return m_reg != nullptr ? *m_reg : RuntimeFacade::current().enttRegistry();
    }
    void applyAnimation(entt::entity entity,
                        const std::optional<Vec2>& targetScale,
                        const std::optional<Vec2>& targetOffset,
                        float duration,
                        const Vec2& defaultScale,
                        const Vec2& defaultOffset)
    {
        ui::animation::TweenOptions options;
        options.duration = duration;
        options.easing = policies::Easing::EASE_OUT_QUAD;
        options.mode = policies::Play::ONCE;
        options.autoCleanup = true;

        ui::animation::StartTransformAnimation(entity, targetScale, targetOffset, options, defaultScale, defaultOffset);
    }

    [[nodiscard]] static bool isDropTargetEnabled(entt::registry& reg, entt::entity target)
    {
        if (!reg.valid(target)) return false;

        const bool hasDropMarker =
            reg.any_of<components::Droppable>(target) || reg.any_of<components::DroppableTag>(target);
        if (!hasDropMarker) return false;

        if (const auto* droppable = reg.try_get<components::Droppable>(target);
            droppable != nullptr && droppable->enabled != policies::Feature::ENABLED)
        {
            return false;
        }

        return true;
    }

    [[nodiscard]] static bool wouldCreateHierarchyCycle(entt::registry& reg, entt::entity source, entt::entity target)
    {
        if (!reg.valid(source) || !reg.valid(target)) return true;
        if (source == target) return true;

        entt::entity current = target;
        while (reg.valid(current) && current != entt::null)
        {
            if (current == source)
            {
                return true;
            }

            const auto* hierarchy = reg.try_get<components::Hierarchy>(current);
            current = hierarchy != nullptr ? hierarchy->parent : entt::null;
        }

        return false;
    }

    void startDragging(entt::entity entity, components::Draggable& draggable)
    {
        if (draggable.dragging) return;

        draggable.dragging = true;
        Dispatcher::Trigger<events::DragStartEvent>(events::DragStartEvent{entity});
        if (draggable.onDragStart) draggable.onDragStart();
    }

    void applyDragDelta(entt::registry& reg,
                        entt::entity entity,
                        const components::Draggable& draggable,
                        const Vec2& delta)
    {
        auto* pos = reg.try_get<components::Position>(entity);
        if (pos == nullptr) return;

        if (!draggable.lockX) pos->value.x() += delta.x();
        if (!draggable.lockY) pos->value.y() += delta.y();
    }

    void applyDragAnimation(entt::registry& reg, entt::entity entity)
    {
        auto* interact = reg.try_get<components::InteractiveAnimation>(entity);
        if (interact == nullptr) return;

        std::optional<Vec2> targetScale = interact->dragScale.has_value() ? interact->dragScale : interact->pressScale;
        std::optional<Vec2> targetOffset =
            interact->dragLiftOffset.has_value() ? interact->dragLiftOffset : interact->pressOffset;

        applyAnimation(
            entity, targetScale, targetOffset, interact->dragDuration, interact->normalScale, interact->normalOffset);
    }

    /**
     * @brief 处理命中点的指针移动事件
     * @param event 命中点指针移动事件数据
     */
    void onHitPointerMove(const ui::events::HitPointerMove& event)
    {
        auto& reg = effectiveReg();
        auto& ctx = RuntimeFacade::current().state();
        entt::entity entity = ctx.activeEntity;

        if (!reg.valid(entity)) return;

        auto* draggable = reg.try_get<components::Draggable>(entity);
        if (draggable == nullptr || draggable->enabled != policies::Feature::ENABLED) return;

        if (event.raw.delta == Vec2{0, 0}) return;

        startDragging(entity, *draggable);
        applyDragDelta(reg, entity, *draggable, event.raw.delta);

        Dispatcher::Trigger<events::DragMoveEvent>(events::DragMoveEvent{
            .source = entity,
            .delta = event.raw.delta,
            .hoverTarget = ctx.hoveredEntity,
        });

        if (draggable->onDragMove) draggable->onDragMove(event.raw.delta);

        applyDragAnimation(reg, entity);
    }

    void onMousePress(const ui::events::MousePressEvent& event)
    {
        auto& reg = effectiveReg();
        entt::entity entity = event.entity;

        if (!reg.valid(entity)) return;

        if (auto* draggable = reg.try_get<components::Draggable>(entity))
        {
            draggable->dragging = false;
        }

        if (auto* interact = reg.try_get<components::InteractiveAnimation>(entity))
        {
            applyAnimation(entity,
                           interact->pressScale,
                           interact->pressOffset,
                           interact->pressDuration,
                           interact->normalScale,
                           interact->normalOffset);
        }
    }

    void onMouseRelease(const ui::events::MouseReleaseEvent& event)
    {
        auto& reg = effectiveReg();
        auto& ctx = RuntimeFacade::current().state();
        entt::entity entity = event.entity;

        if (!reg.valid(entity)) return;

        if (auto* draggable = reg.try_get<components::Draggable>(entity); draggable != nullptr)
        {
            entt::entity dropTarget = entt::null;
            if (draggable->dragging)
            {
                const entt::entity hovered = ctx.hoveredEntity;
                if (isDropTargetEnabled(reg, hovered) && !wouldCreateHierarchyCycle(reg, entity, hovered))
                {
                    dropTarget = hovered;
                    Dispatcher::Trigger<events::DragDroppedEvent>(
                        events::DragDroppedEvent{.source = entity, .target = dropTarget});
                }

                Dispatcher::Trigger<events::DragEndEvent>(
                    events::DragEndEvent{.source = entity, .dropTarget = dropTarget});
                if (draggable->onDragEnd) draggable->onDragEnd();
            }
            draggable->dragging = false;
        }

        if (auto* interact = reg.try_get<components::InteractiveAnimation>(entity))
        {
            // 恢复到 Hover 状态 (如果 Hover 存在且仍在悬浮)，否则恢复 Normal
            bool isHovered = (ctx.hoveredEntity == entity);

            std::optional<Vec2> targetScale = (isHovered && interact->hoverScale.has_value())
                                                ? interact->hoverScale
                                                : std::optional<Vec2>(interact->normalScale);
            std::optional<Vec2> targetOffset = (isHovered && interact->hoverOffset.has_value())
                                                 ? interact->hoverOffset
                                                 : std::optional<Vec2>(interact->normalOffset);

            float duration = interact->pressDuration; // 恢复速度通常快一点

            applyAnimation(entity, targetScale, targetOffset, duration, interact->normalScale, interact->normalOffset);
        }
    }

    /**
     * @brief 处理点击事件
     * @param event 点击事件数据
     */
    void onClickEvent(const ui::events::ClickEvent& event)
    {
        auto& reg = effectiveReg();
        if (!reg.valid(event.entity)) return;

        // 处理点击回调
        auto* clickable = reg.try_get<ui::components::Clickable>(event.entity);
        if (clickable != nullptr && clickable->enabled == policies::Feature::ENABLED && clickable->onClick)
        {
            Logger::info("Entity {} clicked", static_cast<uint32_t>(event.entity));
            clickable->onClick();
        }
    }

    /**
     * @brief 处理悬浮事件
     * @param event 悬浮事件数据
     */
    void onHoverEvent(const ui::events::HoverEvent& event)
    {
        auto& reg = effectiveReg();
        if (!reg.valid(event.entity)) return;

        // 处理 Hover 回调
        auto* hoverable = reg.try_get<ui::components::Hoverable>(event.entity);
        if (hoverable != nullptr && hoverable->enabled == policies::Feature::ENABLED && hoverable->onHover)
        {
            hoverable->onHover();
        }

        // 处理悬停动效
        if (auto* interact = reg.try_get<components::InteractiveAnimation>(event.entity))
        {
            applyAnimation(event.entity,
                           interact->hoverScale,
                           interact->hoverOffset,
                           interact->hoverDuration,
                           interact->normalScale,
                           interact->normalOffset);
        }
    }

    /**
     * @brief 处理取消悬浮事件
     * @param event 取消悬浮事件数据
     */
    void onUnhoverEvent(const ui::events::UnhoverEvent& event)
    {
        auto& reg = effectiveReg();
        if (!reg.valid(event.entity)) return;

        // 处理 Unhover 回调
        auto* hoverable = reg.try_get<ui::components::Hoverable>(event.entity);
        if (hoverable != nullptr && hoverable->enabled == policies::Feature::ENABLED && hoverable->onUnhover)
        {
            hoverable->onUnhover();
        }

        // 恢复正常状态
        if (auto* interact = reg.try_get<components::InteractiveAnimation>(event.entity))
        {
            // 恢复到 Normal Scale/Offset
            applyAnimation(event.entity,
                           std::optional<Vec2>(interact->normalScale),
                           std::optional<Vec2>(interact->normalOffset),
                           interact->hoverDuration,
                           interact->normalScale,
                           interact->normalOffset);
        }
    }

    void onDragDroppedDefault(const ui::events::DragDroppedEvent& event)
    {
        auto& reg = effectiveReg();
        if (!reg.valid(event.source) || !reg.valid(event.target)) return;
        if (!isDropTargetEnabled(reg, event.target)) return;
        if (wouldCreateHierarchyCycle(reg, event.source, event.target)) return;

        ui::hierarchy::AddChild(event.target, event.source);
    }
};
} // namespace ui::systems