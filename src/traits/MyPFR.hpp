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
#include <array>
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
// CountFields - 编译期统计聚合结构体字段数，上限 MAX_FIELD_COUNT 避免无限递归
// =============================================================================
inline constexpr std::size_t MAX_FIELD_COUNT = 16;

template <typename T, std::size_t... Is>
constexpr bool IsConstructibleN(std::index_sequence<Is...> /*seq*/) noexcept
{
    return requires { T{((void)Is, AnyType{})...}; };
}

template <typename T, std::size_t Lo, std::size_t Hi>
constexpr std::size_t CountFieldsImpl() noexcept
{
    if constexpr (Lo >= Hi)
    {
        return Lo;
    }
    constexpr std::size_t MID = (Lo + Hi + 1) / 2;
    if constexpr (IsConstructibleN<T>(std::make_index_sequence<MID>{}))
    {
        return CountFieldsImpl<T, MID, Hi>();
    }
    else
    {
        return CountFieldsImpl<T, Lo, MID - 1>();
    }
}

template <typename T>
constexpr std::size_t CountFields() noexcept
{
    return CountFieldsImpl<T, 0, MAX_FIELD_COUNT>();
}

template <typename T, std::size_t Count>
struct TupleAccessor;

template <typename T>
struct TupleAccessor<T, 0>
{
    static auto tieMembers(T& /*value*/) noexcept { return std::tuple<>{}; }
};

template <typename T>
struct TupleAccessor<T, 1>
{
    static auto tieMembers(T& value) noexcept
    {
        auto& [v1] = value;
        return std::tie(v1);
    }
};

template <typename T>
struct TupleAccessor<T, 2>
{
    static auto tieMembers(T& value) noexcept
    {
        auto& [v1, v2] = value;
        return std::tie(v1, v2);
    }
};

template <typename T>
struct TupleAccessor<T, 3>
{
    static auto tieMembers(T& value) noexcept
    {
        auto& [v1, v2, v3] = value;
        return std::tie(v1, v2, v3);
    }
};

template <typename T>
struct TupleAccessor<T, 4>
{
    static auto tieMembers(T& value) noexcept
    {
        auto& [v1, v2, v3, v4] = value;
        return std::tie(v1, v2, v3, v4);
    }
};

template <typename T>
struct TupleAccessor<T, 5>
{
    static auto tieMembers(T& value) noexcept
    {
        auto& [v1, v2, v3, v4, v5] = value;
        return std::tie(v1, v2, v3, v4, v5);
    }
};

template <typename T>
struct TupleAccessor<T, 6>
{
    static auto tieMembers(T& value) noexcept
    {
        auto& [v1, v2, v3, v4, v5, v6] = value;
        return std::tie(v1, v2, v3, v4, v5, v6);
    }
};

template <typename T>
struct TupleAccessor<T, 7>
{
    static auto tieMembers(T& value) noexcept
    {
        auto& [v1, v2, v3, v4, v5, v6, v7] = value;
        return std::tie(v1, v2, v3, v4, v5, v6, v7);
    }
};

template <typename T>
struct TupleAccessor<T, 8>
{
    static auto tieMembers(T& value) noexcept
    {
        auto& [v1, v2, v3, v4, v5, v6, v7, v8] = value;
        return std::tie(v1, v2, v3, v4, v5, v6, v7, v8);
    }
};

template <typename T>
struct TupleAccessor<T, 9>
{
    static auto tieMembers(T& value) noexcept
    {
        auto& [v1, v2, v3, v4, v5, v6, v7, v8, v9] = value;
        return std::tie(v1, v2, v3, v4, v5, v6, v7, v8, v9);
    }
};

template <typename T>
struct TupleAccessor<T, 10>
{
    static auto tieMembers(T& value) noexcept
    {
        auto& [v1, v2, v3, v4, v5, v6, v7, v8, v9, v10] = value;
        return std::tie(v1, v2, v3, v4, v5, v6, v7, v8, v9, v10);
    }
};

template <typename T>
struct TupleAccessor<T, 11>
{
    static auto tieMembers(T& value) noexcept
    {
        auto& [v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11] = value;
        return std::tie(v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11);
    }
};

template <typename T>
struct TupleAccessor<T, 12>
{
    static auto tieMembers(T& value) noexcept
    {
        auto& [v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12] = value;
        return std::tie(v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12);
    }
};

template <typename T>
struct TupleAccessor<T, 13>
{
    static auto tieMembers(T& value) noexcept
    {
        auto& [v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13] = value;
        return std::tie(v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13);
    }
};

