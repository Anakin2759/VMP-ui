#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "common/CustomEvent.hpp"
#include "common/EntityTypes.hpp"
#include "ui/ErrorCodes.hpp"
#include "ui/Geometry.hpp"
#include "ui/Policies.hpp"
#include "ui/Result.hpp"
#include "ui/TweenOptions.hpp"
#include "ui/api/Scale.hpp"
#include "common/Tags.hpp"
#include "ui/api/Theme.hpp"
#include "common/Types.hpp"
#include "common/components/Data.hpp"
#include "common/components/Animation.hpp"
#include "common/components/Interaction.hpp"
#include "common/components/Layout.hpp"
#include "common/components/Visual.hpp"
#include "common/components/Window.hpp"
#include "core/UiRuntime.hpp"
#include "core/WindowSync.hpp"
#include "SDL3/SDL_video.h"
#include "entt/entity/entity.hpp"

namespace ui
{
class UiRuntime;
}

namespace ui::utils
{
void MarkVisualChanged(UiRuntime& runtime, ui::entity entity);
void MarkLayoutAndVisualChanged(UiRuntime& runtime, ui::entity entity);
void MarkLayoutDirty(UiRuntime& runtime, ui::entity entity);
void MarkRenderDirty(UiRuntime& runtime, ui::entity entity);

inline void MarkVisualChanged(UiRuntime& runtime, entt::entity entity)
{
    MarkVisualChanged(runtime, static_cast<ui::entity>(entity));
}

inline void MarkLayoutAndVisualChanged(UiRuntime& runtime, entt::entity entity)
{
    MarkLayoutAndVisualChanged(runtime, static_cast<ui::entity>(entity));
}

inline void MarkLayoutDirty(UiRuntime& runtime, entt::entity entity)
{
    MarkLayoutDirty(runtime, static_cast<ui::entity>(entity));
}

inline void MarkRenderDirty(UiRuntime& runtime, entt::entity entity)
{
    MarkRenderDirty(runtime, static_cast<ui::entity>(entity));
}
}  // namespace ui::utils

namespace ui::detail
{

static_assert(sizeof(ui::entity) == sizeof(entt::entity), "ui::entity must match entt::entity storage size");
static_assert(ui::null_entity == static_cast<ui::entity>(entt::null), "ui::null_entity must match entt::null");

[[nodiscard]] constexpr entt::entity ToInternal(ui::entity entity) noexcept
{
    return static_cast<entt::entity>(entity);
}

[[nodiscard]] constexpr ui::entity ToPublic(entt::entity entity) noexcept
{
    return static_cast<ui::entity>(entity);
}

[[nodiscard]] inline ui::Result<ui::entity> ToPublic(ui::Result<entt::entity>&& result)
{
    if (!result)
    {
        return std::unexpected(std::move(result).error());
    }
    return ToPublic(*result);
}

[[nodiscard]] inline ui::Result<ui::entity> ToPublic(const ui::Result<entt::entity>& result)
{
    if (!result)
    {
        return std::unexpected(result.error());
    }
    return ToPublic(*result);
}

}  // namespace ui::detail

namespace ui::detail::animation
{
// 实现位于 src/helper/HelperAnimation.cpp（非 inline，摊薄 entt 模板实例化，
// 降低每个包含 Helper.hpp 的 TU 的编译内存峰值）。
void MarkRenderDirtyInternal(Registry& reg, entt::entity entity);
void ConfigureTiming(Registry& reg, entt::entity entity, const ui::animation::TweenOptions& options);
void StartPositionAnimation(Registry& reg, entt::entity entity, const Vec2& from, const Vec2& to,
                            const ui::animation::TweenOptions& options = {});
void StartAlphaAnimation(Registry& reg, entt::entity entity, float from, float to,
                         const ui::animation::TweenOptions& options = {});
void StartScaleAnimation(Registry& reg, entt::entity entity, const Vec2& from, const Vec2& to,
                         const ui::animation::TweenOptions& options = {});
void StartRenderOffsetAnimation(Registry& reg, entt::entity entity, const Vec2& from, const Vec2& to,
                                const ui::animation::TweenOptions& options = {});
void StartColorAnimation(Registry& reg, entt::entity entity, const Color& from, const Color& to,
                         const ui::animation::TweenOptions& options = {});
void StartTransformAnimation(Registry& reg, entt::entity entity, const std::optional<Vec2>& targetScale,
                             const std::optional<Vec2>& targetOffset,
                             const ui::animation::TweenOptions& options = {},
                             const Vec2& defaultScale = {1.0F, 1.0F}, const Vec2& defaultOffset = {0.0F, 0.0F});
void StopAnimation(Registry& reg, entt::entity entity);

// ==================== P2-4：暂停/恢复/完成/取消 + 回调 ====================

void PauseAnimation(Registry& reg, entt::entity entity);
void ResumeAnimation(Registry& reg, entt::entity entity);
void FinishAnimation(Registry& reg, entt::entity entity, bool settleToEnd = false);
void CancelAnimation(Registry& reg, entt::entity entity, bool settleToEnd = false);
void SetAnimationCallbacks(Registry& reg, entt::entity entity, ui::Callback<> onComplete, ui::Callback<> onCancel = {},
                           ui::Callback<> onStart = {});
}  // namespace ui::detail::animation

