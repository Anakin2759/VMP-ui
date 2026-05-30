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
 * 不需要保存和合并，直接在实体上应用一系列设置。
 *
 * 用法示例（模块内零开销组合，Chain<F> 就地内联）:
 *   using namespace ui::chains;
 *   entity | Size(100, 40) | BackgroundColor(Color::Red()) | Show();
 *
 * 跨模块存储/传递时使用 AnyChain（concept-model 类型擦除，类型稳定）:
 *   AnyChain style = AnyChain{Size(100, 40) | BackgroundColor(Color::Blue())};
 *   AnyChain combined = std::move(style) | AnyChain{Show()};
 *
 * ************************************************************************
 * @copyright Copyright (c) 2026 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */

#pragma once

#include "Entity.hpp"
#include <concepts>
#include <functional>
#include <memory>
#include <utility>

namespace ui::actions
{
template <auto Fn>
struct EntityAction;
} // namespace ui::actions

namespace ui::chains
{

// =========================================================================
// § 1  ChainActionTag — 标记"参与管道组合的内部链节点"
//      所有可直接用于 operator| 的链节点（Chain、AnyChain）均继承此 tag。
//      EntityAction 不直接组合，不携带 tag；其 bind() 产出的 Chain 已有 tag。
// =========================================================================
struct ChainActionTag
{
};

// =========================================================================
// § 2  Concepts
// =========================================================================

/// 宽松：任意可以接受 ui::entity 的可调用对象（entity | x 入口使用）
template <typename T>
concept Action = std::invocable<T, ui::entity>;

/// 标签检测：类型是否携带 ChainActionTag（独立为辅助 concept，便于单独 static_assert 诊断）
template <typename T>
concept TaggedAsChainAction = std::derived_from<std::remove_cvref_t<T>, ChainActionTag>
                           || requires { typename std::remove_cvref_t<T>::is_chain_action_tag; };

/// 严格：Action + 携带 ChainActionTag（Chain::operator| 的参与者约束）
/// 防止将任意 lambda / 函数指针意外接入管道；用户需显式 Chain{fn} 包装
template <typename T>
concept ChainAction = Action<T> && TaggedAsChainAction<T>;

// =========================================================================
// § 3  AnyChain — concept-model 类型擦除，跨模块稳定传递/存储
//      Concept：虚调用接口（公开，供 operator| 内 Combined 局部类继承）
//      Model<F>：包裹具体 ChainAction 类型，转发调用至 F::operator()
//      move-only；AnyChain | AnyChain 产出新 AnyChain（类型始终稳定）
// =========================================================================
class AnyChain : public ChainActionTag
{
public:
    using is_chain_action_tag = void;

    struct Concept
    {
        Concept() = default;
        Concept(const Concept&) = delete;
        Concept& operator=(const Concept&) = delete;
        Concept(Concept&&) = delete;
        Concept& operator=(Concept&&) = delete;
        virtual void invoke(ui::entity) = 0;
        virtual ~Concept() = default;
    };

    template <ChainAction F>
        requires(!std::same_as<std::decay_t<F>, AnyChain>)
    explicit AnyChain(F&& action) : m_impl(std::make_unique<Model<std::decay_t<F>>>(std::forward<F>(action)))
    {
    }

    AnyChain(AnyChain&&) noexcept = default;
    AnyChain& operator=(AnyChain&&) noexcept = default;
    AnyChain(const AnyChain&) = delete;
    AnyChain& operator=(const AnyChain&) = delete;
    ~AnyChain() = default;

    void operator()(ui::entity entity) { m_impl->invoke(entity); }

    friend AnyChain operator|(AnyChain&& lhs, AnyChain&& rhs);

private:
    // Model 约束收紧至 ChainAction（比 Action 更严格）：
    // 构造路径已保证此处 F 满足 ChainAction，显式标注意图更清晰
    template <ChainAction F>
    struct Model final : Concept
    {
        F func;
        explicit Model(F callable) : func(std::move(callable)) {}
        void invoke(ui::entity entity) override { std::invoke(func, entity); }
    };

    explicit AnyChain(std::unique_ptr<Concept> impl) noexcept : m_impl(std::move(impl)) {}

