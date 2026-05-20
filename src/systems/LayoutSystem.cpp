#include "LayoutSystem.hpp"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <unordered_set>
#include <utility>
#include <vector>

#include "api/Utils.hpp"
#include "common/Components.hpp"
#include "common/Events.hpp"
#include "common/Policies.hpp"
#include "common/Tags.hpp"
#include "singleton/Dispatcher.hpp"
#include "singleton/Logger.hpp"
#include "singleton/Registry.hpp"

namespace ui::systems
{
namespace
{

constexpr int MAX_HIERARCHY_DEPTH = 1000;
constexpr float SCROLLBAR_GUTTER = 14.0F;
constexpr float DEFAULT_LEAF_WIDTH = 100.0F;
constexpr float DEFAULT_LEAF_HEIGHT = 20.0F;

[[nodiscard]] bool IsLayoutParticipant(entt::entity entity)
{
    // TableCellWidget 实体的 Position/Size 由 TableRenderer 手动写入，不参与 Yoga 布局树
    if (Registry::AnyOf<components::TableCellWidgetTag>(entity))
    {
        return false;
    }
    return Registry::AnyOf<components::SpacerTag>(entity) ||
           Registry::AllOf<components::Position, components::Size>(entity);
}

void DetachFromOwnerIfNeeded(YGNodeRef ownerNode, YGNodeRef childNode, YGNodeRef targetNode)
{
    if (ownerNode != nullptr && ownerNode != targetNode)
    {
        YGNodeRemoveChild(ownerNode, childNode);
    }
}

[[nodiscard]] policies::Alignment ResolveContainerAlignment(entt::entity entity,
                                                            const components::LayoutInfo& layoutInfo)
{
    const auto* scrollArea = Registry::TryGet<components::ScrollArea>(entity);
    if (scrollArea == nullptr)
    {
        return layoutInfo.alignment;
    }

    policies::Alignment alignment = layoutInfo.alignment;

    auto hasHorizontalAlignment = [&](policies::Alignment value)
    {
        return policies::HasFlag(value, policies::Alignment::LEFT) ||
               policies::HasFlag(value, policies::Alignment::HCENTER) ||
               policies::HasFlag(value, policies::Alignment::RIGHT);
    };

    auto hasVerticalAlignment = [&](policies::Alignment value)
    {
        return policies::HasFlag(value, policies::Alignment::TOP) ||
               policies::HasFlag(value, policies::Alignment::VCENTER) ||
               policies::HasFlag(value, policies::Alignment::BOTTOM);
    };

    if (!hasHorizontalAlignment(alignment))
    {
        alignment = alignment | policies::Alignment::HCENTER;
    }

    if (!hasVerticalAlignment(alignment))
    {
        alignment = alignment | policies::Alignment::VCENTER;
    }

    switch (scrollArea->scroll)
    {
        case policies::Scroll::Vertical:
            alignment = static_cast<policies::Alignment>((static_cast<uint8_t>(alignment) &
                                                          ~static_cast<uint8_t>(policies::Alignment::BOTTOM) &
                                                          ~static_cast<uint8_t>(policies::Alignment::VCENTER)) |
                                                         static_cast<uint8_t>(policies::Alignment::TOP));
            break;
        case policies::Scroll::Horizontal:
            alignment = static_cast<policies::Alignment>((static_cast<uint8_t>(alignment) &
                                                          ~static_cast<uint8_t>(policies::Alignment::RIGHT) &
                                                          ~static_cast<uint8_t>(policies::Alignment::HCENTER)) |
                                                         static_cast<uint8_t>(policies::Alignment::LEFT));
            break;
        case policies::Scroll::Both:
            alignment = policies::Alignment::TOP_LEFT;
            break;
        case policies::Scroll::NONE:
        default:
            break;
    }

    return alignment;
}

[[nodiscard]] Vec4 ResolveEffectivePadding(entt::entity entity)
{
    Vec4 paddingValues{0.0F, 0.0F, 0.0F, 0.0F};
    if (const auto* padding = Registry::TryGet<components::Padding>(entity))
    {
        paddingValues = padding->values;
    }

    const auto* scrollArea = Registry::TryGet<components::ScrollArea>(entity);
    if (scrollArea == nullptr || policies::HasFlag(scrollArea->scrollBar, policies::ScrollBar::NoVisibility))
    {
        return paddingValues;
    }

    if (scrollArea->scroll == policies::Scroll::Vertical || scrollArea->scroll == policies::Scroll::Both)
    {
        paddingValues.y() += SCROLLBAR_GUTTER;
    }

    if (scrollArea->scroll == policies::Scroll::Horizontal || scrollArea->scroll == policies::Scroll::Both)
    {
        paddingValues.z() += SCROLLBAR_GUTTER;
    }

    return paddingValues;
}

void SetPaddingIfChanged(YGNodeRef node, YGEdge edge, float value)
{
    const YGValue current = YGNodeStyleGetPadding(node, edge);
    if (current.unit != YGUnitPoint || current.value != value)
    {
        YGNodeStyleSetPadding(node, edge, value);
    }
}

void ConfigureLayoutDirectionAndGap(entt::entity entity, YGNodeRef node)
{
    const auto* layoutInfo = Registry::TryGet<components::LayoutInfo>(entity);
    if (layoutInfo == nullptr)
    {
        return;
    }

    const YGFlexDirection targetDirection =
        (layoutInfo->direction == policies::LayoutDirection::VERTICAL) ? YGFlexDirectionColumn : YGFlexDirectionRow;
    if (YGNodeStyleGetFlexDirection(node) != targetDirection)
    {
        YGNodeStyleSetFlexDirection(node, targetDirection);
    }

    const YGValue currentGap = YGNodeStyleGetGap(node, YGGutterAll);
    if (currentGap.unit != YGUnitPoint || currentGap.value != layoutInfo->spacing)
    {
        YGNodeStyleSetGap(node, YGGutterAll, layoutInfo->spacing);
    }
}

void ConfigurePadding(entt::entity entity, YGNodeRef node)
{
    const Vec4 effectivePadding = ResolveEffectivePadding(entity);
    SetPaddingIfChanged(node, YGEdgeTop, effectivePadding.x());
    SetPaddingIfChanged(node, YGEdgeRight, effectivePadding.y());
    SetPaddingIfChanged(node, YGEdgeBottom, effectivePadding.z());
    SetPaddingIfChanged(node, YGEdgeLeft, effectivePadding.w());
}

[[nodiscard]] bool ConfigureSpacerNode(entt::entity entity, YGNodeRef node)
{
    if (!Registry::AnyOf<components::SpacerTag>(entity))
    {
        return false;
    }

    const auto* spacer = Registry::TryGet<components::Spacer>(entity);
    const float stretchFactor = spacer != nullptr ? static_cast<float>(spacer->stretchFactor) : 1.0F;

    if (YGNodeStyleGetFlexGrow(node) != stretchFactor)
    {
        YGNodeStyleSetFlexGrow(node, stretchFactor);
    }

    if (YGNodeStyleGetFlexShrink(node) != 1.0F)
    {
        YGNodeStyleSetFlexShrink(node, 1.0F);
    }

    const YGValue basis = YGNodeStyleGetFlexBasis(node);
    if (basis.unit != YGUnitPoint || basis.value != 0.0F)
    {
        YGNodeStyleSetFlexBasis(node, 0.0F);
    }

    const YGValue minWidth = YGNodeStyleGetMinWidth(node);
    if (minWidth.unit != YGUnitPoint || minWidth.value != 0.0F)
    {
        YGNodeStyleSetMinWidth(node, 0.0F);
    }

    const YGValue minHeight = YGNodeStyleGetMinHeight(node);
    if (minHeight.unit != YGUnitPoint || minHeight.value != 0.0F)
    {
        YGNodeStyleSetMinHeight(node, 0.0F);
    }

    return true;
}

void ConfigureMainAxisFlex(const components::Size& sizeComp, YGNodeRef node, bool isRow, bool parentIsScrollArea)
{
    const bool widthFill = policies::HasFlag(sizeComp.sizePolicy, policies::Size::HFill);
    const bool widthFixed = policies::HasFlag(sizeComp.sizePolicy, policies::Size::HFixed);
    const bool heightFill = policies::HasFlag(sizeComp.sizePolicy, policies::Size::VFill);
    const bool heightFixed = policies::HasFlag(sizeComp.sizePolicy, policies::Size::VFixed);

    const bool mainAxisFill = (isRow && widthFill) || (!isRow && heightFill);
    if (mainAxisFill)
    {
        if (YGNodeStyleGetFlexGrow(node) != 1.0F)
        {
            YGNodeStyleSetFlexGrow(node, 1.0F);
        }
        if (YGNodeStyleGetFlexShrink(node) != 1.0F)
        {
            YGNodeStyleSetFlexShrink(node, 1.0F);
        }

        const YGValue basis = YGNodeStyleGetFlexBasis(node);
        if (basis.unit != YGUnitPoint || basis.value != 0.0F)
        {
            YGNodeStyleSetFlexBasis(node, 0.0F);
        }
        return;
    }

    if (YGNodeStyleGetFlexGrow(node) != 0.0F)
    {
        YGNodeStyleSetFlexGrow(node, 0.0F);
    }

    const bool mainAxisFixed = (isRow && widthFixed) || (!isRow && heightFixed);
    const float shrinkValue = (mainAxisFixed || parentIsScrollArea) ? 0.0F : 1.0F;
    if (YGNodeStyleGetFlexShrink(node) != shrinkValue)
    {
        YGNodeStyleSetFlexShrink(node, shrinkValue);
    }
}

void ConfigureCrossAxisStretch(const components::Size& sizeComp, YGNodeRef node, bool isRow)
{
    const bool widthFill = policies::HasFlag(sizeComp.sizePolicy, policies::Size::HFill);
    const bool heightFill = policies::HasFlag(sizeComp.sizePolicy, policies::Size::VFill);
    const bool crossAxisFill = (isRow && heightFill) || (!isRow && widthFill);
    if (crossAxisFill && YGNodeStyleGetAlignSelf(node) != YGAlignStretch)
    {
        YGNodeStyleSetAlignSelf(node, YGAlignStretch);
    }
}

void ConfigureFlexBehaviorFromSize(const components::Size& sizeComp,
                                   YGNodeRef node,
                                   bool isRow,
                                   bool parentIsScrollArea)
{
    ConfigureMainAxisFlex(sizeComp, node, isRow, parentIsScrollArea);
    ConfigureCrossAxisStretch(sizeComp, node, isRow);
}

void ConfigureExplicitWidth(const components::Size& sizeComp, YGNodeRef node)
{
    const bool widthFixed = policies::HasFlag(sizeComp.sizePolicy, policies::Size::HFixed);
    const bool widthAuto = policies::HasFlag(sizeComp.sizePolicy, policies::Size::HAuto);
    const bool widthPercent = policies::HasFlag(sizeComp.sizePolicy, policies::Size::HPercentage);

    const YGValue currentWidth = YGNodeStyleGetWidth(node);
    if (widthFixed && sizeComp.size.x() > 0.0F)
    {
        if (currentWidth.unit != YGUnitPoint || currentWidth.value != sizeComp.size.x())
        {
            YGNodeStyleSetWidth(node, sizeComp.size.x());
        }
        return;
    }

    if (widthPercent)
    {
        const float percentValue = sizeComp.percentage * 100.0F;
        if (currentWidth.unit != YGUnitPercent || currentWidth.value != percentValue)
        {
            YGNodeStyleSetWidthPercent(node, percentValue);
        }
        return;
    }

    if (widthAuto && currentWidth.unit != YGUnitAuto)
    {
        YGNodeStyleSetWidthAuto(node);
    }
}

void ConfigureExplicitHeight(const components::Size& sizeComp, YGNodeRef node)
{
    const bool heightFixed = policies::HasFlag(sizeComp.sizePolicy, policies::Size::VFixed);
    const bool heightAuto = policies::HasFlag(sizeComp.sizePolicy, policies::Size::VAuto);
    const bool heightPercent = policies::HasFlag(sizeComp.sizePolicy, policies::Size::VPercentage);

    const YGValue currentHeight = YGNodeStyleGetHeight(node);
    if (heightFixed && sizeComp.size.y() > 0.0F)
    {
        if (currentHeight.unit != YGUnitPoint || currentHeight.value != sizeComp.size.y())
        {
            YGNodeStyleSetHeight(node, sizeComp.size.y());
        }
        return;
    }

    if (heightPercent)
    {
        const float percentValue = sizeComp.percentage * 100.0F;
        if (currentHeight.unit != YGUnitPercent || currentHeight.value != percentValue)
        {
            YGNodeStyleSetHeightPercent(node, percentValue);
        }
        return;
    }

    if (!heightAuto)
    {
        return;
    }

    if (sizeComp.size.y() > 0.0F)
    {
        if (currentHeight.unit != YGUnitPoint || currentHeight.value != sizeComp.size.y())
        {
            YGNodeStyleSetHeight(node, sizeComp.size.y());
        }
        return;
    }

    if (currentHeight.unit != YGUnitAuto)
    {
        YGNodeStyleSetHeightAuto(node);
    }
}

void ConfigureExplicitSize(const components::Size& sizeComp, YGNodeRef node)
{
    ConfigureExplicitWidth(sizeComp, node);
    ConfigureExplicitHeight(sizeComp, node);
}

void ConfigureMinMaxSize(const components::Size& sizeComp, YGNodeRef node)
{
    auto setMinOrMax = [node](auto getter, auto setter, float value, bool isMax)
    {
        const YGValue current = getter(node);
        if (isMax)
        {
            if (value < FLT_MAX && (current.unit != YGUnitPoint || current.value != value))
            {
                setter(node, value);
            }
            return;
        }

        if (value > 0.0F && (current.unit != YGUnitPoint || current.value != value))
        {
            setter(node, value);
        }
    };

    setMinOrMax(YGNodeStyleGetMinWidth, YGNodeStyleSetMinWidth, sizeComp.minSize.x(), false);
    setMinOrMax(YGNodeStyleGetMinHeight, YGNodeStyleSetMinHeight, sizeComp.minSize.y(), false);
    setMinOrMax(YGNodeStyleGetMaxWidth, YGNodeStyleSetMaxWidth, sizeComp.maxSize.x(), true);
    setMinOrMax(YGNodeStyleGetMaxHeight, YGNodeStyleSetMaxHeight, sizeComp.maxSize.y(), true);
}

void ConfigureSizeNode(entt::entity entity, YGNodeRef node)
{
    auto* sizeComp = Registry::TryGet<components::Size>(entity);
    if (sizeComp == nullptr)
    {
        return;
    }

    policies::LayoutDirection parentDirection = policies::LayoutDirection::VERTICAL;
    bool parentIsScrollArea = false;
    if (const auto* hierarchy = Registry::TryGet<components::Hierarchy>(entity))
    {
        if (hierarchy->parent != entt::null)
        {
            if (const auto* parentLayout = Registry::TryGet<components::LayoutInfo>(hierarchy->parent))
            {
                parentDirection = parentLayout->direction;
            }
            parentIsScrollArea = Registry::AnyOf<components::ScrollArea>(hierarchy->parent);
        }
    }

    const bool isRow = (parentDirection == policies::LayoutDirection::HORIZONTAL);
    ConfigureFlexBehaviorFromSize(*sizeComp, node, isRow, parentIsScrollArea);
    ConfigureExplicitSize(*sizeComp, node);
    ConfigureMinMaxSize(*sizeComp, node);
}

void ConfigureAbsolutePosition(entt::entity entity, YGNodeRef node)
{
    auto* positionComp = Registry::TryGet<components::Position>(entity);
    if (positionComp == nullptr)
    {
        return;
    }

    const bool horizontalAbsolute =
        policies::HasFlag(positionComp->positionPolicy, policies::Position::HAbsolute);
    const bool verticalAbsolute = policies::HasFlag(positionComp->positionPolicy, policies::Position::VAbsolute);
    if (!horizontalAbsolute && !verticalAbsolute)
    {
        return;
    }

    if (YGNodeStyleGetPositionType(node) != YGPositionTypeAbsolute)
    {
        YGNodeStyleSetPositionType(node, YGPositionTypeAbsolute);
    }

    if (horizontalAbsolute)
    {
        const YGValue currentLeft = YGNodeStyleGetPosition(node, YGEdgeLeft);
        if (currentLeft.unit != YGUnitPoint || currentLeft.value != positionComp->value.x())
        {
            YGNodeStyleSetPosition(node, YGEdgeLeft, positionComp->value.x());
        }
    }

    if (verticalAbsolute)
    {
        const YGValue currentTop = YGNodeStyleGetPosition(node, YGEdgeTop);
        if (currentTop.unit != YGUnitPoint || currentTop.value != positionComp->value.y())
        {
            YGNodeStyleSetPosition(node, YGEdgeTop, positionComp->value.y());
        }
    }
}

[[nodiscard]] YGJustify ResolveJustifyContent(policies::Alignment alignment, bool isRow)
{
    auto has = [&](policies::Alignment flag) { return policies::HasFlag(alignment, flag); };

    if (isRow)
    {
        if (has(policies::Alignment::HCENTER))
        {
            return YGJustifyCenter;
        }
        if (has(policies::Alignment::RIGHT))
        {
            return YGJustifyFlexEnd;
        }
        if (has(policies::Alignment::LEFT))
        {
            return YGJustifyFlexStart;
        }
        return YGJustifyCenter;
    }

    if (has(policies::Alignment::VCENTER))
    {
        return YGJustifyCenter;
    }
    if (has(policies::Alignment::BOTTOM))
    {
        return YGJustifyFlexEnd;
    }
    if (has(policies::Alignment::TOP))
    {
        return YGJustifyFlexStart;
    }
    return YGJustifyCenter;
}

[[nodiscard]] YGAlign ResolveAlignItems(policies::Alignment alignment, bool isRow)
{
    auto has = [&](policies::Alignment flag) { return policies::HasFlag(alignment, flag); };

    if (isRow)
    {
        if (has(policies::Alignment::VCENTER))
        {
            return YGAlignCenter;
        }
        if (has(policies::Alignment::BOTTOM))
        {
            return YGAlignFlexEnd;
        }
        if (has(policies::Alignment::TOP))
        {
            return YGAlignFlexStart;
        }
        return YGAlignCenter;
    }

    if (has(policies::Alignment::HCENTER))
    {
        return YGAlignCenter;
    }
    if (has(policies::Alignment::RIGHT))
    {
        return YGAlignFlexEnd;
    }
    if (has(policies::Alignment::LEFT))
    {
        return YGAlignFlexStart;
    }
    return YGAlignCenter;
}

void ConfigureOverflow(entt::entity entity, YGNodeRef node)
{
    const YGOverflow targetOverflow =
        Registry::AnyOf<components::ScrollArea>(entity) ? YGOverflowScroll : YGOverflowHidden;
    if (YGNodeStyleGetOverflow(node) != targetOverflow)
    {
        YGNodeStyleSetOverflow(node, targetOverflow);
    }
}

void ConfigureContainerAlignmentAndOverflow(entt::entity entity, YGNodeRef node)
{
    const auto* layoutInfo = Registry::TryGet<components::LayoutInfo>(entity);
    if (layoutInfo == nullptr)
    {
        return;
    }

    const bool isRow = (layoutInfo->direction == policies::LayoutDirection::HORIZONTAL);
    const policies::Alignment alignment = ResolveContainerAlignment(entity, *layoutInfo);
    const YGJustify justify = ResolveJustifyContent(alignment, isRow);
    const YGAlign alignItems = ResolveAlignItems(alignment, isRow);

    if (YGNodeStyleGetJustifyContent(node) != justify)
    {
        YGNodeStyleSetJustifyContent(node, justify);
    }

    if (YGNodeStyleGetAlignItems(node) != alignItems)
    {
        YGNodeStyleSetAlignItems(node, alignItems);
    }

    ConfigureOverflow(entity, node);
}

void ConfigureLeafAutoSize(entt::entity entity, YGNodeRef node)
{
    if (Registry::AnyOf<components::LayoutInfo>(entity))
    {
        return;
    }

    const auto* sizeComp = Registry::TryGet<components::Size>(entity);
    if (sizeComp == nullptr)
    {
        return;
    }

    float defaultWidth = DEFAULT_LEAF_WIDTH;
    const float defaultHeight = DEFAULT_LEAF_HEIGHT;

    if (const auto* text = Registry::TryGet<components::Text>(entity))
    {
        if (!text->content.empty())
        {
            defaultWidth = (static_cast<float>(text->content.length()) * 8.0F) + 10.0F;
        }
    }

    if (policies::HasFlag(sizeComp->sizePolicy, policies::Size::HAuto))
    {
        const YGValue currentMinWidth = YGNodeStyleGetMinWidth(node);
        if (currentMinWidth.unit != YGUnitPoint || currentMinWidth.value != defaultWidth)
        {
            YGNodeStyleSetMinWidth(node, defaultWidth);
        }
    }

    if (policies::HasFlag(sizeComp->sizePolicy, policies::Size::VAuto))
    {
        const YGValue currentMinHeight = YGNodeStyleGetMinHeight(node);
        if (currentMinHeight.unit != YGUnitPoint || currentMinHeight.value != defaultHeight)
        {
            YGNodeStyleSetMinHeight(node, defaultHeight);
        }
    }
}

[[nodiscard]] float NormalizeLayoutValue(float value)
{
    return std::isnan(value) ? 0.0F : value;
}

[[nodiscard]] std::vector<float> ComputeTableColWidths(const components::TableInfo& info, float totalWidth)
{
    const int columnCount = info.columnCount;
    std::vector<float> widths(static_cast<size_t>(columnCount));

    if (!info.columnWidths.empty() && static_cast<int>(info.columnWidths.size()) == columnCount)
    {
        for (int col = 0; col < columnCount; ++col)
        {
            widths[static_cast<size_t>(col)] = info.columnWidths[static_cast<size_t>(col)];
        }
        return widths;
    }

    const float fallbackWidth =
        (columnCount > 0) ? (totalWidth / static_cast<float>(columnCount)) : totalWidth;
    for (int col = 0; col < columnCount; ++col)
    {
        widths[static_cast<size_t>(col)] = fallbackWidth;
    }
    return widths;
}

void UpdateTableCellEntityLayouts(entt::entity tableEntity, YGNodeRef tableNode)
{
    const auto* info = Registry::TryGet<components::TableInfo>(tableEntity);
    if (info == nullptr || info->columnCount <= 0)
    {
        return;
    }

    float totalWidth = NormalizeLayoutValue(YGNodeLayoutGetWidth(tableNode));
    if (const auto* sizeComp = Registry::TryGet<components::Size>(tableEntity))
    {
        totalWidth = sizeComp->size.x();
    }

    const std::vector<float> colWidths = ComputeTableColWidths(*info, totalWidth);
    const int rowCount = static_cast<int>(info->cells.size());
    for (int row = 0; row < rowCount; ++row)
    {
        const float cellY = info->headerHeight + (static_cast<float>(row) * info->rowHeight);
        float cellX = 0.0F;
        for (int col = 0; col < info->columnCount; ++col)
        {
            const float colWidth = colWidths[static_cast<size_t>(col)];
            const auto& cell = info->cells[static_cast<size_t>(row)][static_cast<size_t>(col)];
            if (cell.cellEntity != entt::null && Registry::Valid(cell.cellEntity))
            {
                if (auto* pos = Registry::TryGet<components::Position>(cell.cellEntity))
                {
                    pos->value.x() = cellX;
                    pos->value.y() = cellY;
                }
                if (auto* sizeComp = Registry::TryGet<components::Size>(cell.cellEntity))
                {
                    sizeComp->size.x() = colWidth;
                    sizeComp->size.y() = info->rowHeight;
                }
            }
            cellX += colWidth;
        }
    }
}

void UpdateEntityLayoutFromYoga(entt::entity entity, YGNodeRef node)
{
    bool isDirty = false;

    const float left = NormalizeLayoutValue(YGNodeLayoutGetLeft(node));
    const float top = NormalizeLayoutValue(YGNodeLayoutGetTop(node));
    const float width = YGNodeLayoutGetWidth(node);
    const float height = YGNodeLayoutGetHeight(node);

    if (auto* positionComp = Registry::TryGet<components::Position>(entity); positionComp != nullptr)
    {
        if (positionComp->value.x() != left || positionComp->value.y() != top)
        {
            positionComp->value.x() = left;
            positionComp->value.y() = top;
            isDirty = true;
        }
    }

    if (auto* sizeComp = Registry::TryGet<components::Size>(entity); sizeComp != nullptr)
    {
        const bool widthChanged = (!std::isnan(width) && width > 0.0F && sizeComp->size.x() != width);
        const bool heightChanged = (!std::isnan(height) && height > 0.0F && sizeComp->size.y() != height);

        if (widthChanged || heightChanged)
        {
            if (widthChanged)
            {
                sizeComp->size.x() = width;
            }
            if (heightChanged)
            {
                sizeComp->size.y() = height;
            }
            isDirty = true;
        }
    }

    if (isDirty)
    {
        utils::MarkRenderDirty(entity);
    }
}

void UpdateScrollAreaContentSize(entt::entity entity, YGNodeRef node)
{
    auto* scrollArea = Registry::TryGet<components::ScrollArea>(entity);
    if (scrollArea == nullptr || Registry::AnyOf<components::TextEditTag>(entity))
    {
        return;
    }

    const auto* hierarchy = Registry::TryGet<components::Hierarchy>(entity);
    if (hierarchy == nullptr)
    {
        return;
    }

    const std::uint32_t childCount = YGNodeGetChildCount(node);
    if (childCount == 0U)
    {
        return;
    }

    std::uint32_t yogaChildIndex = 0U;
    float maxContentRight = 0.0F;
    float maxContentBottom = 0.0F;

    for (entt::entity child : hierarchy->children)
    {
        if (!IsLayoutParticipant(child))
        {
            continue;
        }

        if (yogaChildIndex >= childCount)
        {
            break;
        }

        YGNodeRef childNode = YGNodeGetChild(node, yogaChildIndex);
        const float childLeft = NormalizeLayoutValue(YGNodeLayoutGetLeft(childNode));
        const float childTop = NormalizeLayoutValue(YGNodeLayoutGetTop(childNode));
        const float childWidth = NormalizeLayoutValue(YGNodeLayoutGetWidth(childNode));
        const float childHeight = NormalizeLayoutValue(YGNodeLayoutGetHeight(childNode));

        maxContentRight = std::max(maxContentRight, childLeft + childWidth);
        maxContentBottom = std::max(maxContentBottom, childTop + childHeight);
        ++yogaChildIndex;
    }

    float paddingRight = 0.0F;
    float paddingBottom = 0.0F;
    if (const auto* padding = Registry::TryGet<components::Padding>(entity); padding != nullptr)
    {
        paddingRight = padding->values.y();
        paddingBottom = padding->values.z();
    }

    const float newContentWidth = maxContentRight + paddingRight;
    const float newContentHeight = maxContentBottom + paddingBottom;

    if (scrollArea->contentSize.x() != newContentWidth || scrollArea->contentSize.y() != newContentHeight)
    {
        scrollArea->contentSize.x() = newContentWidth;
        scrollArea->contentSize.y() = newContentHeight;
        utils::MarkRenderDirty(entity);
    }
}

struct LayoutTraversalFrame
{
    entt::entity entity = entt::null;
    YGNodeRef node = nullptr;
    bool postVisit = false;
};

} // namespace

LayoutSystem::LayoutSystem()
    : m_yogaConfig(YGConfigNew())
{
    Logger::info("[LayoutSystem] Yoga 配置创建完成");
}

LayoutSystem::~LayoutSystem()
{
    clearYogaNodes();
    if (m_yogaConfig != nullptr)
    {
        YGConfigFree(m_yogaConfig);
        m_yogaConfig = nullptr;
    }
}

LayoutSystem::LayoutSystem(LayoutSystem&& other) noexcept
    : m_yogaConfig(other.m_yogaConfig),
      m_entityToNode(std::move(other.m_entityToNode))
{
    other.m_yogaConfig = nullptr;
}

LayoutSystem& LayoutSystem::operator=(LayoutSystem&& other) noexcept
{
    if (this != &other)
    {
        clearYogaNodes();
        if (m_yogaConfig != nullptr)
        {
            YGConfigFree(m_yogaConfig);
        }

        m_yogaConfig = other.m_yogaConfig;
        m_entityToNode = std::move(other.m_entityToNode);
        other.m_yogaConfig = nullptr;
    }
    return *this;
}

void LayoutSystem::registerHandlersImpl()
{
    Dispatcher::Sink<events::UpdateLayout>().connect<&LayoutSystem::update>(*this);
}

void LayoutSystem::unregisterHandlersImpl()
{
    Dispatcher::Sink<events::UpdateLayout>().disconnect<&LayoutSystem::update>(*this);
}

void LayoutSystem::update() noexcept
{
    cleanupInvalidNodes();

    std::unordered_set<entt::entity> dirtyRoots;
    auto dirtyView = Registry::View<components::LayoutDirtyTag>();
    if (!dirtyView.empty())
    {
        for (entt::entity entity : dirtyView)
        {
            if (!Registry::Valid(entity))
            {
                continue;
            }

            syncNodeRecursive(entity);

            const entt::entity root = findRoot(entity);
            if (Registry::Valid(root))
            {
                dirtyRoots.insert(root);
            }
        }
        Registry::Clear<components::LayoutDirtyTag>();
    }

    if (dirtyRoots.empty())
    {
        return;
    }

    for (entt::entity root : dirtyRoots)
    {
        if (!Registry::AllOf<components::Hierarchy, components::Position, components::Size, components::RootTag>(root))
        {
            continue;
        }

        if (!m_entityToNode.contains(root))
        {
            syncNodeRecursive(root);
        }

        YGNodeRef rootNode = getOrCreateNode(root);
        if (rootNode == nullptr)
        {
            continue;
        }

        float rootWidth = YGUndefined;
        float rootHeight = YGUndefined;
        if (auto* sizeComp = Registry::TryGet<components::Size>(root); sizeComp != nullptr)
        {
            rootWidth = sizeComp->size.x();
            rootHeight = sizeComp->size.y();
        }

        YGNodeCalculateLayout(rootNode, rootWidth, rootHeight, YGDirectionLTR);
        applyYogaLayout(root, rootNode);
        applyWindowCentering(root, rootWidth, rootHeight);
    }
}

entt::entity LayoutSystem::findRoot(entt::entity entity) const
{
    entt::entity current = entity;
    int safetyCounter = 0;

    while (Registry::Valid(current) && safetyCounter++ < MAX_HIERARCHY_DEPTH)
    {
        if (Registry::AnyOf<components::RootTag>(current))
        {
            return current;
        }

        const auto* hierarchy = Registry::TryGet<components::Hierarchy>(current);
        if (hierarchy == nullptr || hierarchy->parent == entt::null)
        {
            break;
        }

        current = hierarchy->parent;
    }

    return entt::null;
}

void LayoutSystem::clearYogaNodes()
{
    for (auto& [entity, node] : m_entityToNode)
    {
        (void)entity;
        if (node != nullptr)
        {
            YGNodeFree(node);
        }
    }
    m_entityToNode.clear();
}

void LayoutSystem::cleanupInvalidNodes()
{
    auto nodeIterator = m_entityToNode.begin();
    while (nodeIterator != m_entityToNode.end())
    {
        if (!Registry::Valid(nodeIterator->first))
        {
            if (nodeIterator->second != nullptr)
            {
                YGNodeFree(nodeIterator->second);
            }
            nodeIterator = m_entityToNode.erase(nodeIterator);
            continue;
        }

        ++nodeIterator;
    }
}

YGNodeRef LayoutSystem::createYogaNode() const
{
    return YGNodeNewWithConfig(m_yogaConfig);
}

YGNodeRef LayoutSystem::getOrCreateNode(entt::entity entity)
{
    const auto nodeIterator = m_entityToNode.find(entity);
    if (nodeIterator != m_entityToNode.end())
    {
        return nodeIterator->second;
    }

    YGNodeRef node = createYogaNode();
    m_entityToNode[entity] = node;
    configureYogaNode(entity, node);
    return node;
}

void LayoutSystem::syncNodeRecursive(entt::entity entity)
{
    std::vector<entt::entity> pendingEntities{entity};
    while (!pendingEntities.empty())
    {
        const entt::entity current = pendingEntities.back();
        pendingEntities.pop_back();

        if (!Registry::Valid(current))
        {
            continue;
        }

        YGNodeRef node = getOrCreateNode(current);
        configureYogaNode(current, node);
        syncChildren(current, node);

        const auto* hierarchy = Registry::TryGet<components::Hierarchy>(current);
        if (hierarchy == nullptr || hierarchy->children.empty())
        {
            continue;
        }

        for (auto childIt = hierarchy->children.rbegin(); childIt != hierarchy->children.rend(); ++childIt)
        {
            if (Registry::Valid(*childIt) && IsLayoutParticipant(*childIt))
            {
                pendingEntities.push_back(*childIt);
            }
        }
    }
}

void LayoutSystem::syncChildren(entt::entity entity, YGNodeRef node)
{
    const auto* hierarchy = Registry::TryGet<components::Hierarchy>(entity);

    std::vector<YGNodeRef> expectedChildren;
    if (hierarchy != nullptr && !hierarchy->children.empty())
    {
        expectedChildren.reserve(hierarchy->children.size());
        for (entt::entity child : hierarchy->children)
        {
            if (!IsLayoutParticipant(child))
            {
                continue;
            }

            YGNodeRef childNode = getOrCreateNode(child);
            if (childNode != nullptr)
            {
                expectedChildren.push_back(childNode);
            }
        }
    }

    const std::uint32_t currentCount = YGNodeGetChildCount(node);
    const bool sameCount = currentCount == static_cast<std::uint32_t>(expectedChildren.size());
    bool isStructureMatch = sameCount;
    if (isStructureMatch)
    {
        for (std::uint32_t index = 0U; index < currentCount; ++index)
        {
            if (YGNodeGetChild(node, index) != expectedChildren[index])
            {
                isStructureMatch = false;
                break;
            }
        }
    }

    if (isStructureMatch)
    {
        return;
    }

    YGNodeRemoveAllChildren(node);
    std::uint32_t insertIndex = 0U;
    for (YGNodeRef childNode : expectedChildren)
    {
        YGNodeRef ownerNode = YGNodeGetOwner(childNode);
        DetachFromOwnerIfNeeded(ownerNode, childNode, node);
        YGNodeInsertChild(node, childNode, insertIndex);
        ++insertIndex;
    }
}

void LayoutSystem::configureYogaNode(entt::entity entity, YGNodeRef node)
{
    ConfigureLayoutDirectionAndGap(entity, node);
    ConfigurePadding(entity, node);
    if (ConfigureSpacerNode(entity, node))
    {
        return;
    }

    ConfigureSizeNode(entity, node);
    ConfigureAbsolutePosition(entity, node);
    ConfigureContainerAlignmentAndOverflow(entity, node);
    ConfigureLeafAutoSize(entity, node);
}

void LayoutSystem::applyYogaLayout(entt::entity entity, YGNodeRef node)
{
    if (node == nullptr)
    {
        return;
    }

    std::vector<LayoutTraversalFrame> traversalStack;
    traversalStack.push_back({entity, node, false});

    while (!traversalStack.empty())
    {
        const LayoutTraversalFrame frame = traversalStack.back();
        traversalStack.pop_back();

        if (frame.node == nullptr)
        {
            continue;
        }

        if (frame.postVisit)
        {
            UpdateScrollAreaContentSize(frame.entity, frame.node);
            continue;
        }

        UpdateEntityLayoutFromYoga(frame.entity, frame.node);
        if (Registry::AnyOf<components::TableTag>(frame.entity))
        {
            UpdateTableCellEntityLayouts(frame.entity, frame.node);
        }
        traversalStack.push_back({frame.entity, frame.node, true});

        const auto* hierarchy = Registry::TryGet<components::Hierarchy>(frame.entity);
        if (hierarchy == nullptr || hierarchy->children.empty())
        {
            continue;
        }

        std::vector<LayoutTraversalFrame> childFrames;
        childFrames.reserve(hierarchy->children.size());

        std::uint32_t yogaChildIndex = 0U;
        const std::uint32_t childCount = YGNodeGetChildCount(frame.node);
        for (entt::entity child : hierarchy->children)
        {
            if (!IsLayoutParticipant(child) || yogaChildIndex >= childCount)
            {
                continue;
            }

            childFrames.push_back({child, YGNodeGetChild(frame.node, yogaChildIndex), false});
            ++yogaChildIndex;
        }

        for (auto childIt = childFrames.rbegin(); childIt != childFrames.rend(); ++childIt)
        {
            traversalStack.push_back(*childIt);
        }
    }
}

void LayoutSystem::applyWindowCentering(entt::entity root, float screenWidth, float screenHeight)
{
    auto* positionComp = Registry::TryGet<components::Position>(root);
    auto* sizeComp = Registry::TryGet<components::Size>(root);
    if (positionComp == nullptr || sizeComp == nullptr || sizeComp->size.x() <= 0.0F || sizeComp->size.y() <= 0.0F)
    {
        return;
    }

    const bool horizontalFixed = policies::HasFlag(positionComp->positionPolicy, policies::Position::HFixed);
    const bool verticalFixed = policies::HasFlag(positionComp->positionPolicy, policies::Position::VFixed);
    bool centerH = policies::HasFlag(positionComp->positionPolicy, policies::Position::HCenter);
    bool centerV = policies::HasFlag(positionComp->positionPolicy, policies::Position::VCenter);
    const bool isDefault = (positionComp->positionPolicy == policies::Position::Default);
    const bool implicitCenter = (positionComp->value.x() == 0.0F && positionComp->value.y() == 0.0F);

    if (horizontalFixed)
    {
        centerH = false;
    }
    if (verticalFixed)
    {
        centerV = false;
    }

    const bool shouldApplyFallbackCenter = !isDefault && !centerH && !centerV && implicitCenter;
    if ((isDefault || shouldApplyFallbackCenter) && !horizontalFixed)
    {
        centerH = true;
    }
    if ((isDefault || shouldApplyFallbackCenter) && !verticalFixed)
    {
        centerV = true;
    }

    if (centerH)
    {
        positionComp->value.x() = (screenWidth - sizeComp->size.x()) / 2.0F;
    }
    if (centerV)
    {
        positionComp->value.y() = (screenHeight - sizeComp->size.y()) / 2.0F;
    }
}

} // namespace ui::systems