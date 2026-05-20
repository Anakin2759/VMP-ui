# PmkUi — Copilot 指引

## 项目概览

C++23 自研 ECS UI 静态库（从 PestManKill 独立），基于 EnTT + SDL3 GPU + Yoga Flexbox 布局。使用 CMake 4.0 构建，MSVC / Clang-cl 编译，静态链接所有依赖。

## 架构与模块边界

```
src/          → UI 静态库（ui）源码
example/      → example_ui_demo 可执行示例
tests/        → ui_tests 单元测试
```

**设计要点**：UI 模块使用 `Registry` 全局单例访问 `entt::registry`；System 均使用 `EnableRegister<Derived>` CRTP，接口方法名为 `registerHandlersImpl`。事件循环基于 ASIO `io_context`（`src/core/EventLoop.hpp`）。

## 构建命令

```bash
# 配置（Ninja + clang-cl，Debug）
cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Debug
# 构建
cmake --build build --config Debug
# 启用测试
cmake -B build -DENABLE_BUILD_TESTS=ON
# 启用 clang-tidy
cmake -B build -DENABLE_CLANG_TIDY=ON
```

VS Code 任务 `build Debug (CMake)` 可直接运行构建。

## 核心模式与约定

### 管道 DSL（UI 构建）

UI 实体通过 `operator|` 链式配置，定义在 `src/api/Chains.hpp`：

```cpp
using namespace ui::chains;
auto btn = ui::factory::CreateButton();
btn | Size(100, 40) | BackgroundColor(Color::Blue()) | Text(U"开始") | OnClick([]{...}) | Show();
parent | AddChild(btn);
```

组合样式可复用：`auto style = Size(100,40) | BackgroundColor(Color::Red());`

### View 层是纯函数

客户端视图声明在 `src/client/View/` 下，每个视图是 `inline void Create*()` 自由函数（非类）。通过检查 `BaseInfo.alias` 防止重复创建。

### 组件设计

组件是纯数据结构（无行为），内部标记 `using is_component_tag = void;`。Tag 用 `is_tags_tag`，Event 用 `is_event_tag`。配合 `Component` / `UiTag` concept 做编译期约束。

### 消息协议

`src/shared/messages/` 下定义所有网络消息。每条消息继承 `MessageBase<Derived>` CRTP，必须：
- 定义 `static constexpr uint16_t CMD_ID`
- 实现 `serializeImpl()` / `deserializeImpl()`（使用 `PacketWriter`/`PacketReader`）
- 实现 `toJsonImpl()` 用于调试

`MessageDispatcher` 按 `CMD_ID` 路由到类型化处理器。

### 事件系统

- `[IMMEDIATE]` 事件用 `Dispatcher::Trigger()` 立即执行
- `[BUFFERED]` 事件用 `Dispatcher::Enqueue()` 延迟到下一帧处理

## 命名约定

| 类别 | 规则 | 示例 |
|------|------|------|
| 文件名 | PascalCase；`.hpp` 扩展名 | `Components.hpp`, `RenderSystem.hpp` |
| 类/结构体 | PascalCase | `LayoutSystem`, `RenderSystem` |
| 命名空间 | 小写 | `ui::factory`, `ui::chains` |
| 公共函数 | PascalCase | `CreateButton()`, `EmplaceOrReplace()` |
| 私有/实现函数 | camelCase | `registerHandlersImpl()` |
| 成员变量 | `m_` 前缀 + camelCase | `m_registry`, `m_yogaConfig` |
| 常量 | `static constexpr` UPPER_SNAKE | `CMD_ID`, `MAX_LOG_FILE_SIZE` |

## 错误处理约定

UI 层使用统一 `Result<T>` 基础设施（`src/common/Result.hpp` + `src/common/ErrorCodes.hpp/.cpp`）：

```cpp
// 别名
template <typename T>
using Result = std::expected<T, std::error_code>;   // Result<void> 直接使用

// 错误枚举（ui_errc，20 个码，段位预留 100）
enum class ui_errc : int { success = 0, invalid_argument = 1, device_unavailable = 2, ... };

// 工厂函数
MakeError(ui_errc::xxx)   // → std::unexpected<std::error_code>
Ok()                      // → Result<void> 成功值
make_error_code(ui_errc)  // ADL 路由至 UiErrorCategory 单例（.cpp 中，非 inline）
```

- `std::formatter<ui_errc>` 已落地，可直接 `std::format("{}", ec)`。
- 旧 `src/common/UiErrors.hpp`（`FontErrc/IconErrc`）过渡期并存，待 WP-A3 迁移。

## C++23 特性使用

项目广泛使用现代 C++ 特性，生成代码时应优先：
- `std::expected<T, E>` 替代异常做错误处理（UI 层统一用 `ui::Result<T>`）
- `std::move_only_function` 替代 `std::function`（事件回调）
- Concepts 做模板约束（`Component`, `UiTag`, `Action`）
- `std::source_location` 自动捕获日志调用位置
- `std::span` 传递缓冲区
- Deducing this（`this auto`）

## 关键第三方库

| 库 | 用途 | 注意事项 |
|----|------|----------|
| SDL3 | GPU 渲染 + 窗口 | **SDL3 API**（非 SDL2），无 `SDL_setenv` 等旧接口 |
| EnTT | ECS 框架 | `entt::registry` + `entt::dispatcher` + `entt::poly` |
| Yoga | Flexbox 布局 | 通过 `YGNodeRef` 管理布局树 |
| ASIO | 事件循环异步 I/O | Standalone 模式（`ASIO_STANDALONE`），非 Boost；用于 `EventLoop` |
| spdlog | 日志 | Header-only 模式 |
| Eigen | 线性代数 | `Vec2 = Eigen::Vector2f` 用于 UI 数学类型 |

## 测试

Google Test 框架，测试位于 `tests/unittest/`，需 `-DENABLE_BUILD_TESTS=ON`。使用 `TEST_F` fixture 模式。
