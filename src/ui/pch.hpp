#pragma once
// ============================================================
// ui 模块预编译头文件
// 包含在 ui 源文件中高频使用的稳定头文件，加速增量编译。
// 注意：只放「几乎不变」的第三方/STL 头，项目自身头文件不放在此处。
// ============================================================
// NOLINTBEGIN
// ---------- C++ 标准库 ----------
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <expected>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>
// NOLINTEND
//  ---------- EnTT (ECS 框架) ----------
#include <entt/entt.hpp>

// ---------- SDL3 ----------
#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>

// ---------- Eigen (线性代数) ----------
#include <Eigen/Core>
#include <Eigen/Dense>
#include <Eigen/Geometry>

// ---------- Yoga (Flexbox 布局) ----------
#include <yoga/Yoga.h>

// ---------- spdlog ----------
#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
