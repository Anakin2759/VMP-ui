/**
 * ************************************************************************
 *
 * @file Registry.h
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2026-01-26
 * @version 0.1
 * @brief UI模块的全局实体注册表单例封装
  - 基于 EnTT 实现的全局实体注册表单例
  - 提供统一的实体和组件管理接口
 *
 * ************************************************************************
 * @copyright Copyright (c) 2026 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */
#pragma once

#include <cstdio>
#include <exception>

#include <entt/entt.hpp>

#include "traits/ComponentsTraits.hpp"

namespace ui
{
using traits::ComponentOrUiTag;

class UiRuntime;
class UiRuntimeScope;
class WorkerMailbox;

class Registry
{
    friend class UiRuntime;
    friend class UiRuntimeScope;
    friend class RuntimeFacade;
    friend class WorkerMailbox;

public:
    static Registry& current()
    {
        auto* instance = activeInstance();
        if (instance == nullptr) [[unlikely]]
        {
            [[maybe_unused]] const int ignored =
                std::fputs("[Registry] current() called outside UiRuntimeScope\n", stderr);
            std::terminate();
        }
        return *instance;
    }

    // -------------------------------------------------------------------------
    // Instance methods — dependency-injected systems use these via Registry&
    // -------------------------------------------------------------------------

    [[nodiscard]] entt::entity create() { return m_registry.create(); }

    [[nodiscard]] bool valid(entt::entity entity) const noexcept { return m_registry.valid(entity); }

    void destroy(entt::entity entity) { m_registry.destroy(entity); }

    template <ComponentOrUiTag... Type>
    [[nodiscard]] auto view()
    {
        return m_registry.view<Type...>();
    }

    template <ComponentOrUiTag... Type, typename... Exclude>
    [[nodiscard]] auto view(entt::exclude_t<Exclude...> excl)
    {
        return m_registry.view<Type...>(excl);
    }

    template <ComponentOrUiTag... Type>
    [[nodiscard]] auto view() const
    {
        return m_registry.view<Type...>();
    }

    template <ComponentOrUiTag... Type, typename... Exclude>
    [[nodiscard]] auto view(entt::exclude_t<Exclude...> excl) const
    {
        return m_registry.view<Type...>(excl);
    }

    template <typename... Owned, typename... Get, typename... Exclude>
    [[nodiscard]] auto group(entt::get_t<Get...> get = {}, entt::exclude_t<Exclude...> exclude = {})
    {
        return m_registry.group<Owned...>(get, exclude);
    }

    template <ComponentOrUiTag Type>
    [[nodiscard]] Type& get(entt::entity entity)
    {
        return m_registry.get<Type>(entity);
    }

    template <ComponentOrUiTag Type>
    [[nodiscard]] const Type& get(entt::entity entity) const
    {
        return m_registry.get<Type>(entity);
    }

    template <ComponentOrUiTag Type>
    [[nodiscard]] Type* try_get(entt::entity entity) noexcept
    {
        return m_registry.try_get<Type>(entity);
    }

    template <ComponentOrUiTag Type>
    [[nodiscard]] const Type* try_get(entt::entity entity) const noexcept
    {
        return m_registry.try_get<Type>(entity);
    }

    template <ComponentOrUiTag Type, typename... Args>
    decltype(auto) emplace(entt::entity entity, Args&&... args)
    {
        return m_registry.emplace<Type>(entity, std::forward<Args>(args)...);
    }

    template <ComponentOrUiTag Type, typename... Args>
    Type& replace(entt::entity entity, Args&&... args)
    {
        return m_registry.replace<Type>(entity, std::forward<Args>(args)...);
    }

    template <ComponentOrUiTag Type, typename... Args>
    decltype(auto) emplace_or_replace(entt::entity entity, Args&&... args)
    {
        return m_registry.emplace_or_replace<Type>(entity, std::forward<Args>(args)...);
    }

    template <ComponentOrUiTag Type, typename... Args>
    Type& get_or_emplace(entt::entity entity, Args&&... args)
    {
        return m_registry.get_or_emplace<Type>(entity, std::forward<Args>(args)...);
    }