template <typename T>
struct TupleAccessor<T, 14>
{
    static auto tieMembers(T& value) noexcept
    {
        auto& [v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14] = value;
        return std::tie(v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14);
    }
};

template <typename T>
struct TupleAccessor<T, 15>
{
    static auto tieMembers(T& value) noexcept
    {
        auto& [v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15] = value;
        return std::tie(v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15);
    }
};

template <typename T>
struct TupleAccessor<T, 16>
{
    static auto tieMembers(T& value) noexcept
    {
        auto& [v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16] = value;
        return std::tie(v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16);
    }
};

// =============================================================================
// PfrAccessor - 通过结构化绑定将聚合体成员解包为 tuple
// T 可以是 const 限定：T& 模板形参自然推导为 const SomeStruct&
// =============================================================================
struct PfrAccessor
{
    template <typename T>
    static auto tieMembers(T& value) noexcept
    {
        constexpr std::size_t FIELD_COUNT = CountFields<std::remove_cvref_t<T>>();
        static_assert(FIELD_COUNT <= MAX_FIELD_COUNT,
                      "Member count exceeds MAX_FIELD_COUNT. Increase MAX_FIELD_COUNT and extend TupleAccessor.");
        return TupleAccessor<T, FIELD_COUNT>::tieMembers(value);
    }
};

// StructureToTuple - 单一模板处理 const/non-const（T 由编译器推导，无需 const_cast）
template <typename T>
auto StructureToTuple(T& obj) noexcept
{
    static_assert(std::is_aggregate_v<std::remove_cvref_t<T>>, "StructureToTuple requires an aggregate type");
    return PfrAccessor::tieMembers(obj);
}

// ForEachField - 对每个字段调用 func(field)，支持 const 对象
template <typename T, typename F>
constexpr void ForEachField(T& obj, F&& func)
{
    auto tup = StructureToTuple(obj);
    std::apply([&func](auto&&... fields) { (func(std::forward<decltype(fields)>(fields)), ...); }, tup);
}

// ForEachFieldWithIndex - 对每个字段调用 func(field, index_constant)，支持编译期索引
template <typename T, typename F>
constexpr void ForEachFieldWithIndex(T& obj, F&& func)
{
    auto tup = StructureToTuple(obj);
    [&]<std::size_t... Is>(std::index_sequence<Is...>)
    {
        (std::forward<F>(func)(std::get<Is>(tup), std::integral_constant<std::size_t, Is>{}), ...);
    }(std::make_index_sequence<std::tuple_size_v<decltype(tup)>>{});
}

// field_get - 按索引获取成员引用（避免与 std::get 产生 ADL 冲突）
template <std::size_t I, typename T>
constexpr decltype(auto) FieldGet(T& obj) noexcept
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
        // GCC / MSVC: "... {M = obj.field_name}" / "... <{obj.field_name}>(void)"
#if defined(__clang__)
        const std::size_t end = sig.find_last_of(']');
#else
        const std::size_t end = sig.find_last_of('}');
#endif
        if (end == std::string_view::npos) return sig;
        const std::size_t dot = sig.rfind('.', end);
        if (dot == std::string_view::npos || dot + 1 >= end) return sig;
        return sig.substr(dot + 1, end - dot - 1);
    }
};

template <auto& Member>
constexpr std::string_view GetNameImpl() noexcept
{
#if defined(_MSC_VER) && !defined(__clang__)
    return NameParser::cleanup({__FUNCSIG__, sizeof(__FUNCSIG__) - 1});
#else
    return NameParser::cleanup({__PRETTY_FUNCTION__, sizeof(__PRETTY_FUNCTION__) - 1});
#endif
}

template <typename T>
inline constexpr T FAKE_DUMMY_OBJECT = {};

template <typename T, std::size_t Count>
struct NameAccessorImpl;

template <typename T>
struct NameAccessorImpl<T, 0>
{
    template <std::size_t I>
    static constexpr std::string_view getName() noexcept
    {
        return "out_of_range";
    }
};

template <typename T>
struct NameAccessorImpl<T, 1>
{
    template <std::size_t I>
    static constexpr std::string_view getName() noexcept
    {
        constexpr auto NAMES = []() noexcept
        {
            auto& [v1] = FAKE_DUMMY_OBJECT<T>;
            return std::array<std::string_view, 1>{GetNameImpl<v1>()};
        }();
        if constexpr (I < NAMES.size()) return NAMES[I];
        return "out_of_range";
    }
};