    std::unique_ptr<Concept> m_impl;
};

/// 组合两个 AnyChain：产出新 AnyChain（类型稳定，适合跨模块存储）
inline AnyChain operator|(AnyChain&& lhs, AnyChain&& rhs)
{
    struct Combined final : AnyChain::Concept
    {
        AnyChain left, right;
        Combined(AnyChain lhsChain, AnyChain rhsChain) : left(std::move(lhsChain)), right(std::move(rhsChain)) {}
        void invoke(ui::entity entity) override
        {
            left(entity);
            right(entity);
        }
    };
    return AnyChain{std::make_unique<Combined>(std::move(lhs), std::move(rhs))};
}

// =========================================================================
// § 4  Chain<F> — concept 约束的零开销模板链节点，用于模块内内联组合
//      F 必须满足 Action（编译期约束，取代运行期 std::function 检查）
//      继承 ChainActionTag：自身满足 ChainAction，可参与严格组合
// =========================================================================
template <Action F>
struct Chain : ChainActionTag
{
    using is_chain_action_tag = void;

    F func;

    constexpr explicit Chain(F&& callable) : func(std::move(callable)) {}

    /// 仅接受 ChainAction（携带 tag）类型，阻止意外组合任意可调用对象
    /// 用户自定义 lambda 需显式 Chain{fn} 包装后方可参与组合
    template <typename Self, ChainAction Next>
    auto operator|(this Self&& self, Next&& next)
    {
        auto combined =
            // C++23 std::forward_like：按 Self 的值类别转发 func（rvalue→move，lvalue→copy）
            [lhs = std::forward_like<Self>(self.func), rhs = std::forward<Next>(next)](ui::entity entity) mutable
        {
            std::invoke(lhs, entity);
            std::invoke(rhs, entity);
        };
        return Chain<decltype(combined)>{std::move(combined)};
    }

    // deducing-this：const/非const/rvalue 均正确分发
    void operator()(this auto&& self, ui::entity entity)
    {
        std::invoke(std::forward_like<decltype(self)>(self.func), entity);
    }
};

// CTAD：防止 Chain{lvalue} 推导出 Chain<F&> 导致悬垂引用
template <typename F>
Chain(F&&) -> Chain<std::decay_t<F>>;

// =========================================================================
// § 5  operator| — Entity 管道入口
//      宽松使用 Action（不要求 tag），允许直接 entity | anyCallable
//      Chain | x 则由成员 operator| 严格要求 ChainAction
// =========================================================================
template <Action F>
ui::entity operator|(ui::entity entity, Chain<F>& chain)
{
    chain(entity);
    return entity;
}

template <Action F>
ui::entity operator|(ui::entity entity, const Chain<F>& chain)
{
    chain(entity);
    return entity;
}

template <Action F>
ui::entity operator|(ui::entity entity, Chain<F>&& chain)
{
    std::move(chain)(entity);
    return entity;
}

inline ui::entity operator|(ui::entity entity, AnyChain& chain)
{
    chain(entity);
    return entity;
}

inline ui::entity operator|(ui::entity entity, AnyChain&& chain)
{
    auto owned = std::move(chain);
    owned(entity);
    return entity;
}

// =========================================================================
// § 6  Call — 工厂函数，捕获参数并延迟调用目标函数
// =========================================================================
template <auto Func, typename... Args>
auto Call(Args&&... args)
{
    return ui::actions::EntityAction<Func>{}.bind(std::forward<Args>(args)...);
}

} // namespace ui::chains

// =========================================================================
// § 7  EntityAction — NTTP 动作工厂，bind() 产出携带 tag 的 Chain<F>
//      自身不直接参与 operator|，不携带 chain_action_tag
// =========================================================================
namespace ui::actions
{
template <auto Fn>
struct EntityAction
{
    template <typename... Args>
    void operator()(ui::entity entity, Args&&... args) const
    {
        Fn(entity, std::forward<Args>(args)...);
    }

    template <typename... Args>
    auto bind(Args&&... args) const
    {
        return ui::chains::Chain{[... args = std::forward<Args>(args)](ui::entity entity) mutable
                                 { Fn(entity, std::move(args)...); }};
    }
};
} // namespace ui::actions
