/**
 * ************************************************************************
 *
 * @file LayoutSystem.h
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2025-12-11 (Finalized)
 * @version 0.4
 * @brief UI布局系统 - 基于 Yoga (Flexbox) 引擎
 *
 * 使用 Facebook Yoga 库实现 Flexbox 布局算法。
 * 负责计算和设置所有UI实体的位置 (Position) 和尺寸 (Size) 组件。
    - 将 ECS 实体树映射为 Yoga 节点树。
    - 根据 Size 和 LayoutInfo 组件设置 Yoga 节点属性。
    - 处理自动居中和填充等布局需求。
    - 计算完成后将结果回写到 Position 和 Size 组件。
    - 支持脏化标记，优化布局计算频率。
    - 对渲染系统无感知，纯粹的布局计算。
 *
 * ************************************************************************
 * @copyright Copyright (c) 2025 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */

#pragma once
#include <entt/entt.hpp>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <cmath>

// Yoga 布局引擎
#include <yoga/Yoga.h>

#include "common/Components.hpp"
#include "common/Tags.hpp"
#include "common/Policies.hpp"
#include "common/Events.hpp"
#include "interface/Isystem.hpp"
#include "singleton/Registry.hpp"
#include "singleton/Dispatcher.hpp" // 包含 Dispatcher
#include "singleton/Logger.hpp"
#include "traits/ComponentsTraits.hpp"
#include "traits/PoliciesTraits.hpp"
#include "api/Utils.hpp"

namespace ui::systems
{

/**
 * @brief 基于 Yoga Flexbox 的布局系统
 *
 * 将 ECS 实体树映射到 Yoga 节点树，计算布局后回写到 Position/Size 组件。
 */
class LayoutSystem : public ui::interface::EnableRegister<LayoutSystem>
{
public:
    LayoutSystem() : m_yogaConfig(YGConfigNew()) { Logger::info("[LayoutSystem] Yoga 配置创建完成"); }

    ~LayoutSystem()
    {
        clearYogaNodes();
        if (m_yogaConfig != nullptr)
        {
            YGConfigFree(m_yogaConfig);
            m_yogaConfig = nullptr;
        }
    }

    // 禁用拷贝
    LayoutSystem(const LayoutSystem&) = delete;
    LayoutSystem& operator=(const LayoutSystem&) = delete;

    // 自定义移动构造：转移资源所有权
    LayoutSystem(LayoutSystem&& other) noexcept
        : m_yogaConfig(other.m_yogaConfig), m_entityToNode(std::move(other.m_entityToNode))
    {
        other.m_yogaConfig = nullptr; // 防止源对象析构时释放资源
    }

    // 自定义移动赋值：转移资源所有权
    LayoutSystem& operator=(LayoutSystem&& other) noexcept
    {
        if (this != &other)
        {
            // 释放当前资源
            clearYogaNodes();
            if (m_yogaConfig != nullptr)
            {
                YGConfigFree(m_yogaConfig);
            }

            // 转移资源
            m_yogaConfig = other.m_yogaConfig;
            m_entityToNode = std::move(other.m_entityToNode);

            // 清空源对象
            other.m_yogaConfig = nullptr;
        }
        return *this;
    }

    void registerHandlersImpl() { Dispatcher::Sink<events::UpdateLayout>().connect<&LayoutSystem::update>(*this); }

    void unregisterHandlersImpl() { Dispatcher::Sink<events::UpdateLayout>().disconnect<&LayoutSystem::update>(*this); }

