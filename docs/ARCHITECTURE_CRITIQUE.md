# PmkUi 架构锐评

> 版本：v0.2 · 日期：2026-05-19 · 审查范围：`src/` 全部模块 + `tests/` + `example/`

---

## 总体评分

| 维度 | 评分 | 说明 |
|------|------|------|
| 架构分层 | ★★★★☆ | 层次清晰，局部存在反向依赖 |
| ECS 使用 | ★★★★☆ | 善用 entt，部分组件违反纯数据原则 |
| 渲染管线 | ★★★★★ | 三阶段收集–合批–提交设计优雅 |
| 线程安全 | ★★☆☆☆ | 多 UiRuntime/ThreadPool 场景存在数据竞争风险 |
| 可测性 | ★★★☆☆ | 核心系统缺乏测试覆盖，无 CI |
| 代码整洁 | ★★★☆☆ | 存在上帝组件、内联实现过长、几处错误逻辑 |

---

## 一、core 模块

### ✅ ⚠️ C1 — EventLoop 永恒空转（`EventLoop.cpp:22`）

```cpp
while (m_running.load()) {
    m_ioContext->run_for(std::chrono::milliseconds(1)); // 每 1ms 唤醒一次
    if (m_defaultHandler) { m_defaultHandler(); }
}
```

**问题**：即使无任何 SDL 事件、无 ASIO 任务，进程依然每 1ms 被强制唤醒，主线程 CPU 占用接近 100%。`run_for(1ms)` 在 io_context 队列为空时等价于 `sleep(1ms)`，但下层帧率已由 `RenderTask::delayTime=16ms` 控制，多余的 15 次唤醒毫无意义。

**影响**：笔记本/嵌入式场景下电池寿命，以及多进程并发时 CPU 争用。

---

### ✅ ⚠️ C2 — `RenderSystem` 静态首帧标记跨实例共享（`RenderSystem.cpp:351`）

```cpp
static bool firstUpdate = true; // 函数级静态变量！
```

函数级静态变量在同进程内所有 `UiRuntime` 实例共享，第一个运行时初始化后置 false，第二个运行时永远跳过首帧初始化逻辑，破坏多运行时隔离设计。

---

### ℹ️ C3 — `DslPaser.hpp` 是空存根（`core/DslPaser.hpp`）

JS DSL 解析器仅有注释声明，无任何实现，但已放入 `core/` 目录。对代码阅读者产生误导，建议移入 `docs/ideas/` 或添加 `#error "Not implemented"` 警告。

---

## 二、systems 模块

### ✅ 🔴 C4 — TextInputSystem 静态键盘状态多实例串台（`TextInputSystem.hpp:85-87`）

```cpp
inline static SDL_Keycode heldKey     = SDLK_UNKNOWN;
inline static uint64_t   keyPressTime = 0;
inline static uint64_t   lastRepeatTime = 0;
```

`inline static` 是类级别全局状态，多个 `TextInputSystem` 实例（对应多 UiRuntime / 多窗口）共享同一份键盘状态。当焦点在两个窗口之间切换时，键盘重复逻辑会串台——窗口 A 持有的按键将在窗口 B 中持续重复触发。

---

### ✅ 🔴 C5 — TimerSystem 静态任务表多实例共享（`TimerSystem.hpp:83`）

```cpp
static std::vector<globalcontext::TimerTask> tasks;
static uint32_t nextTaskId;
```

同 C4，类静态成员让所有运行时共享 timer 列表，timer ID 全局递增，无隔离。多运行时场景下定时器可能在错误的 `registry` 上下文中触发回调，产生 UB。

---

### ✅ ⚠️ C6 — StateSystem 反向依赖 factory 层（`StateSystem.hpp:44`）

```cpp
// StateSystem 中直接调用 factory 函数
ui::factory::CloseDropDownPopup(dropDownEntity);
```

系统层（`systems/`）反向依赖 API 层（`api/factory`），破坏架构分层约束。`systems/` 只应依赖 `common/`、`interface/`、`singleton/`，绝不应调用 factory 接口。

---

### ✅ C7 — HitTestSystem 信号连接维护负担重（`HitTestSystem.hpp:36-77`）

