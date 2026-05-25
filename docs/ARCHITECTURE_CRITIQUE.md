# PmkUi 架构锐评

> 版本：v0.4 · 日期：2026-05-25 · 审查范围：`src/` 全部模块 + `tests/` + `example/`

> 相关文档：[SVG 支持评估与实现规划](SVG_SUPPORT_PLAN.md)

---

## 总体评分

| 维度     | 评分       | 说明                                         |
| -------- | ---------- | -------------------------------------------- |
| 架构分层 | ★★★★★ | 分层约束完整，反向依赖已全部消除（C6/C25）             |
| ECS 使用 | ★★★★☆ | 善用 entt；14 处 `on_event<>` 仍违反纯数据原则         |
| 渲染管线 | ★★★★★ | 三阶段收集–合批–提交；RenderSystem 已拆分子文件        |
| 线程安全 | ★★★☆☆ | 多 Runtime 路由已消除（C21–C23）；entt::registry 跨线程竞争根因未解 |
| 可测性   | ★★★☆☆ | 90 用例全绿；核心系统覆盖率 0，无 CI                   |
| 代码整洁 | ★★★★☆ | Components 按域拆分落地；StateSystem.hpp 1233 行为主要欠账 |

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

### ✅ ℹ️ C14 — FontManager 裸 FT_Library/FT_Face 析构（`FontManager.hpp:62-82`）

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

### ⚠️ C17 — Image::textureId 是裸 void*（`components/Visual.hpp`）

```cpp
void* textureId = nullptr; // 类型擦除，无生命周期管理
```

纹理的生命周期完全依赖外部管理，组件销毁前若未显式释放将产生纹理泄漏。建议改为 `std::shared_ptr<ImageHandle>` 配合 `ImageManager` 引用计数。

补充：当前图像资源链路在 `ImageManager` 侧仅稳定覆盖 bmp/png/jpeg，`.svg` 会落入失败分支。SVG 支持已单独形成规划文档 [SVG_SUPPORT_PLAN.md](SVG_SUPPORT_PLAN.md)。后续如果推进 SVG，不建议只在解码层临时加分支而继续保留 `void* textureId` 生命周期模型，否则会把“格式扩展”和“资源所有权”两个问题耦合放大。

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

| ID  | 级别      | 模块                      | 描述                                   |
| --- | --------- | ------------------------- | -------------------------------------- |
| C1  | ✅ 已完成 | core/EventLoop            | 1ms 固定轮询空转浪费 CPU               |
| C2  | ✅ 已完成 | core/RenderSystem         | `static bool firstUpdate` 跨实例共享 |
| C3  | ℹ️      | core/DslPaser             | 空存根混入 core 目录                   |
| C4  | ✅ 已完成 | systems/TextInputSystem   | `inline static` 键盘状态多实例串台   |
| C5  | ✅ 已完成 | systems/TimerSystem       | 类静态任务表多实例共享                 |
| C6  | ✅ 已完成 | systems/StateSystem       | 反向依赖 factory 层                    |
| C7  | ✅ 已完成 | systems/HitTestSystem     | 信号连接代码维护负担重                 |
| C8  | ℹ️      | systems/ThemeSystem       | 纯存根占用系统槽位                     |
| C9  | ✅ 已完成 | renderers/IconRenderer    | `~Texture` 位运算逻辑错误            |
| C10 | ✅ 已完成 | renderers/TableRenderer   | 约 350 行实现内联在 .hpp               |
| C11 | ✅ 已完成 | renderers/TableRenderer   | 表头文字颜色硬编码                     |
| C12 | ✅ 已完成 | renderers/TableRenderer   | 单元格 Position 写入时机晚于 HitTest   |
| C13 | ✅ 已完成 | managers/TextTextureCache | 无 GPU 设备 null 保护                  |
| C14 | ✅ 已完成 | managers/FontManager      | 裸 FreeType/HarfBuzz 指针无 RAII       |
| C15 | ✅ 已完成 | common/ScrollArea         | 组件混杂运行时交互状态                 |
| C16 | ✅ 已完成 | common/TableInfo          | 回调违反纯数据原则                     |
| C17 | ⚠️      | common/Image              | `void*` textureId 无生命周期管理     |
| C18 | 🔴        | singleton/Registry        | thread_local 无法防止跨线程竞争        |
| C19 | 🔴        | tests/                    | 核心系统无测试覆盖                     |
| C20 | 🔴        | CI                        | 无持续集成管道                         |