    template <ComponentOrUiTag Type>
    void remove(entt::entity entity)
    {
        m_registry.remove<Type>(entity);
    }

    template <ComponentOrUiTag... Type>
    [[nodiscard]] bool any_of(entt::entity entity) const
    {
        return m_registry.any_of<Type...>(entity);
    }

    template <ComponentOrUiTag... Type>
    [[nodiscard]] bool all_of(entt::entity entity) const
    {
        return m_registry.all_of<Type...>(entity);
    }

    template <ComponentOrUiTag... Type>
    void clear()
    {
        m_registry.clear<Type...>();
    }

    void clear() { m_registry.clear(); }

    template <ComponentOrUiTag Type>
    [[nodiscard]] auto on_construct()
    {
        return m_registry.on_construct<Type>();
    }

    template <ComponentOrUiTag Type>
    [[nodiscard]] auto on_destroy()
    {
        return m_registry.on_destroy<Type>();
    }

    template <ComponentOrUiTag Type>
    [[nodiscard]] auto on_update()
    {
        return m_registry.on_update<Type>();
    }

    [[nodiscard]] auto& ctx() noexcept { return m_registry.ctx(); }

    [[nodiscard]] const auto& ctx() const noexcept { return m_registry.ctx(); }

    // Legacy PascalCase entrypoints stay for compatibility with existing UI call sites.
    // NOLINTBEGIN(readability-identifier-naming)
    [[deprecated(
        "Use injected entt::registry& or RuntimeFacade::current().enttRegistry() instead of Registry::Create().")]]
    static entt::entity Create()
    {
        return current().m_registry.create();
    }

    template <ComponentOrUiTag... Type>
    [[deprecated("Use injected entt::registry& or RuntimeFacade::current().enttRegistry().view(...) instead of "
                 "Registry::View().")]]
    static auto View()
    {
        return current().m_registry.view<Type...>();
    }

    template <typename... Owned, typename... Get, typename... Exclude>
    [[deprecated("Use injected entt::registry& or RuntimeFacade::current().enttRegistry().group(...) instead of "
                 "Registry::Group().")]]
    static auto Group(entt::get_t<Get...> get = {}, entt::exclude_t<Exclude...> exclude = {})
    {
        return current().m_registry.group<Owned...>(get, exclude);
    }

    template <ComponentOrUiTag Type>
    [[deprecated("Use injected entt::registry& or RuntimeFacade::current().enttRegistry().get(...) instead of "
                 "Registry::Get().")]]
    static auto Get(::entt::entity entity) -> Type&
    {
        return current().m_registry.get<Type>(entity);
    }

    template <ComponentOrUiTag Type>
    [[deprecated("Use injected entt::registry& or RuntimeFacade::current().enttRegistry().try_get(...) instead of "
                 "Registry::TryGet().")]]
    static auto TryGet(::entt::entity entity) -> Type*
    {
        return current().m_registry.try_get<Type>(entity);
    }

    template <ComponentOrUiTag Type, typename... Args>
    [[deprecated("Use injected entt::registry& or RuntimeFacade::current().enttRegistry().emplace(...) instead of "
                 "Registry::Emplace().")]]
    static decltype(auto) Emplace(::entt::entity entity, Args&&... args)
    {
        return current().m_registry.emplace<Type>(entity, std::forward<Args>(args)...);
    }

    template <ComponentOrUiTag Type, typename... Args>
    [[deprecated("Use injected entt::registry& or RuntimeFacade::current().enttRegistry().replace(...) instead of "
                 "Registry::Replace().")]]
    static auto Replace(::entt::entity entity, Args&&... args) -> Type&
    {
        return current().m_registry.replace<Type>(entity, std::forward<Args>(args)...);
    }