`registerHandlersImpl()` 中注册了约 20 个 `OnConstruct/OnUpdate/OnDestroy` 信号，`unregisterHandlersImpl()` 中必须等长一一断开，共约 40 处连接/断开代码。新增可交互组件时极容易遗漏对称的 `disconnect`，导致悬空信号和崩溃。

---

### ℹ️ C8 — ThemeSystem 是纯存根（`systems/ThemeSystem.hpp`）

已注册到 `SystemManager` 中，占用系统槽位，但无任何实现。每帧都会被遍历调用 `registerHandlers/update`，空耗调用开销。

---

## 三、renderers 模块

### ✅ 🔴 C9 — IconRenderer 字体图标判断逻辑错误（`IconRenderer.hpp:90`）

```cpp
// 当前（错误）
else if (HasFlag(iconComp->type, ~policies::IconFlag::Texture))

// 正确
else if (!HasFlag(iconComp->type, policies::IconFlag::Texture))
```

`~policies::IconFlag::Texture` 对枚举位反，产生的掩码含义是"Texture 位为 0、其余位全为 1"。`HasFlag(x, ~Texture)` 实际上对几乎所有非零 `type` 都返回 true，完全不等于"不是纹理图标"的语义。

**具体后果**：
- 当 `type == Default`（值为 0）时，两个分支均不进入，图标直接返回空渲染
- 当 `type` 同时包含 Texture 和其他 Flag 时，走字体分支而非纹理分支
- 造成已在生产环境运行的字体图标偶发渲染失败

---

### ✅ ⚠️ C10 — TableRenderer 全部实现内联在 .hpp（`TableRenderer.hpp`，约 350 行）

`collect()`、`updateCellWidgetLayouts()`、`computeColWidths()`、表头/单元格文字渲染全部内联在头文件中。每个包含该头文件的翻译单元都编译一份，显著增加编译时间和链接体积。

---

### ✅ ⚠️ C11 — TableRenderer 表头文字颜色硬编码白色（`TableRenderer.hpp:108`）

```cpp
static const Eigen::Vector4f headerTextColor{1.0F, 1.0F, 1.0F, 1.0F}; // 永远白色
```

`TableInfo` 已有 `cellTextColor` 字段，但表头文字颜色被遗忘，无法通过 API 或主题控制。

---

### ✅ ⚠️ C12 — TableRenderer::updateCellWidgetLayouts 写入时机错误

单元格控件的 `Position`/`Size` 在**渲染阶段**由 `TableRenderer::collect()` 写入，晚于 `LayoutSystem`（布局阶段）运行。`HitTestSystem` 使用的是 `LayoutSystem` 写入后的坐标；如果单元格控件刚被添加还未经历一次 `LayoutSystem::update()`，hit-test 坐标与渲染坐标可能不一致，导致首帧点击失效。

---

## 四、managers 模块

### ✅ ⚠️ C13 — TextTextureCache 无 GPU 设备保护（`TextTextureCache.hpp:88`）

`getOrUpload()` 在调用前未检查 `DeviceManager` 是否已初始化（Fallback 模式下 SDL GPU 设备为 null），可能通过 `createAndCacheTexture()` 触发空指针 SDL GPU API 调用，产生未定义行为。

---

### ℹ️ C14 — FontManager 裸 FT_Library/FT_Face 析构（`FontManager.hpp:62-82`）

FreeType / HarfBuzz 资源用裸指针持有，析构函数手动调用 `hb_font_destroy`、`FT_Done_Face` 等，没有 RAII 封装。若中途发生 `std::terminate`（如 worker 线程未捕获异常），资源会泄漏。建议用 `unique_ptr` + 自定义 deleter 封装。

---

## 五、common 模块

### ✅ ⚠️ C15 — ScrollArea 混杂交互状态（`Components.hpp:185-205`）

```cpp
struct ScrollArea {
    // 配置数据
    policies::Scroll scroll = policies::Scroll::None;
    Vec2 scrollOffset{...};
    // ↓↓↓ 运行时交互状态不应放在数据组件里
    bool scrollbarHovered = false;
    bool scrollbarPressed = false;
    bool trackHovered     = false;
};
```

运行时 UI 交互状态应拆分为独立的 Tag/State 组件（如 `ScrollBarInteractionState`），保持 `ScrollArea` 的纯数据语义。

---