> 🔴 严重缺陷（影响正确性/稳定性）　⚠️ 中等问题（影响可维护性）　ℹ️ 轻微问题（建议改进）

---

## v0.3 增量锐评（2026-05-20）

> 范围：在 v0.2 基础上对 `src/` 全部子目录复盘，对照已落地的 OP-01 ~ OP-13 修复并寻找新引入或被掩盖的问题。
> 测试基线：90 个用例全绿（2026-05-19）。本次锐评新增 **11 条**问题（C21–C31），其中 P0 级 3 条。

### 1. 已修复确认

下列 v0.2 缺陷在源码中已验证落地：

| ID              | 验证点                                                                                                                                                                                            |
| --------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| C1              | [EventLoop.cpp:23](../src/core/EventLoop.cpp#L23) 已改为 `asio::steady_timer::async_wait` + `io_context->run()`，1ms 轮询消除                                                                    |
| C2 / C5         | `RenderSystem::m_firstUpdate` 成员化；`TimerSystem` 任务表迁移到 `RuntimeFacade::ensureContext<globalcontext::TimerContext>()`（[TimerSystem.cpp:42](../src/systems/TimerSystem.cpp#L42)）     |
| C4              | `TextInputSystem::m_heldKey` / `m_keyPressTime` / `m_lastRepeatTime` 成员化（[TextInputSystem.hpp:95-97](../src/systems/TextInputSystem.hpp#L95-L97)）                                         |
| C6              | `StateSystem` 不再 include `factory`，下游解耦完成                                                                                                                                            |
| C7              | 抽出 `connectInvalidateConstructUpdateDestroy<>` / `disconnectInvalidate*` 模板，HitTestSystem 注册侧从 ~40 行降到 ~14 行（[HitTestSystem.hpp:48-79](../src/systems/HitTestSystem.hpp#L48-L79)） |
| C9              | `IconRenderer.hpp:90` 已改为 `!HasFlag(iconComp->type, policies::IconFlag::Texture)`                                                                                                          |
| C10 / C11 / C12 | `TableRenderer` 拆出 `.cpp`；`TableInfo::headerTextColor` 字段落地（[Components.hpp:549](../src/common/Components.hpp#L549)）；单元格几何写入前移                                              |
| C13             | `TextTextureCache` 对 `device == nullptr` 已有保护分支                                                                                                                                        |
| C15             | `ScrollBarInteractionState` 拆为独立组件（[Components.hpp:208-214](../src/common/Components.hpp#L208-L214)），`ScrollArea` 回归纯数据                                                            |
| C16             | `TableInfo::onCellClicked` 已删除，迁移到 `events::TableCellClicked`                                                                                                                          |

### 2. 仍存在问题

| ID  | 级别 | 现状                                                                                                                        |
| --- | ---- | --------------------------------------------------------------------------------------------------------------------------- |
| C3  | ℹ️ | `DslPaser.hpp` 已加 TODO 注释，但仍占用 `core/` 槽位；建议下沉到 `docs/ideas/`                                        |
| C8  | ℹ️ | `ThemeSystem.hpp` 仍只有注释（[ThemeSystem.hpp:1-18](../src/systems/ThemeSystem.hpp#L1-L18)），暂未注册到 SystemManager 槽位 |
| C14 | ✅ | `FontManager` 已改用 RAII `unique_ptr`（`FtLibraryDeleter`/`FtFaceDeleter`/`HbFontDeleter`）封装（[FontManager.hpp:929-945](../src/managers/FontManager.hpp#L929-L945)）|
| C17 | ⚠️ | `Image::textureId` 仍是 `void*`（[components/Visual.hpp](../src/common/components/Visual.hpp)）                            |
| C18 | 🔴   | `Registry::activeInstance()` 仍是 `thread_local`，无跨线程访问栅栏                                                      |
| C19 | 🔴   | `tests/unittest/` 仅 11 个文件，LayoutSystem / RenderSystem / HitTestSystem / StateSystem 仍 0 覆盖                       |
| C20 | 🔴   | 仓库根仍无 `.github/workflows/`，CI 未建                                                                                  |

### 3. 新发现问题（C21 起）

#### ✅ C21 — `TextInputSystem` 用类静态 `s_current` 做单实例路由

`processKeyRepeat()` 入口为 `static`，靠类静态 `s_current` 指向最后注册的实例；多 UiRuntime 时第一个运行时键盘重复逻辑路由丢失，将 C4 的"状态串台"变为"路由丢失"。**已修复，见 v0.4 § 1。**

#### ✅ C22 — `Dispatcher::current()` / `Registry::current()` 仅靠 `assert` 防御

Release 构建下 `assert` 被宏掉，空指针解引用直接产生 UB；调用栈被 inline 后排查成本高。**已修复，见 v0.4 § 1。**

#### ✅ C23 — `InteractionSystem::pollSdlEvents()` / `TextInputSystem::processKeyRepeat()` 仍是 `static` 入口

`TaskChain::InputTask` 直接调用静态成员，多 UiRuntime 同帧调度时 SDL 事件归属错误（C18 thread_local 单激活假设的下游表现）。**已修复，见 v0.4 § 1。**

#### ✅ C24 — `SystemManager` 系统更新顺序由 `addSystem` 调用顺序隐式决定

无 Phase / Priority 字段，插入位置错误（如 `LayoutSystem` 晚于 `RenderSystem`）会渲染上一帧布局结果，编译期/单测均发现不了。**已修复，见 v0.4 § 1。**

#### ✅ C25 — `ActionSystem` 绕过 `Dispatcher::Sink<>()` 包装层

`ActionSystem` 直访裸 `entt::dispatcher`，失去 `traits::Events` concept 约束；风格与其余系统不一致，新增事件类型可能逃过检查。**已修复，见 v0.4 § 1。**

#### ✅ C26 — `Components.hpp` 单文件已突破 740 行

单文件缺乏按域拆分（visual / layout / interaction / data），740+ 行持续增长，定位与合并冲突成本高。**已修复，见 v0.4 § 1。**

#### ✅ C27 — `RenderSystem.cpp` 单文件 ≥ 720 行违反 SRP

多职责混合：后端选择、设备/资源生命周期、`collect → batch → submit` 主循环、Fallback 切换等，`update()` ~200 行、`ensureInitialized()` ~140 行。**已修复，见 v0.4 § 1。**

#### ✅ C28 — `EventLoop::quit()` 用 `io_context::stop()` 强制终止

`stop()` 丢弃未处理 handler，GPU 帧半提交状态可能触发 SDL_GPU 验证层告警。**已修复，见 v0.4 § 1。**

#### ✅ C29 — `TimerSystem::cancelTask` 线性扫描 + ID 单调递增不回收

O(n) 线性扫描取消，`nextTaskId` 不回收，`cancelled` 任务积压导致 `tasks` 向量单调增长。**已修复，见 v0.4 § 1。**

#### ℹ️ C30 — 大量 `.hpp`-only 的 system / renderer 仍未拆分

继 `TableRenderer` 拆分后，[HitTestSystem.hpp](../src/systems/HitTestSystem.hpp)（371 行）、[StateSystem.hpp](../src/systems/StateSystem.hpp)（**1233 行**）、[TextTextureCache.hpp](../src/managers/TextTextureCache.hpp)（430 行）等仍是 inline 头文件。每个 TU 重复编译，`StateSystem.hpp` 尤为突出。

#### ℹ️ C31 — 组件层 `on_event<>` 别名仍广泛使用（[components/Interaction.hpp:18](../src/common/components/Interaction.hpp#L18) 及 14 处使用）

OP-13 仅迁移了 `TableInfo::onCellClicked`，`TextEdit::onSubmit / onTextChanged`、`Button::onClick`、`Drag::onDragMove`、`Hover::onHover` 等仍嵌在数据组件里。`std::move_only_function` 让组件不可拷贝，违反纯数据原则，且后续 entt snapshot / 序列化、热重载会受阻。

### 4. 总体评分变化

| 维度     | v0.2       | v0.3       | 说明                                                               |
| -------- | ---------- | ---------- | ------------------------------------------------------------------ |
| 架构分层 | ★★★★☆ | ★★★★☆ | C6 解耦落地；ActionSystem 直访 enttDispatcher 暴露不一致           |
| ECS 使用 | ★★★★☆ | ★★★★☆ | ScrollBarInteractionState 拆出；但 13 处 `on_event<>` 仍在组件内 |
| 渲染管线 | ★★★★★ | ★★★★☆ | TableRenderer 拆分加分；RenderSystem.cpp 720+ 行 SRP 违反凸显      |
| 线程安全 | ★★☆☆☆ | ★★☆☆☆ | thread_local 单激活未变；C21 新引入"静态路由"再次踩雷              |
| 可测性   | ★★★☆☆ | ★★★☆☆ | 测试 90 个绿，但核心系统覆盖未补、CI 仍缺                          |
| 代码整洁 | ★★★☆☆ | ★★★☆☆ | TableRenderer +1、Components.hpp / RenderSystem.cpp -1             |

### 5. v0.3 问题清单（接续 C20）

| ID  | 级别 | 模块                            | 描述                                                                 |
| --- | ---- | ------------------------------- | -------------------------------------------------------------------- |
| C21 | ✅ 已完成 | systems/TextInputSystem         | `static s_current` 单实例路由，多 runtime 串台再现                 |
| C22 | ✅ 已完成 | singleton/Registry & Dispatcher | `current()` 仅 assert，Release 下 UB                               |
| C23 | ✅ 已完成 | systems/Interaction & TextInput | `static` 入口依赖 thread_local 激活的 runtime                      |
| C24 | ✅ 已完成 | core/SystemManager              | 系统执行顺序由调用顺序隐式决定                                       |
| C25 | ✅ 已完成 | systems/ActionSystem            | 绕过 `Dispatcher::Sink<>()` 直访 enttDispatcher                    |
| C26 | ✅ 已完成 | common/Components               | 单文件 740+ 行未按域拆分                                             |
| C27 | ✅ 已完成 | systems/RenderSystem            | `.cpp` 720+ 行多职责，违反 SRP                                     |
| C28 | ✅ 已完成 | core/EventLoop                  | `quit()` 强制 stop，丢弃未消费任务                                 |
| C29 | ✅ 已完成 | systems/TimerSystem             | cancelTask O(n) + ID 无回收 + cancelled 任务残留                     |
| C30 | ℹ️      | systems/managers                | 大量 `.hpp`-only 实现（StateSystem **1233** 行 / HitTest 371 行 / TextTextureCache 430 行）|
| C31 | ℹ️      | common/Components               | 14 处 `on_event<>` 仍嵌在数据组件中（OP-28 P3 未推进）             |

---

## v0.4 增量锐评（2026-05-25）

> 范围：在 v0.3 基础上对 `src/` 全部子目录复盘，对照已落地的 OP-15、OP-19 ~ OP-27 逐条验证。  
> 测试基线：90 个用例全绿（2026-05-25，无变化）。本次无新增问题条目，共 **10 条**已修复确认。

### 1. 已修复确认

| ID  | 验证点 |
| --- | ------ |
| C14 | `FontManager` 成员改用三个 RAII 包装：`unique_ptr<FT_Library_Rec_, FtLibraryDeleter>`、`unique_ptr<FT_Face_Rec_, FtFaceDeleter>`、`unique_ptr<hb_font_t, HbFontDeleter>`（[FontManager.hpp:929-945](../src/managers/FontManager.hpp#L929-L945)），`std::terminate` 时不再泄漏 |
| C21 | `s_current` 静态路由消除；改为 `Dispatcher::Sink<events::TickKeyRepeat>().connect<&TextInputSystem::doProcessKeyRepeat>(*this)`，每个 UiRuntime 实例各自订阅/退订，不再共享路由（[TextInputSystem.hpp:29,36](../src/systems/TextInputSystem.hpp#L29)）|
| C22 | `Registry::current()` / `Dispatcher::current()` 改为 `[[unlikely]] + std::fputs + std::terminate()`（[Registry.hpp:43-46](../src/singleton/Registry.hpp#L43-L46)），Release 构建下空指针解引用 UB 已消除 |
| C23 | `InteractionSystem::pollSdlEvents()` 改为非静态成员方法，由 `SystemManager::pollInput()` 统一调度；`TaskChain` 中原有静态调用已移除（[InteractionSystem.hpp:77](../src/systems/InteractionSystem.hpp#L77)）|
| C24 | `ISystem` 接口新增 `getPhase()` 虚调用，`SystemPhase` 枚举（`Input/Logic/Render`）落地（[ISystem.hpp:26,47](../src/interface/Isystem.hpp#L26)）；`SystemManager::registerAllHandlers()` 以索引排序后重组保证执行顺序（[SystemManager.cpp:78-87](../src/core/SystemManager.cpp#L78)）|
| C25 | `ActionSystem` 全面改用 `Dispatcher::Sink<events::X>()` 包装，不再直访裸 `entt::dispatcher`，与其他系统风格统一（[ActionSystem.hpp:44-62](../src/systems/ActionSystem.hpp#L44)）|
| C26 | `Components.hpp` 拆分为 6 个领域头文件：`components/Visual.hpp`（97 行）、`Layout.hpp`（160 行）、`Interaction.hpp`（114 行）、`Animation.hpp`（111 行）、`Data.hpp`（338 行）、`Window.hpp`（32 行）；`Components.hpp` 自身仅保留 8 行聚合 include |
| C27 | `RenderSystem.cpp` 缩减为 21 行存根注释；实现拆分至 `systems/render/` 子目录：`RenderBackend.cpp`（477 行，后端初始化/设备生命周期）、`RenderFrame.cpp`（339 行，每帧渲染逻辑）、`RenderResources.cpp`（104 行，GPU 资源创建/渲染器注册）|
| C28 | `EventLoop::quit()` 已移除 `m_ioContext->stop()` 调用，改为仅重置 work guard，让 io_context 在现有 handler 处理完毕后自然退出（[EventLoop.cpp:90-91](../src/core/EventLoop.cpp#L90)）|
| C29 | `TimerContext::tasks` 改为 `unordered_map<uint32_t, TimerTask>`，`cancelTask` 由 O(n) 线性扫描变为 O(1) `find`；`update()` 末尾以 `std::erase_if` 批量清理 `cancelled` 任务，不再单调增长（[TimerSystem.cpp:68,122](../src/systems/TimerSystem.cpp#L68)）|

### 2. 仍存在问题

| ID  | 级别 | 现状 |
| --- | ---- | ---- |
| C3  | ℹ️ | `DslPaser.hpp` TODO 注释已加，但仍占用 `core/` 槽位，未下沉到 `docs/ideas/` |
| C8  | ℹ️ | `ThemeSystem.hpp` 18 行纯存根（WP-B P1 未启动），占用 SystemManager 注册名额 |
| C17 | ⚠️ | `Image::textureId` 仍是 `void*`（[components/Visual.hpp](../src/common/components/Visual.hpp)），纹理生命周期依赖外部管理 |
| C18 | 🔴 | `Registry::current()` / `Dispatcher::current()` 已加 terminate 防御，但 `entt::registry` 本身不具备线程安全；跨线程访问根本风险未消除 |
| C19 | 🔴 | 90 个用例全绿（11 文件），`LayoutSystem` / `RenderSystem` / `HitTestSystem` / `StateSystem` 仍 0 覆盖 |
| C20 | 🔴 | 仓库根无 `.github/workflows/`，CI 未建 |
| C30 | ℹ️ | `StateSystem.hpp` 扩展至 **1233 行**（较 v0.3 锐评时的 600+ 几近翻倍），`HitTestSystem.hpp` 371 行、`TextTextureCache.hpp` 430 行仍为纯头文件 inline 实现；大型 inline 实现造成编译时间膨胀 |
| C31 | ℹ️ | `on_event<>` 别名保留在 `Interaction.hpp`（`onClick`/`onDragStart`/`onDragEnd`/`onDragMove`/`onHover`/`onUnhover`/`onPress`/`onRelease`，8 处）与 `Data.hpp`（`onSubmit`/`onTextChanged`/`onValueChanged`/`onScroll`/`onChanged×2`，6 处），共 14 处；`std::move_only_function` 使组件不可拷贝，OP-28 P3 未推进 |

### 3. 总体评分变化

| 维度     | v0.3       | v0.4       | 说明 |
| -------- | ---------- | ---------- | ---- |
| 架构分层 | ★★★★☆ | ★★★★★ | C25 ActionSystem 对齐 `Dispatcher::Sink<>()`，分层无漏洞 |
| ECS 使用 | ★★★★☆ | ★★★★☆ | C26 按域拆分落地；14 处 `on_event<>` 仍破坏纯数据原则 |
| 渲染管线 | ★★★★☆ | ★★★★★ | C27 RenderSystem 拆分为 3 子文件，SRP 已满足 |
| 线程安全 | ★★☆☆☆ | ★★★☆☆ | C21/C22/C23 修复消除多 Runtime 路由错乱；C18 根因未动 |
| 可测性   | ★★★☆☆ | ★★★☆☆ | 测试数量不变（90），核心系统覆盖率依旧 0 |
| 代码整洁 | ★★★☆☆ | ★★★★☆ | C26/C27/C29 均已落地；StateSystem.hpp 1233 行为主要欠账 |