template <typename T>
struct NameAccessorImpl<T, 2>
{
    template <std::size_t I>
    static constexpr std::string_view getName() noexcept
    {
        constexpr auto NAMES = []() noexcept
        {
            auto& [v1, v2] = FAKE_DUMMY_OBJECT<T>;
            return std::array<std::string_view, 2>{GetNameImpl<v1>(), GetNameImpl<v2>()};
        }();
        if constexpr (I < NAMES.size()) return NAMES[I];
        return "out_of_range";
    }
};

template <typename T>
struct NameAccessorImpl<T, 3>
{
    template <std::size_t I>
    static constexpr std::string_view getName() noexcept
    {
        constexpr auto NAMES = []() noexcept
        {
            auto& [v1, v2, v3] = FAKE_DUMMY_OBJECT<T>;
            return std::array<std::string_view, 3>{GetNameImpl<v1>(), GetNameImpl<v2>(), GetNameImpl<v3>()};
        }();
        if constexpr (I < NAMES.size()) return NAMES[I];
        return "out_of_range";
    }
};

template <typename T>
struct NameAccessorImpl<T, 4>
{
    template <std::size_t I>
    static constexpr std::string_view getName() noexcept
    {
        constexpr auto NAMES = []() noexcept
        {
            auto& [v1, v2, v3, v4] = FAKE_DUMMY_OBJECT<T>;
            return std::array<std::string_view, 4>{
                GetNameImpl<v1>(), GetNameImpl<v2>(), GetNameImpl<v3>(), GetNameImpl<v4>()};
        }();
        if constexpr (I < NAMES.size()) return NAMES[I];
        return "out_of_range";
    }
};

template <typename T>
struct NameAccessorImpl<T, 5>
{
    template <std::size_t I>
    static constexpr std::string_view getName() noexcept
    {
        constexpr auto NAMES = []() noexcept
        {
            auto& [v1, v2, v3, v4, v5] = FAKE_DUMMY_OBJECT<T>;
            return std::array<std::string_view, 5>{
                GetNameImpl<v1>(), GetNameImpl<v2>(), GetNameImpl<v3>(), GetNameImpl<v4>(), GetNameImpl<v5>()};
        }();
        if constexpr (I < NAMES.size()) return NAMES[I];
        return "out_of_range";
    }
};

template <typename T>
struct NameAccessorImpl<T, 6>
{
    template <std::size_t I>
    static constexpr std::string_view getName() noexcept
    {
        constexpr auto NAMES = []() noexcept
        {
            auto& [v1, v2, v3, v4, v5, v6] = FAKE_DUMMY_OBJECT<T>;
            return std::array<std::string_view, 6>{GetNameImpl<v1>(),
                                                   GetNameImpl<v2>(),
                                                   GetNameImpl<v3>(),
                                                   GetNameImpl<v4>(),
                                                   GetNameImpl<v5>(),
                                                   GetNameImpl<v6>()};
        }();
        if constexpr (I < NAMES.size()) return NAMES[I];
        return "out_of_range";
    }
};

template <typename T>
struct NameAccessorImpl<T, 7>
{
    template <std::size_t I>
    static constexpr std::string_view getName() noexcept
    {
        constexpr auto NAMES = []() noexcept
        {
            auto& [v1, v2, v3, v4, v5, v6, v7] = FAKE_DUMMY_OBJECT<T>;
            return std::array<std::string_view, 7>{GetNameImpl<v1>(),
                                                   GetNameImpl<v2>(),
                                                   GetNameImpl<v3>(),
                                                   GetNameImpl<v4>(),
                                                   GetNameImpl<v5>(),
                                                   GetNameImpl<v6>(),
                                                   GetNameImpl<v7>()};
        }();
        if constexpr (I < NAMES.size()) return NAMES[I];
        return "out_of_range";
    }
};

template <typename T>
struct NameAccessorImpl<T, 8>
{
    template <std::size_t I>
    static constexpr std::string_view getName() noexcept
    {
        constexpr auto NAMES = []() noexcept
        {
            auto& [v1, v2, v3, v4, v5, v6, v7, v8] = FAKE_DUMMY_OBJECT<T>;
            return std::array<std::string_view, 8>{GetNameImpl<v1>(),
                                                   GetNameImpl<v2>(),
                                                   GetNameImpl<v3>(),
                                                   GetNameImpl<v4>(),
                                                   GetNameImpl<v5>(),
                                                   GetNameImpl<v6>(),
                                                   GetNameImpl<v7>(),
                                                   GetNameImpl<v8>()};
        }();
        if constexpr (I < NAMES.size()) return NAMES[I];
        return "out_of_range";
    }
};

