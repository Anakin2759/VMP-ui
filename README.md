# VMP-ui

> **⚠ Learning / Practice Project — Features are incomplete and APIs may change without notice.**

C++23 ECS-based UI static library. Built on [EnTT](https://github.com/skypjack/entt) + [SDL3](https://github.com/libsdl-org/SDL) GPU + [Yoga](https://yogalayout.dev/) Flexbox layout.

Vermin must perish

---

[中文说明](#pmkui-中文) | English

## Features

- **ECS architecture** — UI widgets are EnTT entities; layout, rendering, and interaction are separate systems
- **GPU rendering** — SDL3 GPU backend with HLSL shaders (SPIR-V + DXIL); SDF font rendering
- **Flexbox layout** — Full Yoga layout engine; all sizes in logical pixels
- **Chain DSL** — `operator|` pipeline for composable, declarative widget configuration
- **Unicode text** — FreeType + HarfBuzz shaping; bundled Noto Sans SC variable font
- **Material Symbols** — Built-in icon font with codepoint lookup
- **Borderless window** — Custom title bar with DWM compositing (Win32), Motif Hints (X11), SDL borderless (Wayland)
- **Event system** — ASIO `io_context` event loop + EnTT dispatcher; immediate and buffered events
- **Animation** — Fade-in and extensible animation components
- **Logging** — spdlog-backed logger with rotating file sink; injectable callback via `ui::log::SetCallback`
- **Error handling** — `ui::Result<T>` (`std::expected<T, std::error_code>`) throughout; no exceptions in hot paths
- **Static linking** — All dependencies compiled and linked statically

## Platform Support

| Platform    | Renderer          | Window System      | Status       |
| ----------- | ----------------- | ------------------ | ------------ |
| Windows 10+ | SDL3 GPU (DXIL)   | Win32 DWM          | ✅ Primary   |
| Linux       | SDL3 GPU (SPIR-V) | X11                | ✅ Supported |
| Linux       | SDL3 GPU (SPIR-V) | Wayland (via SDL3) | ✅ Supported |

## Requirements

- **CMake** 3.31+
- **C++ compiler** with C++23 support: MSVC 19.33+, Clang-cl 17+, GCC 13+, Clang 17+
- **Ninja** (recommended generator)
- **Linux / X11**: `libx11-dev` (Ubuntu/Debian) · `libX11-devel` (Fedora) · `libx11` (Arch)
- **DXC** *(optional)*: Vulkan SDK or Windows Kits DXC for shader recompilation

## Building

```bash
# Configure (Debug, Ninja + clang-cl)
cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Debug

# Build
cmake --build build

# With tests
cmake -G Ninja -B build -DENABLE_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build
```

### CMake Options

| Option                           | Default  | Description                                                                  |
| -------------------------------- | -------- | ---------------------------------------------------------------------------- |
| `BUILD_EXAMPLES`               | `ON`   | Build `example_ui_demo`                                                    |
| `ENABLE_BUILD_TESTS`           | `OFF`  | Build unit tests (Google Test)                                               |
| `ENABLE_LTO`                   | `ON`   | IPO/LTO for Release builds                                                   |
| `ENABLE_CLANG_TIDY`            | `OFF`  | Enable clang-tidy static analysis                                            |
| `UI_LINUX_WINDOW_SYSTEM`       | `AUTO` | Linux backend:`AUTO` · `X11` · `WAYLAND`                             |
| `UI_ENABLE_SHADER_COMPILATION` | `AUTO` | Shader compilation:`AUTO` · `ON` (require DXC) · `OFF` (precompiled) |
| `UI_FORCE_CPU_RENDER`          | `OFF`  | Force SDL_Renderer software fallback (no GPU)                                |
| `UI_ENABLE_MULTITHREAD`        | `OFF`  | Enable worker threads in `ThreadPool`                                      |
| `UI_RESOURCE_BACKEND`          | `CMRC` | Resource embedding:`CMRC` · `STD_EMBED`                                 |

## Quick Start

```cpp
#include <ui.hpp>

using namespace ui::chains;

int main(int argc, char* argv[])
{
    auto app = *ui::factory::CreateApplication(std::span(argv, argc));

    // Create a borderless window
    auto win = ui::factory::CreateWindow("Demo");

    // Create a vertical layout
    auto vbox = ui::factory::CreateVBoxLayout();
    win | AddChild(vbox);

    // Chain-configure a button
    auto btn = ui::factory::CreateButton("Click me");
    btn | Size(120, 36)
        | BackgroundColor(Color{0x2563EBFF})
        | TextColor(Color::White())
        | OnClick([] { ui::log::Info("clicked!"); });
    vbox | AddChild(btn);

    win | Show();
    app->exec();
}
```

### Chain DSL

All configuration is applied through `operator|`. Chains are composable and reusable:

```cpp
// Reusable style
auto primaryBtn = Size(120, 36) | BackgroundColor(Color{0x2563EBFF}) | TextColor(Color::White());

auto btn1 = ui::factory::CreateButton("OK");
auto btn2 = ui::factory::CreateButton("Cancel");
btn1 | primaryBtn | OnClick([] { /* ... */ }) | Show();
btn2 | primaryBtn | Show();
```

### Logging

```cpp
// Use built-in log functions (std::format arguments supported)
ui::log::Info("Window created: {}x{}", width, height);
ui::log::Warning("DPI scale = {:.2f}", scale);

// Route to your own sink
ui::log::SetCallback([](ui::log::Level lvl, std::string_view msg) {
    my_engine_log(msg);
});

// Redirect log file
ui::log::SetFilePath("logs/ui.log");
```

### Setting App Icon

```cpp
ui::factory::SetAppIcon("path/to/icon.png");
```

## Project Structure

```
include/          → Public header (ui.hpp)
src/
  api/            → Public API (Factory, Chains, Log, Icon, Controls…)
  common/         → Components, Tags, Events, Types, Result, ErrorCodes
  core/           → Application, EventLoop, RenderFrame, PlatformWindow
  systems/        → ECS systems (render, layout, input, animation…)
  managers/       → FontManager, IconManager, CommandBuffer…
  renderers/      → Widget-specific render logic
  singleton/      → Registry, Dispatcher, Logger
  traits/         → CRTP helpers (EnableRegister)
  assets/         → Embedded shaders (SPIR-V / DXIL) and fonts
example/
  ui_demo/        → Example application
tests/
  unittest/       → Google Test unit tests
third_party/      → SDL3, EnTT, Yoga, FreeType, HarfBuzz, ASIO, spdlog,
                    Eigen, stb, cmrc, googletest
```

## Third-Party Licenses

| Library                                         | License                |
| ----------------------------------------------- | ---------------------- |
| [SDL3](https://github.com/libsdl-org/SDL)          | Zlib                   |
| [EnTT](https://github.com/skypjack/entt)           | MIT                    |
| [Yoga](https://yogalayout.dev/)                    | MIT                    |
| [FreeType](https://freetype.org/)                  | FTL                    |
| [HarfBuzz](https://harfbuzz.github.io/)            | MIT                    |
| [ASIO](https://think-async.com/Asio/)              | Boost Software License |
| [spdlog](https://github.com/gabime/spdlog)         | MIT                    |
| [Eigen](https://eigen.tuxfamily.org/)              | MPL2                   |
| [stb](https://github.com/nothings/stb)             | MIT / Public Domain    |
| [cmrc](https://github.com/vector-of-bool/cmrc)     | MIT                    |
| [GoogleTest](https://github.com/google/googletest) | BSD-3-Clause           |
| Noto Sans SC                                    | SIL OFL 1.1            |
| Material Symbols                                | Apache 2.0             |

## License

MIT — see [LICENSE](LICENSE)

---

> **⚠ 学习 / 练手项目 —— 功能尚未完成，API 可能随时变更，不建议用于生产环境。**

基于 C++23 ECS 架构的 UI 静态库。底层依赖 [EnTT](https://github.com/skypjack/entt) + [SDL3](https://github.com/libsdl-org/SDL) GPU 渲染 + [Yoga](https://yogalayout.dev/) Flexbox 布局。

---

[English](#pmkui) | 中文说明

## 特性

- **ECS 架构** — UI 组件是 EnTT 实体；布局、渲染、交互各为独立 System
- **GPU 渲染** — SDL3 GPU 后端 + HLSL 着色器（SPIR-V / DXIL）；SDF 字体渲染
- **Flexbox 布局** — 完整 Yoga 布局引擎；所有尺寸使用逻辑像素
- **Chain DSL** — `operator|` 管道语法，声明式、可复用的组件配置
- **Unicode 文字** — FreeType + HarfBuzz 文字整形；内置 Noto Sans SC 可变字体
- **Material Symbols** — 内置图标字体，支持 Unicode 码点查找
- **无边框窗口** — Win32 DWM 透明合成、X11 Motif Hints、Wayland SDL borderless
- **事件系统** — ASIO `io_context` 事件循环 + EnTT Dispatcher；支持即时与缓冲事件
- **动画** — 淡入动画及可扩展动画组件
- **日志** — spdlog 后端，自动轮转文件；可通过 `ui::log::SetCallback` 注入自定义输出
- **错误处理** — 全局使用 `ui::Result<T>`（`std::expected<T, std::error_code>`）；热路径不抛异常
- **静态链接** — 所有依赖均静态编译链接

## 平台支持

| 平台        | 渲染器            | 窗口系统             | 状态        |
| ----------- | ----------------- | -------------------- | ----------- |
| Windows 10+ | SDL3 GPU (DXIL)   | Win32 DWM            | ✅ 主要平台 |
| Linux       | SDL3 GPU (SPIR-V) | X11                  | ✅ 支持     |
| Linux       | SDL3 GPU (SPIR-V) | Wayland（经由 SDL3） | ✅ 支持     |

## 环境要求

- **CMake** 3.31+
- **C++ 编译器**（C++23）：MSVC 19.33+、Clang-cl 17+、GCC 13+、Clang 17+
- **Ninja**（推荐生成器）
- **Linux / X11**：`libx11-dev`（Ubuntu/Debian）· `libX11-devel`（Fedora）· `libx11`（Arch）
- **DXC** *(可选)*：Vulkan SDK 或 Windows Kits DXC，用于重新编译着色器

## 构建

```bash
# 配置（Debug，Ninja + clang-cl）
cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Debug

# 构建
cmake --build build

# 启用单元测试
cmake -G Ninja -B build -DENABLE_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build
```

### CMake 选项

| 选项                             | 默认值   | 说明                                                               |
| -------------------------------- | -------- | ------------------------------------------------------------------ |
| `BUILD_EXAMPLES`               | `ON`   | 构建 `example_ui_demo` 示例程序                                  |
| `ENABLE_BUILD_TESTS`           | `OFF`  | 构建单元测试（Google Test）                                        |
| `ENABLE_LTO`                   | `ON`   | Release 构建启用 IPO/LTO                                           |
| `ENABLE_CLANG_TIDY`            | `OFF`  | 启用 clang-tidy 静态分析                                           |
| `UI_LINUX_WINDOW_SYSTEM`       | `AUTO` | Linux 窗口后端：`AUTO` · `X11` · `WAYLAND`                 |
| `UI_ENABLE_SHADER_COMPILATION` | `AUTO` | 着色器编译：`AUTO` · `ON`（强制 DXC）· `OFF`（使用预编译） |
| `UI_FORCE_CPU_RENDER`          | `OFF`  | 强制使用 SDL_Renderer 软件渲染（无 GPU）                           |
| `UI_ENABLE_MULTITHREAD`        | `OFF`  | 启用 `ThreadPool` 工作线程                                       |
| `UI_RESOURCE_BACKEND`          | `CMRC` | 资源嵌入方式：`CMRC` · `STD_EMBED`                            |

## 快速开始

```cpp
#include <ui.hpp>

using namespace ui::chains;

int main(int argc, char* argv[])
{
    auto app = *ui::factory::CreateApplication(std::span(argv, argc));

    // 创建无边框窗口
    auto win = ui::factory::CreateWindow("Demo");

    // 创建垂直布局
    auto vbox = ui::factory::CreateVBoxLayout();
    win | AddChild(vbox);

    // 链式配置按钮
    auto btn = ui::factory::CreateButton("点击我");
    btn | Size(120, 36)
        | BackgroundColor(Color{0x2563EBFF})
        | TextColor(Color::White())
        | OnClick([] { ui::log::Info("已点击！"); });
    vbox | AddChild(btn);

    win | Show();
    app->exec();
}
```

### Chain DSL

所有配置均通过 `operator|` 链式应用，样式可复用：

```cpp
// 定义可复用样式
auto primaryBtn = Size(120, 36) | BackgroundColor(Color{0x2563EBFF}) | TextColor(Color::White());

auto btn1 = ui::factory::CreateButton("确定");
auto btn2 = ui::factory::CreateButton("取消");
btn1 | primaryBtn | OnClick([] { /* ... */ }) | Show();
btn2 | primaryBtn | Show();
```

### 日志

```cpp
// 内置日志函数（支持 std::format 参数）
ui::log::Info("窗口已创建: {}x{}", width, height);
ui::log::Warning("DPI 缩放 = {:.2f}", scale);

// 注入自定义输出
ui::log::SetCallback([](ui::log::Level lvl, std::string_view msg) {
    my_engine_log(msg);
});

// 重定向日志文件
ui::log::SetFilePath("logs/ui.log");
```

### 设置应用图标

```cpp
ui::factory::SetAppIcon("path/to/icon.png");
```

## 目录结构

```
include/          → 公开头文件（ui.hpp）
src/
  api/            → 公开 API（Factory、Chains、Log、Icon、Controls…）
  common/         → Components、Tags、Events、Types、Result、ErrorCodes
  core/           → Application、EventLoop、RenderFrame、PlatformWindow
  systems/        → ECS 系统（渲染、布局、输入、动画…）
  managers/       → FontManager、IconManager、CommandBuffer…
  renderers/      → 各组件渲染逻辑
  singleton/      → Registry、Dispatcher、Logger
  traits/         → CRTP 辅助（EnableRegister）
  assets/         → 嵌入着色器（SPIR-V / DXIL）和字体
example/
  ui_demo/        → 示例程序
tests/
  unittest/       → Google Test 单元测试
third_party/      → SDL3、EnTT、Yoga、FreeType、HarfBuzz、ASIO、spdlog、
                    Eigen、stb、cmrc、googletest
```

## 第三方库许可

| 库                                              | 许可证                 |
| ----------------------------------------------- | ---------------------- |
| [SDL3](https://github.com/libsdl-org/SDL)          | Zlib                   |
| [EnTT](https://github.com/skypjack/entt)           | MIT                    |
| [Yoga](https://yogalayout.dev/)                    | MIT                    |
| [FreeType](https://freetype.org/)                  | FTL                    |
| [HarfBuzz](https://harfbuzz.github.io/)            | MIT                    |
| [ASIO](https://think-async.com/Asio/)              | Boost Software License |
| [spdlog](https://github.com/gabime/spdlog)         | MIT                    |
| [Eigen](https://eigen.tuxfamily.org/)              | MPL2                   |
| [stb](https://github.com/nothings/stb)             | MIT / Public Domain    |
| [cmrc](https://github.com/vector-of-bool/cmrc)     | MIT                    |
| [GoogleTest](https://github.com/google/googletest) | BSD-3-Clause           |
| Noto Sans SC                                    | SIL OFL 1.1            |
| Material Symbols                                | Apache 2.0             |

## 许可证

MIT — 见 [LICENSE](LICENSE)
