#pragma once
// ============================================================
// net 模块预编译头文件
// 包含在 net 源文件中高频使用的稳定头文件，加速增量编译。
// ============================================================

// ---------- C++ 标准库 ----------
// NOLINTBEGIN
#include <atomic>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>
// NOLINTEND

//  ---------- ASIO (异步 I/O) ----------
//  asio.hpp 是最大的单文件头，预编译收益最显著
#include <asio.hpp>
#include <asio/experimental/channel.hpp>
