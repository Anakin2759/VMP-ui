#include "Utils.hpp"
#include <algorithm>
#include <cstdint>
#include <vector>
#include <ranges>
#include <utility>
#include <string>
#include "../singleton/Registry.hpp"
#include "../singleton/Dispatcher.hpp"
#include "../systems/TimerSystem.hpp"
#include "entt/entity/fwd.hpp"
#include "entt/entity/entity.hpp"
#include "common/Tags.hpp"
#include "common/components/Layout.hpp"
#include "common/Policies.hpp"
#include "common/components/Window.hpp"
#include "common/Events.hpp"
#include "common/Types.hpp"
#include "common/components/Interaction.hpp"
#include "common/components/Data.hpp"
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

Vec2 GetAbsolutePosition(::entt::entity entity)
{
    std::vector<entt::entity> path;
    entt::entity current = entity;
    while (current != entt::null && Registry::Valid(current))
    {
        path.push_back(current);
        const auto* hierarchy = Registry::TryGet<components::Hierarchy>(current);
        current = hierarchy == nullptr ? entt::null : hierarchy->parent;
    }

    Vec2 position(0.0F, 0.0F);
    for (auto currentEntity : std::views::reverse(path))
    {
        if (Registry::AnyOf<components::WindowTag, components::DialogTag>(currentEntity))
        {
            continue;
        }

        const auto* positionComp = Registry::TryGet<components::Position>(currentEntity);
        if (positionComp != nullptr)
        {
            position += positionComp->value;
        }

        // 若当前祖先节点（非目标实体本身）是滚动容器，
        // 其子内容被 RenderSystem 整体偏移 -scrollOffset，命中测试需同步扣除。
        if (currentEntity != entity)
        {
            const auto* scrollArea = Registry::TryGet<components::ScrollArea>(currentEntity);
            if (scrollArea != nullptr)
            {
                position.x() -= scrollArea->scrollOffset.x();
                position.y() -= scrollArea->scrollOffset.y();
            }
        }
    }

    return position;
}

Rect GetEntityRect(::entt::entity entity)
{
    if (!Registry::Valid(entity))
    {
        return {};
    }

    const auto* sizeComp = Registry::TryGet<components::Size>(entity);
    if (sizeComp == nullptr)
    {
        return {GetAbsolutePosition(entity), Vec2(0.0F, 0.0F)};
    }

    return {GetAbsolutePosition(entity), sizeComp->size};
}

Rect GetScrollViewportRect(::entt::entity entity)
{
    const Rect entityRect = GetEntityRect(entity);
    const auto* padding = Registry::TryGet<components::Padding>(entity);
    if (padding == nullptr)
    {
        return entityRect;
    }

    const float left = padding->values.w();
    const float top = padding->values.x();
    const float right = padding->values.y();
    const float bottom = padding->values.z();

    return {entityRect.x() + left,
            entityRect.y() + top,
            std::max(0.0F, entityRect.width() - left - right),
            std::max(0.0F, entityRect.height() - top - bottom)};
}

float GetScrollViewportLength(::entt::entity entity, bool isVertical)
{
    const Rect viewportRect = GetScrollViewportRect(entity);
    return isVertical ? viewportRect.height() : viewportRect.width();
}

float GetScrollContentLength(::entt::entity entity, bool isVertical)
{
    const auto* scrollArea = Registry::TryGet<components::ScrollArea>(entity);
    if (scrollArea == nullptr)
    {
        return 0.0F;
    }

    return isVertical ? scrollArea->contentSize.y() : scrollArea->contentSize.x();
}

float GetScrollMaxOffset(::entt::entity entity, bool isVertical)
{
    const float contentLength = GetScrollContentLength(entity, isVertical);
    const float viewportLength = GetScrollViewportLength(entity, isVertical);
    return std::max(0.0F, contentLength - viewportLength);
}

components::VerticalScrollbarGeometry GetVerticalScrollbarGeometry(::entt::entity entity)
{
    components::VerticalScrollbarGeometry geometry;
    const auto* scrollArea = Registry::TryGet<components::ScrollArea>(entity);
    if (scrollArea == nullptr)
    {
        return geometry;
    }

    const bool hasVerticalScroll =
        scrollArea->scroll == policies::Scroll::VERTICAL || scrollArea->scroll == policies::Scroll::BOTH;
    if (!hasVerticalScroll)
    {
        return geometry;
    }

    geometry.containerRect = GetEntityRect(entity);
    geometry.viewportRect = GetScrollViewportRect(entity);
    geometry.viewportHeight = geometry.viewportRect.height();
    geometry.contentHeight = scrollArea->contentSize.y();
    geometry.trackHeight = geometry.containerRect.height();
    geometry.maxScroll = std::max(0.0F, geometry.contentHeight - geometry.viewportHeight);

    if (geometry.contentHeight <= geometry.viewportHeight || geometry.trackHeight <= 0.0F)
    {
        return geometry;
    }

    geometry.trackRect = {geometry.containerRect.x() + geometry.containerRect.width() -
                              components::ScrollArea::SCROLLBAR_TRACK_WIDTH -
                              components::ScrollArea::SCROLLBAR_TRACK_PADDING,
                          geometry.containerRect.y(),
                          components::ScrollArea::SCROLLBAR_TRACK_WIDTH,
                          geometry.trackHeight};

    const float visibleRatio = geometry.viewportHeight / geometry.contentHeight;
    geometry.thumbHeight =
        std::max(components::ScrollArea::SCROLLBAR_THUMB_MIN_SIZE, geometry.trackHeight * visibleRatio);

    const float scrollRatio = geometry.maxScroll > 0.0F
                                  ? std::clamp(scrollArea->scrollOffset.y() / geometry.maxScroll, 0.0F, 1.0F)
                                  : 0.0F;
    const float thumbTravel = std::max(0.0F, geometry.trackHeight - geometry.thumbHeight);
    const float thumbTop = geometry.trackRect.y() + (thumbTravel * scrollRatio) +
                           components::ScrollArea::SCROLLBAR_THUMB_INSET;

    geometry.thumbRect = {geometry.containerRect.x() + geometry.containerRect.width() -
                              components::ScrollArea::SCROLLBAR_THUMB_WIDTH -
                              components::ScrollArea::SCROLLBAR_TRACK_PADDING - 1.0F,
                          thumbTop,
                          components::ScrollArea::SCROLLBAR_THUMB_WIDTH,
                          std::max(0.0F,
                                   geometry.thumbHeight -
                                       (components::ScrollArea::SCROLLBAR_THUMB_INSET * 2.0F))};
    geometry.visible = true;
    return geometry;
}

void InvokeTask(VoidCallback func)
{
    systems::TimerSystem::addTask(0, std::move(func), true);
}
/**
 * @brief 注册一个定时任务，返回任务句柄
 * @param interval 间隔时间（毫秒）
 * @param func 任务函数
 * @return 任务句柄
 */
TaskHandle TimerCallback(uint32_t interval, VoidCallback func)
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
/**
 * @brief 判断实体别名是否存在
 * @param alias 实体别名
 * @return true 实体存在
 * @return false 实体不存在
 */
bool IsEntityExist(const std::string& alias)
{
    auto view = Registry::View<components::BaseInfo>();

     return std::ranges::any_of(view,
                        [&view, &alias](entt::entity entity) -> bool
                        { return view.get<components::BaseInfo>(entity).alias == alias; });

}

} // namespace ui::utils