    /**
     * @brief 每帧更新布局
     */
    void update() noexcept
    {
        // 1. 清理无效实体的节点
        cleanupInvalidNodes();

        std::unordered_set<entt::entity> dirtyRoots;

        // 2. 处理脏节点：同步 ECS 数据到 Yoga 节点
        // 只重建标记为 LayoutDirty 的子树
        auto dirtyView = Registry::View<components::LayoutDirtyTag>();
        if (!dirtyView.empty())
        {
            for (auto entity : dirtyView)
            {
                // 确保实体有效
                if (Registry::Valid(entity))
                {
                    syncNodeRecursive(entity);

                    // 追踪受影响的根节点
                    if (entt::entity root = findRoot(entity); Registry::Valid(root))
                    {
                        dirtyRoots.insert(root);
                    }
                }
            }
            // 清除脏标记
            Registry::Clear<components::LayoutDirtyTag>();
        }

        // 3. 布局计算 (仅针对包含脏节点的根节点)
        // 只有当根节点本身或其子节点发生变化时才重新计算
        if (dirtyRoots.empty()) return;

        bool needRetryForCentering = false;

        for (auto root : dirtyRoots)
        {
            // 必须是有效的根节点组件组合
            if (!Registry::AllOf<components::Hierarchy, components::Position, components::Size, components::RootTag>(
                    root))
            {
                continue;
            }

            // 获取或创建根节点 (使用缓存)
            YGNodeRef rootNode = getOrCreateNode(root);
            if (rootNode == nullptr) continue;

            // 4. 获取根容器尺寸（用于布局计算）
            float rootWidth = YGUndefined;
            float rootHeight = YGUndefined;
            auto sizeComp = Registry::TryGet<components::Size>(root);
            if (sizeComp)
            {
                rootWidth = sizeComp->size.x();
                rootHeight = sizeComp->size.y();
            }

            // 5. 计算布局
            YGNodeCalculateLayout(rootNode, rootWidth, rootHeight, YGDirectionLTR);

            // 6. 回写布局结果到 ECS
            applyYogaLayout(root, rootNode, 0.0F, 0.0F);

            applyWindowCentering(root, rootWidth, rootHeight);
        }
    }

private:
    YGConfigRef m_yogaConfig = nullptr;
    std::unordered_map<entt::entity, YGNodeRef> m_entityToNode;

    /**
     * @brief 查找实体所属的 UI 树根节点 (带有 RootTag)
     */
    entt::entity findRoot(entt::entity entity) const
    {
        entt::entity current = entity;
        // 防止无限循环 (虽然 tree 结构不应该有环)
        int safetyCounter = 0;
        constexpr int MAX_DEPTH = 1000;

        while (Registry::Valid(current) && safetyCounter++ < MAX_DEPTH)
        {
            if (Registry::AnyOf<components::RootTag>(current))
            {
                return current;
            }

            const auto* hierarchy = Registry::TryGet<components::Hierarchy>(current);
            if (hierarchy != nullptr && hierarchy->parent != entt::null)
            {
                current = hierarchy->parent;
            }
            else
            {
                break;
            }
        }
        return entt::null;
    }

    // ===================== Yoga 节点管理 =====================

    void clearYogaNodes()
    {
        // 释放所有节点
        for (auto& [entity, node] : m_entityToNode)
        {
            if (node != nullptr)
            {
                YGNodeFree(node);
            }
        }
        m_entityToNode.clear();
    }

    void cleanupInvalidNodes()
    {
        auto iterator = m_entityToNode.begin();
        while (iterator != m_entityToNode.end())
        {
            if (!Registry::Valid(iterator->first))
            {
                if (iterator->second)
                {
                    YGNodeFree(iterator->second);
                }
                iterator = m_entityToNode.erase(iterator);
            }
            else
            {
                ++iterator;
            }
        }
    }

    YGNodeRef createYogaNode() { return YGNodeNewWithConfig(m_yogaConfig); }

    YGNodeRef getOrCreateNode(entt::entity entity)
    {
        auto it = m_entityToNode.find(entity);
        if (it != m_entityToNode.end())
        {
            return it->second;
        }

        YGNodeRef node = createYogaNode();
        m_entityToNode[entity] = node;

        // 新节点初始化配置
        configureYogaNode(entity, node);

        // 为安全起见，新节点我们默认其子结构需要同步
        syncChildren(entity, node);

        return node;
    }

    /**
     * @brief 同步节点及其子树配置
     * 对应 rebuild marked dirty subtree
     */
    void syncNodeRecursive(entt::entity entity)
    {
        YGNodeRef node = getOrCreateNode(entity);

        // 更新样式配置
        configureYogaNode(entity, node);

        // 同步子节点结构
        syncChildren(entity, node);
    }