namespace ui::controls::bridge
{
inline void SetSliderRange(Registry& r, entt::entity e, float min, float max)
{
    if (!r.valid(e))
        return;
    auto& v = r.get_or_emplace<components::SliderInfo>(e);
    v.minValue = std::min(min, max);
    v.maxValue = std::max(min, max);
    v.currentValue = std::clamp(v.currentValue, v.minValue, v.maxValue);
    utils::MarkLayoutAndVisualChanged(r.runtime(), detail::ToPublic(e));
}
inline void SetSliderValue(Registry& r, entt::entity e, float value)
{
    if (!r.valid(e))
        return;
    auto& v = r.get_or_emplace<components::SliderInfo>(e);
    const float x = std::clamp(value, v.minValue, v.maxValue);
    if (std::abs(v.currentValue - x) < 0.0001F)
        return;
    v.currentValue = x;
    if (v.onValueChanged)
        v.onValueChanged(x);
    utils::MarkVisualChanged(r.runtime(), detail::ToPublic(e));
}
inline void SetSliderStep(Registry& r, entt::entity e, float value)
{
    if (!r.valid(e))
        return;
    r.get_or_emplace<components::SliderInfo>(e).step = std::max(0.0F, value);
    utils::MarkVisualChanged(r.runtime(), e);
}
inline void SetSliderOrientation(Registry& r, entt::entity e, policies::Orientation value)
{
    if (!r.valid(e))
        return;
    r.get_or_emplace<components::SliderInfo>(e).vertical = value;
    auto& size = r.get_or_emplace<components::Size>(e);
    size.size = value == policies::Orientation::VERTICAL ? Vec2{scale::Metric(28.0F), scale::Metric(200.0F)}
                                                         : Vec2{scale::Metric(200.0F), scale::Metric(28.0F)};
    size.sizePolicy = policies::Size::FIXED;
    utils::MarkLayoutAndVisualChanged(r.runtime(), e);
}
inline void SetSliderOnValueChanged(Registry& r, entt::entity e, components::on_event<float> cb)
{
    if (r.valid(e))
        r.get_or_emplace<components::SliderInfo>(e).onValueChanged = std::move(cb);
}
inline void SetSliderTrackColor(Registry& r, entt::entity e, const Color& v)
{
    if (!r.valid(e))
        return;
    r.get_or_emplace<components::SliderInfo>(e).trackColor = v;
    utils::MarkVisualChanged(r.runtime(), e);
}
inline void SetSliderFillColor(Registry& r, entt::entity e, const Color& v)
{
    if (!r.valid(e))
        return;
    r.get_or_emplace<components::SliderInfo>(e).fillColor = v;
    utils::MarkVisualChanged(r.runtime(), e);
}
inline void SetSliderThumbColor(Registry& r, entt::entity e, const Color& v)
{
    if (!r.valid(e))
        return;
    r.get_or_emplace<components::SliderInfo>(e).thumbColor = v;
    utils::MarkVisualChanged(r.runtime(), e);
}
inline void SetSliderThumbSize(Registry& r, entt::entity e, float v)
{
    if (!r.valid(e))
        return;
    auto& x = r.get_or_emplace<components::SliderInfo>(e);
    x.thumbSize = std::max(scale::Metric(4.0F), scale::Metric(v));
    x.thumbRadius = x.thumbSize * 0.5F;
    utils::MarkVisualChanged(r.runtime(), e);
}
inline void SetSliderTrackThickness(Registry& r, entt::entity e, float v)
{
    if (!r.valid(e))
        return;
    r.get_or_emplace<components::SliderInfo>(e).trackThickness = std::max(scale::Metric(2.0F), scale::Metric(v));
    utils::MarkVisualChanged(r.runtime(), e);
}
inline void SetProgressValue(Registry& r, entt::entity e, float v)
{
    if (!r.valid(e))
        return;
    r.get_or_emplace<components::ProgressBar>(e).progress = std::clamp(v, 0.0F, 1.0F);
    utils::MarkVisualChanged(r.runtime(), e);
}
inline void SetProgressFillColor(Registry& r, entt::entity e, const Color& v)
{
    if (!r.valid(e))
        return;
    r.get_or_emplace<components::ProgressBar>(e).fillColor = v;
    utils::MarkVisualChanged(r.runtime(), e);
}
inline void SetProgressBackgroundColor(Registry& r, entt::entity e, const Color& v)
{
    if (!r.valid(e))
        return;
    r.get_or_emplace<components::ProgressBar>(e).backgroundColor = v;
    utils::MarkVisualChanged(r.runtime(), e);
}
inline void SetProgressLabelVisibility(Registry& r, entt::entity e, policies::LabelVisibility v)
{
    if (!r.valid(e))
        return;
    r.get_or_emplace<components::ProgressBar>(e).showLabel = v;
    utils::MarkVisualChanged(r.runtime(), e);
}
inline void SetProgressAnimated(Registry& r, entt::entity e, policies::AnimationState v)
{
    if (!r.valid(e))
        return;
    r.get_or_emplace<components::ProgressBar>(e).animated = v;
    utils::MarkVisualChanged(r.runtime(), e);
}
inline void SetScrollMode(Registry& r, entt::entity e, policies::Scroll v)
{
    if (!r.valid(e))
        return;
    r.get_or_emplace<components::ScrollArea>(e).scroll = v;
    utils::MarkLayoutAndVisualChanged(r.runtime(), e);
}
inline void SetScrollBarPolicy(Registry& r, entt::entity e, policies::ScrollBar v)
{
    if (!r.valid(e))
        return;
    r.get_or_emplace<components::ScrollArea>(e).scrollBar = v;
    utils::MarkLayoutAndVisualChanged(r.runtime(), e);
}
inline void SetScrollAnchor(Registry& r, entt::entity e, policies::ScrollAnchor v)
{
    if (!r.valid(e))
        return;
    r.get_or_emplace<components::ScrollArea>(e).anchor = v;
    utils::MarkLayoutAndVisualChanged(r.runtime(), e);
}
inline void SetScrollSpeed(Registry& r, entt::entity e, float v)
{
    if (!r.valid(e))
        return;
    r.get_or_emplace<components::ScrollArea>(e).scrollSpeed = std::max(1.0F, v);
    utils::MarkVisualChanged(r.runtime(), e);
}
inline void SetCheckBoxChecked(Registry& r, entt::entity e, bool v)
{
    if (!r.valid(e))
        return;
    if (auto* x = r.try_get<components::CheckBox>(e))
    {
        x->checked = v;
        utils::MarkVisualChanged(r.runtime(), e);
    }
}
inline void SetCheckBoxOnChanged(Registry& r, entt::entity e, components::on_event<bool> cb)
{
    if (r.valid(e))
        if (auto* x = r.try_get<components::CheckBox>(e))
            x->onChanged = std::move(cb);
}
inline void SetDropDownOptions(Registry& r, entt::entity e, std::vector<std::string> v)
{
    if (!r.valid(e))
        return;
    if (auto* x = r.try_get<components::DropDown>(e))
    {
        x->options = std::move(v);
        x->selectedIndex = 0;
        if (auto* text = r.try_get<components::Text>(e))
            text->content = x->selectedText();
        utils::MarkVisualChanged(r.runtime(), e);
    }
}
inline void SetDropDownSelected(Registry& r, entt::entity e, int v)
{
    if (!r.valid(e))
        return;
    if (auto* x = r.try_get<components::DropDown>(e))
    {
        x->selectedIndex = x->options.empty() ? 0 : std::clamp(v, 0, static_cast<int>(x->options.size()) - 1);
        if (auto* text = r.try_get<components::Text>(e))
            text->content = x->selectedText();
        utils::MarkVisualChanged(r.runtime(), e);
    }
}
inline void SetDropDownOnChanged(Registry& r, entt::entity e, components::on_event<int> cb)
{
    if (r.valid(e))
        if (auto* x = r.try_get<components::DropDown>(e))
            x->onChanged = std::move(cb);
}
inline void SetDraggable(Registry& r, entt::entity e, bool v)
{
    if (r.valid(e))
        r.get_or_emplace<components::Draggable>(e).enabled =
            v ? policies::Feature::ENABLED : policies::Feature::DISABLED;
}
inline void SetDragLockAxis(Registry& r, entt::entity e, bool x, bool y)
{
    if (!r.valid(e))
        return;
    auto& v = r.get_or_emplace<components::Draggable>(e);
    v.lockX = x;
    v.lockY = y;
}
inline void SetOnDragStart(Registry& r, entt::entity e, components::on_event<> cb)
{
    if (r.valid(e))
        r.get_or_emplace<components::Draggable>(e).onDragStart = std::move(cb);
}
inline void SetOnDragEnd(Registry& r, entt::entity e, components::on_event<> cb)
{
    if (r.valid(e))
        r.get_or_emplace<components::Draggable>(e).onDragEnd = std::move(cb);
}
inline void SetOnDragMove(Registry& r, entt::entity e, components::on_event<Vec2> cb)
{
    if (r.valid(e))
        r.get_or_emplace<components::Draggable>(e).onDragMove = std::move(cb);
}
inline void SetDroppable(Registry& r, entt::entity e, bool v)
{
    if (r.valid(e))
        r.get_or_emplace<components::Droppable>(e).enabled =
            v ? policies::Feature::ENABLED : policies::Feature::DISABLED;
}
}  // namespace ui::controls::bridge

