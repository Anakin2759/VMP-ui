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
 * @brief 系统接口 - 定义注册和注销事件处理器的方法
 */
struct ISystem : entt::type_list<>
{
    template <typename Base>
    struct type : Base
    {
        void registerHandlers() { entt::poly_call<0>(*this); }
        void unregisterHandlers() { entt::poly_call<1>(*this); }
    };

    template <typename T>
    using impl = entt::value_list<&T::registerHandlers, &T::unregisterHandlers>;
};

/**
 * @brief 启用注册功能的系统基类模板
 */
template <typename Derived>
struct EnableRegister
{
    /**
     * @brief 注册事件处理器
     */
    void registerHandlers() { static_cast<Derived*>(this)->registerHandlersImpl(); }
    /**
     * @brief 注销事件处理器
     */
    void unregisterHandlers() { static_cast<Derived*>(this)->unregisterHandlersImpl(); }
};
} // namespace ui::interface