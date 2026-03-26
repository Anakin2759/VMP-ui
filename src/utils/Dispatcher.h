/**
 * ************************************************************************
 *
 * @file Dispatcher.h
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2025-12-05
 * @version 0.2
 * @brief 全局单例事件分发器
 *
 * 支持两种事件分发模式：
 * 1. 紧急事件 (trigger) - 立即执行，同步调用所有监听器
 * 2. 缓冲区事件 (enqueue) - 加入队列，在事件循环的 update() 中批量处理
 *
 * 使用指南：
 * - trigger: 用于需要立即响应的事件，如 QuitRequested, UpdateRendering,
 *   WindowGraphicsContextSetEvent
 * - enqueue: 用于可以延迟处理的原始输入或状态事件
 * - update: 在事件循环每帧调用，处理所有缓冲区事件
 *
 * ************************************************************************
 * @copyright Copyright (c) 2025 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */

#pragma once
#include <sys/stat.h>
#include <entt/entt.hpp>

namespace utils
{
class Dispatcher
{
public:
    static entt::dispatcher& getInstance()
    {
        static entt::dispatcher instance;
        return instance;
    }
    Dispatcher(const Dispatcher&) = delete;
    Dispatcher& operator=(const Dispatcher&) = delete;
    Dispatcher(Dispatcher&&) = delete;
    Dispatcher& operator=(Dispatcher&&) = delete;

private:
    Dispatcher() = default;
    ~Dispatcher() = default;
};
} // namespace utils