namespace ui::detail::text
{
inline void SetText(Registry& reg, entt::entity entity, const std::string& content)
{
    if (!reg.valid(entity) || !reg.any_of<components::Text>(entity))
        return;
    reg.get<components::Text>(entity).content = content;
    utils::MarkLayoutDirty(reg.runtime(), entity);
}

inline void SetButtonEnabled(Registry& reg, entt::entity entity, bool enabled)
{
    if (!reg.valid(entity))
        return;
    if (enabled)
        reg.remove<components::DisabledTag>(entity);
    else
        reg.emplace_or_replace<components::DisabledTag>(entity);
}

inline void SetTextContent(Registry& reg, entt::entity entity, const std::string& content)
{
    if (!reg.valid(entity))
        return;
    reg.get_or_emplace<components::Text>(entity).content = content;
    utils::MarkLayoutDirty(reg.runtime(), entity);
}

inline void SetTextWordWrap(Registry& reg, entt::entity entity, policies::TextWrap mode)
{
    if (!reg.valid(entity))
        return;
    reg.get_or_emplace<components::Text>(entity).wordWrap = mode;
    utils::MarkLayoutDirty(reg.runtime(), entity);
}

inline void SetTextAlignment(Registry& reg, entt::entity entity, policies::Alignment alignment)
{
    if (!reg.valid(entity))
        return;
    reg.get_or_emplace<components::Text>(entity).alignment = alignment;
    utils::MarkLayoutDirty(reg.runtime(), entity);
}

inline void SetTextColor(Registry& reg, entt::entity entity, const Color& color)
{
    if (!reg.valid(entity))
        return;
    if (auto* text = reg.try_get<components::Text>(entity))
        text->color = color;
    if (auto* edit = reg.try_get<components::TextEdit>(entity))
        edit->textColor = color;
}

[[nodiscard]] inline std::string GetTextEditContent(Registry& reg, entt::entity entity)
{
    if (reg.valid(entity))
    {
        if (const auto* edit = reg.try_get<components::TextEdit>(entity))
            return edit->buffer;
    }
    return {};
}

inline void SetTextEditContent(Registry& reg, entt::entity entity, const std::string& content)
{
    if (!reg.valid(entity))
        return;
    if (auto* edit = reg.try_get<components::TextEdit>(entity))
    {
        edit->buffer = content;
        edit->cursorPosition = std::min(edit->cursorPosition, content.size());
        edit->hasSelection = false;
        edit->selectionStart = 0;
        edit->selectionEnd = 0;
    }
}

inline void SetPasswordMode(Registry& reg, entt::entity entity, policies::TextFlag mode)
{
    if (!reg.valid(entity))
        return;
    if (auto* edit = reg.try_get<components::TextEdit>(entity))
        edit->inputMode |= mode;
}

inline void SetClickCallback(Registry& reg, entt::entity entity, components::on_event<> callback)
{
    if (!reg.valid(entity))
        return;
    auto& clickable = reg.get_or_emplace<components::Clickable>(entity);
    clickable.onClick = std::move(callback);
    clickable.enabled = policies::Feature::ENABLED;
}

inline void SetOnSubmit(Registry& reg, entt::entity entity, components::on_event<> callback)
{
    if (!reg.valid(entity))
        return;
    if (auto* edit = reg.try_get<components::TextEdit>(entity))
        edit->onSubmit = std::move(callback);
}

inline void SetOnTextChanged(Registry& reg, entt::entity entity, components::on_event<const std::string&> callback)
{
    if (!reg.valid(entity))
        return;
    if (auto* edit = reg.try_get<components::TextEdit>(entity))
        edit->onTextChanged = std::move(callback);
}

inline void SetLineHeight(Registry& reg, entt::entity entity, float height)
{
    if (!reg.valid(entity))
        return;
    reg.get_or_emplace<components::Text>(entity).lineHeight = scale::Metric(height);
    utils::MarkLayoutDirty(reg.runtime(), entity);
}

inline void SetCharacterSpacing(Registry& reg, entt::entity entity, float spacing)
{
    if (!reg.valid(entity))
        return;
    reg.get_or_emplace<components::Text>(entity).letterSpacing = scale::Metric(spacing);
    utils::MarkLayoutDirty(reg.runtime(), entity);
}

inline void SetTextWrapWidth(Registry& reg, entt::entity entity, float width)
{
    if (!reg.valid(entity))
        return;
    reg.get_or_emplace<components::Text>(entity).wrapWidth = scale::Metric(width);
    utils::MarkLayoutDirty(reg.runtime(), entity);
}

inline void SetFontSize(Registry& reg, entt::entity entity, float size)
{
    if (!reg.valid(entity))
        return;
    reg.get_or_emplace<components::Text>(entity).fontSize = scale::Metric(size);
    utils::MarkLayoutDirty(reg.runtime(), entity);
}
}  // namespace ui::detail::text

