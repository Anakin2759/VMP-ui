#pragma once
// ============================================================
// client 模块预编译头文件
// 包含在 client 源文件中高频使用的稳定头文件，加速增量编译。
// client 通过链接 ui 间接使用大量 SDL3/EnTT/Eigen 头文件，
// 在此预编译可避免 View 层头文件链每次都重新解析第三方库。
// ============================================================

// ---------- C++ 标准库 ----------
#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

// ---------- EnTT (ECS 框架，View 层广泛使用) ----------
#include <entt/entt.hpp>


// ---------- spdlog ----------
#include <spdlog/spdlog.h>
