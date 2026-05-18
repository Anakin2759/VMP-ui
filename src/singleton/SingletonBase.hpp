/**
 * ************************************************************************
 *
 * @file SingletonBase.h
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2026-01-26
 * @version 0.1
 * @brief 单例基类模板
 *
 * ************************************************************************
 * @copyright Copyright (c) 2026 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */

#pragma once

#include <memory>
#include <mutex>
#include <utility>

template <typename Device>
class SingletonBase
{
public:
    // 使用可变参数模板 args...
    template <typename... Args>
    static Device& getInstance(Args&&... args)
    {
        // 使用 lambda 配合静态局部变量，确保只初始化一次
        static Device instance(std::forward<Args>(args)...);
        return instance;
    }

    SingletonBase(const SingletonBase&) = delete;
    SingletonBase& operator=(const SingletonBase&) = delete;
    SingletonBase(SingletonBase&&) = delete;
    SingletonBase& operator=(SingletonBase&&) = delete;

protected:
    // 构造函数设为 protected，防止外部直接创建
    SingletonBase() = default;
    virtual ~SingletonBase() = default;
};