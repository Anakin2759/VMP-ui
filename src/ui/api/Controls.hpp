#pragma once

#include <entt/entt.hpp>
#include "../common/Policies.hpp"
#include "../common/Types.hpp"
#include "../common/Components.hpp"
#include "Chains.hpp"

namespace ui::controls
{
void SetSliderRange(::entt::entity entity, float minValue, float maxValue);
void SetSliderValue(::entt::entity entity, float value);
void SetSliderStep(::entt::entity entity, float step);
void SetSliderOrientation(::entt::entity entity, policies::Orientation orientation);
void SetSliderOnValueChanged(::entt::entity entity, components::on_event<float> callback);

void SetProgressValue(::entt::entity entity, float progress);
void SetProgressFillColor(::entt::entity entity, const Color& color);
void SetProgressBackgroundColor(::entt::entity entity, const Color& color);
void SetProgressLabelVisibility(::entt::entity entity, policies::LabelVisibility visibility);
void SetProgressAnimated(::entt::entity entity, policies::AnimationState animated);

void SetScrollMode(::entt::entity entity, policies::Scroll mode);
void SetScrollBarPolicy(::entt::entity entity, policies::ScrollBar policy);
void SetScrollAnchor(::entt::entity entity, policies::ScrollAnchor anchor);
void SetScrollSpeed(::entt::entity entity, float speed);
} // namespace ui::controls

namespace ui::chains
{
inline auto SliderRange(float minValue, float maxValue)
{
    return Call<ui::controls::SetSliderRange>(minValue, maxValue);
}

inline auto SliderValue(float value)
{
    return Call<ui::controls::SetSliderValue>(value);
}

inline auto SliderStep(float step)
{
    return Call<ui::controls::SetSliderStep>(step);
}

inline auto SliderOrientation(ui::policies::Orientation orientation)
{
    return Call<ui::controls::SetSliderOrientation>(orientation);
}

inline auto OnSliderValueChanged(ui::components::on_event<float> callback)
{
    return Call<ui::controls::SetSliderOnValueChanged>(std::move(callback));
}

inline auto ProgressValue(float value)
{
    return Call<ui::controls::SetProgressValue>(value);
}

inline auto ProgressFillColor(const Color& color)
{
    return Call<ui::controls::SetProgressFillColor>(color);
}

inline auto ProgressBackgroundColor(const Color& color)
{
    return Call<ui::controls::SetProgressBackgroundColor>(color);
}

inline auto ProgressLabel(ui::policies::LabelVisibility visibility)
{
    return Call<ui::controls::SetProgressLabelVisibility>(visibility);
}

inline auto ProgressAnimated(ui::policies::AnimationState animated)
{
    return Call<ui::controls::SetProgressAnimated>(animated);
}

inline auto ScrollMode(ui::policies::Scroll mode)
{
    return Call<ui::controls::SetScrollMode>(mode);
}

inline auto ScrollBarPolicy(ui::policies::ScrollBar policy)
{
    return Call<ui::controls::SetScrollBarPolicy>(policy);
}

inline auto ScrollAnchor(ui::policies::ScrollAnchor anchor)
{
    return Call<ui::controls::SetScrollAnchor>(anchor);
}

inline auto ScrollSpeed(float speed)
{
    return Call<ui::controls::SetScrollSpeed>(speed);
}
} // namespace ui::chains
