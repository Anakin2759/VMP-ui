/**
 * ************************************************************************
 *
 * @file Chains.hpp
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2026-02-09
 * @version 0.1
 * @brief UI API 的链式调用扩展
 *

 * 实现参数外置链式调用。
 * 允许使用管道操作符 | 对实体进行连续配置。
    不需要保存和合并, 直接在实体上应用一系列设置。
 *
 * 用法示例:
 * using namespace ui::chains;
 * entity | FixedSize(100, 100)
 *        | BackgroundColor(Color::Red)
 *        | Show();
 *
 * ************************************************************************
 * @copyright Copyright (c) 2026 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */

#pragma once

#include <entt/entt.hpp>
#include <concepts>
#include <utility>
#include <functional>

namespace ui::actions
{
/**
 * @brief 实体动作封装
 * @note 约定：模块级动作常量统一使用 UPPER_SNAKE_CASE + _ACTION 后缀。
 */
template <auto Fn>
struct EntityAction;
} // namespace ui::actions

namespace ui::chains
{

// 概念定义：可以接收 entity 的可调用对象
template <typename T>
concept Action = std::invocable<T, ::entt::entity>;

/**
 * @brief 链式动作包装器
 * 支持组合和执行
 */
template <typename F>
struct Chain
{
    mutable F func;

    constexpr explicit Chain(F&& callable) : func(std::move(callable)) {}

    // 动作组合: Chain | Chain -> Chain
    // 允许将多个设置组合成一个可复用的样式/配置块
    template <typename Self, typename Next>
    auto operator|(this Self&& self, Next&& next)
        requires Action<Next>
    {
        auto combined = [currentAction = std::forward<Self>(self).func,
                         nextAction = std::forward<Next>(next)](::entt::entity entity) mutable
        {
            std::invoke(currentAction, entity);
            std::invoke(nextAction, entity);
        };
        return Chain<decltype(combined)>{std::move(combined)};
    }

    // 执行动作
    void operator()(::entt::entity entity) const { std::invoke(func, entity); }
};

/**
 * @brief 管道操作符: Entity | Chain -> Entity
 * 应用动作到实体，并返回实体以继续链式调用
 */
template <typename F>
::entt::entity operator|(::entt::entity entity, const Chain<F>& chain)
{
    chain(entity);
    return entity;
}

/**
 * @brief 包装器工厂函数
 * 捕获参数并延迟调用目标函数
 * @tparam Func 可调用对象类型
 * @tparam Args 参数类型列表
 * @param args 参数列表
 * @return Chain 包装后的链式动作
 */
template <auto Func, typename... Args>
auto Call(Args&&... args)
{
    return ui::actions::EntityAction<Func>{}.bind(std::forward<Args>(args)...);
}

} // namespace ui::chains

namespace ui::actions
{
template <auto Fn>
struct EntityAction
{
    template <typename... Args>
    void operator()(::entt::entity entity, Args&&... args) const
    {
        Fn(entity, std::forward<Args>(args)...);
    }

    template <typename... Args>
    auto bind(Args&&... args) const
    {
        return ui::chains::Chain{[... args = std::forward<Args>(args)](::entt::entity entity) mutable
                                 { Fn(entity, std::move(args)...); }};
    }
};
} // namespace ui::actions
