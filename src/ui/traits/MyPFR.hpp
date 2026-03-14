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
#include <cstddef>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

namespace ui::traits
{

// =============================================================================
// AnyType - 可隐式转换为任意类型，用于聚合体构造探测
// =============================================================================
struct AnyType
{
    template <typename T>
        requires(!std::is_same_v<std::remove_cvref_t<T>, AnyType>)
    operator T() const noexcept; // 隐式转换到任何类型 //NOLINT
};

// =============================================================================
// CountFields - 编译期统计聚合结构体字段数，上限 kMaxFields 避免无限递归
// =============================================================================
inline constexpr std::size_t kMaxFields = 16; // NOLINT

template <typename T, std::size_t... Is>
constexpr bool IsConstructibleN(std::index_sequence<Is...> /*seq*/) noexcept
{
    return requires { T{((void)Is, AnyType{})...}; };
}

template <typename T, std::size_t N = 0>
constexpr std::size_t CountFields() noexcept
{
    if constexpr (N < kMaxFields && IsConstructibleN<T>(std::make_index_sequence<N + 1>{}))
    {
        return CountFields<T, N + 1>(); // NOLINT
    }
    else
    {
        return N;
    }
}

// =============================================================================
// PfrAccessor - 通过结构化绑定将聚合体成员解包为 tuple
// T 可以是 const 限定：T& 模板形参自然推导为 const SomeStruct&
// =============================================================================
struct PfrAccessor
{
    template <typename T>
    static auto tieMembers(T& val) noexcept // NOLINT
    {
        constexpr std::size_t N = CountFields<std::remove_cvref_t<T>>(); // NOLINT

        // clang-format off
        if constexpr (N == 0)  { return std::tuple<>{}; }
        else if constexpr (N == 1)  { auto& [v1] = val; return std::tie(v1); }
        else if constexpr (N == 2)  { auto& [v1,v2] = val; return std::tie(v1,v2); }
        else if constexpr (N == 3)  { auto& [v1,v2,v3] = val; return std::tie(v1,v2,v3); }
        else if constexpr (N == 4)  { auto& [v1,v2,v3,v4] = val; return std::tie(v1,v2,v3,v4); }
        else if constexpr (N == 5)  { auto& [v1,v2,v3,v4,v5] = val; return std::tie(v1,v2,v3,v4,v5); }
        else if constexpr (N == 6)  { auto& [v1,v2,v3,v4,v5,v6] = val; return std::tie(v1,v2,v3,v4,v5,v6); }
        else if constexpr (N == 7)  { auto& [v1,v2,v3,v4,v5,v6,v7] = val; return std::tie(v1,v2,v3,v4,v5,v6,v7); }
        else if constexpr (N == 8)  { auto& [v1,v2,v3,v4,v5,v6,v7,v8] = val; return std::tie(v1,v2,v3,v4,v5,v6,v7,v8); }
        else if constexpr (N == 9)  { auto& [v1,v2,v3,v4,v5,v6,v7,v8,v9] = val; return std::tie(v1,v2,v3,v4,v5,v6,v7,v8,v9); }
        else if constexpr (N == 10) { auto& [v1,v2,v3,v4,v5,v6,v7,v8,v9,v10] = val; return std::tie(v1,v2,v3,v4,v5,v6,v7,v8,v9,v10); }
        else if constexpr (N == 11) { auto& [v1,v2,v3,v4,v5,v6,v7,v8,v9,v10,v11] = val; return std::tie(v1,v2,v3,v4,v5,v6,v7,v8,v9,v10,v11); }
        else if constexpr (N == 12) { auto& [v1,v2,v3,v4,v5,v6,v7,v8,v9,v10,v11,v12] = val; return std::tie(v1,v2,v3,v4,v5,v6,v7,v8,v9,v10,v11,v12); }
        else if constexpr (N == 13) { auto& [v1,v2,v3,v4,v5,v6,v7,v8,v9,v10,v11,v12,v13] = val; return std::tie(v1,v2,v3,v4,v5,v6,v7,v8,v9,v10,v11,v12,v13); }
        else if constexpr (N == 14) { auto& [v1,v2,v3,v4,v5,v6,v7,v8,v9,v10,v11,v12,v13,v14] = val; return std::tie(v1,v2,v3,v4,v5,v6,v7,v8,v9,v10,v11,v12,v13,v14); }
        else if constexpr (N == 15) { auto& [v1,v2,v3,v4,v5,v6,v7,v8,v9,v10,v11,v12,v13,v14,v15] = val; return std::tie(v1,v2,v3,v4,v5,v6,v7,v8,v9,v10,v11,v12,v13,v14,v15); }
        else if constexpr (N == 16) { auto& [v1,v2,v3,v4,v5,v6,v7,v8,v9,v10,v11,v12,v13,v14,v15,v16] = val; return std::tie(v1,v2,v3,v4,v5,v6,v7,v8,v9,v10,v11,v12,v13,v14,v15,v16); }
        else { static_assert(N <= kMaxFields, "Member count exceeds kMaxFields. Increase kMaxFields and extend PfrAccessor."); return std::tuple<>{}; }
        // clang-format on
    }
};

// StructureToTuple - 单一模板处理 const/non-const（T 由编译器推导，无需 const_cast）
template <typename T>
auto StructureToTuple(T& obj) noexcept
{
    return PfrAccessor::tieMembers(obj);
}

// ForEachField - 对每个字段调用 func(field)，支持 const 对象
template <typename T, typename F>
constexpr void ForEachField(T&& obj, F&& func) // NOLINT
{
    auto tup = StructureToTuple(obj);
    std::apply([&func](auto&&... fields) { (func(std::forward<decltype(fields)>(fields)), ...); }, tup);
}

// field_get - 按索引获取成员引用（避免与 std::get 产生 ADL 冲突）
template <std::size_t I, typename T>
constexpr decltype(auto) field_get(T& obj) noexcept // NOLINT
{
    return std::get<I>(StructureToTuple(obj));
}

// =============================================================================
// 字段名提取（依赖编译器函数签名）
// =============================================================================

struct NameParser
{
    static constexpr std::string_view cleanup(std::string_view sig) noexcept
    {
        // 目标：从函数签名中切出 "field_name"
        // Clang:  "... [M = obj.field_name]"
        // GCC:    "... {M = obj.field_name}"
        // MSVC:   "... <{obj.field_name}>(void)"
#if defined(__clang__)
        const std::size_t end = sig.find_last_of(']');
        const std::size_t start = sig.find_last_of('.', end) + 1;
#elif defined(__GNUC__)
        const std::size_t end = sig.find_last_of('}');
        const std::size_t start = sig.find_last_of('.', end) + 1;
#elif defined(_MSC_VER)
        const std::size_t end = sig.find_last_of('}');
        const std::size_t start = sig.find_last_of('.', end) + 1;
#else
        const std::size_t end = sig.size();
        const std::size_t start = 0;
#endif
        if (start == std::string_view::npos || start >= end) return sig;
        return sig.substr(start, end - start);
    }
};

template <auto& M>
constexpr std::string_view get_name_impl() noexcept // NOLINT
{
#if defined(_MSC_VER) && !defined(__clang__)
    return NameParser::cleanup({__FUNCSIG__, sizeof(__FUNCSIG__) - 1});
#else
    return NameParser::cleanup({__PRETTY_FUNCTION__, sizeof(__PRETTY_FUNCTION__) - 1});
#endif
}

template <typename T>
extern const T fake_dummy_obj;

template <typename T>
struct NameAccessor
{
    template <std::size_t I>
    static constexpr std::string_view get_name() noexcept // NOLINT
    {
        constexpr std::size_t COUNT = CountFields<T>();

        // clang-format off
        if      constexpr (COUNT == 1)  { auto& [v1] = fake_dummy_obj<T>; if constexpr (I==0) return get_name_impl<v1>(); }
        else if constexpr (COUNT == 2)  { auto& [v1,v2] = fake_dummy_obj<T>; if constexpr (I==0) return get_name_impl<v1>(); if constexpr (I==1) return get_name_impl<v2>(); }
        else if constexpr (COUNT == 3)  { auto& [v1,v2,v3] = fake_dummy_obj<T>; if constexpr (I==0) return get_name_impl<v1>(); if constexpr (I==1) return get_name_impl<v2>(); if constexpr (I==2) return get_name_impl<v3>(); }
        else if constexpr (COUNT == 4)  { auto& [v1,v2,v3,v4] = fake_dummy_obj<T>; if constexpr (I==0) return get_name_impl<v1>(); if constexpr (I==1) return get_name_impl<v2>(); if constexpr (I==2) return get_name_impl<v3>(); if constexpr (I==3) return get_name_impl<v4>(); }
        else if constexpr (COUNT == 5)  { auto& [v1,v2,v3,v4,v5] = fake_dummy_obj<T>; if constexpr (I==0) return get_name_impl<v1>(); if constexpr (I==1) return get_name_impl<v2>(); if constexpr (I==2) return get_name_impl<v3>(); if constexpr (I==3) return get_name_impl<v4>(); if constexpr (I==4) return get_name_impl<v5>(); }
        else if constexpr (COUNT == 6)  { auto& [v1,v2,v3,v4,v5,v6] = fake_dummy_obj<T>; if constexpr (I==0) return get_name_impl<v1>(); if constexpr (I==1) return get_name_impl<v2>(); if constexpr (I==2) return get_name_impl<v3>(); if constexpr (I==3) return get_name_impl<v4>(); if constexpr (I==4) return get_name_impl<v5>(); if constexpr (I==5) return get_name_impl<v6>(); }
        else if constexpr (COUNT == 7)  { auto& [v1,v2,v3,v4,v5,v6,v7] = fake_dummy_obj<T>; if constexpr (I==0) return get_name_impl<v1>(); if constexpr (I==1) return get_name_impl<v2>(); if constexpr (I==2) return get_name_impl<v3>(); if constexpr (I==3) return get_name_impl<v4>(); if constexpr (I==4) return get_name_impl<v5>(); if constexpr (I==5) return get_name_impl<v6>(); if constexpr (I==6) return get_name_impl<v7>(); }
        else if constexpr (COUNT == 8)  { auto& [v1,v2,v3,v4,v5,v6,v7,v8] = fake_dummy_obj<T>; if constexpr (I==0) return get_name_impl<v1>(); if constexpr (I==1) return get_name_impl<v2>(); if constexpr (I==2) return get_name_impl<v3>(); if constexpr (I==3) return get_name_impl<v4>(); if constexpr (I==4) return get_name_impl<v5>(); if constexpr (I==5) return get_name_impl<v6>(); if constexpr (I==6) return get_name_impl<v7>(); if constexpr (I==7) return get_name_impl<v8>(); }
        else if constexpr (COUNT == 9)  { auto& [v1,v2,v3,v4,v5,v6,v7,v8,v9] = fake_dummy_obj<T>; if constexpr (I==0) return get_name_impl<v1>(); if constexpr (I==1) return get_name_impl<v2>(); if constexpr (I==2) return get_name_impl<v3>(); if constexpr (I==3) return get_name_impl<v4>(); if constexpr (I==4) return get_name_impl<v5>(); if constexpr (I==5) return get_name_impl<v6>(); if constexpr (I==6) return get_name_impl<v7>(); if constexpr (I==7) return get_name_impl<v8>(); if constexpr (I==8) return get_name_impl<v9>(); }
        else if constexpr (COUNT == 10) { auto& [v1,v2,v3,v4,v5,v6,v7,v8,v9,v10] = fake_dummy_obj<T>; if constexpr (I==0) return get_name_impl<v1>(); if constexpr (I==1) return get_name_impl<v2>(); if constexpr (I==2) return get_name_impl<v3>(); if constexpr (I==3) return get_name_impl<v4>(); if constexpr (I==4) return get_name_impl<v5>(); if constexpr (I==5) return get_name_impl<v6>(); if constexpr (I==6) return get_name_impl<v7>(); if constexpr (I==7) return get_name_impl<v8>(); if constexpr (I==8) return get_name_impl<v9>(); if constexpr (I==9) return get_name_impl<v10>(); }
        else if constexpr (COUNT == 11) { auto& [v1,v2,v3,v4,v5,v6,v7,v8,v9,v10,v11] = fake_dummy_obj<T>; if constexpr (I==0) return get_name_impl<v1>(); if constexpr (I==1) return get_name_impl<v2>(); if constexpr (I==2) return get_name_impl<v3>(); if constexpr (I==3) return get_name_impl<v4>(); if constexpr (I==4) return get_name_impl<v5>(); if constexpr (I==5) return get_name_impl<v6>(); if constexpr (I==6) return get_name_impl<v7>(); if constexpr (I==7) return get_name_impl<v8>(); if constexpr (I==8) return get_name_impl<v9>(); if constexpr (I==9) return get_name_impl<v10>(); if constexpr (I==10) return get_name_impl<v11>(); }
        else if constexpr (COUNT == 12) { auto& [v1,v2,v3,v4,v5,v6,v7,v8,v9,v10,v11,v12] = fake_dummy_obj<T>; if constexpr (I==0) return get_name_impl<v1>(); if constexpr (I==1) return get_name_impl<v2>(); if constexpr (I==2) return get_name_impl<v3>(); if constexpr (I==3) return get_name_impl<v4>(); if constexpr (I==4) return get_name_impl<v5>(); if constexpr (I==5) return get_name_impl<v6>(); if constexpr (I==6) return get_name_impl<v7>(); if constexpr (I==7) return get_name_impl<v8>(); if constexpr (I==8) return get_name_impl<v9>(); if constexpr (I==9) return get_name_impl<v10>(); if constexpr (I==10) return get_name_impl<v11>(); if constexpr (I==11) return get_name_impl<v12>(); }
        else if constexpr (COUNT == 13) { auto& [v1,v2,v3,v4,v5,v6,v7,v8,v9,v10,v11,v12,v13] = fake_dummy_obj<T>; if constexpr (I==0) return get_name_impl<v1>(); if constexpr (I==1) return get_name_impl<v2>(); if constexpr (I==2) return get_name_impl<v3>(); if constexpr (I==3) return get_name_impl<v4>(); if constexpr (I==4) return get_name_impl<v5>(); if constexpr (I==5) return get_name_impl<v6>(); if constexpr (I==6) return get_name_impl<v7>(); if constexpr (I==7) return get_name_impl<v8>(); if constexpr (I==8) return get_name_impl<v9>(); if constexpr (I==9) return get_name_impl<v10>(); if constexpr (I==10) return get_name_impl<v11>(); if constexpr (I==11) return get_name_impl<v12>(); if constexpr (I==12) return get_name_impl<v13>(); }
        else if constexpr (COUNT == 14) { auto& [v1,v2,v3,v4,v5,v6,v7,v8,v9,v10,v11,v12,v13,v14] = fake_dummy_obj<T>; if constexpr (I==0) return get_name_impl<v1>(); if constexpr (I==1) return get_name_impl<v2>(); if constexpr (I==2) return get_name_impl<v3>(); if constexpr (I==3) return get_name_impl<v4>(); if constexpr (I==4) return get_name_impl<v5>(); if constexpr (I==5) return get_name_impl<v6>(); if constexpr (I==6) return get_name_impl<v7>(); if constexpr (I==7) return get_name_impl<v8>(); if constexpr (I==8) return get_name_impl<v9>(); if constexpr (I==9) return get_name_impl<v10>(); if constexpr (I==10) return get_name_impl<v11>(); if constexpr (I==11) return get_name_impl<v12>(); if constexpr (I==12) return get_name_impl<v13>(); if constexpr (I==13) return get_name_impl<v14>(); }
        else if constexpr (COUNT == 15) { auto& [v1,v2,v3,v4,v5,v6,v7,v8,v9,v10,v11,v12,v13,v14,v15] = fake_dummy_obj<T>; if constexpr (I==0) return get_name_impl<v1>(); if constexpr (I==1) return get_name_impl<v2>(); if constexpr (I==2) return get_name_impl<v3>(); if constexpr (I==3) return get_name_impl<v4>(); if constexpr (I==4) return get_name_impl<v5>(); if constexpr (I==5) return get_name_impl<v6>(); if constexpr (I==6) return get_name_impl<v7>(); if constexpr (I==7) return get_name_impl<v8>(); if constexpr (I==8) return get_name_impl<v9>(); if constexpr (I==9) return get_name_impl<v10>(); if constexpr (I==10) return get_name_impl<v11>(); if constexpr (I==11) return get_name_impl<v12>(); if constexpr (I==12) return get_name_impl<v13>(); if constexpr (I==13) return get_name_impl<v14>(); if constexpr (I==14) return get_name_impl<v15>(); }
        else if constexpr (COUNT == 16) { auto& [v1,v2,v3,v4,v5,v6,v7,v8,v9,v10,v11,v12,v13,v14,v15,v16] = fake_dummy_obj<T>; if constexpr (I==0) return get_name_impl<v1>(); if constexpr (I==1) return get_name_impl<v2>(); if constexpr (I==2) return get_name_impl<v3>(); if constexpr (I==3) return get_name_impl<v4>(); if constexpr (I==4) return get_name_impl<v5>(); if constexpr (I==5) return get_name_impl<v6>(); if constexpr (I==6) return get_name_impl<v7>(); if constexpr (I==7) return get_name_impl<v8>(); if constexpr (I==8) return get_name_impl<v9>(); if constexpr (I==9) return get_name_impl<v10>(); if constexpr (I==10) return get_name_impl<v11>(); if constexpr (I==11) return get_name_impl<v12>(); if constexpr (I==12) return get_name_impl<v13>(); if constexpr (I==13) return get_name_impl<v14>(); if constexpr (I==14) return get_name_impl<v15>(); if constexpr (I==15) return get_name_impl<v16>(); }
        // clang-format on

        return "out_of_range";
    }
};

// field_name<I, T>() - 获取第 I 个成员的名称字符串
template <std::size_t I, typename T>
constexpr std::string_view field_name() noexcept // NOLINT
{
    return NameAccessor<std::remove_cvref_t<T>>::template get_name<I>();
}

} // namespace ui::traits