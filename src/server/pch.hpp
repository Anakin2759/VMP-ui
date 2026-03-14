#pragma once
// ============================================================
// server 模块预编译头文件
// 包含在 server 源文件中高频使用的稳定头文件，加速增量编译。
// ============================================================

// ---------- C++ 标准库 ----------
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

// ---------- ASIO (异步 I/O) ----------
#include <asio.hpp>

// ---------- spdlog ----------
#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

// ---------- EnTT (ECS 框架) ----------
#include <entt/entt.hpp>

// ---------- nlohmann JSON ----------
#include <nlohmann/json.hpp>