### ✅ ⚠️ C16 — TableInfo 持有回调违反纯数据原则（`Components.hpp:543`）

```cpp
on_event<int, int> onCellClicked; // move_only_function 在组件里
```

`move_only_function` 使组件不可拷贝，限制 entt 某些批量操作（如 `snapshot/reload`、`view` 迭代时的隐式拷贝）。建议迁移为 `Dispatcher::Sink<TableCellClickedEvent>()` 订阅模式，与其他控件保持一致。

---

### ⚠️ C17 — Image::textureId 是裸 void*（`Components.hpp:345`）

```cpp
void* textureId = nullptr; // 类型擦除，无生命周期管理
```

纹理的生命周期完全依赖外部管理，组件销毁前若未显式释放将产生纹理泄漏。建议改为 `std::shared_ptr<ImageHandle>` 配合 `ImageManager` 引用计数。

---

## 六、singleton 模块

### 🔴 C18 — Registry/Dispatcher thread_local 设计无法防止跨线程竞争（`Registry.hpp:158`）

```cpp
static thread_local Registry* instance = nullptr;
```

`thread_local` 仅保证当前线程活跃上下文正确，但 `entt::registry` 本身**不是线程安全**的。若开启 `UI_ENABLE_MULTITHREAD=ON` 并在 `ThreadPool` worker 线程中访问 `Registry::View<>()` 或 `Registry::Emplace<>()`，将产生数据竞争。注释已有警告，但无任何编译期或运行时防护措施。

---

## 七、测试与 CI

### 🔴 C19 — 核心系统无测试覆盖

以下系统完全无测试：`LayoutSystem`、`RenderSystem`（依赖 GPU）、`HitTestSystem`、`StateSystem`、`TimerSystem`、`InteractionSystem`、`TextInputSystem`。占系统总数约 65%，任何重构都是盲飞。

### 🔴 C20 — 无持续集成管道

根目录无 `.github/workflows/`，测试完全依赖手动触发。C9 的 `~Texture` Bug 在有 CI 的情况下本可被回归测试发现。

---

## 问题清单汇总

| ID | 级别 | 模块 | 描述 |
|----|------|------|------|
| C1 | ✅ 已完成 | core/EventLoop | 1ms 固定轮询空转浪费 CPU |
| C2 | ✅ 已完成 | core/RenderSystem | `static bool firstUpdate` 跨实例共享 |
| C3 | ℹ️ | core/DslPaser | 空存根混入 core 目录 |
| C4 | ✅ 已完成 | systems/TextInputSystem | `inline static` 键盘状态多实例串台 |
| C5 | ✅ 已完成 | systems/TimerSystem | 类静态任务表多实例共享 |
| C6 | ✅ 已完成 | systems/StateSystem | 反向依赖 factory 层 |
| C7 | ✅ 已完成 | systems/HitTestSystem | 信号连接代码维护负担重 |
| C8 | ℹ️ | systems/ThemeSystem | 纯存根占用系统槽位 |
| C9 | ✅ 已完成 | renderers/IconRenderer | `~Texture` 位运算逻辑错误 |
| C10 | ✅ 已完成 | renderers/TableRenderer | 约 350 行实现内联在 .hpp |
| C11 | ✅ 已完成 | renderers/TableRenderer | 表头文字颜色硬编码 |
| C12 | ✅ 已完成 | renderers/TableRenderer | 单元格 Position 写入时机晚于 HitTest |
| C13 | ✅ 已完成 | managers/TextTextureCache | 无 GPU 设备 null 保护 |
| C14 | ℹ️ | managers/FontManager | 裸 FreeType/HarfBuzz 指针无 RAII |
| C15 | ✅ 已完成 | common/ScrollArea | 组件混杂运行时交互状态 |
| C16 | ✅ 已完成 | common/TableInfo | 回调违反纯数据原则 |
| C17 | ⚠️ | common/Image | `void*` textureId 无生命周期管理 |
| C18 | 🔴 | singleton/Registry | thread_local 无法防止跨线程竞争 |
| C19 | 🔴 | tests/ | 核心系统无测试覆盖 |
| C20 | 🔴 | CI | 无持续集成管道 |

> 🔴 严重缺陷（影响正确性/稳定性）　⚠️ 中等问题（影响可维护性）　ℹ️ 轻微问题（建议改进）

