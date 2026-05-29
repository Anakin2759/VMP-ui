#include "Controls.hpp"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>
#include <string>
#include "Utils.hpp"
#include "singleton/Registry.hpp"
#include "entt/entity/fwd.hpp"
#include "common/components/Data.hpp"
#include "common/Policies.hpp"
#include "common/components/Layout.hpp"
#include "common/components/Interaction.hpp"
#include "common/Types.hpp"

namespace ui::controls
{
void SetSliderRange(::entt::entity entity, float minValue, float maxValue)
{
    if (!Registry::Valid(entity)) return;

    auto& slider = Registry::GetOrEmplace<components::SliderInfo>(entity);
    slider.minValue = std::min(minValue, maxValue);
    slider.maxValue = std::max(minValue, maxValue);
    slider.currentValue = std::clamp(slider.currentValue, slider.minValue, slider.maxValue);

    ui::utils::MarkLayoutAndVisualChanged(entity);
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

    ui::utils::MarkVisualChanged(entity);
}

void SetSliderStep(::entt::entity entity, float step)
{
    if (!Registry::Valid(entity)) return;

    auto& slider = Registry::GetOrEmplace<components::SliderInfo>(entity);
    slider.step = std::max(0.0F, step);
    ui::utils::MarkVisualChanged(entity);
}

void SetSliderOrientation(::entt::entity entity, policies::Orientation orientation)
{
    if (!Registry::Valid(entity)) return;

    auto& slider = Registry::GetOrEmplace<components::SliderInfo>(entity);
    slider.vertical = orientation;

    auto& size = Registry::GetOrEmplace<components::Size>(entity);
    if (orientation == policies::Orientation::VERTICAL)
    {
        size.size = {28.0F, 200.0F};
    }
    else
    {
        size.size = {200.0F, 28.0F};
    }
    size.sizePolicy = ui::policies::Size::FIXED;

    ui::utils::MarkLayoutAndVisualChanged(entity);
}

void SetSliderOnValueChanged(::entt::entity entity, components::on_event<float> callback)
{
    if (!Registry::Valid(entity)) return;

    auto& slider = Registry::GetOrEmplace<components::SliderInfo>(entity);
    slider.onValueChanged = std::move(callback);
}

void SetSliderTrackColor(::entt::entity entity, const Color& color)
{
    if (!Registry::Valid(entity)) return;
    Registry::GetOrEmplace<components::SliderInfo>(entity).trackColor = color;
    ui::utils::MarkVisualChanged(entity);
}

void SetSliderFillColor(::entt::entity entity, const Color& color)
{
    if (!Registry::Valid(entity)) return;
    Registry::GetOrEmplace<components::SliderInfo>(entity).fillColor = color;
    ui::utils::MarkVisualChanged(entity);
}

void SetSliderThumbColor(::entt::entity entity, const Color& color)
{
    if (!Registry::Valid(entity)) return;
    Registry::GetOrEmplace<components::SliderInfo>(entity).thumbColor = color;
    ui::utils::MarkVisualChanged(entity);
}

void SetSliderThumbSize(::entt::entity entity, float size)
{
    if (!Registry::Valid(entity)) return;
    auto& slider = Registry::GetOrEmplace<components::SliderInfo>(entity);
    slider.thumbSize = std::max(4.0F, size);
    slider.thumbRadius = slider.thumbSize * 0.5F; // 保持圆形比例
    ui::utils::MarkVisualChanged(entity);
}

void SetSliderTrackThickness(::entt::entity entity, float thickness)
{
    if (!Registry::Valid(entity)) return;
    Registry::GetOrEmplace<components::SliderInfo>(entity).trackThickness = std::max(2.0F, thickness);
    ui::utils::MarkVisualChanged(entity);
}

void SetProgressValue(::entt::entity entity, float progress)
{
    if (!Registry::Valid(entity)) return;

    auto& bar = Registry::GetOrEmplace<components::ProgressBar>(entity);
    bar.progress = std::clamp(progress, 0.0F, 1.0F);
    ui::utils::MarkVisualChanged(entity);
}

void SetProgressFillColor(::entt::entity entity, const Color& color)
{
    if (!Registry::Valid(entity)) return;

    auto& bar = Registry::GetOrEmplace<components::ProgressBar>(entity);
    bar.fillColor = color;
    ui::utils::MarkVisualChanged(entity);
}

void SetProgressBackgroundColor(::entt::entity entity, const Color& color)
{
    if (!Registry::Valid(entity)) return;

    auto& bar = Registry::GetOrEmplace<components::ProgressBar>(entity);
    bar.backgroundColor = color;
    ui::utils::MarkVisualChanged(entity);
}

void SetProgressLabelVisibility(::entt::entity entity, policies::LabelVisibility visibility)
{
    if (!Registry::Valid(entity)) return;

    auto& bar = Registry::GetOrEmplace<components::ProgressBar>(entity);
    bar.showLabel = visibility;
    ui::utils::MarkVisualChanged(entity);
}

void SetProgressAnimated(::entt::entity entity, policies::AnimationState animated)
{
    if (!Registry::Valid(entity)) return;

    auto& bar = Registry::GetOrEmplace<components::ProgressBar>(entity);
    bar.animated = animated;
    ui::utils::MarkVisualChanged(entity);
}

void SetScrollMode(::entt::entity entity, policies::Scroll mode)
{
    if (!Registry::Valid(entity)) return;

    auto& scrollArea = Registry::GetOrEmplace<components::ScrollArea>(entity);
    scrollArea.scroll = mode;
    ui::utils::MarkLayoutAndVisualChanged(entity);
}

void SetScrollBarPolicy(::entt::entity entity, policies::ScrollBar policy)
{
    if (!Registry::Valid(entity)) return;

    auto& scrollArea = Registry::GetOrEmplace<components::ScrollArea>(entity);
    scrollArea.scrollBar = policy;
    ui::utils::MarkLayoutAndVisualChanged(entity);
}

void SetScrollAnchor(::entt::entity entity, policies::ScrollAnchor anchor)
{
    if (!Registry::Valid(entity)) return;

    auto& scrollArea = Registry::GetOrEmplace<components::ScrollArea>(entity);
    scrollArea.anchor = anchor;
    ui::utils::MarkLayoutAndVisualChanged(entity);
}

void SetScrollSpeed(::entt::entity entity, float speed)
{
    if (!Registry::Valid(entity)) return;

    auto& scrollArea = Registry::GetOrEmplace<components::ScrollArea>(entity);
    scrollArea.scrollSpeed = std::max(1.0F, speed);
    ui::utils::MarkVisualChanged(entity);
}

void SetCheckBoxChecked(::entt::entity entity, bool checked)
{
    if (!Registry::Valid(entity)) return;
    auto* checkBox = Registry::TryGet<components::CheckBox>(entity);
    if (checkBox == nullptr) return;
    checkBox->checked = checked;
    ui::utils::MarkVisualChanged(entity);
}

void SetCheckBoxOnChanged(::entt::entity entity, components::on_event<bool> callback)
{
    if (!Registry::Valid(entity)) return;
    auto* checkBox = Registry::TryGet<components::CheckBox>(entity);
    if (checkBox == nullptr) return;
    checkBox->onChanged = std::move(callback);
}

void SetDropDownOptions(::entt::entity entity, std::vector<std::string> options)
{
    if (!Registry::Valid(entity)) return;
    auto* dropDown = Registry::TryGet<components::DropDown>(entity);
    if (dropDown == nullptr) return;
    dropDown->options = std::move(options);
    dropDown->selectedIndex = 0;
    if (auto* text = Registry::TryGet<components::Text>(entity))
    {
        text->content = dropDown->selectedText();
    }
    ui::utils::MarkVisualChanged(entity);
}

void SetDropDownSelected(::entt::entity entity, int index)
{
    if (!Registry::Valid(entity)) return;
    auto* dropDown = Registry::TryGet<components::DropDown>(entity);
    if (dropDown == nullptr) return;
    dropDown->selectedIndex =
        dropDown->options.empty() ? 0 : std::clamp(index, 0, static_cast<int>(dropDown->options.size()) - 1);
    if (auto* text = Registry::TryGet<components::Text>(entity))
    {
        text->content = dropDown->selectedText();
    }
    ui::utils::MarkVisualChanged(entity);
}

void SetDropDownOnChanged(::entt::entity entity, components::on_event<int> callback)
{
    if (!Registry::Valid(entity)) return;
    auto* dropDown = Registry::TryGet<components::DropDown>(entity);
    if (dropDown == nullptr) return;
    dropDown->onChanged = std::move(callback);
}

// ─── Drag / Drop ─────────────────────────────────────────────────────────────

void SetDraggable(::entt::entity entity, bool enabled)
{
    if (!Registry::Valid(entity)) return;
    auto& draggable = Registry::GetOrEmplace<components::Draggable>(entity);
    draggable.enabled = enabled ? policies::Feature::ENABLED : policies::Feature::DISABLED;
}

void SetDragLockAxis(::entt::entity entity, bool lockX, bool lockY)
{
    if (!Registry::Valid(entity)) return;
    auto& draggable = Registry::GetOrEmplace<components::Draggable>(entity);
    draggable.lockX = lockX;
    draggable.lockY = lockY;
}

void SetOnDragStart(::entt::entity entity, components::on_event<> callback)
{
    if (!Registry::Valid(entity)) return;
    auto& draggable = Registry::GetOrEmplace<components::Draggable>(entity);
    draggable.onDragStart = std::move(callback);
}

void SetOnDragEnd(::entt::entity entity, components::on_event<> callback)
{
    if (!Registry::Valid(entity)) return;
    auto& draggable = Registry::GetOrEmplace<components::Draggable>(entity);
    draggable.onDragEnd = std::move(callback);
}

void SetOnDragMove(::entt::entity entity, components::on_event<Vec2> callback)
{
    if (!Registry::Valid(entity)) return;
    auto& draggable = Registry::GetOrEmplace<components::Draggable>(entity);
    draggable.onDragMove = std::move(callback);
}

void SetDroppable(::entt::entity entity, bool enabled)
{
    if (!Registry::Valid(entity)) return;
    auto& droppable = Registry::GetOrEmplace<components::Droppable>(entity);
    droppable.enabled = enabled ? policies::Feature::ENABLED : policies::Feature::DISABLED;
}

} // namespace ui::controls