namespace ui::detail::table
{
inline void SetColumns(Registry& reg, entt::entity e, int count, std::vector<std::string> headers = {})
{
    auto& v = reg.get_or_emplace<components::TableInfo>(e);
    v.columnCount = count;
    v.headers = std::move(headers);
    for (auto& row : v.cells)
        row.resize(static_cast<size_t>(count));
}
inline void SetColumnWidths(Registry& reg, entt::entity e, std::vector<float> values)
{
    auto& v = reg.get_or_emplace<components::TableInfo>(e);
    if (v.columnSizing == policies::TableColumnSizing::FIXED || v.columnSizing == policies::TableColumnSizing::ADAPTIVE)
        for (auto& x : values)
            x = scale::Metric(x);
    v.columnWidths = std::move(values);
}
inline void AddRow(Registry& reg, entt::entity e, std::vector<std::string> values)
{
    auto& v = reg.get_or_emplace<components::TableInfo>(e);
    std::vector<components::TableCell> row(static_cast<size_t>(v.columnCount));
    for (int i = 0; i < v.columnCount && std::cmp_less(i, values.size()); ++i)
        row[static_cast<size_t>(i)].text = std::move(values[static_cast<size_t>(i)]);
    v.cells.push_back(std::move(row));
}
inline void SetCell(Registry& reg, entt::entity e, int row, int col, std::string value)
{
    auto* v = reg.try_get<components::TableInfo>(e);
    if (v && row >= 0 && std::cmp_less(row, v->cells.size()) && col >= 0 && col < v->columnCount)
        v->cells[static_cast<size_t>(row)][static_cast<size_t>(col)].text = std::move(value);
}
inline void SetCellColor(Registry& reg, entt::entity e, int row, int col, Color text, Color bg)
{
    auto* v = reg.try_get<components::TableInfo>(e);
    if (v && row >= 0 && std::cmp_less(row, v->cells.size()) && col >= 0 && col < v->columnCount)
    {
        auto& c = v->cells[static_cast<size_t>(row)][static_cast<size_t>(col)];
        c.textColor = text;
        c.bgColor = bg;
    }
}
inline void ClearRows(Registry& reg, entt::entity e)
{
    if (auto* v = reg.try_get<components::TableInfo>(e))
    {
        v->cells.clear();
        v->selectedRow = -1;
    }
}
inline void SetSelectedRow(Registry& reg, entt::entity e, int v)
{
    reg.get_or_emplace<components::TableInfo>(e).selectedRow = v;
}
inline void SetHeaderTextColor(Registry& reg, entt::entity e, Color v)
{
    reg.get_or_emplace<components::TableInfo>(e).headerTextColor = v;
}
inline void SetColumnSizing(Registry& reg, entt::entity e, policies::TableColumnSizing v)
{
    reg.get_or_emplace<components::TableInfo>(e).columnSizing = v;
}
inline void SetMinColumnWidths(Registry& reg, entt::entity e, std::vector<float> v)
{
    for (auto& x : v)
        x = scale::Metric(x);
    reg.get_or_emplace<components::TableInfo>(e).minColumnWidths = std::move(v);
}
inline void SetMinRowHeight(Registry& reg, entt::entity e, float v)
{
    reg.get_or_emplace<components::TableInfo>(e).minRowHeight =
        std::max(0.0F, scale::Metric(v));
}
inline void SetRowHeight(Registry& reg, entt::entity e, float v)
{
    reg.get_or_emplace<components::TableInfo>(e).rowHeight =
        std::max(0.0F, scale::Metric(v));
}
[[nodiscard]] inline float MinColumnWidthAt(const components::TableInfo& info, int columnIndex)
{
    if (std::cmp_less(columnIndex, info.minColumnWidths.size()))
    {
        return std::max(0.0F, info.minColumnWidths.at(static_cast<size_t>(columnIndex)));
    }
    return 0.0F;
}

[[nodiscard]] inline std::vector<float> ComputeEqualColumnWidths(const components::TableInfo& info, float visibleWidth)
{
    std::vector<float> widths(static_cast<size_t>(info.columnCount), 0.0F);
    const float equalWidth = visibleWidth / static_cast<float>(info.columnCount);
    for (int columnIndex = 0; columnIndex < info.columnCount; ++columnIndex)
    {
        widths.at(static_cast<size_t>(columnIndex)) = std::max(equalWidth, MinColumnWidthAt(info, columnIndex));
    }
    return widths;
}

[[nodiscard]] inline std::vector<float> ComputeFixedColumnWidths(const components::TableInfo& info, float visibleWidth)
{
    if (!std::cmp_equal(info.columnWidths.size(), info.columnCount))
    {
        return ComputeEqualColumnWidths(info, visibleWidth);
    }
    std::vector<float> widths(static_cast<size_t>(info.columnCount), 0.0F);
    for (int columnIndex = 0; columnIndex < info.columnCount; ++columnIndex)
    {
        widths.at(static_cast<size_t>(columnIndex)) =
            std::max(info.columnWidths.at(static_cast<size_t>(columnIndex)), MinColumnWidthAt(info, columnIndex));
    }
    return widths;
}

[[nodiscard]] inline float TotalColumnWeight(const components::TableInfo& info)
{
    float totalWeight = 0.0F;
    if (std::cmp_equal(info.columnWidths.size(), info.columnCount))
    {
        for (const float weight : info.columnWidths)
            totalWeight += std::max(0.0F, weight);
    }
    return totalWeight > 0.0F ? totalWeight : static_cast<float>(info.columnCount);
}

[[nodiscard]] inline std::vector<float> ComputeProportionalColumnWidths(const components::TableInfo& info,
                                                                        float visibleWidth)
{
    const float totalWeight = TotalColumnWeight(info);
    std::vector<float> widths(static_cast<size_t>(info.columnCount), 0.0F);
    for (int columnIndex = 0; columnIndex < info.columnCount; ++columnIndex)
    {
        float weight = 1.0F;
        if (std::cmp_less(columnIndex, info.columnWidths.size()))
        {
            weight = std::max(0.0F, info.columnWidths.at(static_cast<size_t>(columnIndex)));
        }
        widths.at(static_cast<size_t>(columnIndex)) =
            std::max((weight / totalWeight) * visibleWidth, MinColumnWidthAt(info, columnIndex));
    }
    return widths;
}

[[nodiscard]] inline std::vector<float> ComputeAdaptiveColumnWidths(const components::TableInfo& info,
                                                                    float visibleWidth)
{
    std::vector<float> widths(static_cast<size_t>(info.columnCount), 0.0F);
    float fixedTotal = 0.0F;
    int flexCount = 0;
    for (int columnIndex = 0; columnIndex < info.columnCount; ++columnIndex)
    {
        const float fixedWidth = std::cmp_less(columnIndex, info.columnWidths.size())
                                     ? info.columnWidths.at(static_cast<size_t>(columnIndex))
                                     : 0.0F;
        if (fixedWidth > 0.0F)
        {
            widths.at(static_cast<size_t>(columnIndex)) = std::max(fixedWidth, MinColumnWidthAt(info, columnIndex));
            fixedTotal += widths.at(static_cast<size_t>(columnIndex));
        }
        else
        {
            ++flexCount;
        }
    }
    const float remainingWidth = std::max(0.0F, visibleWidth - fixedTotal);
    const float flexWidth = flexCount > 0 ? remainingWidth / static_cast<float>(flexCount) : 0.0F;
    for (int columnIndex = 0; columnIndex < info.columnCount; ++columnIndex)
    {
        const bool fixed = std::cmp_less(columnIndex, info.columnWidths.size()) &&
                           info.columnWidths.at(static_cast<size_t>(columnIndex)) > 0.0F;
        if (!fixed)
        {
            widths.at(static_cast<size_t>(columnIndex)) = std::max(flexWidth, MinColumnWidthAt(info, columnIndex));
        }
    }
    return widths;
}

[[nodiscard]] inline std::vector<float> ComputeColumnWidths(const components::TableInfo& info, float tableWidth)
{
    if (info.columnCount <= 0)
        return {};
    const float visibleWidth = std::max(0.0F, tableWidth);
    switch (info.columnSizing)
    {
        case policies::TableColumnSizing::FIXED:
            return ComputeFixedColumnWidths(info, visibleWidth);
        case policies::TableColumnSizing::PROPORTIONAL:
            return ComputeProportionalColumnWidths(info, visibleWidth);
        case policies::TableColumnSizing::ADAPTIVE:
            return ComputeAdaptiveColumnWidths(info, visibleWidth);
        case policies::TableColumnSizing::EQUAL:
        default:
            return ComputeEqualColumnWidths(info, visibleWidth);
    }
}

inline void SetCellWidget(Registry& reg, entt::entity tableEntity, int row, int col, entt::entity widgetEntity)
{
    auto* info = reg.try_get<components::TableInfo>(tableEntity);
    if (info == nullptr || row < 0 || !std::cmp_less(row, info->cells.size()) || col < 0 || col >= info->columnCount ||
        !reg.valid(widgetEntity))
        return;

    auto& cell = info->cells.at(static_cast<size_t>(row)).at(static_cast<size_t>(col));
    if (cell.cellEntity != entt::null && cell.cellEntity != widgetEntity && reg.valid(cell.cellEntity))
    {
        if (auto* hierarchy = reg.try_get<components::Hierarchy>(tableEntity))
        {
            std::erase(hierarchy->children, cell.cellEntity);
        }
        if (auto* hierarchy = reg.try_get<components::Hierarchy>(cell.cellEntity))
            hierarchy->parent = entt::null;
    }

    cell.cellEntity = widgetEntity;
    reg.emplace_or_replace<components::TableCellWidgetTag>(widgetEntity);
    auto& children = reg.get_or_emplace<components::Hierarchy>(tableEntity).children;
    if (std::ranges::find(children, widgetEntity) == children.end())
    {
        reg.get_or_emplace<components::Hierarchy>(widgetEntity).parent = tableEntity;
        reg.remove<components::RootTag>(widgetEntity);
        children.push_back(widgetEntity);
    }
}
}  // namespace ui::detail::table

