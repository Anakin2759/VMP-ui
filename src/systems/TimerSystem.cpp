/**
 * ************************************************************************
 *
 * @file TimerSystem.cpp
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2026-02-07
 * @version 0.1
 * @brief 定时器系统实现
 *
 * ************************************************************************
 * @copyright Copyright (c) 2026 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */

#include "TimerSystem.hpp"
#include "core/RuntimeFacade.hpp"
#include "singleton/Logger.hpp"
#include "singleton/Registry.hpp"
#include "common/GlobalContext.hpp"
#include "common/Tags.hpp"
#include "api/Utils.hpp"
#include "common/Events.hpp"
#include "common/Types.hpp"
#include "common/components/Data.hpp"
#include <cstdint>
#include <utility>

namespace ui::systems
{

void TimerSystem::registerHandlersImpl()
{
    m_disp->sink<events::UpdateTimer>().connect<&TimerSystem::onUpdateTimer>(*this);
}

void TimerSystem::unregisterHandlersImpl()
{
    m_disp->sink<events::UpdateTimer>().disconnect<&TimerSystem::onUpdateTimer>(*this);
}

uint32_t TimerSystem::addTask(uint32_t interval, VoidCallback func, bool singleShot)
{
    auto& frameCtx = RuntimeFacade::current().frame();
    auto& timerCtx = RuntimeFacade::current().ensureContext<globalcontext::TimerContext>();

    uint32_t taskId = timerCtx.nextTaskId++;

    globalcontext::TimerTask task;
    task.id = taskId;
    task.func = std::move(func);
    task.intervalMs = interval;
    task.remainingMs = interval;
    task.singleShot = singleShot;
    task.frameSlot = frameCtx.frameSlot;
    task.cancelled = false;

    timerCtx.tasks.emplace(taskId, std::move(task));

    Logger::info("TimerSystem: Added task {} with interval {}ms (singleShot={})", taskId, interval, singleShot);
    return taskId;
}

void TimerSystem::cancelTask(uint32_t handle)
{
    auto& timerCtx = RuntimeFacade::current().ensureContext<globalcontext::TimerContext>();
    auto taskIterator = timerCtx.tasks.find(handle);
    if (taskIterator != timerCtx.tasks.end())
    {
        taskIterator->second.cancelled = true;
        Logger::info("TimerSystem: Cancelled task {}", handle);
    }
}

void TimerSystem::update(uint32_t deltaMs)
{
    auto& frameCtx = RuntimeFacade::current().frame();
    auto& timerCtx = RuntimeFacade::current().ensureContext<globalcontext::TimerContext>();

    // 处理所有任务
    for (auto& [taskId, task] : timerCtx.tasks)
    {
        if (task.cancelled)
        {
            continue;
        }

        // 减少剩余时间
        if (task.remainingMs > deltaMs)
        {
            task.remainingMs -= deltaMs;
        }
        else
        {
            task.remainingMs = 0;
        }

        // 检查是否到期且帧槽位已切换
        if (task.remainingMs == 0 && task.frameSlot != frameCtx.frameSlot)
        {
            // 执行任务
            task.func();

            // 如果是单次任务，标记为已取消
            if (task.singleShot)
            {
                task.cancelled = true;
            }
            else
            {
                // 重置剩余时间
                task.remainingMs = task.intervalMs;
            }
        }

        // 更新帧槽位
        task.frameSlot = frameCtx.frameSlot;
    }

    // 清理已取消的任务（C++20 std::erase_if 支持 unordered_map）
    std::erase_if(timerCtx.tasks, [](const auto& pair) { return pair.second.cancelled; });

    // 驱动所有获焦 TextEdit 的 Caret 闪烁（固定时间间隔，点击后立即显示）
    const float deltaSec = static_cast<float>(deltaMs) / 1000.0F;
    auto caretView = Registry::View<components::Caret, components::FocusedTag>();
    for (auto entity : caretView)
    {
        auto& caret = caretView.get<components::Caret>(entity);
        caret.elapsedTime += deltaSec;
        if (caret.elapsedTime >= caret.blinkInterval)
        {
            caret.elapsedTime -= caret.blinkInterval;
            caret.visible = !caret.visible;
            ui::utils::MarkRenderDirty(entity);
        }
    }
}

void TimerSystem::onUpdateTimer([[maybe_unused]] const events::UpdateTimer& event)
{
    // UpdateTimer 事件会在每帧触发，我们在这里更新定时器
    // 但实际的 deltaMs 需要从 FrameContext 获取
    auto* frameCtx = RuntimeFacade::current().tryFrame();
    if (frameCtx != nullptr)
    {
        update(frameCtx->intervalMs);
    }
}

} // namespace ui::systems
