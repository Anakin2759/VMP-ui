/**
 * ************************************************************************
 *
 * @file TimerSystem.hpp
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2026-02-03
 * @version 0.1
 * @brief 定时器系统 - 处理定时任务的调度和执行
    取代现在的简单计时器实现，提供更灵活和强大的定时任务管理功能
    - 支持一次性和重复定时任务
    - 支持任务取消和修改
    - 支持任务优先级和分组
    - 提供任务执行状态查询接口

    - 管理全局定时任务列表
    - 在每帧更新时检查并执行到期任务
    - 检查任务的取消请求
    - 保存任务的执行状态

    调用流程:
    1. 注册: 业务逻辑调用 TimerSystem::addTask() 注册任务 (指定间隔、回调、类型)
    2. 驱动: 游戏主循环/UpdateSystem 计算帧时间 deltaMs
    3. 触发: 系统接收 UpdateTimer 事件或每帧调用 update(deltaMs)
    4. 检查: 遍历所有活动任务，累加流逝时间
    5. 执行: 若累加时间 >= 设定间隔 -> 执行回调
    6. 维护: 单次任务执行后移除，循环任务重置计时
 *
 * ************************************************************************
 * @copyright Copyright (c) 2026 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */

#pragma once
#include <cstdint>
#include <entt/entt.hpp>
#include <vector>
#include "../common/Events.hpp"
#include "../common/GlobalContext.hpp"
#include "../interface/Isystem.hpp"
#include "../singleton/Dispatcher.hpp"
namespace ui::systems
{
/**
 * @brief 定时器系统
 */
class TimerSystem : public interface::EnableRegister<TimerSystem>
{
public:
    /**
     * @brief 注册事件处理器
     */
    void registerHandlersImpl();

    /**
     * @brief 注销事件处理器
     */
    void unregisterHandlersImpl();

    ui::interface::SystemPhase getPhase() { return ui::interface::SystemPhase::Frame; }

    /**
     * @brief 添加定时任务
     * @param interval 间隔时间（毫秒）
     * @param func 任务函数
     * @param singleShot 是否单次执行（默认为false，重复执行）
     * @return 任务句柄
     */
    static uint32_t addTask(uint32_t interval, VoidCallback func, bool singleShot = false);

    /**
     * @brief 取消定时任务
     * @param handle 任务句柄
     */
    static void cancelTask(uint32_t handle);

    /**
     * @brief 更新定时器状态（每帧调用）
     * @param deltaMs 时间增量（毫秒）
     */
    static void update(uint32_t deltaMs);

private:
    void onUpdateTimer(const events::UpdateTimer& event);
};
} // namespace ui::systems