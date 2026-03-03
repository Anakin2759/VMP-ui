/**
 * ************************************************************************
 *
 * @file MyPFR.hpp
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2026-03-03
 * @version 0.1
 * @brief 模仿PFR实现的反射系统，支持结构体成员访问和修改，适用于UI组件属性管理
 *
 * ************************************************************************
 * @copyright Copyright (c) 2026 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */

#pragma once
#include <type_traits>
#include <utility>
#include <tuple>
namespace ui::traits
{
struct AnyType
{
    template <typename T>
        requires(!std::is_same_v<std::remove_cvref_t<T>, AnyType>)
    operator T(); // 隐式转换到任何类型 //NOLINT
};

template <typename T, typename... Args>
concept constructible_from_n = requires { T{std::declval<Args>()...}; };

// 探测核心：利用 requires 检查聚合初始化
template <typename T, std::size_t... Is>
constexpr bool IsConstructibleN(std::index_sequence<Is...>)
{
    return requires { T{((void)Is, AnyType{})...}; };
}

// 递归查找：从 Max 开始向下找，或者从 0 开始向上找
template <typename T, std::size_t N = 0>
constexpr std::size_t CountFields()
{
    // 如果可以用 N+1 个参数构造，尝试探测更高的数量
    if constexpr (IsConstructibleN<T>(std::make_index_sequence<N + 1>{}))
    {
        return CountFields<T, N + 1>();
    }
    else
    {
        return N;
    }
}

struct PfrAccessor
{
    template <typename T>
    static auto tieMembers(T& val)
    {
        constexpr size_t COUNT = CountFields<std::remove_cvref_t<T>>();

        if constexpr (COUNT == 0)
        {
            return std::tie();
        }
        else if constexpr (COUNT == 1)
        {
            auto& [v1] = val;
            return std::tie(v1);
        }
        else if constexpr (COUNT == 2)
        {
            auto& [v1, v2] = val;
            return std::tie(v1, v2);
        }
        else if constexpr (COUNT == 3)
        {
            auto& [v1, v2, v3] = val;
            return std::tie(v1, v2, v3);
        }
        else if constexpr (COUNT == 4)
        {
            auto& [v1, v2, v3, v4] = val;
            return std::tie(v1, v2, v3, v4);
        }
        else if constexpr (COUNT == 5)
        {
            auto& [v1, v2, v3, v4, v5] = val;
            return std::tie(v1, v2, v3, v4, v5);
        }
        else if constexpr (COUNT == 6)
        {
            auto& [v1, v2, v3, v4, v5, v6] = val;
            return std::tie(v1, v2, v3, v4, v5, v6);
        }
        else if constexpr (COUNT == 7)
        {
            auto& [v1, v2, v3, v4, v5, v6, v7] = val;
            return std::tie(v1, v2, v3, v4, v5, v6, v7);
        }
        else if constexpr (COUNT == 8)
        {
            auto& [v1, v2, v3, v4, v5, v6, v7, v8] = val;
            return std::tie(v1, v2, v3, v4, v5, v6, v7, v8);
        }
        else if constexpr (COUNT == 9)
        {
            auto& [v1, v2, v3, v4, v5, v6, v7, v8, v9] = val;
            return std::tie(v1, v2, v3, v4, v5, v6, v7, v8, v9);
        }
        else if constexpr (COUNT == 10)
        {
            auto& [v1, v2, v3, v4, v5, v6, v7, v8, v9, v10] = val;
            return std::tie(v1, v2, v3, v4, v5, v6, v7, v8, v9, v10);
        }
        else
        {
            static_assert(COUNT <= 10, "Unsupported member count! Please extend PfrAccessor.");
            return std::tie();
        }
    }
};

template <typename T>
auto StructureToTuple(T&& obj) // NOLINT
{
    // 自动路由到对应数量的结构化绑定分支
    return PfrAccessor::tieMembers(obj);
}

template <typename T, typename F>
void for_each_field(T&& obj, F&& func) // NOLINT
{
    auto t = structureToTuple(obj);
    // 使用 std::apply 展开元组，并对每个元素执行 func
    std::apply([&](auto&&... args) { (func(args), ...); }, t);
}

} // namespace ui::traits