    /**
     * @brief 同步子节点列表
     */
    void syncChildren(entt::entity entity, YGNodeRef node)
    {
        const auto* hierarchy = Registry::TryGet<components::Hierarchy>(entity);

        // 1. 收集预期的有序子节点列表
        std::vector<YGNodeRef> expectedChildren;
        if (hierarchy != nullptr && !hierarchy->children.empty())
        {
            expectedChildren.reserve(hierarchy->children.size());

            for (entt::entity child : hierarchy->children)
            {
                // Spacer 没有 Size 组件，但仍需参与布局
                const bool isSpacer = Registry::AnyOf<components::SpacerTag>(child);
                const bool hasLayoutComponents = Registry::AllOf<components::Position, components::Size>(child);

                if (!isSpacer && !hasLayoutComponents) continue;

                // 获取或创建子节点
                YGNodeRef childNode = getOrCreateNode(child);

                if (childNode != nullptr)
                {
                    expectedChildren.push_back(childNode);
                }
            }
        }

        // 2. 差量检测：对比现有 Yoga 子节点与预期是否一致
        const uint32_t currentCount = YGNodeGetChildCount(node);
        bool isStructureMatch = (currentCount == expectedChildren.size());

        if (isStructureMatch)
        {
            for (uint32_t i = 0; i < currentCount; ++i)
            {
                if (YGNodeGetChild(node, i) != expectedChildren[i])
                {
                    isStructureMatch = false;
                    break;
                }
            }
        }

        // 3. 如果结构一致，直接跳过 (优化点)
        if (isStructureMatch) return;

        // 4. 重建子节点关联
        // 简单处理：全部移除再重新添加，确保顺序正确
        // 由于节点是缓存的，YGNodeRemoveAllChildren 只是断开连接，不会释放子节点内存
        YGNodeRemoveAllChildren(node);

        for (uint32_t i = 0; i < expectedChildren.size(); ++i)
        {
            YGNodeRef childNode = expectedChildren[i];

            // 确保子节点没有挂在其他父节点下 (处理 Reparent 情况)
            YGNodeRef oldParent = YGNodeGetOwner(childNode);
            if (oldParent != nullptr && oldParent != node)
            {
                YGNodeRemoveChild(oldParent, childNode);
            }

            YGNodeInsertChild(node, childNode, i);
        }
    }

    /**
     * @brief 递归构建 Yoga 节点树 (已废弃，由 getOrCreateNode 和 syncNodeRecursive 替代)
     * 保留此函数名只是为了兼容旧代码引用（如果还有），但实际上用不上
     */
    YGNodeRef buildYogaTree(entt::entity entity) { return getOrCreateNode(entity); }

    [[nodiscard]] static policies::Alignment resolveContainerAlignment(entt::entity entity,
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

    [[nodiscard]] static Vec4 resolveEffectivePadding(entt::entity entity)
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

        constexpr float SCROLLBAR_GUTTER = 14.0F;

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

    static void setPaddingIfChanged(YGNodeRef node, YGEdge edge, float value)
    {
        YGValue current = YGNodeStyleGetPadding(node, edge);
        if (current.unit != YGUnitPoint || current.value != value)
        {
            YGNodeStyleSetPadding(node, edge, value);
        }
    }

    static void configureLayoutDirectionAndGap(entt::entity entity, YGNodeRef node)
    {
        const auto* layoutInfo = Registry::TryGet<components::LayoutInfo>(entity);
        if (layoutInfo == nullptr)
        {
            return;
        }

        YGFlexDirection targetDirection =
            (layoutInfo->direction == policies::LayoutDirection::VERTICAL) ? YGFlexDirectionColumn : YGFlexDirectionRow;
        if (YGNodeStyleGetFlexDirection(node) != targetDirection)
        {
            YGNodeStyleSetFlexDirection(node, targetDirection);
        }

        YGValue currentGap = YGNodeStyleGetGap(node, YGGutterAll);
        if (currentGap.unit != YGUnitPoint || currentGap.value != layoutInfo->spacing)
        {
            YGNodeStyleSetGap(node, YGGutterAll, layoutInfo->spacing);
        }
    }

    static void configurePadding(entt::entity entity, YGNodeRef node)
    {
        const Vec4 effectivePadding = resolveEffectivePadding(entity);
        setPaddingIfChanged(node, YGEdgeTop, effectivePadding.x());
        setPaddingIfChanged(node, YGEdgeRight, effectivePadding.y());
        setPaddingIfChanged(node, YGEdgeBottom, effectivePadding.z());
        setPaddingIfChanged(node, YGEdgeLeft, effectivePadding.w());
    }

    static bool configureSpacerNode(entt::entity entity, YGNodeRef node)
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

        YGValue basis = YGNodeStyleGetFlexBasis(node);
        if (basis.unit != YGUnitPoint || basis.value != 0.0F)
        {
            YGNodeStyleSetFlexBasis(node, 0.0F);
        }

        YGValue minWidth = YGNodeStyleGetMinWidth(node);
        if (minWidth.unit != YGUnitPoint || minWidth.value != 0.0F)
        {
            YGNodeStyleSetMinWidth(node, 0.0F);
        }

        YGValue minHeight = YGNodeStyleGetMinHeight(node);
        if (minHeight.unit != YGUnitPoint || minHeight.value != 0.0F)
        {
            YGNodeStyleSetMinHeight(node, 0.0F);
        }

        return true;
    }