    template <ComponentOrUiTag Type, typename... Args>
    [[deprecated("Use injected entt::registry& or RuntimeFacade::current().enttRegistry().emplace_or_replace(...) "
                 "instead of Registry::EmplaceOrReplace().")]]
    static void EmplaceOrReplace(::entt::entity entity, Args&&... args)
    {
        current().m_registry.emplace_or_replace<Type>(entity, std::forward<Args>(args)...);
    }
    template <ComponentOrUiTag Type, typename... Args>
    [[deprecated("Use injected entt::registry& or RuntimeFacade::current().enttRegistry().get_or_emplace(...) instead "
                 "of Registry::GetOrEmplace().")]]
    static auto GetOrEmplace(::entt::entity entity, Args&&... args) -> Type&
    {
        return current().m_registry.get_or_emplace<Type>(entity, std::forward<Args>(args)...);
    }

    template <ComponentOrUiTag Type>
    [[deprecated("Use injected entt::registry& or RuntimeFacade::current().enttRegistry().remove(...) instead of "
                 "Registry::Remove().")]]
    static void Remove(::entt::entity entity)
    {
        current().m_registry.remove<Type>(entity);
    }

    template <ComponentOrUiTag... Type>
    [[deprecated("Use injected entt::registry& or RuntimeFacade::current().enttRegistry().any_of(...) instead of "
                 "Registry::AnyOf().")]]
    static auto AnyOf(::entt::entity entity) -> bool
    {
        return current().m_registry.any_of<Type...>(entity);
    }

    template <ComponentOrUiTag... Type>
    [[deprecated("Use injected entt::registry& or RuntimeFacade::current().enttRegistry().all_of(...) instead of "
                 "Registry::AllOf().")]]
    static auto AllOf(::entt::entity entity) -> bool
    {
        return current().m_registry.all_of<Type...>(entity);
    }

    [[deprecated("Use injected entt::registry& or RuntimeFacade::current().enttRegistry().valid(...) instead of "
                 "Registry::Valid().")]]
    static bool Valid(::entt::entity entity)
    {
        return current().m_registry.valid(entity);
    }

    [[deprecated("Use injected entt::registry& or RuntimeFacade::current().enttRegistry().destroy(...) instead of "
                 "Registry::Destroy().")]]
    static void Destroy(::entt::entity entity)
    {
        current().m_registry.destroy(entity);
    }

    template <ComponentOrUiTag... Type>
    [[deprecated("Use injected entt::registry& or RuntimeFacade::current().enttRegistry().clear(...) instead of "
                 "Registry::Clear().")]]
    static auto Clear()
    {
        return current().m_registry.clear<Type...>();
    }

    [[deprecated("Use injected entt::registry& or RuntimeFacade::current().enttRegistry().clear() instead of "
                 "Registry::Clear().")]]
    static void Clear()
    {
        current().m_registry.clear();
    }

    template <ComponentOrUiTag Type>
    [[deprecated("Use injected entt::registry& or RuntimeFacade::current().enttRegistry().on_update<Type>() instead of "
                 "Registry::OnUpdate().")]]
    static auto OnUpdate()
    {
        return current().m_registry.on_update<Type>();
    }
    template <ComponentOrUiTag Type>
    [[deprecated("Use injected entt::registry& or RuntimeFacade::current().enttRegistry().on_destroy<Type>() instead "
                 "of Registry::OnDestroy().")]]
    static auto OnDestroy()
    {
        return current().m_registry.on_destroy<Type>();
    }
    /**
     * @brief 组件构造事件回调连接器
     * @return entt::sink<ComponentConstructedSignature>
     */
    template <ComponentOrUiTag Type>
    [[deprecated("Use injected entt::registry& or RuntimeFacade::current().enttRegistry().on_construct<Type>() instead "
                 "of Registry::OnConstruct().")]]
    static auto OnConstruct()
    {
        return current().m_registry.on_construct<Type>();
    }
    // NOLINTEND(readability-identifier-naming)

private:
    static Registry*& activeInstance()
    {
        static thread_local Registry* instance = nullptr;
        return instance;
    }

    static Registry* swapActiveInstance(Registry* instance)
    {
        Registry* previous = activeInstance();
        activeInstance() = instance;
        return previous;
    }

    Registry() = default;

    [[nodiscard]] entt::registry& raw() noexcept { return m_registry; }

    [[nodiscard]] const entt::registry& raw() const noexcept { return m_registry; }

    entt::registry m_registry;
};
} // namespace ui