template <typename T>
struct NameAccessorImpl<T, 9>
{
    template <std::size_t I>
    static constexpr std::string_view getName() noexcept
    {
        constexpr auto NAMES = []() noexcept
        {
            auto& [v1, v2, v3, v4, v5, v6, v7, v8, v9] = FAKE_DUMMY_OBJECT<T>;
            return std::array<std::string_view, 9>{GetNameImpl<v1>(),
                                                   GetNameImpl<v2>(),
                                                   GetNameImpl<v3>(),
                                                   GetNameImpl<v4>(),
                                                   GetNameImpl<v5>(),
                                                   GetNameImpl<v6>(),
                                                   GetNameImpl<v7>(),
                                                   GetNameImpl<v8>(),
                                                   GetNameImpl<v9>()};
        }();
        if constexpr (I < NAMES.size()) return NAMES[I];
        return "out_of_range";
    }
};

template <typename T>
struct NameAccessorImpl<T, 10>
{
    template <std::size_t I>
    static constexpr std::string_view getName() noexcept
    {
        constexpr auto NAMES = []() noexcept
        {
            auto& [v1, v2, v3, v4, v5, v6, v7, v8, v9, v10] = FAKE_DUMMY_OBJECT<T>;
            return std::array<std::string_view, 10>{GetNameImpl<v1>(),
                                                    GetNameImpl<v2>(),
                                                    GetNameImpl<v3>(),
                                                    GetNameImpl<v4>(),
                                                    GetNameImpl<v5>(),
                                                    GetNameImpl<v6>(),
                                                    GetNameImpl<v7>(),
                                                    GetNameImpl<v8>(),
                                                    GetNameImpl<v9>(),
                                                    GetNameImpl<v10>()};
        }();
        if constexpr (I < NAMES.size()) return NAMES[I];
        return "out_of_range";
    }
};

template <typename T>
struct NameAccessorImpl<T, 11>
{
    template <std::size_t I>
    static constexpr std::string_view getName() noexcept
    {
        constexpr auto NAMES = []() noexcept
        {
            auto& [v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11] = FAKE_DUMMY_OBJECT<T>;
            return std::array<std::string_view, 11>{GetNameImpl<v1>(),
                                                    GetNameImpl<v2>(),
                                                    GetNameImpl<v3>(),
                                                    GetNameImpl<v4>(),
                                                    GetNameImpl<v5>(),
                                                    GetNameImpl<v6>(),
                                                    GetNameImpl<v7>(),
                                                    GetNameImpl<v8>(),
                                                    GetNameImpl<v9>(),
                                                    GetNameImpl<v10>(),
                                                    GetNameImpl<v11>()};
        }();
        if constexpr (I < NAMES.size()) return NAMES[I];
        return "out_of_range";
    }
};

template <typename T>
struct NameAccessorImpl<T, 12>
{
    template <std::size_t I>
    static constexpr std::string_view getName() noexcept
    {
        constexpr auto NAMES = []() noexcept
        {
            auto& [v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12] = FAKE_DUMMY_OBJECT<T>;
            return std::array<std::string_view, 12>{GetNameImpl<v1>(),
                                                    GetNameImpl<v2>(),
                                                    GetNameImpl<v3>(),
                                                    GetNameImpl<v4>(),
                                                    GetNameImpl<v5>(),
                                                    GetNameImpl<v6>(),
                                                    GetNameImpl<v7>(),
                                                    GetNameImpl<v8>(),
                                                    GetNameImpl<v9>(),
                                                    GetNameImpl<v10>(),
                                                    GetNameImpl<v11>(),
                                                    GetNameImpl<v12>()};
        }();
        if constexpr (I < NAMES.size()) return NAMES[I];
        return "out_of_range";
    }
};

