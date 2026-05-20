# PmkUi 架构主线文档

> 版本：v0.2 · 日期：2026-05-19  
> 项目：从 PestManKill 独立的 C++23 自研 ECS UI 静态库

---

## 目录

1. [项目概览](#1-项目概览)
2. [目录结构](#2-目录结构)
3. [模块职责](#3-模块职责)
4. [核心数据流](#4-核心数据流)
5. [ECS 设计约定](#5-ecs-设计约定)
6. [渲染管线](#6-渲染管线)
7. [事件系统](#7-事件系统)
8. [管道 DSL](#8-管道-dsl)
9. [错误处理](#9-错误处理)
10. [构建与配置](#10-构建与配置)
11. [测试矩阵](#11-测试矩阵)
12. [第三方依赖](#12-第三方依赖)

---

## 1. 项目概览

PmkUi 是一个基于 **EnTT ECS + SDL3 GPU + Yoga Flexbox** 的 C++23 UI 静态库，目标是为游戏/工具应用提供可嵌入的声明式 UI 框架。

**核心特性**

- ECS 驱动：全部 UI 状态存储在 `entt::registry` 组件中，无虚函数继承层级
- 管道 DSL：通过 `operator|` 链式配置控件，一行代码完成布局+样式+事件绑定
- 多后端渲染：SDL3 GPU（D3D12 → Vulkan → Metal 自动降级）+ 纯软件 Fallback
- Yoga Flexbox：完整支持 CSS Flexbox 布局语义
- 异步事件循环：基于 ASIO standalone `io_context` 的单线程帧管线

---

## 2. 目录结构

```
src/
├── core/           主循环骨架（Application、EventLoop、SystemManager、
│                   UiRuntime、TaskChain、RenderContext）
├── api/            对外 DSL + 工厂函数（Factory、Chains、Controls、
│                   Layout、Text、Icon、Table、Animation...）
├── systems/        ECS 系统层（交互、布局、渲染、动画、输入、定时器...）
├── renderers/      渲染器（每类控件独立渲染器，实现 IRenderer）
├── managers/       资源 & GPU 管理（Device、Font、Icon、Image、
│                   BatchManager、CommandBuffer、TextTextureCache...）
├── common/         纯数据定义（Components、Tags、Events、Policies、
│                   RenderTypes、Result、ThreadPool...）
├── interface/      纯虚接口（ISystem、IRenderer、IBackendRenderer）
├── singleton/      线程局部单例（Registry、Dispatcher、Logger）
└── traits/         编译期约束（Component、UiTag、Events concept...）

example/            可运行演示程序（ui_demo）
tests/unittest/     Google Test 单元测试
docs/               架构文档（本文）
```

---

## 3. 模块职责

### 3.1 core — 应用骨架

| 类 | 职责 |
|----|------|
| `Application` | 持有所有系统和管理器，驱动主循环 `exec()` |
| `EventLoop` | 封装 `asio::io_context`，提供 `invoke()` 异步投递和 `registerDefaultHandler()` 帧回调 |
| `SystemManager` | 通过 `entt::poly<ISystem>` 存储和驱动所有系统 |
| `UiRuntime` | 持有 `entt::registry` + `entt::dispatcher` 实例 |
| `UiRuntimeScope` | RAII 激活/恢复当前线程的活跃运行时上下文 |
| `RuntimeFacade` | 跨模块的全局访问门面（`getRegistry()`、`getDispatcher()` 等） |
| `TaskChain` | 帧管线 DSL，将 QueuedTask/InputTask/RenderTask 组合为 `defaultHandler` |
| `RenderContext` | 渲染阶段的共享状态包（位置、尺寸、alpha、scissor 栈、管理器指针等） |

### 3.2 api — 对外接口

所有面向用户的操作均通过此模块暴露：

- `Factory.*`：`CreateButton / CreateLabel / CreateTable / ...`（创建实体并挂载初始组件）
- `Chains.hpp`：`operator|` 管道 DSL 实现
- `Controls.*`、`Layout.*`、`Text.*`、`Icon.*`、`Table.*`：具体组件的修改接口
- `Utils.*`：`GetAbsolutePosition`、`HasAlignment`、`MarkVisualChanged` 等工具函数

### 3.3 systems — ECS 系统层

| 系统 | 触发时机 | 职责 |
|------|----------|------|
| `InteractionSystem` | InputTask | `SDL_PollEvent()` → 分发 `RawPointerMove/Button/KeyInput` |
| `HitTestSystem` | BUFFERED 事件 | 碰撞检测、Z-Order 排序、命中实体缓存 |
| `StateSystem` | BUFFERED 事件 | 焦点/悬浮/活跃/滚动 状态机 |
| `ActionSystem` | BUFFERED 事件 | 触发 `Clickable::onClick`、`OnSliderValueChanged` 等回调 |
| `LayoutSystem` | `UpdateLayout` IMMEDIATE | Yoga Flexbox 布局计算，回写 `Position/Size` 组件 |
| `RenderSystem` | `UpdateRendering` IMMEDIATE | 协调所有 `IRenderer` 完成收集→合批→GPU 提交 |
| `TweenSystem` | `UpdateEvent` BUFFERED | 动画插值，更新 `TweenComponent` 中的进度值 |
| `TimerSystem` | `UpdateTimer` BUFFERED | 触发到期的 `TimerTask` 回调 |
| `TextInputSystem` | BUFFERED 事件 | 文字输入、键盘重复、IME |
| `ShortcutSystem` | BUFFERED 事件 | 全局快捷键分发 |
| `PlatformWindowSystem` | `SDL_AddEventWatch` | 窗口移动/缩放/最大化/关闭事件桥接 |
| `ThemeSystem` | — | **存根，待实现** |

### 3.4 renderers — 分离渲染器

每个渲染器实现 `IRenderer`，仅负责一类控件的 `collect()` 阶段：

| 渲染器 | `getPriority()` | 负责的实体类型 |
|--------|-----------------|----------------|
| `ShapeRenderer` | 0 | 所有带 `Background/Border/Shadow` 的实体（背景 + 边框） |
| `TableRenderer` | 5 | `TableTag` — 表格行背景、网格线、表头/单元格文字 |
| `ProgressBarRenderer` | 8 | `ProgressBarTag` |
| `CheckBoxRenderer` | 9 | `CheckBoxTag` |
| `SliderRenderer` | 9 | `SliderTag` |
| `DropDownRenderer` | 9 | `DropDownTag` |
| `ScrollBarRenderer` | 10 | 带 `ScrollArea` 的实体 |
| `TextRenderer` | 10 | `TextTag / ButtonTag / LabelTag / TextEditTag` |
| `ImageRenderer` | 15 | `ImageTag` |
| `IconRenderer` | 20 | `Icon` 组件 |
| `CanvasRenderer` | 25 | `CanvasTag` |
| `FallbackBackendRenderer` | — | Fallback 后端专用 |

### 3.5 managers — 资源与 GPU 管理

| 管理器 | 职责 |
|--------|------|
| `DeviceManager` | SDL3 GPU 设备初始化（D3D12 → Vulkan → CPU 降级），Pipeline 创建入口 |
| `FontManager` | FreeType2 + HarfBuzz 字体加载，文字光栅化为 alpha-mask |
| `IconManager` | 纹理图标 + 字体图标注册与查询 |
| `ImageManager` | 异步图片解码（stb_image），GPU 纹理上传 |
| `BatchManager` | 逐帧收集顶点数据，相同 `(texture, scissor)` 批次合并 |
| `CommandBuffer` | 封装 GPU 命令录制和 Pipeline 绑定 |
| `PipelineCache` | 着色器 Pipeline 缓存，按 `(topology, blend, msaa)` 键索引 |
| `TextTextureCache` | 文字纹理 LRU 缓存（上限 256 条目），alpha-mask 格式 |
| `ResourceProvider` | cmrc / std::embed 内嵌资源抽象 |

---

## 4. 核心数据流

```
┌──────────────────────────────────────────────────────────────────┐
│  主线程：EventLoop::exec()                                        │
│                                                                  │
│  ① run_for(1ms)  ── 消费 asio::post 投递的任务                   │
│                                                                  │
│  ② defaultHandler()                                              │
│      │                                                           │
│      ├── [QueuedTask]                                            │
│      │    ├─ FrameContext 时间戳更新                              │
│      │    ├─ Dispatcher::Update()  → 派发 BUFFERED 事件          │
│      │    │    ├─ UpdateEvent     → TweenSystem::update()        │
│      │    │    ├─ UpdateTimer     → TimerSystem::update()        │
│      │    │    ├─ RawPointerMove  → HitTestSystem                │
│      │    │    ├─ RawPointerButton→ StateSystem → ActionSystem   │
│      │    │    └─ RawKeyInput     → TextInputSystem              │
│      │    └─ QuitRequested        → Application::quit()         │
│      │                                                           │
│      ├── [InputTask]                                             │
│      │    └─ InteractionSystem::pollSdlEvents()                  │
│      │         └─ SDL_PollEvent() → Dispatcher::Enqueue(...)     │
│      │                                                           │
│      └── [RenderTask]（每 16ms 触发一次）                         │
│           ├─ Trigger<UpdateLayout>                               │
│           │   └─ LayoutSystem: Yoga → Position/Size 回写         │
│           ├─ Trigger<UpdateRendering>                            │
│           │   └─ RenderSystem:                                   │
│           │       DFS 遍历实体树                                  │
│           │       → IRenderer::collect() × N                    │
│           │       → BatchManager::optimize()                     │
│           │       → CommandBuffer::submitBatches()               │
│           │       → IBackendRenderer::drawBatch()                │
│           │       → endFrame()                                   │
│           └─ Trigger<EndFrame>                                   │
└──────────────────────────────────────────────────────────────────┘
```

---

## 5. ECS 设计约定

### 组件（Component）

- 纯数据结构，**无任何行为**，必须标记 `using is_component_tag = void`
- 通过 `Registry::Emplace<T>(entity)` 挂载，`Registry::TryGet<T>(entity)` 安全读取
- 不得持有回调、`move_only_function`、非平凡析构的智能指针（除 `std::shared_ptr`）

### Tag（UiTag）

- 空结构体，仅用于类型标记（如 `ButtonTag`、`VisibleTag`、`TableCellWidgetTag`）
- 标记 `using is_tags_tag = void`，通过 `Registry::AnyOf<Tag>(entity)` 查询

### 事件（Event）

- 标记 `using is_event_tag = void`
- `[IMMEDIATE]` 事件用 `Dispatcher::Trigger<E>()` 立即执行（当帧）
- `[BUFFERED]` 事件用 `Dispatcher::Enqueue<E>()` 延迟到 `Dispatcher::Update()` 批量派发
- 系统通过 `Dispatcher::Sink<E>().connect<&Handler>()` 在 `registerHandlersImpl()` 中订阅

### 系统（System）

- 实现 `EnableRegister<Derived>` CRTP，提供 `registerHandlersImpl()` / `unregisterHandlersImpl()`
- 通过 `entt::poly<ISystem>` 装箱存入 `SystemManager::m_systems`
- 不得直接调用 `api/factory` 层（系统只依赖 `common/interface/singleton`）

### 实体层级

```
Window entity
└─ Panel/Layout entity (HBoxLayout / VBoxLayout)
   └─ Widget entity (Button / Label / CheckBox / ...)
      └─ TableCellWidget entity（TableCellWidgetTag，不参与 Yoga）
```

`Hierarchy::parent` + `Hierarchy::children` 维护双向层级，`RenderSystem` 据此 DFS 遍历。

---

## 6. 渲染管线

### 阶段一：Collect（收集）

`RenderSystem::collectRenderData()` 使用显式栈 DFS 遍历实体树，对每个 `VisibleTag` 实体，依次调用已按优先级排序的 `m_renderers`，命中者调用 `collect(entity, context)` 向 `BatchManager` 提交矩形/顶点数据。

**RenderContext 传递规则**：
- `context.position` = 当前实体世界坐标（父坐标 + 子相对坐标 - 父 ScrollOffset）
- `context.size` = 当前实体尺寸（来自 `Size` 组件）
- `context.alpha` = 累积透明度（`context.alpha × Alpha.value`）
- `context.currentScissor` = 当前有效裁剪区域（ScrollArea 推入/弹出）

### 阶段二：Optimize（批次合并）

`BatchManager::optimize()`：相邻批次若 `texture` 和 `scissor` 相同则合并顶点缓冲，减少 draw call 数量。逐实体参数（`opacity`、`radius`、`border_width` 等）已下沉至每个顶点的属性（`UiVertex`），不参与合批条件判断。

### 阶段三：Submit（GPU 提交）

```
CommandBuffer::submitBatches()
  └─ for batch in batches:
       beginBatch(texture, scissor)
       IBackendRenderer::drawBatch(batch)
         ├─ GPU 路径：SDL_GPURenderPass + SDL3 GPU Pipeline
         └─ Fallback：SDL_RenderGeometry (SDL_Renderer)
  └─ endFrame() → SDL_SubmitGPUCommandBuffer / SDL_RenderPresent
```

**着色器说明**：

- 文字纹理（alpha-mask）：`pushConstants.padding = 2.0F`，shader 按 alpha 值着色，支持任意颜色不重复上传
- 图标纹理（预乘 alpha）：`pushConstants.padding = 1.0F`，shader 直接输出预乘值
- 纯色矩形（白色纹理）：`pushConstants.padding = 0.0F`，shader 乘以顶点颜色

---

## 7. 事件系统

### 7.1 即时事件（IMMEDIATE）

在 `Dispatcher::Trigger<E>(args...)` 调用时**同步执行**所有监听器，当帧内完成。主要用于帧内的逻辑推进（`UpdateLayout`、`UpdateRendering`、`EndFrame`）。

### 7.2 缓冲事件（BUFFERED）

通过 `Dispatcher::Enqueue<E>(args...)` 投入队列，在下一次 `Dispatcher::Update()` 时批量派发。主要用于跨帧的输入事件（`RawPointerMove`、`RawPointerButton`、`RawKeyInput`）。

### 7.3 订阅示例

```cpp
// 在 registerHandlersImpl() 中
Dispatcher::Sink<events::RawPointerButton>()
    .connect<&HitTestSystem::onPointerButton>(this);

// 在 unregisterHandlersImpl() 中
Dispatcher::Sink<events::RawPointerButton>()
    .disconnect<&HitTestSystem::onPointerButton>(this);
```

---

## 8. 管道 DSL

### 8.1 基本用法

```cpp
using namespace ui::chains;

auto btn = ui::factory::CreateButton("确认", "confirmBtn");
btn | FixedSize(120.0F, 36.0F)
    | BackgroundColor({0.2F, 0.5F, 0.85F, 1.0F})
    | BorderRadius(6.0F)
    | OnClick([] { /* ... */ })
    | Show();

parent | AddChild(btn);
```

### 8.2 样式复用

```cpp
// 预组合为可复用 Chain 常量
auto primaryStyle = FixedSize(120.0F, 36.0F)
                  | BackgroundColor({0.2F, 0.5F, 0.85F, 1.0F})
                  | BorderRadius(6.0F);

btnA | primaryStyle | Text("确认") | OnClick(...);
btnB | primaryStyle | Text("取消") | OnClick(...);
```

### 8.3 内部机制

`Chain<F>` 包装一个 `(entt::entity) -> void` 可调用对象：

```
entity | Chain<F>          →  立即调用 F(entity)，返回 entity（允许继续链式）
Chain<F> | Chain<G>        →  创建 Chain<Combined<F,G>>（延迟组合，零开销）
```

使用 C++23 Deducing This（`this auto&&`）处理左值/右值复用。

---

## 9. 错误处理

UI 层统一使用 `ui::Result<T>` 基础设施：

```cpp
// 类型别名
template <typename T>
using Result = std::expected<T, std::error_code>;

// 工厂
MakeError(ui_errc::invalid_argument)  // → std::unexpected<std::error_code>
Ok()                                  // → Result<void> 成功值

// 错误码（ui_errc，20 个预定义码）
enum class ui_errc : int {
    success = 0,
    invalid_argument = 1,
    device_unavailable = 2,
    // ...
};
```

`std::formatter<ui_errc>` 已注册，可直接 `std::format("{}", ec)` 输出可读错误描述。

---

## 10. 构建与配置

### 10.1 基本构建

```bash
# 配置（Ninja + clang-cl，Debug）
cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Debug

# 构建
cmake --build build --config Debug

# 启用测试
cmake -B build -DENABLE_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build -V

# 启用 clang-tidy 静态分析
cmake -B build -DENABLE_CLANG_TIDY=ON
```

### 10.2 CMake 选项

**顶层选项**

| 选项 | 默认值 | 说明 |
|------|--------|------|
| `ENABLE_BUILD_TESTS` | OFF | 编译 Google Test 单元测试 |
| `BUILD_EXAMPLES` | ON | 编译 example_ui_demo |
| `ENABLE_LTO` | ON | Release/RelWithDebInfo 启用 IPO/LTO |
| `ENABLE_CLANG_TIDY` | OFF | 开启静态分析（注意 clang-cl 下 OOM 风险） |

**库级选项（`src/CMakeLists.txt`）**

| 选项 | 默认值 | 说明 |
|------|--------|------|
| `UI_ENABLE_MULTITHREAD` | OFF | ON 时 ThreadPool 启动 asio::thread_pool worker 线程 |
| `UI_FORCE_CPU_RENDER` | OFF | 强制使用 SDL_Renderer 软件后端 |
| `UI_ENABLE_SHADER_COMPILATION` | AUTO | AUTO/ON/OFF 控制 HLSL 编译；AUTO 自动检测 DXC |
| `UI_RESOURCE_BACKEND` | CMRC | CMRC（内嵌）/ STD_EMBED（实验性） |
| `ENABLE_COVERAGE` | OFF | 代码覆盖率插桩 |
| `UI_ENABLE_ASAN` | OFF | AddressSanitizer |

---

## 11. 测试矩阵

| 测试文件 | 覆盖模块 |
|---------|---------|
| `test_BatchManager.cpp` | `BatchManager` 批次合并逻辑 |
| `test_result.cpp` | `Result<T>` / `ui_errc` / formatter |
| `test_TaskChain.cpp` | `TaskChain` 帧管线组合 |
| `test_UiRuntime.cpp` | `UiRuntime` / `UiRuntimeScope` 多实例隔离 |
| `test_TweenSystem.cpp` | `TweenSystem` + `ActionSystem` 动画全链路 |
| `test_TextUtils.cpp` | UTF-8 工具函数 |
| `test_ResourceProviderCoverage.cpp` | `ResourceProvider` cmrc 资源 |
| `test_UtilsCoverage.cpp` | `api/Utils` |
| `test_VisibilityCoverage.cpp` | `api/Visibility` Show/Hide |
| `test_HierarchyCoverage.cpp` | `api/Hierarchy` AddChild/RemoveChild |
| `test_MainWindow.cpp` | 主窗口创建集成验证 |

**缺失覆盖**：`LayoutSystem`、`RenderSystem`、`HitTestSystem`、`StateSystem`、`TimerSystem`、`InteractionSystem`、`TextInputSystem`、`TableRenderer`、`IconRenderer`。

---

## 12. 第三方依赖

| 库 | 版本/来源 | 用途 | 链接方式 |
|----|-----------|------|----------|
| SDL3 | third_party/SDL | 窗口 + GPU 渲染 + 输入 | 静态 |
| EnTT | third_party/entt | ECS 框架 | header-only |
| Yoga | third_party/yoga | Flexbox 布局 | 静态 |
| ASIO | third_party/asio 1.36.0 | 事件循环 + 线程池 | header-only (standalone) |
| FreeType | third_party/freetype | 字体光栅化 | 静态 |
| HarfBuzz | third_party/harfbuzz | 文字塑形 | 静态 |
| spdlog | third_party/spdlog | 日志 | header-only |
| Eigen | third_party/eigen-5.0.0 | 线性代数（Vec2/Vec4） | header-only |
| stb | third_party/stb | 图片解码（stb_image） | header-only |
| cmrc | third_party/cmrc | 内嵌资源 | header-only |
| Google Test | third_party/googletest | 单元测试 | 静态（测试专用） |
