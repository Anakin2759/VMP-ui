/**
 * ************************************************************************
 *
 * @file Utils.hpp
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2026-02-06
 * @version 0.1
 * @brief 对外部提供的通用工具函数
 *
 * ************************************************************************
 * @copyright Copyright (c) 2026 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */
#pragma once

#include <cstdint>
#include <entt/entt.hpp>
#include "../common/Policies.hpp"
#include "Chains.hpp"
namespace ui::utils
{

bool HasAlignment(policies::Alignment value, policies::Alignment flag);
void SetWindowFlag(::entt::entity entity, policies::WindowFlag flag);

void MarkLayoutChanged(::entt::entity entity);
void MarkVisualChanged(::entt::entity entity);
void MarkLayoutAndVisualChanged(::entt::entity entity);

void MarkLayoutDirty(::entt::entity entity);
void MarkRenderDirty(::entt::entity entity);
void CloseWindow(::entt::entity entity);
void QuitUiEventLoop();

void InvokeTask(std::move_only_function<void()> func);
using TaskHandle = uint32_t;
/**
 * @brief 注册一个定时任务，返回任务句柄
 * @param interval 间隔时间（毫秒）
 * @param func 任务函数
 * @return 任务句柄
 */
TaskHandle TimerCallback(uint32_t interval, std::move_only_function<void()> func);
/**
 * @brief 取消注册一个定时任务
 * @param handle 任务句柄
 */
void CancelQueuedTask(TaskHandle handle);

// 判断根据别名判断实体是否存在
bool IsEntityExist(const std::string& alias);

} // namespace ui::utils

namespace ui::actions
{
namespace utils
{
inline constexpr EntityAction<&ui::utils::SetWindowFlag> SET_WINDOW_FLAG_ACTION{};
} // namespace utils
} // namespace ui::actions

namespace ui::chains
{
inline auto WindowFlag(policies::WindowFlag flag)
{
    return ui::actions::utils::SET_WINDOW_FLAG_ACTION.bind(flag);
}

} // namespace ui::chains
