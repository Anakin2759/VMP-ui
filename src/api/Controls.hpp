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

#include "Entity.hpp"
#include "common/Policies.hpp"
#include "common/Types.hpp"
#include "common/components/Interaction.hpp"
#include "Chains.hpp"

namespace ui::controls
{
void SetSliderRange(ui::entity entity, float minValue, float maxValue);
void SetSliderValue(ui::entity entity, float value);
void SetSliderStep(ui::entity entity, float step);
void SetSliderOrientation(ui::entity entity, policies::Orientation orientation);
void SetSliderOnValueChanged(ui::entity entity, components::on_event<float> callback);
void SetSliderTrackColor(ui::entity entity, const Color& color);
void SetSliderFillColor(ui::entity entity, const Color& color);
void SetSliderThumbColor(ui::entity entity, const Color& color);
void SetSliderThumbSize(ui::entity entity, float size);
void SetSliderTrackThickness(ui::entity entity, float thickness);

void SetProgressValue(ui::entity entity, float progress);
void SetProgressFillColor(ui::entity entity, const Color& color);
void SetProgressBackgroundColor(ui::entity entity, const Color& color);
void SetProgressLabelVisibility(ui::entity entity, policies::LabelVisibility visibility);
void SetProgressAnimated(ui::entity entity, policies::AnimationState animated);

void SetScrollMode(ui::entity entity, policies::Scroll mode);
void SetScrollBarPolicy(ui::entity entity, policies::ScrollBar policy);
void SetScrollAnchor(ui::entity entity, policies::ScrollAnchor anchor);
void SetScrollSpeed(ui::entity entity, float speed);

// CheckBox
void SetCheckBoxChecked(ui::entity entity, bool checked);
void SetCheckBoxOnChanged(ui::entity entity, components::on_event<bool> callback);

// DropDown
void SetDropDownOptions(ui::entity entity, std::vector<std::string> options);
void SetDropDownSelected(ui::entity entity, int index);
void SetDropDownOnChanged(ui::entity entity, components::on_event<int> callback);

// Drag / Drop
void SetDraggable(ui::entity entity, bool enabled);
void SetDragLockAxis(ui::entity entity, bool lockX, bool lockY);
void SetOnDragStart(ui::entity entity, components::on_event<> callback);
void SetOnDragEnd(ui::entity entity, components::on_event<> callback);
void SetOnDragMove(ui::entity entity, components::on_event<Vec2> callback);
void SetDroppable(ui::entity entity, bool enabled);
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
inline constexpr EntityAction<&ui::controls::SetSliderTrackColor> SET_SLIDER_TRACK_COLOR_ACTION{};
inline constexpr EntityAction<&ui::controls::SetSliderFillColor> SET_SLIDER_FILL_COLOR_ACTION{};
inline constexpr EntityAction<&ui::controls::SetSliderThumbColor> SET_SLIDER_THUMB_COLOR_ACTION{};
inline constexpr EntityAction<&ui::controls::SetSliderThumbSize> SET_SLIDER_THUMB_SIZE_ACTION{};
inline constexpr EntityAction<&ui::controls::SetSliderTrackThickness> SET_SLIDER_TRACK_THICKNESS_ACTION{};
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
// Drag / Drop actions
inline constexpr EntityAction<&ui::controls::SetDraggable> SET_DRAGGABLE_ACTION{};
inline constexpr EntityAction<&ui::controls::SetDragLockAxis> SET_DRAG_LOCK_AXIS_ACTION{};
inline constexpr EntityAction<&ui::controls::SetOnDragStart> SET_ON_DRAG_START_ACTION{};
inline constexpr EntityAction<&ui::controls::SetOnDragEnd> SET_ON_DRAG_END_ACTION{};
inline constexpr EntityAction<&ui::controls::SetOnDragMove> SET_ON_DRAG_MOVE_ACTION{};
inline constexpr EntityAction<&ui::controls::SetDroppable> SET_DROPPABLE_ACTION{};
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

inline auto SliderTrackColor(const Color& color)
{
    return ui::actions::controls::SET_SLIDER_TRACK_COLOR_ACTION.bind(color);
}

inline auto SliderFillColor(const Color& color)
{
    return ui::actions::controls::SET_SLIDER_FILL_COLOR_ACTION.bind(color);
}

inline auto SliderThumbColor(const Color& color)
{
    return ui::actions::controls::SET_SLIDER_THUMB_COLOR_ACTION.bind(color);
}

inline auto SliderThumbSize(float size)
{
    return ui::actions::controls::SET_SLIDER_THUMB_SIZE_ACTION.bind(size);
}

inline auto SliderTrackThickness(float thickness)
{
    return ui::actions::controls::SET_SLIDER_TRACK_THICKNESS_ACTION.bind(thickness);
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
inline auto DropDownOptions(std::vector<std::string> options)
{
    return ui::actions::controls::SET_DROPDOWN_OPTIONS_ACTION.bind(std::move(options));
}

inline auto DropDownSelected(int index)
{
    return ui::actions::controls::SET_DROPDOWN_SELECTED_ACTION.bind(index);
}

inline auto OnDropDownChanged(ui::components::on_event<int> callback)
{
    return ui::actions::controls::SET_DROPDOWN_ON_CHANGED_ACTION.bind(std::move(callback));
}

// Drag / Drop DSL
inline auto Draggable(bool enabled = true)
{
    return ui::actions::controls::SET_DRAGGABLE_ACTION.bind(enabled);
}

inline auto DragLockAxis(bool lockX, bool lockY)
{
    return ui::actions::controls::SET_DRAG_LOCK_AXIS_ACTION.bind(lockX, lockY);
}

inline auto OnDragStart(ui::components::on_event<> callback)
{
    return ui::actions::controls::SET_ON_DRAG_START_ACTION.bind(std::move(callback));
}

inline auto OnDragEnd(ui::components::on_event<> callback)
{
    return ui::actions::controls::SET_ON_DRAG_END_ACTION.bind(std::move(callback));
}

inline auto OnDragMove(ui::components::on_event<Vec2> callback)
{
    return ui::actions::controls::SET_ON_DRAG_MOVE_ACTION.bind(std::move(callback));
}

inline auto Droppable(bool enabled = true)
{
    return ui::actions::controls::SET_DROPPABLE_ACTION.bind(enabled);
}
} // namespace ui::chains
