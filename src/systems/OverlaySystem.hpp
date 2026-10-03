/**
 * ************************************************************************
 *
 * @file OverlaySystem.hpp
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2026-08-13
 * @version 0.1
 * @brief 浮层系统 - 统一管理 Overlay/Popup 的生命周期
 * 什么是 Overlay/Popup？它们是 UI 组件的浮层，通常用于显示临时信息、菜单、对话框等。OverlaySystem 负责管理这些浮层的打开、关闭、焦点切换和叠加顺序。
 *
 * 职责：
 * - 监听 OverlayOpenRequest：分配统一 z-order、写入 OverlayLayer、压入浮层栈
 * - 监听 OverlayCloseRequest：出栈、恢复焦点到 owner
 * - 监听 OverlayCloseAllRequest：从栈顶到栈底依次关闭
 * - 监听 HitPointerButton：命中点不在任何浮层子树内时，关闭最顶层浮层（外部点击关闭）
 *
 * 焦点恢复：关闭浮层时，将焦点恢复到该浮层的 owner（若 owner 仍有效且可聚焦）。
 * 实际焦点切换复用 StateSystem::setFocus（经 FocusChangeRequest 事件解耦）。
 *
 * ************************************************************************
 * @copyright Copyright (c) 2026 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */
#pragma once

#include <entt/entt.hpp>

#include "common/Events.hpp"
#include "core/UiRuntime.hpp"
#include "interface/ISystem.hpp"
#include "utils/Dispatcher.hpp"
#include "utils/Registry.hpp"

namespace ui::systems
{

class OverlaySystem : public ui::interface::EnableRegister<OverlaySystem>
{
   public:
    OverlaySystem() = default;
    explicit OverlaySystem(UiRuntime& runtime) : m_reg(&runtime.registry()), m_disp(&runtime.dispatcher())
    {
    }

    void registerHandlersImpl();
    void unregisterHandlersImpl();

    [[nodiscard]] ui::interface::SystemPhase getPhase()
    {
        return ui::interface::SystemPhase::LOGIC;
    }

   private:
    void onOpenRequest(const events::OverlayOpenRequest& event);
    void onCloseRequest(const events::OverlayCloseRequest& event);
    void onCloseAllRequest(const events::OverlayCloseAllRequest& event);
    void onHitPointerButton(const events::HitPointerButton& event);

    /// 判断 entity 是否位于 anchor 的子树内。
    bool isDescendantOf(entt::entity entity, entt::entity anchor) const;

    Registry* m_reg = nullptr;
    Dispatcher* m_disp = nullptr;
};

}  // namespace ui::systems
