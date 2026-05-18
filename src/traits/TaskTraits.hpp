#pragma once

#include <utility> // for std::to_underlying (C++23)

namespace ui::traits
{
template <typename T, typename... Args>
concept Task = std::invocable<T, Args...>;

// 2. 任务包装器：支持重入和链式调用
template <typename F>
struct Chain
{
    F func;

    // 显式构造
    explicit Chain(F&& f) : func(std::forward<F>(f)) {}

    // C++23 deducing this: 处理左值（拷贝）和右值（移动）
    // 从而完美支持“多次重入”
    template <typename Self, typename Next>
    auto operator|(this Self&& self, Next&& next)
        requires Task<Next, std::invoke_result_t<F>> // 约束：下一步必须能接收上一步的结果
    {
        // 返回一个闭包，将当前任务与下一任务串联
        auto combined = [f = std::forward<Self>(self).func, n = std::forward<Next>(next)](auto&&... args) mutable
        { return std::invoke(n, std::invoke(f, std::forward<decltype(args)>(args)...)); };

        return Chain<decltype(combined)>{std::move(combined)};
    }

    // 执行入口
    template <typename... Args>
    auto operator()(Args&&... args)
    {
        return std::invoke(func, std::forward<Args>(args)...);
    }
};


} // namespace ui::traits