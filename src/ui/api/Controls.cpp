#include "Controls.hpp"

#include <algorithm>
#include <cmath>
#include "Utils.hpp"
#include "../singleton/Registry.hpp"

namespace ui::controls
{
void SetSliderRange(::entt::entity entity, float minValue, float maxValue)
{
    if (!Registry::Valid(entity)) return;

    auto& slider = Registry::GetOrEmplace<components::SliderInfo>(entity);
    slider.minValue = std::min(minValue, maxValue);
    slider.maxValue = std::max(minValue, maxValue);
    slider.currentValue = std::clamp(slider.currentValue, slider.minValue, slider.maxValue);

    Registry::EmplaceOrReplace<components::LayoutDirtyTag>(entity);
    ui::utils::MarkRenderDirty(entity);
}

void SetSliderValue(::entt::entity entity, float value)
{
    if (!Registry::Valid(entity)) return;

    auto& slider = Registry::GetOrEmplace<components::SliderInfo>(entity);
    const float clamped = std::clamp(value, slider.minValue, slider.maxValue);
    if (std::abs(slider.currentValue - clamped) < 0.0001F) return;

    slider.currentValue = clamped;
    if (slider.onValueChanged)
    {
        slider.onValueChanged(slider.currentValue);
    }

    ui::utils::MarkRenderDirty(entity);
}

void SetSliderStep(::entt::entity entity, float step)
{
    if (!Registry::Valid(entity)) return;

    auto& slider = Registry::GetOrEmplace<components::SliderInfo>(entity);
    slider.step = std::max(0.0F, step);
    ui::utils::MarkRenderDirty(entity);
}

void SetSliderOrientation(::entt::entity entity, policies::Orientation orientation)
{
    if (!Registry::Valid(entity)) return;

    auto& slider = Registry::GetOrEmplace<components::SliderInfo>(entity);
    slider.vertical = orientation;

    auto& size = Registry::GetOrEmplace<components::Size>(entity);
    if (orientation == policies::Orientation::Vertical)
    {
        size.size = {28.0F, 200.0F};
    }
    else
    {
        size.size = {200.0F, 28.0F};
    }
    size.sizePolicy = ui::policies::Size::Fixed;

    Registry::EmplaceOrReplace<components::LayoutDirtyTag>(entity);
    ui::utils::MarkRenderDirty(entity);
}

void SetSliderOnValueChanged(::entt::entity entity, components::on_event<float> callback)
{
    if (!Registry::Valid(entity)) return;

    auto& slider = Registry::GetOrEmplace<components::SliderInfo>(entity);
    slider.onValueChanged = std::move(callback);
}

void SetProgressValue(::entt::entity entity, float progress)
{
    if (!Registry::Valid(entity)) return;

    auto& bar = Registry::GetOrEmplace<components::ProgressBar>(entity);
    bar.progress = std::clamp(progress, 0.0F, 1.0F);
    ui::utils::MarkRenderDirty(entity);
}

void SetProgressFillColor(::entt::entity entity, const Color& color)
{
    if (!Registry::Valid(entity)) return;

    auto& bar = Registry::GetOrEmplace<components::ProgressBar>(entity);
    bar.fillColor = color;
    ui::utils::MarkRenderDirty(entity);
}

void SetProgressBackgroundColor(::entt::entity entity, const Color& color)
{
    if (!Registry::Valid(entity)) return;

    auto& bar = Registry::GetOrEmplace<components::ProgressBar>(entity);
    bar.backgroundColor = color;
    ui::utils::MarkRenderDirty(entity);
}

void SetProgressLabelVisibility(::entt::entity entity, policies::LabelVisibility visibility)
{
    if (!Registry::Valid(entity)) return;

    auto& bar = Registry::GetOrEmplace<components::ProgressBar>(entity);
    bar.showLabel = visibility;
    ui::utils::MarkRenderDirty(entity);
}

void SetProgressAnimated(::entt::entity entity, policies::AnimationState animated)
{
    if (!Registry::Valid(entity)) return;

    auto& bar = Registry::GetOrEmplace<components::ProgressBar>(entity);
    bar.animated = animated;
    ui::utils::MarkRenderDirty(entity);
}

void SetScrollMode(::entt::entity entity, policies::Scroll mode)
{
    if (!Registry::Valid(entity)) return;

    auto& scrollArea = Registry::GetOrEmplace<components::ScrollArea>(entity);
    scrollArea.scroll = mode;
    Registry::EmplaceOrReplace<components::LayoutDirtyTag>(entity);
    ui::utils::MarkRenderDirty(entity);
}

void SetScrollBarPolicy(::entt::entity entity, policies::ScrollBar policy)
{
    if (!Registry::Valid(entity)) return;

    auto& scrollArea = Registry::GetOrEmplace<components::ScrollArea>(entity);
    scrollArea.scrollBar = policy;
    ui::utils::MarkRenderDirty(entity);
}

void SetScrollAnchor(::entt::entity entity, policies::ScrollAnchor anchor)
{
    if (!Registry::Valid(entity)) return;

    auto& scrollArea = Registry::GetOrEmplace<components::ScrollArea>(entity);
    scrollArea.anchor = anchor;
    ui::utils::MarkRenderDirty(entity);
}

void SetScrollSpeed(::entt::entity entity, float speed)
{
    if (!Registry::Valid(entity)) return;

    auto& scrollArea = Registry::GetOrEmplace<components::ScrollArea>(entity);
    scrollArea.scrollSpeed = std::max(1.0F, speed);
    ui::utils::MarkRenderDirty(entity);
}
} // namespace ui::controls