template <typename T>
struct NameAccessorImpl<T, 13>
{
    template <std::size_t I>
    static constexpr std::string_view getName() noexcept
    {
        constexpr auto NAMES = []() noexcept
        {
            auto& [v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13] = FAKE_DUMMY_OBJECT<T>;
            return std::array<std::string_view, 13>{GetNameImpl<v1>(),
                                                    GetNameImpl<v2>(),
                                                    GetNameImpl<v3>(),
                                                    GetNameImpl<v4>(),
                                                    GetNameImpl<v5>(),
                                                    GetNameImpl<v6>(),
                                                    GetNameImpl<v7>(),
                                                    GetNameImpl<v8>(),
                                                    GetNameImpl<v9>(),
                                                    GetNameImpl<v10>(),
                                                    GetNameImpl<v11>(),
                                                    GetNameImpl<v12>(),
                                                    GetNameImpl<v13>()};
        }();
        if constexpr (I < NAMES.size()) return NAMES[I];
        return "out_of_range";
    }
};

template <typename T>
struct NameAccessorImpl<T, 14>
{
    template <std::size_t I>
    static constexpr std::string_view getName() noexcept
    {
        constexpr auto NAMES = []() noexcept
        {
            auto& [v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14] = FAKE_DUMMY_OBJECT<T>;
            return std::array<std::string_view, 14>{GetNameImpl<v1>(),
                                                    GetNameImpl<v2>(),
                                                    GetNameImpl<v3>(),
                                                    GetNameImpl<v4>(),
                                                    GetNameImpl<v5>(),
                                                    GetNameImpl<v6>(),
                                                    GetNameImpl<v7>(),
                                                    GetNameImpl<v8>(),
                                                    GetNameImpl<v9>(),
                                                    GetNameImpl<v10>(),
                                                    GetNameImpl<v11>(),
                                                    GetNameImpl<v12>(),
                                                    GetNameImpl<v13>(),
                                                    GetNameImpl<v14>()};
        }();
        if constexpr (I < NAMES.size()) return NAMES[I];
        return "out_of_range";
    }
};

template <typename T>
struct NameAccessorImpl<T, 15>
{
    template <std::size_t I>
    static constexpr std::string_view getName() noexcept
    {
        constexpr auto NAMES = []() noexcept
        {
            auto& [v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15] = FAKE_DUMMY_OBJECT<T>;
            return std::array<std::string_view, 15>{GetNameImpl<v1>(),
                                                    GetNameImpl<v2>(),
                                                    GetNameImpl<v3>(),
                                                    GetNameImpl<v4>(),
                                                    GetNameImpl<v5>(),
                                                    GetNameImpl<v6>(),
                                                    GetNameImpl<v7>(),
                                                    GetNameImpl<v8>(),
                                                    GetNameImpl<v9>(),
                                                    GetNameImpl<v10>(),
                                                    GetNameImpl<v11>(),
                                                    GetNameImpl<v12>(),
                                                    GetNameImpl<v13>(),
                                                    GetNameImpl<v14>(),
                                                    GetNameImpl<v15>()};
        }();
        if constexpr (I < NAMES.size()) return NAMES[I];
        return "out_of_range";
    }
};

template <typename T>
struct NameAccessorImpl<T, 16>
{
    template <std::size_t I>
    static constexpr std::string_view getName() noexcept
    {
        constexpr auto NAMES = []() noexcept
        {
            auto& [v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16] = FAKE_DUMMY_OBJECT<T>;
            return std::array<std::string_view, 16>{GetNameImpl<v1>(),
                                                    GetNameImpl<v2>(),
                                                    GetNameImpl<v3>(),
                                                    GetNameImpl<v4>(),
                                                    GetNameImpl<v5>(),
                                                    GetNameImpl<v6>(),
                                                    GetNameImpl<v7>(),
                                                    GetNameImpl<v8>(),
                                                    GetNameImpl<v9>(),
                                                    GetNameImpl<v10>(),
                                                    GetNameImpl<v11>(),
                                                    GetNameImpl<v12>(),
                                                    GetNameImpl<v13>(),
                                                    GetNameImpl<v14>(),
                                                    GetNameImpl<v15>(),
                                                    GetNameImpl<v16>()};
        }();
        if constexpr (I < NAMES.size()) return NAMES[I];
        return "out_of_range";
    }
};

template <typename T>
struct NameAccessor
{
    template <std::size_t I>
    static constexpr std::string_view getName() noexcept
    {
        constexpr std::size_t FIELD_COUNT = CountFields<T>();
        return NameAccessorImpl<T, FIELD_COUNT>::template getName<I>();
    }
};

// field_name<I, T>() - 获取第 I 个成员的名称字符串
template <std::size_t I, typename T>
constexpr std::string_view FieldName() noexcept
{
    return NameAccessor<std::remove_cvref_t<T>>::template getName<I>();
}

} // namespace ui::traits