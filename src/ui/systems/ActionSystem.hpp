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
#include "../common/Events.hpp"
#include "../singleton/Dispatcher.hpp"
#include "../singleton/Registry.hpp"
#include "../singleton/Logger.hpp"
#include "../interface/Isystem.hpp"
#include "../api/Animation.hpp"
#include "../common/Components.hpp"
#include "../common/GlobalContext.hpp"
namespace ui::systems
{
class ActionSystem : public ui::interface::EnableRegister<ActionSystem>
{
public:
    void registerHandlersImpl()
    {
        // 只监听抽象事件
        Dispatcher::Sink<ui::events::ClickEvent>().connect<&ActionSystem::onClickEvent>(*this);
        Dispatcher::Sink<ui::events::HoverEvent>().connect<&ActionSystem::onHoverEvent>(*this);
        Dispatcher::Sink<ui::events::UnhoverEvent>().connect<&ActionSystem::onUnhoverEvent>(*this);

        // 监听底层事件以驱动交互动效
        Dispatcher::Sink<ui::events::MousePressEvent>().connect<&ActionSystem::onMousePress>(*this);
        Dispatcher::Sink<ui::events::MouseReleaseEvent>().connect<&ActionSystem::onMouseRelease>(*this);
        Dispatcher::Sink<ui::events::HitPointerMove>().connect<&ActionSystem::onHitPointerMove>(*this);
    }

    void unregisterHandlersImpl()
    {
        Dispatcher::Sink<ui::events::ClickEvent>().disconnect<&ActionSystem::onClickEvent>(*this);
        Dispatcher::Sink<ui::events::HoverEvent>().disconnect<&ActionSystem::onHoverEvent>(*this);
        Dispatcher::Sink<ui::events::UnhoverEvent>().disconnect<&ActionSystem::onUnhoverEvent>(*this);

        Dispatcher::Sink<ui::events::MousePressEvent>().disconnect<&ActionSystem::onMousePress>(*this);
        Dispatcher::Sink<ui::events::MouseReleaseEvent>().disconnect<&ActionSystem::onMouseRelease>(*this);
        Dispatcher::Sink<ui::events::HitPointerMove>().disconnect<&ActionSystem::onHitPointerMove>(*this);
    }

private:
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
    /**
     * @brief 处理命中点的指针移动事件
     * @param event 命中点指针移动事件数据
     */
    void onHitPointerMove(const ui::events::HitPointerMove& event)
    {
        auto& ctx = Registry::ctx().get<globalcontext::StateContext>();
        entt::entity entity = ctx.activeEntity;

        if (!Registry::Valid(entity)) return;

        // 检查是否可拖拽
        if (auto* draggable = Registry::TryGet<components::Draggable>(entity);
            draggable != nullptr && draggable->enabled == policies::Feature::Enabled)
        {
            // 只有当鼠标移动且按下时
            if (event.raw.delta == Vec2{0, 0}) return;

            if (!draggable->dragging)
            {
                draggable->dragging = true;
                if (draggable->onDragStart) draggable->onDragStart();
            }

            // 应用移动
            if (auto* pos = Registry::TryGet<components::Position>(entity))
            {
                if (!draggable->lockX) pos->value.x() += event.raw.delta.x();
                if (!draggable->lockY) pos->value.y() += event.raw.delta.y();
            }

            // 触发回调
            if (draggable->onDragMove) draggable->onDragMove(event.raw.delta);

            // 应用拖拽动效 (如果配置了 InteractiveAnimation)
            if (auto* interact = Registry::TryGet<components::InteractiveAnimation>(entity))
            {
                // 切换到 Drag 状态 (优先使用 drag 配置，否则回退到 press 配置)
                std::optional<Vec2> targetScale =
                    interact->dragScale.has_value() ? interact->dragScale : interact->pressScale;
                std::optional<Vec2> targetOffset =
                    interact->dragLiftOffset.has_value() ? interact->dragLiftOffset : interact->pressOffset;

                applyAnimation(entity,
                               targetScale,
                               targetOffset,
                               interact->dragDuration,
                               interact->normalScale,
                               interact->normalOffset);
            }
        }
    }

    void onMousePress(const ui::events::MousePressEvent& event)
    {
        entt::entity entity = event.entity;

        if (!Registry::Valid(entity)) return;

        if (auto* draggable = Registry::TryGet<components::Draggable>(entity))
        {
            draggable->dragging = false;
        }

        if (auto* interact = Registry::TryGet<components::InteractiveAnimation>(entity))
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
        auto& ctx = Registry::ctx().get<globalcontext::StateContext>();
        entt::entity entity = event.entity;

        if (!Registry::Valid(entity)) return;

        if (auto* draggable = Registry::TryGet<components::Draggable>(entity); draggable != nullptr)
        {
            if (draggable->dragging && draggable->onDragEnd) draggable->onDragEnd();
            draggable->dragging = false;
        }

        if (auto* interact = Registry::TryGet<components::InteractiveAnimation>(entity))
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
        if (!Registry::Valid(event.entity)) return;

        // 处理点击回调
        auto* clickable = Registry::TryGet<ui::components::Clickable>(event.entity);
        if (clickable != nullptr && clickable->enabled == policies::Feature::Enabled && clickable->onClick)
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
        if (!Registry::Valid(event.entity)) return;

        // 处理 Hover 回调
        auto* hoverable = Registry::TryGet<ui::components::Hoverable>(event.entity);
        if (hoverable != nullptr && hoverable->enabled == policies::Feature::Enabled && hoverable->onHover)
        {
            hoverable->onHover();
        }

        // 处理悬停动效
        if (auto* interact = Registry::TryGet<components::InteractiveAnimation>(event.entity))
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
        if (!Registry::Valid(event.entity)) return;

        // 处理 Unhover 回调
        auto* hoverable = Registry::TryGet<ui::components::Hoverable>(event.entity);
        if (hoverable != nullptr && hoverable->enabled == policies::Feature::Enabled && hoverable->onUnhover)
        {
            hoverable->onUnhover();
        }

        // 恢复正常状态
        if (auto* interact = Registry::TryGet<components::InteractiveAnimation>(event.entity))
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
};
} // namespace ui::systems