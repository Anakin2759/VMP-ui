/**
 * ************************************************************************
 *
 * @file IRenderer.hpp
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2026-01-30
 * @version 0.1
 * @brief 渲染器接口 - 定义所有渲染器的通用接口
 *
 * ************************************************************************
 * @copyright Copyright (c) 2026 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */

#pragma once
#include <entt/entt.hpp>
#include "core/RenderContext.hpp"

namespace ui::core
{

/**
 * @brief 渲染器接口
 *
 * 所有专门的渲染器都应实现此接口。
 * 每个渲染器负责处理特定类型的渲染任务。
 */
class IRenderer
{
public:
    IRenderer() = default;
    virtual ~IRenderer() = default;
    IRenderer(const IRenderer&) = delete;
    IRenderer& operator=(const IRenderer&) = delete;
    IRenderer(IRenderer&&) = delete;
    IRenderer& operator=(IRenderer&&) = delete;

    /**
     * @brief 判断该渲染器是否能处理指定实体
     * @param entity 要检查的实体
     * @return true 如果该渲染器可以处理此实体
     */
    [[nodiscard]] virtual bool canHandle(entt::entity entity) const = 0;

    /**
     * @brief 收集实体的渲染数据并添加到批次管理器
     * @param entity 要渲染的实体
     * @param context 渲染上下文
     */
    virtual void collect(entt::entity entity, RenderContext& context) = 0;

    /**
     * @brief 获取渲染器的优先级（用于排序）
     * @return 优先级值，越小越先执行
     */
    [[nodiscard]] virtual int getPriority() const { return 0; }
};

} // namespace ui::core
