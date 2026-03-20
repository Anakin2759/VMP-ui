/**
 * ************************************************************************
 *
 * @file TweenSystem.h
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2025-12-05
 * @version 0.1
 * @brief UI动画系统
 *
 * 负责更新所有UI动画的ECS系统，事件驱动。
    不负责渲染，只更新动画状态

    在布局和渲染系统之前运行
    基于组件的数据驱动系统

    只做插值计算和状态更新

 *
 * ************************************************************************
 * @copyright Copyright (c) 2025 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */

#pragma once
#include <entt/entt.hpp>
#include <cmath>
#include "../common/Policies.hpp"
#include "../common/Components.hpp"
#include "../common/GlobalContext.hpp"
#include "../common/Events.hpp"
#include "../singleton/Registry.hpp"
#include "../singleton/Dispatcher.hpp"
#include "../api/Utils.hpp"
#include "../interface/Isystem.hpp"
#include <sys/stat.h>

namespace ui::systems
{

class TweenSystem : public ui::interface::EnableRegister<TweenSystem>
{
public:
    void registerHandlersImpl() { Dispatcher::Sink<events::UpdateEvent>().connect<&TweenSystem::update>(*this); }

    void unregisterHandlersImpl() { Dispatcher::Sink<events::UpdateEvent>().disconnect<&TweenSystem::update>(*this); }

private:
    /**
     * @brief 更新所有活动的动画
     */
    void update()
    {
        float deltaTime = 16.0F; // 默认 16ms
        if (const auto* ctx = ui::Registry::ctx().find<globalcontext::FrameContext>())
        {
            if (ctx->intervalMs > 0)
            {
                deltaTime = static_cast<float>(ctx->intervalMs);
            }
        }

        auto view = ui::Registry::View<components::AnimationTime, components::AnimatingTag>();
        std::vector<entt::entity> completedEntities;

        view.each(
            [this, deltaTime, &completedEntities](entt::entity entity, components::AnimationTime& anim)
            {
                if (anim.state != policies::AnimationState::Playing)
                {
                    completedEntities.push_back(entity);
                    return;
                }

                float time = updateTime(anim, deltaTime);

                // 4. 应用缓动函数计算插值系数
                float val = applyEasing(time, anim.easing);

                // 5. 应用到具体的动画属性组件
                bool dirty = false;
                dirty |= updatePosition(entity, val);
                dirty |= updateAlpha(entity, val);
                dirty |= updateScale(entity, val);
                dirty |= updateRenderOffset(entity, val);
                dirty |= updateColor(entity, val);

                if (dirty)
                {
                    ui::utils::MarkRenderDirty(entity);
                }

                if (anim.mode == policies::Play::ONCE && time >= 1.0F)
                {
                    anim.state = policies::AnimationState::Stopped;
                    completedEntities.push_back(entity);
                }
            });

        for (const auto entity : completedEntities)
        {
            finishAnimation(entity);
        }
    }

    float updateTime(components::AnimationTime& anim, float deltaTime)
    {
        // 1. 更新时间
        anim.elapsed += deltaTime;

        // 2. 计算归一化进度 t [0.0, 1.0]
        float time = (anim.duration > 0.0F) ? (anim.elapsed / anim.duration) : 1.0F;

        // 3. 根据播放模式处理 t
        switch (anim.mode)
        {
            case policies::Play::ONCE:
                time = std::min(time, 1.0F);
                break;
            case policies::Play::LOOP:
                if (time >= 1.0F)
                {
                    time = std::fmod(time, 1.0F);
                    // 为了保持精度，重置 elapsed
                    anim.elapsed = time * anim.duration;
                }
                break;
            case policies::Play::PINGPONG:
            {
                // 计算总周期数
                float totalCycles = time;
                // 取小数部分 x 2
                // 0~1: 正向, 1~2: 反向
                float cycleT = std::fmod(totalCycles, 2.0F);
                if (cycleT > 1.0F)
                {
                    time = 2.0F - cycleT;
                }
                else
                {
                    time = cycleT;
                }
            }
            break;
        }
        return time;
    }

    bool updatePosition(entt::entity entity, float val)
    {
        if (auto* animPos = ui::Registry::TryGet<components::AnimationPosition>(entity))
        {
            if (auto* pos = ui::Registry::TryGet<components::Position>(entity))
            {
                pos->value = animPos->from + ((animPos->to - animPos->from) * val);
                ui::utils::MarkLayoutDirty(entity);
                return true;
            }
        }

        return false;
    }

