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

### ⚠️ C7 — HitTestSystem 信号连接维护负担重（`HitTestSystem.hpp:36-77`）

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

### ⚠️ C10 — TableRenderer 全部实现内联在 .hpp（`TableRenderer.hpp`，约 350 行）

`collect()`、`updateCellWidgetLayouts()`、`computeColWidths()`、表头/单元格文字渲染全部内联在头文件中。每个包含该头文件的翻译单元都编译一份，显著增加编译时间和链接体积。

---

### ✅ ⚠️ C11 — TableRenderer 表头文字颜色硬编码白色（`TableRenderer.hpp:108`）

```cpp
static const Eigen::Vector4f headerTextColor{1.0F, 1.0F, 1.0F, 1.0F}; // 永远白色
```

`TableInfo` 已有 `cellTextColor` 字段，但表头文字颜色被遗忘，无法通过 API 或主题控制。

---

### ⚠️ C12 — TableRenderer::updateCellWidgetLayouts 写入时机错误

单元格控件的 `Position`/`Size` 在**渲染阶段**由 `TableRenderer::collect()` 写入，晚于 `LayoutSystem`（布局阶段）运行。`HitTestSystem` 使用的是 `LayoutSystem` 写入后的坐标；如果单元格控件刚被添加还未经历一次 `LayoutSystem::update()`，hit-test 坐标与渲染坐标可能不一致，导致首帧点击失效。

---

## 四、managers 模块

### ⚠️ C13 — TextTextureCache 无 GPU 设备保护（`TextTextureCache.hpp:88`）

`getOrUpload()` 在调用前未检查 `DeviceManager` 是否已初始化（Fallback 模式下 SDL GPU 设备为 null），可能通过 `createAndCacheTexture()` 触发空指针 SDL GPU API 调用，产生未定义行为。

---

### ℹ️ C14 — FontManager 裸 FT_Library/FT_Face 析构（`FontManager.hpp:62-82`）

FreeType / HarfBuzz 资源用裸指针持有，析构函数手动调用 `hb_font_destroy`、`FT_Done_Face` 等，没有 RAII 封装。若中途发生 `std::terminate`（如 worker 线程未捕获异常），资源会泄漏。建议用 `unique_ptr` + 自定义 deleter 封装。

---

## 五、common 模块

### ⚠️ C15 — ScrollArea 混杂交互状态（`Components.hpp:185-205`）

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

### ⚠️ C16 — TableInfo 持有回调违反纯数据原则（`Components.hpp:543`）

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
| C7 | ⚠️ | systems/HitTestSystem | 信号连接代码维护负担重 |
| C8 | ℹ️ | systems/ThemeSystem | 纯存根占用系统槽位 |
| C9 | ✅ 已完成 | renderers/IconRenderer | `~Texture` 位运算逻辑错误 |
| C10 | ⚠️ | renderers/TableRenderer | 约 350 行实现内联在 .hpp |
| C11 | ✅ 已完成 | renderers/TableRenderer | 表头文字颜色硬编码 |
| C12 | ⚠️ | renderers/TableRenderer | 单元格 Position 写入时机晚于 HitTest |
| C13 | ⚠️ | managers/TextTextureCache | 无 GPU 设备 null 保护 |
| C14 | ℹ️ | managers/FontManager | 裸 FreeType/HarfBuzz 指针无 RAII |
| C15 | ⚠️ | common/ScrollArea | 组件混杂运行时交互状态 |
| C16 | ⚠️ | common/TableInfo | 回调违反纯数据原则 |
| C17 | ⚠️ | common/Image | `void*` textureId 无生命周期管理 |
| C18 | 🔴 | singleton/Registry | thread_local 无法防止跨线程竞争 |
| C19 | 🔴 | tests/ | 核心系统无测试覆盖 |
| C20 | 🔴 | CI | 无持续集成管道 |

> 🔴 严重缺陷（影响正确性/稳定性）　⚠️ 中等问题（影响可维护性）　ℹ️ 轻微问题（建议改进）
