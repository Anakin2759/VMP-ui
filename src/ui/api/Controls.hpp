/**
 * ************************************************************************
 *
 * @file Controls.hpp
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2026-03-23
 * @version 0.1
 * @brief UI 控件相关的 API 定义 - 包含滑动条、进度条、滚动区域等常用控件的属性设置函数
 *
 * ************************************************************************
 * @copyright Copyright (c) 2026 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */
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

// CheckBox
void SetCheckBoxChecked(::entt::entity entity, bool checked);
void SetCheckBoxOnChanged(::entt::entity entity, components::on_event<bool> callback);

// DropDown
void SetDropDownOptions(::entt::entity entity, const std::vector<std::string>& options);
void SetDropDownSelected(::entt::entity entity, int index);
void SetDropDownOnChanged(::entt::entity entity, components::on_event<int> callback);
} // namespace ui::controls

namespace ui::actions
{
namespace controls
{
inline constexpr EntityAction<&ui::controls::SetSliderRange> SET_SLIDER_RANGE_ACTION{};
inline constexpr EntityAction<&ui::controls::SetSliderValue> SET_SLIDER_VALUE_ACTION{};
inline constexpr EntityAction<&ui::controls::SetSliderStep> SET_SLIDER_STEP_ACTION{};
inline constexpr EntityAction<&ui::controls::SetSliderOrientation> SET_SLIDER_ORIENTATION_ACTION{};
inline constexpr EntityAction<&ui::controls::SetSliderOnValueChanged> SET_SLIDER_ON_VALUE_CHANGED_ACTION{};
inline constexpr EntityAction<&ui::controls::SetProgressValue> SET_PROGRESS_VALUE_ACTION{};
inline constexpr EntityAction<&ui::controls::SetProgressFillColor> SET_PROGRESS_FILL_COLOR_ACTION{};
inline constexpr EntityAction<&ui::controls::SetProgressBackgroundColor> SET_PROGRESS_BACKGROUND_COLOR_ACTION{};
inline constexpr EntityAction<&ui::controls::SetProgressLabelVisibility> SET_PROGRESS_LABEL_VISIBILITY_ACTION{};
inline constexpr EntityAction<&ui::controls::SetProgressAnimated> SET_PROGRESS_ANIMATED_ACTION{};
inline constexpr EntityAction<&ui::controls::SetScrollMode> SET_SCROLL_MODE_ACTION{};
inline constexpr EntityAction<&ui::controls::SetScrollBarPolicy> SET_SCROLL_BAR_POLICY_ACTION{};
inline constexpr EntityAction<&ui::controls::SetScrollAnchor> SET_SCROLL_ANCHOR_ACTION{};
inline constexpr EntityAction<&ui::controls::SetScrollSpeed> SET_SCROLL_SPEED_ACTION{};
inline constexpr EntityAction<&ui::controls::SetCheckBoxChecked> SET_CHECKBOX_CHECKED_ACTION{};
inline constexpr EntityAction<&ui::controls::SetCheckBoxOnChanged> SET_CHECKBOX_ON_CHANGED_ACTION{};
inline constexpr EntityAction<&ui::controls::SetDropDownOptions> SET_DROPDOWN_OPTIONS_ACTION{};
inline constexpr EntityAction<&ui::controls::SetDropDownSelected> SET_DROPDOWN_SELECTED_ACTION{};
inline constexpr EntityAction<&ui::controls::SetDropDownOnChanged> SET_DROPDOWN_ON_CHANGED_ACTION{};
} // namespace controls
} // namespace ui::actions

namespace ui::chains
{
inline auto SliderRange(float minValue, float maxValue)
{
    return ui::actions::controls::SET_SLIDER_RANGE_ACTION.bind(minValue, maxValue);
}

inline auto SliderValue(float value)
{
    return ui::actions::controls::SET_SLIDER_VALUE_ACTION.bind(value);
}

inline auto SliderStep(float step)
{
    return ui::actions::controls::SET_SLIDER_STEP_ACTION.bind(step);
}

inline auto SliderOrientation(ui::policies::Orientation orientation)
{
    return ui::actions::controls::SET_SLIDER_ORIENTATION_ACTION.bind(orientation);
}

inline auto OnSliderValueChanged(ui::components::on_event<float> callback)
{
    return ui::actions::controls::SET_SLIDER_ON_VALUE_CHANGED_ACTION.bind(std::move(callback));
}

inline auto ProgressValue(float value)
{
    return ui::actions::controls::SET_PROGRESS_VALUE_ACTION.bind(value);
}

inline auto ProgressFillColor(const Color& color)
{
    return ui::actions::controls::SET_PROGRESS_FILL_COLOR_ACTION.bind(color);
}

inline auto ProgressBackgroundColor(const Color& color)
{
    return ui::actions::controls::SET_PROGRESS_BACKGROUND_COLOR_ACTION.bind(color);
}

inline auto ProgressLabel(ui::policies::LabelVisibility visibility)
{
    return ui::actions::controls::SET_PROGRESS_LABEL_VISIBILITY_ACTION.bind(visibility);
}

inline auto ProgressAnimated(ui::policies::AnimationState animated)
{
    return ui::actions::controls::SET_PROGRESS_ANIMATED_ACTION.bind(animated);
}

inline auto ScrollMode(ui::policies::Scroll mode)
{
    return ui::actions::controls::SET_SCROLL_MODE_ACTION.bind(mode);
}

inline auto ScrollBarPolicy(ui::policies::ScrollBar policy)
{
    return ui::actions::controls::SET_SCROLL_BAR_POLICY_ACTION.bind(policy);
}

inline auto ScrollAnchor(ui::policies::ScrollAnchor anchor)
{
    return ui::actions::controls::SET_SCROLL_ANCHOR_ACTION.bind(anchor);
}

inline auto ScrollSpeed(float speed)
{
    return ui::actions::controls::SET_SCROLL_SPEED_ACTION.bind(speed);
}

// CheckBox DSL
inline auto CheckBoxChecked(bool checked)
{
    return ui::actions::controls::SET_CHECKBOX_CHECKED_ACTION.bind(checked);
}

inline auto OnCheckBoxChanged(ui::components::on_event<bool> callback)
{
    return ui::actions::controls::SET_CHECKBOX_ON_CHANGED_ACTION.bind(std::move(callback));
}

// DropDown DSL
inline auto DropDownOptions(const std::vector<std::string>& options)
{
    return ui::actions::controls::SET_DROPDOWN_OPTIONS_ACTION.bind(options);
}

inline auto DropDownSelected(int index)
{
    return ui::actions::controls::SET_DROPDOWN_SELECTED_ACTION.bind(index);
}

inline auto OnDropDownChanged(ui::components::on_event<int> callback)
{
    return ui::actions::controls::SET_DROPDOWN_ON_CHANGED_ACTION.bind(std::move(callback));
}
} // namespace ui::chains