---

## v0.3 增量锐评（2026-05-20）

> 范围：在 v0.2 基础上对 `src/` 全部子目录复盘，对照已落地的 OP-01 ~ OP-13 修复并寻找新引入或被掩盖的问题。  
> 测试基线：90 个用例全绿（2026-05-19）。本次锐评新增 **11 条**问题（C21–C31），其中 P0 级 3 条。

### 1. 已修复确认

下列 v0.2 缺陷在源码中已验证落地：

| ID | 验证点 |
|----|--------|
| C1 | [EventLoop.cpp:23](../src/core/EventLoop.cpp#L23) 已改为 `asio::steady_timer::async_wait` + `io_context->run()`，1ms 轮询消除 |
| C2 / C5 | `RenderSystem::m_firstUpdate` 成员化；`TimerSystem` 任务表迁移到 `RuntimeFacade::ensureContext<globalcontext::TimerContext>()`（[TimerSystem.cpp:42](../src/systems/TimerSystem.cpp#L42)） |
| C4 | `TextInputSystem::m_heldKey` / `m_keyPressTime` / `m_lastRepeatTime` 成员化（[TextInputSystem.hpp:95-97](../src/systems/TextInputSystem.hpp#L95-L97)） |
| C6 | `StateSystem` 不再 include `factory`，下游解耦完成 |
| C7 | 抽出 `connectInvalidateConstructUpdateDestroy<>` / `disconnectInvalidate*` 模板，HitTestSystem 注册侧从 ~40 行降到 ~14 行（[HitTestSystem.hpp:48-79](../src/systems/HitTestSystem.hpp#L48-L79)） |
| C9 | `IconRenderer.hpp:90` 已改为 `!HasFlag(iconComp->type, policies::IconFlag::Texture)` |
| C10 / C11 / C12 | `TableRenderer` 拆出 `.cpp`；`TableInfo::headerTextColor` 字段落地（[Components.hpp:549](../src/common/Components.hpp#L549)）；单元格几何写入前移 |
| C13 | `TextTextureCache` 对 `device == nullptr` 已有保护分支 |
| C15 | `ScrollBarInteractionState` 拆为独立组件（[Components.hpp:208-214](../src/common/Components.hpp#L208-L214)），`ScrollArea` 回归纯数据 |
| C16 | `TableInfo::onCellClicked` 已删除，迁移到 `events::TableCellClicked` |

### 2. 仍存在问题

| ID | 级别 | 现状 |
|----|------|------|
| C3 | ℹ️ | `DslPaser.hpp` 已加 TODO 注释，但仍占用 `core/` 槽位；建议下沉到 `docs/ideas/` |
| C8 | ℹ️ | `ThemeSystem.hpp` 仍只有注释（[ThemeSystem.hpp:1-18](../src/systems/ThemeSystem.hpp#L1-L18)），暂未注册到 SystemManager 槽位 |
| C14 | ⚠️ | `FontManager` 的 FreeType / HarfBuzz 仍为裸指针，WP-A3 未推进 |
| C17 | ⚠️ | `Image::textureId` 仍是 `void*`（[Components.hpp:306](../src/common/Components.hpp#L306)） |
| C18 | 🔴 | `Registry::activeInstance()` 仍是 `thread_local`，无跨线程访问栅栏 |
| C19 | 🔴 | `tests/unittest/` 仅 11 个文件，LayoutSystem / RenderSystem / HitTestSystem / StateSystem 仍 0 覆盖 |
| C20 | 🔴 | 仓库根仍无 `.github/workflows/`，CI 未建 |

### 3. 新发现问题（C21 起）

#### 🔴 C21 — `TextInputSystem` 用类静态 `s_current` 做单实例路由（[TextInputSystem.hpp:94](../src/systems/TextInputSystem.hpp#L94)）

```cpp
static inline TextInputSystem* s_current = nullptr;

void registerHandlersImpl() {
    s_current = this;     // 后注册者覆盖前注册者
    ...
}
static void processKeyRepeat() {
    if (s_current != nullptr) s_current->doProcessKeyRepeat();
}
```

OP-03 把键盘状态从 `inline static` 迁回了实例成员，但 `processKeyRepeat()` 入口仍是 `static`，靠类静态 `s_current` 指向"最后一个注册的实例"。多 UiRuntime 场景下，第二个运行时注册时覆盖 `s_current`，第一个运行时的 `m_heldKey` 永远不会被推进——把 C4 的"状态串台"换成了"路由丢失"，行为退化未真正消除。

#### 🔴 C22 — `Dispatcher::current()` / `Registry::current()` 仅靠 `assert` 防御（[Dispatcher.hpp:44](../src/singleton/Dispatcher.hpp#L44), [Registry.hpp:40](../src/singleton/Registry.hpp#L40)）

```cpp
assert(instance != nullptr && "No active UiRuntime. Wrap all UI calls in UiRuntimeScope.");
return *instance;
```

Release 构建下 `assert` 被宏掉，空指针解引用直接产生 UB；调用栈被 inline 后排查成本高。应改为常驻检查（`if (!instance) std::terminate(...)` 或抛 `std::logic_error`）。

#### 🔴 C23 — `InteractionSystem::pollSdlEvents()` / `TextInputSystem::processKeyRepeat()` 仍是 `static` 入口（[InteractionSystem.hpp:75](../src/systems/InteractionSystem.hpp#L75), [TaskChain.hpp:121](../src/core/TaskChain.hpp#L121)）

`TaskChain::InputTask` 直接调用静态成员，结果完全取决于"当前 thread_local 激活的 runtime"。多 UiRuntime 同帧调度时，SDL 事件归属错误——这是 C18 thread_local 单激活假设的下游表现。

#### ⚠️ C24 — `SystemManager` 系统更新顺序由 `addSystem` 调用顺序隐式决定（[SystemManager.hpp:64](../src/core/SystemManager.hpp#L64)）

```cpp
template <typename T>
void addSystem(T&& system) { m_systems.emplace_back(std::forward<T>(system)); }
```

无 Phase / Priority 字段。新增系统若插入位置错误（例如把 `LayoutSystem` 放在 `RenderSystem` 之后）会渲染上一帧布局结果，且这种错误编译期/单测都发现不了。

#### ⚠️ C25 — `ActionSystem` 绕过 `Dispatcher::Sink<>()` 包装层（[ActionSystem.hpp:42](../src/systems/ActionSystem.hpp#L42)）

```cpp
auto& dispatcher = RuntimeFacade::current().enttDispatcher();
dispatcher.sink<ui::events::ClickEvent>().connect<&ActionSystem::onClickEvent>(*this);
```

其他所有系统都走 `Dispatcher::Sink<events::X>()`（带 `traits::Events` concept 约束），唯独 `ActionSystem` 直访裸 `entt::dispatcher`。风格不一致 + 失去概念检查，新增事件类型可能逃过约束。

#### ⚠️ C26 — `Components.hpp` 单文件已突破 740 行

继续以"加 struct 到尾巴"的方式扩展（[Components.hpp:294-735](../src/common/Components.hpp#L294-L735) 上仍能看到 14 处 `on_event<>` 字段），缺乏按域拆分（visual / layout / interaction / data），定位与合并冲突成本持续上升。

#### ⚠️ C27 — `RenderSystem.cpp` 单文件 ≥ 720 行违反 SRP（[RenderSystem.cpp](../src/systems/RenderSystem.cpp)）

混合后端选择、设备 / 管线 / 字体 / 图标 / 图像 / 白纹理生命周期、`collect → batch → submit` 主循环、Fallback 切换、Move 构造等多职责。`update()` ~200 行、`ensureInitialized()` ~140 行，理解和单测均困难。

#### ⚠️ C28 — `EventLoop::quit()` 用 `io_context::stop()` 强制终止（[EventLoop.cpp:48](../src/core/EventLoop.cpp#L48)）

```cpp
void EventLoop::quit() {
    if (!m_running.exchange(false)) return;
    m_frameTimer.cancel();
    m_workGuard.reset();
    m_ioContext->stop();   // 丢弃尚未消费的任务
}
```

`stop()` 会立即返回所有 `run()` 调用、丢弃未处理 handler。若退出时刚好处于 GPU `acquireCommandBuffer` 已发出、`submitCommandBuffer` 未发出之间，会留下半提交状态，下次启动可能触发 SDL_GPU 验证层告警。

#### ⚠️ C29 — `TimerSystem::cancelTask` 线性扫描 + ID 单调递增不回收（[TimerSystem.cpp:64](../src/systems/TimerSystem.cpp#L64)）

```cpp
for (auto& task : timerCtx.tasks) {  // O(n)
    if (task.id == handle) { task.cancelled = true; return; }
}
```

`nextTaskId` 永不回收，长时间运行 wrap 后与未被清理的 `cancelled = true` 残留任务会出现 ID 撞车；`update()` 内对 `cancelled` 任务也只是跳过而未删除，`tasks` 单调增长。

#### ℹ️ C30 — 大量 `.hpp`-only 的 system / renderer 仍未拆分

继 `TableRenderer` 拆分后，[HitTestSystem.hpp](../src/systems/HitTestSystem.hpp)、[StateSystem.hpp](../src/systems/StateSystem.hpp)、[IconRenderer.hpp](../src/renderers/IconRenderer.hpp)、[TextTextureCache.hpp](../src/managers/TextTextureCache.hpp) 等仍是 inline 头文件。每个 TU 重复编译；`StateSystem.hpp` 仅一个文件就 600+ 行 inline 实现。

#### ℹ️ C31 — 组件层 `on_event<>` 别名仍广泛使用（[Components.hpp:32](../src/common/Components.hpp#L32) 及 13 处使用）

OP-13 仅迁移了 `TableInfo::onCellClicked`，`TextEdit::onSubmit / onTextChanged`、`Button::onClick`、`Drag::onDragMove`、`Hover::onHover` 等仍嵌在数据组件里。`std::move_only_function` 让组件不可拷贝，违反纯数据原则，且后续 entt snapshot / 序列化、热重载会受阻。

### 4. 总体评分变化

| 维度 | v0.2 | v0.3 | 说明 |
|------|------|------|------|
| 架构分层 | ★★★★☆ | ★★★★☆ | C6 解耦落地；ActionSystem 直访 enttDispatcher 暴露不一致 |
| ECS 使用 | ★★★★☆ | ★★★★☆ | ScrollBarInteractionState 拆出；但 13 处 `on_event<>` 仍在组件内 |
| 渲染管线 | ★★★★★ | ★★★★☆ | TableRenderer 拆分加分；RenderSystem.cpp 720+ 行 SRP 违反凸显 |
| 线程安全 | ★★☆☆☆ | ★★☆☆☆ | thread_local 单激活未变；C21 新引入"静态路由"再次踩雷 |
| 可测性 | ★★★☆☆ | ★★★☆☆ | 测试 90 个绿，但核心系统覆盖未补、CI 仍缺 |
| 代码整洁 | ★★★☆☆ | ★★★☆☆ | TableRenderer +1、Components.hpp / RenderSystem.cpp -1 |

### 5. v0.3 问题清单（接续 C20）

| ID | 级别 | 模块 | 描述 |
|----|------|------|------|
| C21 | 🔴 | systems/TextInputSystem | `static s_current` 单实例路由，多 runtime 串台再现 |
| C22 | 🔴 | singleton/Registry & Dispatcher | `current()` 仅 assert，Release 下 UB |
| C23 | 🔴 | systems/Interaction & TextInput | `static` 入口依赖 thread_local 激活的 runtime |
| C24 | ⚠️ | core/SystemManager | 系统执行顺序由调用顺序隐式决定 |
| C25 | ⚠️ | systems/ActionSystem | 绕过 `Dispatcher::Sink<>()` 直访 enttDispatcher |
| C26 | ⚠️ | common/Components | 单文件 740+ 行未按域拆分 |
| C27 | ⚠️ | systems/RenderSystem | `.cpp` 720+ 行多职责，违反 SRP |
| C28 | ⚠️ | core/EventLoop | `quit()` 强制 stop，丢弃未消费任务 |
| C29 | ⚠️ | systems/TimerSystem | cancelTask O(n) + ID 无回收 + cancelled 任务残留 |
| C30 | ℹ️ | systems/managers | 大量 `.hpp`-only 实现（HitTest / State / Icon / TextTextureCache） |
| C31 | ℹ️ | common/Components | 13 处 `on_event<>` 仍嵌在数据组件中 |

