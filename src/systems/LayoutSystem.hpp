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
#include <memory>
#include <unordered_map>

#include <yoga/Yoga.h>

#include "interface/ISystem.hpp"
#include "core/UiRuntime.hpp"
#include "utils/Dispatcher.hpp"
#include "utils/Registry.hpp"

namespace ui::systems
{

/**
 * @brief 基于 Yoga Flexbox 的布局系统
 *
 * 将 ECS 实体树映射到 Yoga 节点树，计算布局后回写到 Position/Size 组件。
 *
 * @trigger 订阅 events::UpdateLayout
 * @subscribe Registry::on_destroy<Hierarchy> — 实体销毁时自动释放 YGNode
 */
class LayoutSystem : public ui::interface::EnableRegister<LayoutSystem>
{
   public:
    explicit LayoutSystem(UiRuntime& runtime);
    ~LayoutSystem();

    LayoutSystem(const LayoutSystem&) = delete;
    LayoutSystem& operator=(const LayoutSystem&) = delete;
    LayoutSystem(LayoutSystem&& other) noexcept;
    LayoutSystem& operator=(LayoutSystem&& other) noexcept;

    void registerHandlersImpl();
    void unregisterHandlersImpl();
    void update();

    interface::SystemPhase getPhase()
    {
        return interface::SystemPhase::LAYOUT;
    }

   private:
    [[nodiscard]] entt::entity findRoot(entt::entity entity) const;
    void clearYogaNodes();
    void cleanupInvalidNodes();
    [[nodiscard]] YGNodeRef createYogaNode() const;
    [[nodiscard]] YGNodeRef getOrCreateNode(entt::entity entity);
    void syncNodeRecursive(entt::entity entity);
    void syncChildren(entt::entity entity, YGNodeRef node);
    void configureYogaNode(entt::entity entity, YGNodeRef node);
    void applyYogaLayout(entt::entity entity, YGNodeRef node);
    void applyAnchorLayout(entt::entity root);
    void applyWindowCentering(entt::entity root, float screenWidth, float screenHeight);

    /// @brief 实体销毁时自动释放对应 YGNode（由 on_destroy<Hierarchy> 信号触发）
    void onEntityDestroyed(entt::entity entity);

    YGConfigRef m_yogaConfig = nullptr;
    std::unique_ptr<std::unordered_map<entt::entity, YGNodeRef>> m_entityToNode;
    Registry* m_reg = nullptr;
    Dispatcher* m_disp = nullptr;

    /// @brief on_destroy 信号连接句柄（用于 unregisterHandlersImpl 时断开）
    entt::connection m_onDestroyConnection;
};

}  // namespace ui::systems