/**
 * ************************************************************************
 *
 * @file TaskChain.h
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2025-12-25
 * @version 0.1
 * @brief ui 每帧执行的任务链封装
 *
 * 当前采用固定顺序的轻量帧管线：
 * 1. QueuedTask：推进帧上下文、Timer 和事件队列派发
 * 2. InputTask：轮询 SDL 输入并将原始输入事件送入事件系统
 * 3. RenderTask：触发常规帧的 UpdateLayout / UpdateRendering / EndFrame
 *
 * 这里定义的是“常规帧路径”。
 * 少数平台阻塞场景下，InteractionSystem 仍可能直接触发即时布局或渲染补救，
 * 但那不是主帧循环的默认行为。

 *
 * ************************************************************************
 * @copyright Copyright (c) 2025 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */

#pragma once

#include <entt/entt.hpp>
#include "../common/Events.hpp"
#include "../common/GlobalContext.hpp"
#include "RuntimeFacade.hpp"
#include "SystemManager.hpp"
#include "../singleton/Dispatcher.hpp"
namespace ui::tasks
{

// --- 1. 基础 Concept 与 辅助工具 ---

template <typename T>
concept IsTask = requires { typename std::remove_cvref_t<T>::is_task_tag; };

// --- 2. 核心组合器：广播模式 ---

template <typename F, typename G>
struct Combined
{
    using is_task_tag = void;
    F first;
    G second;

    // C++23 deducing this: 处理任务对象的生命周期（左值拷贝/右值移动）
    template <typename Self, typename... Args>
    decltype(auto) operator()(this Self&& self, Args&&... args)
    {
        // 广播模式：每个任务都接收相同的原始参数
        std::invoke(std::forward<Self>(self).first, args...);
        return std::invoke(std::forward<Self>(self).second, std::forward<Args>(args)...);
    }
};

// --- 3. 参数种子节点 (The Wrapper) ---

template <typename... StoredArgs>
struct BoundContext
{
    using is_task_tag = void;
    std::tuple<StoredArgs...> args;

    // 当 Context 遇到第一个任务时，将参数绑定
    template <typename Self, typename Task>
    auto operator|(this Self&& self, Task&& next)
    {
        // 返回一个闭包，它捕获了参数，并且它的 operator() 不需要再传参
        return [storedArgs = std::forward<Self>(self).args, nextTask = std::forward<Task>(next)]() mutable
        { return std::apply(nextTask, storedArgs); };
    }
};

// --- 4. CPO 实现 ---

namespace pipe_cpo
{
struct PipeFn
{
    // 处理任务之间的拼接 (Task | Task)
    template <IsTask F, IsTask G>
    constexpr auto operator()(F&& firstTask, G&& secondTask) const
    {
        return Combined<std::decay_t<F>, std::decay_t<G>>{std::forward<F>(firstTask), std::forward<G>(secondTask)};
    }
};
} // namespace pipe_cpo
inline constexpr pipe_cpo::PipeFn PIPE_COMPOSER{};

// --- 5. 运算符重载 ---

// 情况 A: 任务 | 任务
template <IsTask F, IsTask G>
auto operator|(F&& firstTask, G&& secondTask)
{
    return PIPE_COMPOSER(std::forward<F>(firstTask), std::forward<G>(secondTask));
}

// 情况 B: Context | 任务 (由 BoundContext 内部实现，此处仅为语义辅助)
template <typename... T>
auto WrapArgs(T&&... args)
{
    return BoundContext<std::decay_t<T>...>{std::make_tuple(std::forward<T>(args)...)};
}

// --- 6. 具体任务类实现 ---

struct RenderTask
{
    using is_task_tag = void;
    uint32_t remainingTime = 0;
    uint32_t delayTime = 16;

    void operator()(uint32_t delta)
    {
        if (remainingTime > delta)
        {
            remainingTime -= delta;
            return;
        }
        remainingTime = delayTime;
        // 常规帧刷新顺序：先布局，再渲染，最后提交帧尾状态。
        Dispatcher::Trigger<ui::events::UpdateLayout>();
        Dispatcher::Trigger<ui::events::UpdateRendering>();
        Dispatcher::Trigger<ui::events::EndFrame>(); // 帧结束时批量应用状态更新
    }
};

struct InputTask
{
    using is_task_tag = void;
    SystemManager* systems = nullptr; ///< 由 Application 注入，不可为 nullptr
    uint32_t remainingTime = 0;
    uint32_t delayTime = 32;

    void operator()(uint32_t delta)
    {
        if (remainingTime > delta)
        {
            remainingTime -= delta;
            return;
        }
        remainingTime = delayTime;
        systems->pollInput();                         // InteractionSystem::pollSdlEvents()
        Dispatcher::Trigger<events::TickKeyRepeat>(); // 驱动 TextInputSystem::doProcessKeyRepeat()
    }
};

struct QueuedTask
{
    using is_task_tag = void;

    void operator()(uint32_t delta)
    {
        // 队列阶段先推进帧上下文，再驱动定时器与缓冲事件派发。
        auto& frameContext = RuntimeFacade::current().frame();
        frameContext.intervalMs = delta;
        frameContext.frameSlot = (frameContext.frameSlot + 1) % 2;
        Dispatcher::Trigger<ui::events::UpdateTimer>();
        Dispatcher::Update();
    }
};

} // namespace ui::tasks