/**
 * ************************************************************************
 *
 * @file Isystem.h
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2025-12-20
 * @version 0.1
 * @brief UI系统接口
    不需要处理帧更新，仅注册和注销事件处理器
 *
 * ************************************************************************
 * @copyright Copyright (c) 2025 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */

#pragma once
#include <entt/entt.hpp>

namespace ui::interface
{

/**
 * @brief 系统执行阶段枚举，用于 SystemManager 按正确顺序注册事件处理器
 */
enum class SystemPhase : uint8_t
{
    Input = 0,     ///< 平台/输入层 (PlatformWindow / Interaction / TextInput)
    Logic = 1,     ///< 业务逻辑层 (HitTest / State / Action / Shortcut)
    Animation = 2, ///< 动画层 (Tween)
    Layout = 3,    ///< 布局层 (Layout)
    Render = 4,    ///< 渲染层 (Render)
    Frame = 5,     ///< 帧尾层 (Timer)
};

/**
 * @brief 系统接口 - 定义注册/注销事件处理器、输入轮询和阶段查询方法
 */
struct ISystem : entt::type_list<>
{
    template <typename Base>
    struct type : Base
    {
        void registerHandlers() { entt::poly_call<0>(*this); }
        void unregisterHandlers() { entt::poly_call<1>(*this); }
        void pollInput() { entt::poly_call<2>(*this); }
        SystemPhase getPhase() { return entt::poly_call<3>(*this); }
    };

    template <typename T>
    using impl = entt::value_list<&T::registerHandlers, &T::unregisterHandlers, &T::pollInput, &T::getPhase>;
};

/**
 * @brief 启用注册功能的系统基类模板
 */
template <typename Derived>
struct EnableRegister
{
    void registerHandlers() { static_cast<Derived*>(this)->registerHandlersImpl(); }
    void unregisterHandlers() { static_cast<Derived*>(this)->unregisterHandlersImpl(); }

    /// 默认 no-op：只有需要主动轮询的系统（如 InteractionSystem）才重写
    void pollInput() {}

    /// 默认阶段为 Logic；Input/Animation/Layout/Render/Frame 系统应重写
    SystemPhase getPhase() { return SystemPhase::Logic; }
};
} // namespace ui::interface