    static void
        configureMainAxisFlex(const components::Size& sizeComp, YGNodeRef node, bool isRow, bool parentIsScrollArea)
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

            YGValue basis = YGNodeStyleGetFlexBasis(node);
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
        float shrinkValue = (mainAxisFixed || parentIsScrollArea) ? 0.0F : 1.0F;
        if (YGNodeStyleGetFlexShrink(node) != shrinkValue)
        {
            YGNodeStyleSetFlexShrink(node, shrinkValue);
        }
    }

    static void configureCrossAxisStretch(const components::Size& sizeComp, YGNodeRef node, bool isRow)
    {
        const bool widthFill = policies::HasFlag(sizeComp.sizePolicy, policies::Size::HFill);
        const bool heightFill = policies::HasFlag(sizeComp.sizePolicy, policies::Size::VFill);
        const bool crossAxisFill = (isRow && heightFill) || (!isRow && widthFill);
        if (crossAxisFill && YGNodeStyleGetAlignSelf(node) != YGAlignStretch)
        {
            YGNodeStyleSetAlignSelf(node, YGAlignStretch);
        }
    }

    static void configureFlexBehaviorFromSize(const components::Size& sizeComp,
                                              YGNodeRef node,
                                              bool isRow,
                                              bool parentIsScrollArea)
    {
        configureMainAxisFlex(sizeComp, node, isRow, parentIsScrollArea);
        configureCrossAxisStretch(sizeComp, node, isRow);
    }