namespace ui::utils
{

bool HasAlignment(policies::Alignment value, policies::Alignment flag);
void SetWindowFlag(UiRuntime& runtime, ui::entity entity, policies::WindowFlag flag);
void MarkLayoutChanged(UiRuntime& runtime, ui::entity entity);
void MarkVisualChanged(UiRuntime& runtime, ui::entity entity);
void MarkLayoutAndVisualChanged(UiRuntime& runtime, ui::entity entity);
void MarkLayoutDirty(UiRuntime& runtime, ui::entity entity);
void CloseWindow(UiRuntime& runtime, ui::entity entity);
void QuitUiEventLoop(UiRuntime& runtime);
[[nodiscard]] Vec2 GetAbsolutePosition(UiRuntime& runtime, ui::entity entity);
[[nodiscard]] Rect GetEntityRect(UiRuntime& runtime, ui::entity entity);
[[nodiscard]] Rect GetScrollViewportRect(UiRuntime& runtime, ui::entity entity);
[[nodiscard]] float GetScrollViewportLength(UiRuntime& runtime, ui::entity entity, bool isVertical);
[[nodiscard]] float GetScrollContentLength(UiRuntime& runtime, ui::entity entity, bool isVertical);
[[nodiscard]] float GetScrollMaxOffset(UiRuntime& runtime, ui::entity entity, bool isVertical);
[[nodiscard]] VerticalScrollbarGeometry GetVerticalScrollbarGeometry(UiRuntime& runtime, ui::entity entity);
void InvokeTask(UiRuntime& runtime, VoidCallback func);
using TaskHandle = uint32_t;
TaskHandle TimerCallback(UiRuntime& runtime, uint32_t interval, VoidCallback func);
void CancelQueuedTask(UiRuntime& runtime, TaskHandle handle);
bool IsEntityExist(UiRuntime& runtime, const std::string& alias);

inline void SetWindowFlag(Registry& registry, entt::entity entity, policies::WindowFlag flag)
{
    SetWindowFlag(registry.runtime(), detail::ToPublic(entity), flag);
}

inline void MarkLayoutChanged(Registry& registry, entt::entity entity)
{
    MarkLayoutChanged(registry.runtime(), detail::ToPublic(entity));
}

inline void MarkVisualChanged(Registry& registry, entt::entity entity)
{
    MarkVisualChanged(registry.runtime(), detail::ToPublic(entity));
}

inline void MarkLayoutAndVisualChanged(Registry& registry, entt::entity entity)
{
    MarkLayoutAndVisualChanged(registry.runtime(), detail::ToPublic(entity));
}

inline void MarkLayoutDirty(Registry& registry, entt::entity entity)
{
    MarkLayoutDirty(registry.runtime(), detail::ToPublic(entity));
}

inline void MarkRenderDirty(Registry& registry, entt::entity entity)
{
    MarkRenderDirty(registry.runtime(), detail::ToPublic(entity));
}

[[nodiscard]] inline Vec2 GetAbsolutePosition(Registry& registry, entt::entity entity)
{
    return GetAbsolutePosition(registry.runtime(), detail::ToPublic(entity));
}

[[nodiscard]] inline Rect GetEntityRect(Registry& registry, entt::entity entity)
{
    return GetEntityRect(registry.runtime(), detail::ToPublic(entity));
}

[[nodiscard]] inline Rect GetScrollViewportRect(Registry& registry, entt::entity entity)
{
    return GetScrollViewportRect(registry.runtime(), detail::ToPublic(entity));
}

[[nodiscard]] inline float GetScrollViewportLength(Registry& registry, entt::entity entity, bool isVertical)
{
    return GetScrollViewportLength(registry.runtime(), detail::ToPublic(entity), isVertical);
}

[[nodiscard]] inline float GetScrollContentLength(Registry& registry, entt::entity entity, bool isVertical)
{
    return GetScrollContentLength(registry.runtime(), detail::ToPublic(entity), isVertical);
}

[[nodiscard]] inline float GetScrollMaxOffset(Registry& registry, entt::entity entity, bool isVertical)
{
    return GetScrollMaxOffset(registry.runtime(), detail::ToPublic(entity), isVertical);
}

[[nodiscard]] inline VerticalScrollbarGeometry GetVerticalScrollbarGeometry(Registry& registry, entt::entity entity)
{
    return GetVerticalScrollbarGeometry(registry.runtime(), detail::ToPublic(entity));
}

}  // namespace ui::utils

namespace ui::detail::size
{

inline void SetFixedSize(Registry& reg, entt::entity entity, float width, float height)
{
    if (!reg.valid(entity))
        return;

    auto& size = reg.get_or_emplace<components::Size>(entity);
    size.sizePolicy = policies::Size::FIXED;
    size.size = {scale::Metric(width), scale::Metric(height)};
    size.declaredSize = size.size;
    size.hasDeclaredWidth = true;
    size.hasDeclaredHeight = true;
    utils::MarkLayoutDirty(reg.runtime(), entity);
}

inline void SetSizePolicy(Registry& reg, entt::entity entity, policies::Size policy)
{
    if (!reg.valid(entity))
        return;
    reg.get_or_emplace<components::Size>(entity).sizePolicy = policy;
    utils::MarkLayoutDirty(reg.runtime(), entity);
}

inline void SetSize(Registry& reg, entt::entity entity, float width, float height)
{
    if (!reg.valid(entity))
        return;
    auto& size = reg.get_or_emplace<components::Size>(entity);
    size.size = {scale::Metric(width), scale::Metric(height)};
    size.declaredSize = size.size;
    size.hasDeclaredWidth = true;
    size.hasDeclaredHeight = true;
    utils::MarkLayoutDirty(reg.runtime(), entity);
}

inline void SetPosition(Registry& reg, entt::entity entity, float positionX, float positionY)
{
    if (!reg.valid(entity))
        return;
    reg.get_or_emplace<components::Position>(entity).value = {scale::Metric(positionX), scale::Metric(positionY)};
    utils::MarkLayoutDirty(reg.runtime(), entity);
}

}  // namespace ui::detail::size

