#include "Utils.hpp"
#include <algorithm>
#include <cstdint>
#include "../singleton/Registry.hpp"
#include "../singleton/Dispatcher.hpp"
#include "../common/Components.hpp"
#include "../systems/TimerSystem.hpp"
namespace ui::utils
{
void MarkLayoutChanged(::entt::entity entity)
{
    if (!Registry::Valid(entity)) return;

    entt::entity current = entity;
    while (current != ::entt::null && Registry::Valid(current))
    {
        Registry::EmplaceOrReplace<components::LayoutDirtyTag>(current);
        const auto* hierarchy = Registry::TryGet<components::Hierarchy>(current);
        current = hierarchy != nullptr ? hierarchy->parent : entt::null;
    }
}

void MarkVisualChanged(::entt::entity entity)
{
    if (!Registry::Valid(entity)) return;

    Registry::EmplaceOrReplace<components::RenderDirtyTag>(entity);

    // 向上查找所属根窗口/对话框，确保 RenderSystem 能捕获渲染脏标记
    entt::entity current = entity;
    entt::entity rootWindow = entt::null;

    while (current != entt::null && Registry::Valid(current))
    {
        if (Registry::AnyOf<components::WindowTag, components::DialogTag>(current))
        {
            rootWindow = current;
        }
        const auto* hierarchy = Registry::TryGet<components::Hierarchy>(current);
        current = hierarchy != nullptr ? hierarchy->parent : entt::null;
    }

    if (rootWindow != entt::null && rootWindow != entity)
    {
        Registry::EmplaceOrReplace<components::RenderDirtyTag>(rootWindow);
    }
}

void MarkLayoutAndVisualChanged(::entt::entity entity)
{
    if (!Registry::Valid(entity)) return;

    MarkLayoutChanged(entity);
    MarkVisualChanged(entity);
}

void MarkLayoutDirty(::entt::entity entity)
{
    MarkLayoutChanged(entity);
}

void MarkRenderDirty(::entt::entity entity)
{
    MarkVisualChanged(entity);
}

bool HasAlignment(policies::Alignment value, policies::Alignment flag)
{
    return (static_cast<uint8_t>(value) & static_cast<uint8_t>(flag)) != 0;
}

void SetWindowFlag(::entt::entity entity, policies::WindowFlag flag)
{
    if (!Registry::Valid(entity)) return;
    auto& windowComp = Registry::GetOrEmplace<components::Window>(entity);

    windowComp.flags |= flag;
}

void CloseWindow(::entt::entity entity)
{
    if (!Registry::Valid(entity)) return;
    Dispatcher::Enqueue<events::CloseWindow>(events::CloseWindow{entity});
}

void QuitUiEventLoop()
{
    Dispatcher::Trigger<ui::events::QuitRequested>(ui::events::QuitRequested{});
};

void InvokeTask(std::move_only_function<void()> func)
{
    systems::TimerSystem::addTask(0, std::move(func), true);
}
/**
 * @brief 注册一个定时任务，返回任务句柄
 * @param interval 间隔时间（毫秒）
 * @param func 任务函数
 * @return 任务句柄
 */
TaskHandle TimerCallback(uint32_t interval, std::move_only_function<void()> func)
{
    return systems::TimerSystem::addTask(interval, std::move(func));
}

/**
 * @brief 取消注册一个定时任务
 * @param handle 任务句柄
 */
void CancelQueuedTask(TaskHandle handle)
{
    systems::TimerSystem::cancelTask(handle);
}

bool IsEntityExist(const std::string& alias)
{
    auto view = Registry::View<components::BaseInfo>();
    for (const auto entity : view)
    {
        if (view.get<components::BaseInfo>(entity).alias == alias)
        {
            return true;
        }
    }
    std::ranges::any_of(
        view, [&view, &alias](entt::entity entity) { return view.get<components::BaseInfo>(entity).alias == alias; });
    return false;
}

} // namespace ui::utils