    static void configureExplicitWidth(const components::Size& sizeComp, YGNodeRef node)
    {
        const bool widthFixed = policies::HasFlag(sizeComp.sizePolicy, policies::Size::HFixed);
        const bool widthAuto = policies::HasFlag(sizeComp.sizePolicy, policies::Size::HAuto);
        const bool widthPercent = policies::HasFlag(sizeComp.sizePolicy, policies::Size::HPercentage);

        YGValue currentWidth = YGNodeStyleGetWidth(node);
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
            float percentValue = sizeComp.percentage * 100.0F;
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

    static void configureExplicitHeight(const components::Size& sizeComp, YGNodeRef node)
    {
        const bool heightFixed = policies::HasFlag(sizeComp.sizePolicy, policies::Size::VFixed);
        const bool heightAuto = policies::HasFlag(sizeComp.sizePolicy, policies::Size::VAuto);
        const bool heightPercent = policies::HasFlag(sizeComp.sizePolicy, policies::Size::VPercentage);

        YGValue currentHeight = YGNodeStyleGetHeight(node);
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
            float percentValue = sizeComp.percentage * 100.0F;
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

    static void configureExplicitSize(const components::Size& sizeComp, YGNodeRef node)
    {
        configureExplicitWidth(sizeComp, node);
        configureExplicitHeight(sizeComp, node);
    }

    static void configureMinMaxSize(const components::Size& sizeComp, YGNodeRef node)
    {
        auto setMinOrMax = [node](auto getter, auto setter, float value, bool isMax)
        {
            YGValue current = getter(node);
            if (isMax)
            {
                if (value < FLT_MAX && (current.unit != YGUnitPoint || current.value != value))
                {
                    setter(node, value);
                }
            }
            else if (value > 0.0F && (current.unit != YGUnitPoint || current.value != value))
            {
                setter(node, value);
            }
        };

        setMinOrMax(YGNodeStyleGetMinWidth, YGNodeStyleSetMinWidth, sizeComp.minSize.x(), false);
        setMinOrMax(YGNodeStyleGetMinHeight, YGNodeStyleSetMinHeight, sizeComp.minSize.y(), false);
        setMinOrMax(YGNodeStyleGetMaxWidth, YGNodeStyleSetMaxWidth, sizeComp.maxSize.x(), true);
        setMinOrMax(YGNodeStyleGetMaxHeight, YGNodeStyleSetMaxHeight, sizeComp.maxSize.y(), true);
    }

    static void configureSizeNode(entt::entity entity, YGNodeRef node)
    {
        const auto* sizeComp = Registry::TryGet<components::Size>(entity);
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
        configureFlexBehaviorFromSize(*sizeComp, node, isRow, parentIsScrollArea);
        configureExplicitSize(*sizeComp, node);
        configureMinMaxSize(*sizeComp, node);
    }

    static void configureAbsolutePosition(entt::entity entity, YGNodeRef node)
    {
        const auto* posPolicy = Registry::TryGet<components::Position>(entity);
        if (posPolicy == nullptr)
        {
            return;
        }

        const bool horizontalAbsolute = policies::HasFlag(posPolicy->positionPolicy, policies::Position::HAbsolute);
        const bool verticalAbsolute = policies::HasFlag(posPolicy->positionPolicy, policies::Position::VAbsolute);
        if (!horizontalAbsolute && !verticalAbsolute)
        {
            return;
        }

        if (YGNodeStyleGetPositionType(node) != YGPositionTypeAbsolute)
        {
            YGNodeStyleSetPositionType(node, YGPositionTypeAbsolute);
        }

        const auto* pos = Registry::TryGet<components::Position>(entity);
        if (pos == nullptr)
        {
            return;
        }

        if (horizontalAbsolute)
        {
            YGValue currentLeft = YGNodeStyleGetPosition(node, YGEdgeLeft);
            if (currentLeft.unit != YGUnitPoint || currentLeft.value != pos->value.x())
            {
                YGNodeStyleSetPosition(node, YGEdgeLeft, pos->value.x());
            }
        }

        if (verticalAbsolute)
        {
            YGValue currentTop = YGNodeStyleGetPosition(node, YGEdgeTop);
            if (currentTop.unit != YGUnitPoint || currentTop.value != pos->value.y())
            {
                YGNodeStyleSetPosition(node, YGEdgeTop, pos->value.y());
            }
        }
    }

    [[nodiscard]] static YGJustify resolveJustifyContent(policies::Alignment alignment, bool isRow)
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

    [[nodiscard]] static YGAlign resolveAlignItems(policies::Alignment alignment, bool isRow)
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

    static void configureOverflow(entt::entity entity, YGNodeRef node)
    {
        YGOverflow targetOverflow =
            Registry::AnyOf<components::ScrollArea>(entity) ? YGOverflowScroll : YGOverflowHidden;
        if (YGNodeStyleGetOverflow(node) != targetOverflow)
        {
            YGNodeStyleSetOverflow(node, targetOverflow);
        }
    }

    static void configureContainerAlignmentAndOverflow(entt::entity entity, YGNodeRef node)
    {
        const auto* layoutInfo = Registry::TryGet<components::LayoutInfo>(entity);
        if (layoutInfo == nullptr)
        {
            return;
        }

        const bool isRow = (layoutInfo->direction == policies::LayoutDirection::HORIZONTAL);
        const auto alignment = resolveContainerAlignment(entity, *layoutInfo);
        YGJustify justify = resolveJustifyContent(alignment, isRow);
        YGAlign alignItems = resolveAlignItems(alignment, isRow);

        if (YGNodeStyleGetJustifyContent(node) != justify)
        {
            YGNodeStyleSetJustifyContent(node, justify);
        }

        if (YGNodeStyleGetAlignItems(node) != alignItems)
        {
            YGNodeStyleSetAlignItems(node, alignItems);
        }

        configureOverflow(entity, node);
    }

    static void configureLeafAutoSize(entt::entity entity, YGNodeRef node)
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

        float defaultWidth = 100.0F;
        float defaultHeight = 20.0F;

        if (const auto* text = Registry::TryGet<components::Text>(entity))
        {
            if (!text->content.empty())
            {
                defaultWidth = (static_cast<float>(text->content.length()) * 8.0F) + 10.0F;
            }
        }

        if (policies::HasFlag(sizeComp->sizePolicy, policies::Size::HAuto))
        {
            YGValue currentMinWidth = YGNodeStyleGetMinWidth(node);
            if (currentMinWidth.unit != YGUnitPoint || currentMinWidth.value != defaultWidth)
            {
                YGNodeStyleSetMinWidth(node, defaultWidth);
            }
        }

        if (policies::HasFlag(sizeComp->sizePolicy, policies::Size::VAuto))
        {
            YGValue currentMinHeight = YGNodeStyleGetMinHeight(node);
            if (currentMinHeight.unit != YGUnitPoint || currentMinHeight.value != defaultHeight)
            {
                YGNodeStyleSetMinHeight(node, defaultHeight);
            }
        }
    }

    /**
     * @brief 配置单个 Yoga 节点的样式
     */
    void configureYogaNode(entt::entity entity, YGNodeRef node)
    {
        configureLayoutDirectionAndGap(entity, node);
        configurePadding(entity, node);
        if (configureSpacerNode(entity, node))
        {
            return;
        }

        configureSizeNode(entity, node);
        configureAbsolutePosition(entity, node);
        configureContainerAlignmentAndOverflow(entity, node);
        configureLeafAutoSize(entity, node);
    }

    static void applyYogaLayout(entt::entity entity, YGNodeRef node, float parentX, float parentY)
    {
        if (node == nullptr) return;

        bool isDirty = false; // 用于跟踪当前实体是否发生了布局变化

        // 1. 获取 Yoga 计算结果
        float left = YGNodeLayoutGetLeft(node);
        float top = YGNodeLayoutGetTop(node);
        float width = YGNodeLayoutGetWidth(node);
        float height = YGNodeLayoutGetHeight(node);

        // 安全检查
        if (std::isnan(left)) left = 0.0F;
        if (std::isnan(top)) top = 0.0F;

        // 2. 回写 Position 并检查差异
        if (auto* pos = Registry::TryGet<components::Position>(entity))
        {
            if (pos->value.x() != left || pos->value.y() != top)
            {
                pos->value.x() = left;
                pos->value.y() = top;
                isDirty = true;
            }
        }

        // 3. 回写 Size 并检查差异
        if (auto* size = Registry::TryGet<components::Size>(entity))
        {
            // 只有当 Yoga 返回了有效的正数尺寸且与当前值不同时才更新
            bool widthChanged = (!std::isnan(width) && width > 0 && size->size.x() != width);
            bool heightChanged = (!std::isnan(height) && height > 0 && size->size.y() != height);

            if (widthChanged || heightChanged)
            {
                if (widthChanged) size->size.x() = width;
                if (heightChanged) size->size.y() = height;
                isDirty = true;
            }
        }

        // 如果位置或尺寸变了，标记该实体为脏
        if (isDirty)
        {
            utils::MarkRenderDirty(entity);
        }

        // 4. 递归处理子节点
        const uint32_t childCount = YGNodeGetChildCount(node);
        const auto* hierarchy = Registry::TryGet<components::Hierarchy>(entity);

        float maxContentRight = 0.0F;
        float maxContentBottom = 0.0F;

        if (hierarchy != nullptr && childCount > 0)
        {
            uint32_t yogaChildIndex = 0;
            for (entt::entity child : hierarchy->children)
            {
                const bool isSpacer = Registry::AnyOf<components::SpacerTag>(child);
                const bool hasLayoutComponents = Registry::AllOf<components::Position, components::Size>(child);

                if (!isSpacer && !hasLayoutComponents) continue;

                if (yogaChildIndex < childCount)
                {
                    YGNodeRef childNode = YGNodeGetChild(node, yogaChildIndex);

                    // 递归
                    applyYogaLayout(child, childNode, left, top);

                    // 收集边界用于 ScrollArea
                    float cL = YGNodeLayoutGetLeft(childNode);
                    float cT = YGNodeLayoutGetTop(childNode);
                    float cW = YGNodeLayoutGetWidth(childNode);
                    float cH = YGNodeLayoutGetHeight(childNode);

                    maxContentRight =
                        std::max(maxContentRight, (std::isnan(cL) ? 0.0f : cL) + (std::isnan(cW) ? 0.0f : cW));
                    maxContentBottom =
                        std::max(maxContentBottom, (std::isnan(cT) ? 0.0f : cT) + (std::isnan(cH) ? 0.0f : cH));

                    yogaChildIndex++;
                }
            }
        }

        // 5. 特殊处理 ScrollArea 内容尺寸
        // 注意：如果是 TextEdit 组件，其内容尺寸由 RenderSystem 根据文本动态计算（自动换行），
        // LayoutSystem 此处基于子元素的计算会将其覆盖为 0（因 TextEdit 无子元素），导致 RenderSystem
        // 每帧检测到尺寸变化引发死循环。 因此排除 TextEditTag。
        if (auto* scrollArea = Registry::TryGet<components::ScrollArea>(entity))
        {
            if (Registry::AnyOf<components::TextEditTag>(entity))
            {
                // 跳过 TextEdit 的 contentSize 计算
                return;
            }

            float pR = 0.0F;
            float pB = 0.0F;
            if (auto* padding = Registry::TryGet<components::Padding>(entity))
            {
                pR = padding->values.y();
                pB = padding->values.z();
            }

            float newContentW = maxContentRight + pR;
            float newContentH = maxContentBottom + pB;

            if (scrollArea->contentSize.x() != newContentW || scrollArea->contentSize.y() != newContentH)
            {
                scrollArea->contentSize.x() = newContentW;
                scrollArea->contentSize.y() = newContentH;
                // 如果滚动区域的内容大小变了，通常也需要重新渲染
                utils::MarkRenderDirty(entity);
            }
        }
    }

    // ===================== 窗口居中策略 =====================

    /**
     * @brief 应用窗口居中策略
     *
     * 对于顶层容器（窗口/菜单），如果没有设置固定位置，则自动居中显示。
     * 如果 Position 已有非零值，则优先使用该位置。
     */
    void applyWindowCentering(entt::entity root, float screenWidth, float screenHeight)
    {
        auto* pos = Registry::TryGet<components::Position>(root);
        auto* size = Registry::TryGet<components::Size>(root);

        // 基础检查：没有组件或尺寸无效则不处理
        if (!pos || !size || size->size.x() <= 0.0F || size->size.y() <= 0.0F)
        {
            return;
        }

        // 1. 判断是否需要居中
        // 策略 A: 显式指定了 Default 策略
        bool isDefault = (pos->positionPolicy == policies::Position::Default);

        // 策略 B: 显式指定了居中 Flag
        bool centerH = policies::HasFlag(pos->positionPolicy, policies::Position::HCenter);
        bool centerV = policies::HasFlag(pos->positionPolicy, policies::Position::VCenter);

        // 策略 C: 隐式检查（保留你之前的坐标 0,0 判断，但作为次级优先级）
        // 如果你希望彻底弃用 0,0 判断，可以删掉这一行
        bool implicitCenter = (pos->value.x() == 0.0F && pos->value.y() == 0.0F);

        // 覆盖逻辑：如果设置了 Fixed，强制取消对应的居中意图
        if (policies::HasFlag(pos->positionPolicy, policies::Position::HFixed)) centerH = false;
        if (policies::HasFlag(pos->positionPolicy, policies::Position::VFixed)) centerV = false;

        // 2. 综合决策：如果是 Default 且没有被 Fixed 覆盖，则全居中
        if (isDefault)
        {
            // 在 Default 策略下，除非显式指定了 Fixed，否则默认水平垂直双居中
            if (!policies::HasFlag(pos->positionPolicy, policies::Position::HFixed)) centerH = true;
            if (!policies::HasFlag(pos->positionPolicy, policies::Position::VFixed)) centerV = true;
        }
        else if (!centerH && !centerV && implicitCenter)
        {
            // 如果不是 Default，也没有 Flag，但坐标是 0,0 且没被 Fixed，也可以补救性居中
            if (!policies::HasFlag(pos->positionPolicy, policies::Position::HFixed)) centerH = true;
            if (!policies::HasFlag(pos->positionPolicy, policies::Position::VFixed)) centerV = true;
        }

        // 3. 执行应用
        if (centerH)
        {
            pos->value.x() = (screenWidth - size->size.x()) / 2.0F;
        }
        if (centerV)
        {
            pos->value.y() = (screenHeight - size->size.y()) / 2.0F;
        }
    }
};

} // namespace ui::systems