namespace ui::detail::visibility
{

inline void SetVisible(Registry& reg, entt::entity entity, bool visible)
{
    if (!reg.valid(entity))
        return;
    if (visible)
    {
        reg.emplace_or_replace<components::VisibleTag>(entity);
    }
    else
    {
        reg.remove<components::VisibleTag>(entity);
    }
    utils::MarkLayoutAndVisualChanged(reg.runtime(), entity);
}

inline void Show(Registry& reg, utils::Logger& logger, entt::entity entity)
{
    if (!reg.valid(entity))
        return;
    reg.emplace_or_replace<components::VisibleTag>(entity);
    auto* windowComp = reg.try_get<components::Window>(entity);
    if (windowComp != nullptr && windowComp->windowID != 0)
    {
        SDL_Window* sdlWindow = SDL_GetWindowFromID(windowComp->windowID);
        if (sdlWindow != nullptr)
        {
            window_sync::SyncWindowProperties(reg, logger, entity, *windowComp, sdlWindow);
            SDL_SetWindowPosition(sdlWindow, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
            SDL_ShowWindow(sdlWindow);
            int posX = 0;
            int posY = 0;
            SDL_GetWindowPosition(sdlWindow, &posX, &posY);
            if (auto* position = reg.try_get<components::Position>(entity))
            {
                position->value = Vec2{static_cast<float>(posX), static_cast<float>(posY)};
            }
        }
    }
    utils::MarkLayoutAndVisualChanged(reg.runtime(), entity);
}

inline void Hide(Registry& reg, entt::entity entity)
{
    if (!reg.valid(entity))
        return;
    reg.remove<components::VisibleTag>(entity);
    auto* windowComp = reg.try_get<components::Window>(entity);
    if (windowComp != nullptr && windowComp->windowID != 0)
    {
        if (SDL_Window* sdlWindow = SDL_GetWindowFromID(windowComp->windowID))
            SDL_HideWindow(sdlWindow);
    }
    utils::MarkLayoutAndVisualChanged(reg.runtime(), entity);
}

inline void SetAlpha(Registry& reg, entt::entity entity, float alpha)
{
    if (!reg.valid(entity))
        return;
    reg.get_or_emplace<components::Alpha>(entity).value = std::clamp(alpha, 0.0F, 1.0F);
    utils::MarkVisualChanged(reg.runtime(), entity);
}

inline void SetBackgroundColor(Registry& reg, entt::entity entity, const Color& color)
{
    if (!reg.valid(entity))
        return;
    auto& background = reg.get_or_emplace<components::Background>(entity);
    background.color = color;
    background.enabled = policies::Feature::ENABLED;
    utils::MarkVisualChanged(reg.runtime(), entity);
}

inline void SetBorderRadius(Registry& reg, entt::entity entity, float radius)
{
    if (!reg.valid(entity))
        return;
    auto& background = reg.get_or_emplace<components::Background>(entity);
    const auto CLAMPED = std::max(0.0F, scale::Metric(radius));
    background.borderRadius = {CLAMPED, CLAMPED, CLAMPED, CLAMPED};
    background.enabled = policies::Feature::ENABLED;
    if (auto* border = reg.try_get<components::Border>(entity))
    {
        border->borderRadius = {CLAMPED, CLAMPED, CLAMPED, CLAMPED};
    }
    utils::MarkVisualChanged(reg.runtime(), entity);
}

inline void SetBorderColor(Registry& reg, entt::entity entity, const Color& color)
{
    if (!reg.valid(entity))
        return;
    auto& border = reg.get_or_emplace<components::Border>(entity);
    border.color = color;
    border.enabled = policies::Feature::ENABLED;
    utils::MarkVisualChanged(reg.runtime(), entity);
}

inline void SetBorderThickness(Registry& reg, entt::entity entity, float thickness)
{
    if (!reg.valid(entity))
        return;
    auto& border = reg.get_or_emplace<components::Border>(entity);
    border.thickness = scale::Metric(thickness);
    border.enabled = policies::Feature::ENABLED;
    utils::MarkVisualChanged(reg.runtime(), entity);
}

}  // namespace ui::detail::visibility

namespace ui::detail::layout
{

inline void SetLayoutDirection(Registry& reg, entt::entity entity, policies::LayoutDirection direction)
{
    if (!reg.valid(entity))
        return;
    reg.get_or_emplace<components::LayoutInfo>(entity).direction = direction;
    utils::MarkLayoutDirty(reg.runtime(), entity);
}

inline void SetLayoutSpacing(Registry& reg, entt::entity entity, float spacing)
{
    if (!reg.valid(entity))
        return;
    if (auto* layout = reg.try_get<components::LayoutInfo>(entity))
    {
        layout->spacing = std::max(0.0F, scale::Metric(spacing));
        utils::MarkLayoutDirty(reg.runtime(), entity);
    }
}

inline void SetPadding(Registry& reg, entt::entity entity, float left, float top, float right, float bottom)
{
    if (!reg.valid(entity))
        return;
    reg.get_or_emplace<components::Padding>(entity).values =
        Vec4(scale::Metric(top), scale::Metric(right), scale::Metric(bottom), scale::Metric(left));
    utils::MarkLayoutDirty(reg.runtime(), entity);
}

inline void SetPadding(Registry& reg, entt::entity entity, float padding)
{
    SetPadding(reg, entity, padding, padding, padding, padding);
}

inline void CenterInParent(Registry& reg, entt::entity entity)
{
    if (!reg.valid(entity))
        return;
    if (!reg.all_of<components::Size>(entity))
    {
        return;
    }
    auto& anchors = reg.get_or_emplace<components::Anchors>(entity);
    anchors.value = ui::AnchorSet::CenterIn();
    ++anchors.revision;
    utils::MarkLayoutDirty(reg.runtime(), entity);
}

}  // namespace ui::detail::layout

namespace ui::detail::hierarchy
{

// NOLINTNEXTLINE(misc-no-recursion)
inline void AppendChildrenPostOrder(Registry& reg, entt::entity parent, std::vector<entt::entity>& output)
{
    if (!reg.valid(parent))
        return;
    const auto* hierarchy = reg.try_get<components::Hierarchy>(parent);
    if (hierarchy == nullptr || hierarchy->children.empty())
        return;
    auto childrenCopy = hierarchy->children;
    for (const entt::entity CHILD : childrenCopy)
    {
        if (!reg.valid(CHILD))
            continue;
        AppendChildrenPostOrder(reg, CHILD, output);
        output.push_back(CHILD);
    }
}

inline void RemoveChild(Registry& reg, entt::entity parent, entt::entity child)
{
    if (!reg.valid(parent) || !reg.valid(child))
        return;
    auto* parentHierarchy = reg.try_get<components::Hierarchy>(parent);
    auto* childHierarchy = reg.try_get<components::Hierarchy>(child);
    if (parentHierarchy != nullptr && childHierarchy != nullptr && childHierarchy->parent == parent)
    {
        std::erase(parentHierarchy->children, child);
        childHierarchy->parent = entt::null;
        reg.emplace_or_replace<components::RootTag>(child);
        if (auto* anchors = reg.try_get<components::Anchors>(child))
        {
            bool invalidated = false;
            for (auto& binding : anchors->value.bindings)
            {
                if (binding && binding->target.sibling &&
                    !reg.valid(detail::ToInternal(*binding->target.sibling)))
                {
                    binding.reset();
                    invalidated = true;
                }
            }
            if (invalidated)
            {
                anchors->value.clearUnresolved();
                anchors->lastDiagnostic = AnchorDiagnostic::TargetLost;
                anchors->targetLostReported = true;
            }
        }
        utils::MarkLayoutAndVisualChanged(reg.runtime(), parent);
        utils::MarkLayoutAndVisualChanged(reg.runtime(), child);
    }
}

inline void AddChild(Registry& reg, entt::entity parent, entt::entity child)
{
    if (!reg.valid(parent) || !reg.valid(child))
        return;
    if (parent == child)
        return;
    for (auto ancestor = parent; reg.valid(ancestor);)
    {
        if (ancestor == child)
            return;
        const auto* hierarchy = reg.try_get<components::Hierarchy>(ancestor);
        if (hierarchy == nullptr || hierarchy->parent == entt::null)
            break;
        ancestor = hierarchy->parent;
    }
    auto& childHierarchy = reg.get_or_emplace<components::Hierarchy>(child);
    if (childHierarchy.parent != entt::null && childHierarchy.parent != parent)
        RemoveChild(reg, childHierarchy.parent, child);
    childHierarchy.parent = parent;
    // Reparent 后由 LayoutSystem 在新 root 中重新同步 Yoga 子树；保留实体
    // 但不能让旧父节点的布局快照继续参与本帧锚点求解。
    reg.remove<components::RootTag>(child);
    auto& children = reg.get_or_emplace<components::Hierarchy>(parent).children;
    if (std::ranges::find(children, child) == children.end())
        children.push_back(child);
    if (auto* anchors = reg.try_get<components::Anchors>(child))
    {
        bool invalidated = false;
        for (auto& binding : anchors->value.bindings)
        {
            if (binding && binding->target.sibling)
            {
                const auto sibling = detail::ToInternal(*binding->target.sibling);
                const auto* siblingHierarchy = reg.try_get<components::Hierarchy>(sibling);
                if (!reg.valid(sibling) || siblingHierarchy == nullptr || siblingHierarchy->parent != parent)
                {
                    binding.reset();
                    invalidated = true;
                }
            }
        }
        if (invalidated)
        {
            anchors->value.clearUnresolved();
            anchors->lastDiagnostic = AnchorDiagnostic::TargetLost;
            anchors->targetLostReported = true;
        }
    }
    utils::MarkLayoutAndVisualChanged(reg.runtime(), child);
}

[[nodiscard]] inline std::vector<entt::entity> ChildrenPostOrder(Registry& reg, entt::entity parent)
{
    std::vector<entt::entity> children;
    AppendChildrenPostOrder(reg, parent, children);
    return children;
}

}  // namespace ui::detail::hierarchy

namespace ui::detail::icon
{

inline void SetIcon(Registry& reg, entt::entity entity, const std::string& textureId,
                    policies::IconFlag iconflag = policies::IconFlag::DEFAULT, float iconSize = 16.0F,
                    float spacing = 4.0F)
{
    if (!reg.valid(entity))
        return;
    (void)iconflag;
    auto& icon = reg.get_or_emplace<components::Icon>(entity);
    icon.type |= policies::IconFlag::TEXTURE;
    icon.textureId = textureId;
    icon.fontHandle = nullptr;
    icon.codepoint = 0;
    icon.size = {scale::Metric(iconSize), scale::Metric(iconSize)};
    icon.spacing = scale::Metric(spacing);
    utils::MarkLayoutAndVisualChanged(reg.runtime(), entity);
}

inline void SetIcon(Registry& reg, entt::entity entity, const std::string& fontName, uint32_t codepoint,
                    policies::IconFlag iconflag = policies::IconFlag::DEFAULT, float iconSize = 16.0F,
                    float spacing = 4.0F)
{
    if (!reg.valid(entity))
        return;
    (void)iconflag;
    auto& icon = reg.get_or_emplace<components::Icon>(entity);
    icon.type |= ~policies::IconFlag::TEXTURE;
    static std::unordered_set<std::string> fontNamePool;
    const auto [fontIterator, wasInserted] = fontNamePool.insert(fontName);
    (void)wasInserted;
    icon.fontHandle = fontIterator->c_str();
    icon.codepoint = codepoint;
    icon.textureId.clear();
    icon.size = {scale::Metric(iconSize), scale::Metric(iconSize)};
    icon.spacing = scale::Metric(spacing);
    utils::MarkLayoutAndVisualChanged(reg.runtime(), entity);
}

inline void RemoveIcon(Registry& reg, entt::entity entity)
{
    if (!reg.valid(entity))
        return;
    if (reg.any_of<components::Icon>(entity))
    {
        reg.remove<components::Icon>(entity);
        utils::MarkLayoutAndVisualChanged(reg.runtime(), entity);
    }
}

}  // namespace ui::detail::icon

namespace ui::query::bridge
{

[[nodiscard]] inline bool IsValid(const Registry& reg, entt::entity entity) noexcept
{
    return reg.valid(entity);
}

[[nodiscard]] inline Result<entt::entity> FindByAlias(Registry& reg, std::string_view alias)
{
    if (alias.empty())
        return Err(UiErrc::INVALID_ARGUMENT, "empty alias");
    for (const auto ENTITY : reg.view<components::BaseInfo>())
    {
        if (reg.get<components::BaseInfo>(ENTITY).alias == alias)
            return ENTITY;
    }
    return Err(UiErrc::INVALID_ENTITY, std::string(alias));
}

[[nodiscard]] inline std::string GetAlias(const Registry& reg, entt::entity entity)
{
    if (!reg.valid(entity))
        return {};
    const auto* info = reg.try_get<components::BaseInfo>(entity);
    return info != nullptr ? info->alias : std::string{};
}

}  // namespace ui::query::bridge

namespace ui::theme::bridge
{

[[nodiscard]] inline ThemePalette DefaultDarkTheme()
{
    return ThemePalette{};
}

inline void SetTheme(UiRuntime& runtime, const ThemePalette& palette)
{
    auto& context = runtime.ensureContext<ThemeContext>();
    context.previousPalette = context.palette;
    context.palette = palette;
    ++context.version;
    context.reapplyRequested = true;
}

inline void UseDefaultDarkTheme(UiRuntime& runtime)
{
    ui::theme::bridge::SetTheme(runtime, DefaultDarkTheme());
}

inline void RequestThemeReapply(UiRuntime& runtime)
{
    auto& context = runtime.ensureContext<ThemeContext>();
    context.previousPalette = context.palette;
    context.reapplyRequested = true;
}

[[nodiscard]] inline const ThemePalette& CurrentTheme(UiRuntime& runtime)
{
    return runtime.ensureContext<ThemeContext>().palette;
}

}  // namespace ui::theme::bridge

namespace ui::detail::event_bridge
{
struct ConnectionHandle
{
    std::weak_ptr<EventDomain> domain;
    std::uint64_t token = 0;
};

namespace impl
{
struct CallbackSlot
{
    std::uint64_t token = 0;
    std::shared_ptr<ui::event::EventCallback> callback;
    bool connected = true;
};

struct QueuedCustomEvent
{
    ui::event::EventId id = ui::event::INVALID_EVENT_ID;
    ui::event::EventPayload payload;
};

}  // namespace impl

struct EventDomain
{
    std::unordered_map<std::string, ui::event::EventId> idsByName;
    std::unordered_map<ui::event::EventId, std::string> namesById;
    std::unordered_map<ui::event::EventId, std::vector<ui::detail::event_bridge::impl::CallbackSlot>> callbacks;
    std::unordered_map<std::uint64_t, ui::event::EventId> idsByToken;
    std::vector<ui::detail::event_bridge::impl::QueuedCustomEvent> queue;
    std::uint64_t nextToken = 1;
    ui::event::EventId nextEventId = 1;
};

namespace impl
{
struct EventRegistryContext
{
    std::shared_ptr<EventDomain> domain = std::make_shared<EventDomain>();
};

[[nodiscard]] inline EventRegistryContext& CurrentContext(UiRuntime& runtime)
{
    return runtime.ensureContext<EventRegistryContext>();
}

[[nodiscard]] inline EventDomain& CurrentDomain(UiRuntime& runtime)
{
    return *CurrentContext(runtime).domain;
}

[[nodiscard]] inline std::shared_ptr<EventDomain> CurrentDomainHandle(UiRuntime& runtime)
{
    return CurrentContext(runtime).domain;
}

[[nodiscard]] inline CallbackSlot* FindSlot(EventDomain& context, std::uint64_t token) noexcept
{
    auto idIt = context.idsByToken.find(token);
    if (idIt == context.idsByToken.end())
        return nullptr;
    auto callbacksIt = context.callbacks.find(idIt->second);
    if (callbacksIt == context.callbacks.end())
        return nullptr;
    auto& slots = callbacksIt->second;
    auto slotIt = std::ranges::find_if(slots, [token](const CallbackSlot& slot) { return slot.token == token; });
    return slotIt == slots.end() ? nullptr : &*slotIt;
}

inline void Dispatch(EventDomain& context, ui::event::EventId eventId,
                     const ui::event::EventPayload& payload)
{
    if (eventId == ui::event::INVALID_EVENT_ID)
        return;
    auto callbacksIt = context.callbacks.find(eventId);
    if (callbacksIt == context.callbacks.end())
        return;
    const auto snapshot = callbacksIt->second;
    for (const auto& slot : snapshot)
    {
        const auto* current = FindSlot(context, slot.token);
        if (current != nullptr && current->connected && slot.callback != nullptr && static_cast<bool>(*slot.callback))
            (*slot.callback)(payload);
    }

    callbacksIt = context.callbacks.find(eventId);
    if (callbacksIt != context.callbacks.end())
    {
        auto& slots = callbacksIt->second;
        std::erase_if(slots, [](const CallbackSlot& slot) { return !slot.connected; });
    }
}
}  // namespace impl

[[nodiscard]] inline ui::event::EventId RegisterEvent(UiRuntime& runtime, std::string_view name)
{
    if (name.empty())
        return ui::event::INVALID_EVENT_ID;
    auto& context = impl::CurrentDomain(runtime);
    const std::string KEY{name};
    if (auto eventIt = context.idsByName.find(KEY); eventIt != context.idsByName.end())
        return eventIt->second;
    auto eventId = context.nextEventId++;
    context.idsByName.emplace(KEY, eventId);
    context.namesById.emplace(eventId, KEY);
    return eventId;
}

[[nodiscard]] inline bool IsEventRegistered(UiRuntime& runtime, ui::event::EventId eventId)
{
    return eventId != ui::event::INVALID_EVENT_ID && impl::CurrentDomain(runtime).namesById.contains(eventId);
}

[[nodiscard]] inline bool IsEventRegistered(UiRuntime& runtime, std::string_view name)
{
    return !name.empty() && impl::CurrentDomain(runtime).idsByName.contains(std::string{name});
}

[[nodiscard]] inline ConnectionHandle Connect(UiRuntime& runtime, ui::event::EventId eventId, ui::event::EventCallback callback)
{
    if (eventId == ui::event::INVALID_EVENT_ID || !static_cast<bool>(callback))
        return {};
    auto domain = impl::CurrentDomainHandle(runtime);
    auto& context = *domain;
    if (!context.namesById.contains(eventId))
        return {};
    const auto TOKEN = context.nextToken++;
    context.callbacks[eventId].push_back({.token = TOKEN,
                                          .callback = std::make_shared<ui::event::EventCallback>(std::move(callback)),
                                          .connected = true});
    context.idsByToken.emplace(TOKEN, eventId);
    return {.domain = domain, .token = TOKEN};
}

[[nodiscard]] inline ConnectionHandle Connect(UiRuntime& runtime, std::string_view name, ui::event::EventCallback callback)
{
    return Connect(runtime, RegisterEvent(runtime, name), std::move(callback));
}

inline void Disconnect(EventDomain& context, std::uint64_t token) noexcept
{
    if (token == 0)
        return;
    if (auto* slot = impl::FindSlot(context, token); slot != nullptr)
        slot->connected = false;
    context.idsByToken.erase(token);
}

[[nodiscard]] inline bool Connected(EventDomain& context, std::uint64_t token) noexcept
{
    if (token == 0)
        return false;
    const auto* slot = impl::FindSlot(context, token);
    return slot != nullptr && slot->connected;
}

// NOLINTNEXTLINE(performance-unnecessary-value-param) -- 与公开 API 的按值载荷契约一致。
inline void Trigger(UiRuntime& runtime, ui::event::EventId eventId, ui::event::EventPayload payload)
{
    impl::Dispatch(impl::CurrentDomain(runtime), eventId, payload);
}

// NOLINTNEXTLINE(performance-unnecessary-value-param) -- 与公开 API 的按值载荷契约一致。
inline void Trigger(UiRuntime& runtime, std::string_view name, ui::event::EventPayload payload)
{
    impl::Dispatch(impl::CurrentDomain(runtime), RegisterEvent(runtime, name), payload);
}

inline void Enqueue(UiRuntime& runtime, ui::event::EventId eventId, ui::event::EventPayload payload)
{
    if (!IsEventRegistered(runtime, eventId))
        return;
    impl::CurrentDomain(runtime).queue.push_back({.id = eventId, .payload = std::move(payload)});
}

inline void Enqueue(UiRuntime& runtime, std::string_view name, ui::event::EventPayload payload)
{
    auto eventId = RegisterEvent(runtime, name);
    if (eventId == ui::event::INVALID_EVENT_ID)
        return;
    impl::CurrentDomain(runtime).queue.push_back({.id = eventId, .payload = std::move(payload)});
}

inline void DispatchQueued(UiRuntime& runtime)
{
    auto& context = impl::CurrentDomain(runtime);
    auto pending = std::exchange(context.queue, {});
    for (const auto& event : pending)
        impl::Dispatch(context, event.id, event.payload);
}

}  // namespace ui::detail::event_bridge

namespace ui::detail::canvas
{

inline void Clear(Registry& reg, entt::entity entity)
{
    if (!reg.valid(entity))
        return;
    reg.get_or_emplace<components::CanvasDrawList>(entity).commands.clear();
}

inline void DrawLine(Registry& reg, entt::entity entity, Vec2 from, Vec2 endPos, Color color, float lineWidth = 1.0F)
{
    if (!reg.valid(entity))
        return;
    reg.get_or_emplace<components::CanvasDrawList>(entity).commands.push_back({.type = components::CanvasDrawType::LINE,
                                                                               .p1 = scale::Metric(from),
                                                                               .p2 = scale::Metric(endPos),
                                                                               .p3 = {},
                                                                               .p4 = {},
                                                                               .color = color,
                                                                               .lineWidth = scale::Metric(lineWidth),
                                                                               .points = {}});
}

inline void DrawRect(Registry& reg, entt::entity entity, Vec2 topLeft, Vec2 bottomRight, Color color, float lineWidth = 1.0F)
{
    if (!reg.valid(entity))
        return;
    reg.get_or_emplace<components::CanvasDrawList>(entity).commands.push_back({.type = components::CanvasDrawType::RECT,
                                                                               .p1 = scale::Metric(topLeft),
                                                                               .p2 = scale::Metric(bottomRight),
                                                                               .p3 = {},
                                                                               .p4 = {},
                                                                               .color = color,
                                                                               .lineWidth = scale::Metric(lineWidth),
                                                                               .points = {}});
}

inline void DrawFilledRect(Registry& reg, entt::entity entity, Vec2 topLeft, Vec2 bottomRight, Color color)
{
    if (!reg.valid(entity))
        return;
    reg.get_or_emplace<components::CanvasDrawList>(entity).commands.push_back(
        {.type = components::CanvasDrawType::FILLED_RECT,
         .p1 = scale::Metric(topLeft),
         .p2 = scale::Metric(bottomRight),
         .p3 = {},
         .p4 = {},
         .color = color,
         .lineWidth = scale::Metric(1.0F),
         .points = {}});
}

inline void DrawCircle(Registry& reg, entt::entity entity, Vec2 center, float radius, Color color, float lineWidth = 1.0F)
{
    if (!reg.valid(entity))
        return;
    reg.get_or_emplace<components::CanvasDrawList>(entity).commands.push_back(
        {.type = components::CanvasDrawType::CIRCLE,
         .p1 = scale::Metric(center),
         .p2 = {scale::Metric(radius), 0.0F},
         .p3 = {},
         .p4 = {},
         .color = color,
         .lineWidth = scale::Metric(lineWidth),
         .points = {}});
}

inline void DrawFilledCircle(Registry& reg, entt::entity entity, Vec2 center, float radius, Color color)
{
    if (!reg.valid(entity))
        return;
    reg.get_or_emplace<components::CanvasDrawList>(entity).commands.push_back(
        {.type = components::CanvasDrawType::FILLED_CIRCLE,
         .p1 = scale::Metric(center),
         .p2 = {scale::Metric(radius), 0.0F},
         .p3 = {},
         .p4 = {},
         .color = color,
         .lineWidth = scale::Metric(1.0F),
         .points = {}});
}

inline void DrawPolyline(Registry& reg, entt::entity entity, std::vector<Vec2> points, Color color, float lineWidth = 1.0F)
{
    if (!reg.valid(entity) || points.size() < 2)
        return;
    components::CanvasDrawCommand command;
    command.type = components::CanvasDrawType::POLYLINE;
    command.color = color;
    command.lineWidth = scale::Metric(lineWidth);
    for (auto& point : points)
        point = scale::Metric(point);
    command.points = std::move(points);
    reg.get_or_emplace<components::CanvasDrawList>(entity).commands.push_back(std::move(command));
}

inline void DrawCubicBezier(Registry& reg, entt::entity entity, Vec2 startPos, Vec2 cp1, Vec2 cp2, Vec2 endPos, Color color,
                            float lineWidth = 1.0F)
{
    if (!reg.valid(entity))
        return;
    components::CanvasDrawCommand command;
    command.type = components::CanvasDrawType::CUBIC_BEZIER;
    command.p1 = scale::Metric(startPos);
    command.p2 = scale::Metric(cp1);
    command.p3 = scale::Metric(cp2);
    command.p4 = scale::Metric(endPos);
    command.color = color;
    command.lineWidth = scale::Metric(lineWidth);
    reg.get_or_emplace<components::CanvasDrawList>(entity).commands.push_back(std::move(command));
}

}  // namespace ui::detail::canvas