    bool updateAlpha(entt::entity entity, float val)
    {
        if (auto* animAlpha = ui::Registry::TryGet<components::AnimationAlpha>(entity))
        {
            float currentAlpha = animAlpha->from + ((animAlpha->to - animAlpha->from) * val);

            // Lambda to apply alpha safely
            auto applyAlpha = [currentAlpha](auto* component)
            {
                if (component) component->color.alpha = currentAlpha;
            };

            // 应用到常见组件
            applyAlpha(ui::Registry::TryGet<components::Background>(entity));
            applyAlpha(ui::Registry::TryGet<components::Text>(entity));
            applyAlpha(ui::Registry::TryGet<components::Border>(entity));
            applyAlpha(ui::Registry::TryGet<components::Shadow>(entity));
            applyAlpha(ui::Registry::TryGet<components::Arrow>(entity));

            // Image 组件使用 tintColor
            if (auto* img = ui::Registry::TryGet<components::Image>(entity))
            {
                img->tintColor.alpha = currentAlpha;
            }

            // Alpha 组件
            if (auto* alpha = ui::Registry::TryGet<components::Alpha>(entity))
            {
                alpha->value = currentAlpha;
            }

            return true;
        }

        return false;
    }
    /**
     * @brief 更新缩放动画
     */
    bool updateScale(entt::entity entity, float val)
    {
        if (auto* animScale = ui::Registry::TryGet<components::AnimationScale>(entity))
        {
            auto& scale = ui::Registry::GetOrEmplace<components::Scale>(entity);
            scale.value = animScale->from + ((animScale->to - animScale->from) * val);
            return true;
        }

        return false;
    }

    bool updateRenderOffset(entt::entity entity, float val)
    {
        if (auto* animOffset = ui::Registry::TryGet<components::AnimationRenderOffset>(entity))
        {
            auto& offset = ui::Registry::GetOrEmplace<components::RenderOffset>(entity);
            offset.value = animOffset->from + ((animOffset->to - animOffset->from) * val);
            return true;
        }

        return false;
    }

    bool updateColor(entt::entity entity, float val)
    {
        if (auto* animColor = ui::Registry::TryGet<components::AnimationColor>(entity))
        {
            ui::Color currentColor;
            currentColor.red = animColor->from.red + ((animColor->to.red - animColor->from.red) * val);
            currentColor.green = animColor->from.green + ((animColor->to.green - animColor->from.green) * val);
            currentColor.blue = animColor->from.blue + ((animColor->to.blue - animColor->from.blue) * val);
            currentColor.alpha = animColor->from.alpha + ((animColor->to.alpha - animColor->from.alpha) * val);

            if (auto* background = ui::Registry::TryGet<components::Background>(entity))
            {
                background->color = currentColor;
            }

            return true;
        }

        return false;
    }

    static void finishAnimation(entt::entity entity)
    {
        if (!ui::Registry::Valid(entity)) return;

        auto* animTime = ui::Registry::TryGet<components::AnimationTime>(entity);
        if (animTime == nullptr) return;

        ui::Registry::Remove<components::AnimatingTag>(entity);

        if (!animTime->autoCleanup)
        {
            return;
        }

        ui::Registry::Remove<components::AnimationPosition>(entity);
        ui::Registry::Remove<components::AnimationAlpha>(entity);
        ui::Registry::Remove<components::AnimationScale>(entity);
        ui::Registry::Remove<components::AnimationRenderOffset>(entity);
        ui::Registry::Remove<components::AnimationColor>(entity);
        ui::Registry::Remove<components::AnimationTime>(entity);
    }

    /**
     * @brief 应用缓动函数
     */
    static float applyEasing(float time, policies::Easing easing)
    {
        switch (easing)
        {
            case policies::Easing::LINEAR: // 线性缓动
                return time;
            case policies::Easing::EASE_IN_QUAD: // 二次缓入
                return time * time;
            case policies::Easing::EASE_OUT_QUAD: // 二次缓出
                return time * (2.0F - time);
            case policies::Easing::EASE_IN_OUT_QUAD: // 二次缓入缓出
                return time < 0.5F ? 2.0F * time * time : -1.0F + ((4.0F - (2.0F * time)) * time);
            case policies::Easing::EASE_IN_SINE:
                return 1.0F - std::cos((time * 3.1415926F) / 2.0F);
            case policies::Easing::EASE_OUT_SINE:
                return std::sin((time * 3.1415926F) / 2.0F);
            case policies::Easing::EASE_IN_OUT_SINE:
                return -(std::cos(3.1415926F * time) - 1.0F) / 2.0F;
            default:
                return time;
        }
    }
};

} // namespace ui::systems
