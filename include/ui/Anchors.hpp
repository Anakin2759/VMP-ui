#pragma once

#include "ui/api/Entity.hpp"

#include <array>
#include <cmath>
#include <optional>

namespace ui
{
enum class AnchorLine : std::uint8_t
{
    Left,
    Right,
    Top,
    Bottom,
    HorizontalCenter,
    VerticalCenter,
    Baseline,
};

struct AnchorTarget
{
    std::optional<ui::entity> sibling;

    static constexpr AnchorTarget Parent() noexcept { return {}; }
    static constexpr AnchorTarget Sibling(ui::entity value) noexcept { return {{value}}; }
    [[nodiscard]] constexpr bool isParent() const noexcept { return !sibling.has_value(); }
};

struct AnchorRef
{
    AnchorTarget target = AnchorTarget::Parent();
    AnchorLine line = AnchorLine::Left;
    float offset = 0.0F;
};

struct AnchorMargins
{
    float left = 0.0F;
    float right = 0.0F;
    float top = 0.0F;
    float bottom = 0.0F;
};

struct AnchorSet
{
    std::array<std::optional<AnchorRef>, 7> bindings{};
    AnchorMargins margins{};
    std::optional<float> leftMargin;
    std::optional<float> rightMargin;
    std::optional<float> topMargin;
    std::optional<float> bottomMargin;
    // QtQuick Item 的 baselineOffset：非文本 item 的默认值为 0。
    float baselineOffset = 0.0F;

    [[nodiscard]] static constexpr std::size_t index(AnchorLine line) noexcept
    {
        return static_cast<std::size_t>(line);
    }

    void set(AnchorLine line, AnchorTarget target, float offset = 0.0F)
    {
        bindings[index(line)] = AnchorRef{target, line, offset};
    }

    void set(AnchorLine line, AnchorTarget target, AnchorLine targetLine, float offset)
    {
        bindings[index(line)] = AnchorRef{target, targetLine, offset};
    }

    void clear(AnchorLine line) { bindings[index(line)].reset(); }
    [[nodiscard]] const std::optional<AnchorRef>& get(AnchorLine line) const { return bindings[index(line)]; }

    void setMargins(float value) { margins = {value, value, value, value}; }

    static AnchorSet Fill(AnchorTarget target = AnchorTarget::Parent(), float margin = 0.0F)
    {
        AnchorSet result;
        result.setMargins(margin);
        result.set(AnchorLine::Left, target, 0.0F);
        result.set(AnchorLine::Right, target, 0.0F);
        result.set(AnchorLine::Top, target, 0.0F);
        result.set(AnchorLine::Bottom, target, 0.0F);
        return result;
    }

    static AnchorSet CenterIn(AnchorTarget target = AnchorTarget::Parent(), float x = 0.0F, float y = 0.0F)
    {
        AnchorSet result;
        result.set(AnchorLine::HorizontalCenter, target, x);
        result.set(AnchorLine::VerticalCenter, target, y);
        return result;
    }
};

/// 旧 parent-only 锚点值对象，作为迁移适配层保留。
struct ParentAnchors
{
    std::optional<float> left;
    std::optional<float> right;
    std::optional<float> top;
    std::optional<float> bottom;
    std::optional<float> horizontalCenter;
    std::optional<float> verticalCenter;

    static ParentAnchors Fill(float margin = 0.0F)
    {
        ParentAnchors result;
        result.left = result.right = result.top = result.bottom = margin;
        return result;
    }

    static ParentAnchors CenterIn(float x = 0.0F, float y = 0.0F)
    {
        ParentAnchors result;
        result.horizontalCenter = x;
        result.verticalCenter = y;
        return result;
    }

    [[nodiscard]] AnchorSet toAnchorSet() const
    {
        AnchorSet result;
        if (left) result.set(AnchorLine::Left, AnchorTarget::Parent(), *left);
        if (right) result.set(AnchorLine::Right, AnchorTarget::Parent(), *right);
        if (top) result.set(AnchorLine::Top, AnchorTarget::Parent(), *top);
        if (bottom) result.set(AnchorLine::Bottom, AnchorTarget::Parent(), *bottom);
        if (horizontalCenter) result.set(AnchorLine::HorizontalCenter, AnchorTarget::Parent(), *horizontalCenter);
        if (verticalCenter) result.set(AnchorLine::VerticalCenter, AnchorTarget::Parent(), *verticalCenter);
        return result;
    }
};
} // namespace ui