#include "ui/api/Layout.hpp"

#include "helper/Helper.hpp"
#include <cmath>
#include <functional>
#include <unordered_map>
#include <unordered_set>

namespace ui::layout
{

namespace
{
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
Result<void> ValidateAnchorSet(Registry& reg, entt::entity source, const AnchorSet& anchors)
{
    const auto* sourceHierarchy = reg.try_get<components::Hierarchy>(source);
    if (sourceHierarchy == nullptr || sourceHierarchy->parent == entt::null || !reg.valid(sourceHierarchy->parent))
    {
        return Err(UiErrc::INVALID_ARGUMENT, "Anchors require a valid parent");
    }
    for (const auto& binding : anchors.bindings)
    {
        if (!binding)
        {
            continue;
        }
        if (!std::isfinite(binding->offset))
        {
            return Err(UiErrc::INVALID_ARGUMENT, "Anchor offsets must be finite");
        }
        if (binding->target.sibling)
        {
            const auto sibling = ui::detail::ToInternal(*binding->target.sibling);
            if (!reg.valid(sibling) || sibling == source || sibling == sourceHierarchy->parent ||
                !reg.any_of<components::Position, components::Size>(sibling))
            {
                return Err(UiErrc::INVALID_ARGUMENT, "Anchor target must be a valid sibling");
            }
            const auto* siblingHierarchy = reg.try_get<components::Hierarchy>(sibling);
            if (siblingHierarchy == nullptr || siblingHierarchy->parent != sourceHierarchy->parent)
            {
                return Err(UiErrc::INVALID_ARGUMENT, "Anchor target must share the same parent");
            }
        }
    }
    for (const auto value : {anchors.margins.left, anchors.margins.right, anchors.margins.top, anchors.margins.bottom})
    {
        if (!std::isfinite(value))
        {
            return Err(UiErrc::INVALID_ARGUMENT, "Anchor margins must be finite");
        }
    }
    for (const auto value : {anchors.leftMargin, anchors.rightMargin, anchors.topMargin, anchors.bottomMargin})
    {
        if (value && !std::isfinite(*value))
        {
            return Err(UiErrc::INVALID_ARGUMENT, "Anchor margin override must be finite");
        }
    }
    if (!std::isfinite(anchors.baselineOffset))
    {
        return Err(UiErrc::INVALID_ARGUMENT, "Baseline offset must be finite");
    }
    // Qt Quick 语义：同一轴不能同时绑定 center 与任一 edge；baseline 与垂直方向
    // 的 edge/center 互斥（baseline 已隐含了垂直定位）。
    const bool hasHorizontalEdge = anchors.get(AnchorLine::Left) || anchors.get(AnchorLine::Right);
    const bool hasVerticalEdge = anchors.get(AnchorLine::Top) || anchors.get(AnchorLine::Bottom);
    if ((anchors.get(AnchorLine::Baseline) &&
         (anchors.get(AnchorLine::Top) || anchors.get(AnchorLine::Bottom) || anchors.get(AnchorLine::VerticalCenter))) ||
        (anchors.get(AnchorLine::HorizontalCenter) && hasHorizontalEdge) ||
        (anchors.get(AnchorLine::VerticalCenter) && hasVerticalEdge))
    {
        return Err(UiErrc::INVALID_ARGUMENT, "Conflicting anchors");
    }

    // A sibling anchor is a dependency on the target's axis.  Reject cycles
    // here instead of relying on an order-dependent fixed number of passes.
    const auto axis = [](AnchorLine line) { return line == AnchorLine::Left || line == AnchorLine::Right ||
        line == AnchorLine::HorizontalCenter ? 0 : 1; };
    for (int currentAxis = 0; currentAxis < 2; ++currentAxis)
    {
        std::unordered_map<entt::entity, std::vector<entt::entity>> graph;
        auto add = [&](entt::entity entity, const AnchorSet& value) {
            for (const auto& binding : value.bindings)
            {
                if (!binding || !binding->target.sibling || axis(binding->line) != currentAxis) continue;
                const auto target = detail::ToInternal(*binding->target.sibling);
                if (reg.valid(target)) graph[target].push_back(entity);
            }
        };
        for (const auto entity : reg.view<components::Anchors>()) add(entity, reg.get<components::Anchors>(entity).value);
        add(source, anchors);
        std::unordered_set<entt::entity> visiting;
        std::unordered_set<entt::entity> visited;
        std::function<bool(entt::entity)> visit = [&](entt::entity entity) {
            if (visiting.contains(entity)) return false;
            if (visited.contains(entity)) return true;
            visiting.insert(entity);
            for (const auto next : graph[entity]) if (!visit(next)) return false;
            visiting.erase(entity);
            visited.insert(entity);
            return true;
        };
        for (const auto& [entity, unused] : graph) if (!visit(entity))
            return Err(UiErrc::INVALID_ARGUMENT, "Anchor dependency cycle");
    }
    return Ok();
}
} // namespace

Result<void> SetAnchors(ui::entity entity, const AnchorSet& anchors)
{
    auto& runtime = UiRuntime::current();
    auto& reg = runtime.registry();
    const auto target = ui::detail::ToInternal(entity);
    if (!reg.valid(target) || !reg.all_of<components::Position, components::Size>(target))
    {
        return Err(UiErrc::INVALID_ENTITY);
    }
    if (auto result = ValidateAnchorSet(reg, target, anchors); !result)
    {
        return result;
    }
    auto& state = reg.get_or_emplace<components::Anchors>(target);
    state.value = anchors;
    state.value.clearUnresolved();
    ++state.revision;
    state.lastDiagnostic = AnchorDiagnostic::None;
    state.targetLostReported = false;
    utils::MarkLayoutDirty(runtime, target);
    return Ok();
}

Result<void> SetAnchors(ui::entity entity, const ParentAnchors& anchors)
{
    return SetAnchors(entity, anchors.toAnchorSet());
}

Result<void> SetAnchor(ui::entity entity, AnchorLine line, AnchorTarget target, float offset)
{
    return SetAnchor(entity, line, target, line, offset);
}

Result<void> SetAnchor(ui::entity entity, AnchorLine line, AnchorTarget target, AnchorLine targetLine, float offset)
{
    auto& runtime = UiRuntime::current();
    auto& reg = runtime.registry();
    const auto internal = ui::detail::ToInternal(entity);
    if (!reg.valid(internal) || !reg.all_of<components::Position, components::Size>(internal))
    {
        return Err(UiErrc::INVALID_ENTITY);
    }
    AnchorSet next = reg.any_of<components::Anchors>(internal) ? reg.get<components::Anchors>(internal).value : AnchorSet{};
    next.set(line, target, targetLine, offset);
    return SetAnchors(entity, next);
}

Result<void> ClearAnchor(ui::entity entity, AnchorLine line)
{
    auto& runtime = UiRuntime::current();
    auto& reg = runtime.registry();
    const auto internal = ui::detail::ToInternal(entity);
    if (!reg.valid(internal))
    {
        return Err(UiErrc::INVALID_ENTITY);
    }
    if (auto* state = reg.try_get<components::Anchors>(internal))
    {
        state->value.clear(line);
        ++state->revision;
        utils::MarkLayoutDirty(runtime, internal);
    }
    return Ok();
}

Result<void> SetAnchorMargins(ui::entity entity, const AnchorMargins& margins)
{
    auto& runtime = UiRuntime::current();
    auto& reg = runtime.registry();
    const auto internal = ui::detail::ToInternal(entity);
    if (!reg.valid(internal))
    {
        return Err(UiErrc::INVALID_ENTITY);
    }
    AnchorSet next = reg.any_of<components::Anchors>(internal) ? reg.get<components::Anchors>(internal).value
                                                                : AnchorSet{};
    next.margins = margins;
    return SetAnchors(entity, next);
}

Result<void> SetAnchorMargin(ui::entity entity, AnchorLine line, float margin)
{
    if (!std::isfinite(margin))
    {
        return Err(UiErrc::INVALID_ARGUMENT, "Anchor margin must be finite");
    }
    auto& runtime = UiRuntime::current();
    auto& reg = runtime.registry();
    const auto internal = ui::detail::ToInternal(entity);
    if (!reg.valid(internal))
    {
        return Err(UiErrc::INVALID_ENTITY);
    }
    AnchorSet next = reg.any_of<components::Anchors>(internal) ? reg.get<components::Anchors>(internal).value
                                                                : AnchorSet{};
    switch (line)
    {
        case AnchorLine::Left: next.leftMargin = margin; break;
        case AnchorLine::Right: next.rightMargin = margin; break;
        case AnchorLine::Top: next.topMargin = margin; break;
        case AnchorLine::Bottom: next.bottomMargin = margin; break;
        default: return Err(UiErrc::INVALID_ARGUMENT, "Only edge anchors have margins");
    }
    return SetAnchors(entity, next);
}

Result<void> SetBaselineOffset(ui::entity entity, float offset)
{
    if (!std::isfinite(offset))
    {
        return Err(UiErrc::INVALID_ARGUMENT, "Baseline offset must be finite");
    }
    auto& runtime = UiRuntime::current();
    auto& reg = runtime.registry();
    const auto internal = ui::detail::ToInternal(entity);
    if (!reg.valid(internal))
    {
        return Err(UiErrc::INVALID_ENTITY);
    }
    AnchorSet next = reg.any_of<components::Anchors>(internal) ? reg.get<components::Anchors>(internal).value
                                                                : AnchorSet{};
    next.baselineOffset = offset;
    return SetAnchors(entity, next);
}

Result<void> SetAnchorFill(ui::entity entity, AnchorTarget target, float margin)
{
    if (!std::isfinite(margin))
    {
        return Err(UiErrc::INVALID_ARGUMENT, "Anchor margin must be finite");
    }
    return SetAnchors(entity, AnchorSet::Fill(target, margin));
}

Result<void> SetAnchorCenterIn(ui::entity entity, AnchorTarget target, float offsetX, float offsetY)
{
    return SetAnchors(entity, AnchorSet::CenterIn(target, offsetX, offsetY));
}

Result<void> ClearAllAnchors(ui::entity entity)
{
    auto& runtime = UiRuntime::current();
    auto& reg = runtime.registry();
    const auto target = ui::detail::ToInternal(entity);
    if (!reg.valid(target))
    {
        return Err(UiErrc::INVALID_ENTITY);
    }
    if (reg.any_of<components::Anchors>(target))
    {
        if (auto* size = reg.try_get<components::Size>(target); size != nullptr)
        {
            if (size->hasDeclaredWidth)
            {
                size->size.x() = size->declaredSize.x();
            }
            if (size->hasDeclaredHeight)
            {
                size->size.y() = size->declaredSize.y();
            }
        }
        reg.remove<components::Anchors>(target);
    }
    utils::MarkLayoutDirty(runtime, target);
    return Ok();
}

Result<void> ClearAnchors(ui::entity entity)
{
    auto& reg = UiRuntime::current().registry();
    if (!reg.valid(ui::detail::ToInternal(entity)))
    {
        return Err(UiErrc::INVALID_ENTITY);
    }
    return ClearAllAnchors(entity);
}

void SetLayoutDirection(ui::entity entity, policies::LayoutDirection direction)
{
    ui::detail::layout::SetLayoutDirection(UiRuntime::current().registry(), ui::detail::ToInternal(entity), direction);
}

void SetLayoutSpacing(ui::entity entity, float spacing)
{
    ui::detail::layout::SetLayoutSpacing(UiRuntime::current().registry(), ui::detail::ToInternal(entity), spacing);
}

void SetPadding(ui::entity entity, float left, float top, float right, float bottom)
{
    ui::detail::layout::SetPadding(UiRuntime::current().registry(), ui::detail::ToInternal(entity), left, top, right, bottom);
}

void SetPadding(ui::entity entity, float padding)
{
    ui::detail::layout::SetPadding(UiRuntime::current().registry(), ui::detail::ToInternal(entity), padding);
}

void CenterInParent(ui::entity entity)
{
    ui::detail::layout::CenterInParent(UiRuntime::current().registry(), ui::detail::ToInternal(entity));
}

}  // namespace ui::layout
