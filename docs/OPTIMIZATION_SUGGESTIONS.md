# PmkUi 优化与修改建议

> 版本：v0.2 · 日期：2026-05-19  
> 关联文档：[ARCHITECTURE_CRITIQUE.md](ARCHITECTURE_CRITIQUE.md)（问题来源）

---

## 优先级说明

| 标记 | 含义 |
|------|------|
| 🔴 **P0** | 正确性 Bug，应立即修复（影响功能/稳定性） |
| 🟠 **P1** | 重要改进，影响多实例/多线程场景（1~2 天内） |
| 🟡 **P2** | 中等优化，影响可维护性（1 周内） |
| 🟢 **P3** | 长期重构，需规划（视排期） |

---

## 一、P0 — 立即修复

### ✅ [已完成] OP-01 修复 IconRenderer 字体图标判断逻辑

**文件**：`src/renderers/IconRenderer.hpp:90`  
**问题**：`~policies::IconFlag::Texture` 是位运算取反，不等于逻辑非，几乎对所有非零 type 都返回 true，导致字体图标/纹理图标分支选择混乱，`type == Default` 时两个分支都不进入。

```cpp
// ❌ 当前（错误）
else if (HasFlag(iconComp->type, ~policies::IconFlag::Texture))

// ✅ 正确
else if (!HasFlag(iconComp->type, policies::IconFlag::Texture))
```

**验收**：补充回归测试 `test_IconRenderer.cpp`，覆盖 `type=Texture`、`type=Default`、`type=Font` 三个分支。

> **2026-05-20 已在代码中落地，文档更新为已完成状态。**

---

### ✅ [已完成] OP-02 修复 TableInfo 缺失 headerTextColor 字段

**文件**：`src/renderers/TableRenderer.hpp:108`、`src/common/Components.hpp`  
**问题**：表头文字颜色硬编码为白色，无法通过 API 或主题控制。

```cpp
// Components.hpp — TableInfo 中添加
Color headerTextColor{1.0F, 1.0F, 1.0F, 1.0F}; // 默认白色，可通过 TableHeaderTextColor() Chain 修改

// TableRenderer.hpp — 使用字段替代硬编码
const Eigen::Vector4f headerTextColor = toVec4(info->headerTextColor, 1.0F);
```

同步在 `src/api/Table.hpp` 中添加 `TableHeaderTextColor(const Color&)` Chain Action。

> **2026-05-20 已在代码中落地，文档更新为已完成状态。**

---

## 二、P1 — 重要改进（1~2 天）

### ✅ [已完成] OP-03 TextInputSystem 静态键盘状态改为实例成员

**文件**：`src/systems/TextInputSystem.hpp:85-87`  
**问题**：`inline static` 键盘状态在多 UiRuntime 间共享，多窗口场景下键盘重复串台。

```cpp
// ❌ 当前
inline static SDL_Keycode heldKey     = SDLK_UNKNOWN;
inline static uint64_t   keyPressTime = 0;
inline static uint64_t   lastRepeatTime = 0;

// ✅ 改为普通成员变量
SDL_Keycode m_heldKey      = SDLK_UNKNOWN;
uint64_t    m_keyPressTime = 0;
uint64_t    m_lastRepeatTime = 0;
```

**影响范围**：只需修改 `TextInputSystem.hpp`，不涉及其他文件。

> **2026-05-20 已在代码中落地，文档更新为已完成状态。**

---

### ✅ [已完成] OP-04 TimerSystem 任务表迁移到 registry.ctx()

**文件**：`src/systems/TimerSystem.hpp:83`、`TimerSystem.cpp:30`  
**问题**：类静态 `tasks` 和 `nextTaskId` 跨所有 UiRuntime 实例共享，多运行时场景下 timer ID 冲突、回调在错误 registry 上下文中触发。

**方案**：将 `TimerTask` 列表和 `nextTaskId` 存入 `registry.ctx<GlobalContext>()` 或独立的 `TimerContext`（类似 `FrameContext` 的存储方式）。

```cpp
// 移除类静态
// static std::vector<globalcontext::TimerTask> tasks;
// static uint32_t nextTaskId;

// 替换为 registry.ctx() 访问
auto& timerCtx = Registry::Ref().ctx().get_or_emplace<globalcontext::TimerContext>();
timerCtx.tasks.push_back(...);
```

> **2026-05-20 已在代码中落地，文档更新为已完成状态。**

---

### ✅ [已完成] OP-05 RenderSystem firstUpdate 改为实例成员

**文件**：`src/systems/RenderSystem.cpp:351`  
**问题**：`static bool firstUpdate = true` 函数级静态，所有 UiRuntime 共享，第二个运行时跳过首帧初始化。

```cpp
// ❌ 当前
static bool firstUpdate = true;

// ✅ 在 RenderSystem.hpp 中添加成员
bool m_firstUpdate = true;
```

> **2026-05-20 已在代码中落地，文档更新为已完成状态。**

---

### ✅ [已完成] OP-06 StateSystem 去除 factory 层反向依赖

**文件**：`src/systems/StateSystem.hpp:44`  
**问题**：系统层直接调用 `ui::factory::CloseDropDownPopup()`，破坏分层约束。

**方案**：

1. 在 `src/common/Events.hpp` 添加 `DropDownCloseRequested { entt::entity entity; }`
2. `StateSystem` 改为触发该事件：`Dispatcher::Trigger<events::DropDownCloseRequested>({entity})`
3. DropDown 相关系统（或 Factory 在初始化时注册的监听器）响应该事件执行关闭逻辑

> **2026-05-20 已在代码中落地，文档更新为已完成状态。**

---

### ✅ [已完成] OP-07 EventLoop 减少 CPU 空转

**文件**：`src/core/EventLoop.cpp`  
**问题**：当前 `run_for(1ms)` 每毫秒强制唤醒，帧率受 `RenderTask::delayTime=16ms` 控制，中间 15 次唤醒毫无意义，主线程 CPU 接近满载。

**方案 A（最小改动）**：使用 `SDL_WaitEventTimeout` 替代 `SDL_PollEvent` 等待 SDL 事件，同时通过 `asio::post` 打断等待：

```cpp
// InteractionSystem 中改为 SDL_WaitEventTimeout(1) — 最多等 1ms
// EventLoop 仍保持 run_for，但 run_for 时间拉长到 1ms（已有），
// 下一帧触发时由 asio::post(loopFunc) 打断等待，实现事件驱动唤醒
```

**方案 B（推荐）**：将帧管线提交改为 `asio::post` + `steady_timer`，`io_context` 用 `run()` 替代 `run_for` 轮询，让 ASIO 自己管理睡眠：

```cpp
void scheduleNextFrame() {
    m_frameTimer.expires_after(std::chrono::milliseconds(16));
    m_frameTimer.async_wait([this](const asio::error_code&) {
        m_defaultHandler();
        scheduleNextFrame();
    });
}
// exec() 改为 m_ioContext->run()（阻塞，由 timer 驱动唤醒）
```

> **2026-05-20 已在代码中落地，文档更新为已完成状态。**

---

## 三、P2 — 中等优化（1 周内）

### ✅ [已完成] OP-08 TableRenderer 拆分为 .hpp + .cpp

**文件**：`src/renderers/TableRenderer.hpp`（约 380 行纯实现）  
**问题**：全部实现内联在头文件，每个包含它的 TU 都编译一份，增加编译时间。

**方案**：将 `collect()` 及全部私有方法移入 `src/renderers/TableRenderer.cpp`，`TableRenderer.hpp` 只保留类声明。同步在 `src/CMakeLists.txt` 中添加 `TableRenderer.cpp` 到 `ui` target 的 `SOURCES`。

> **2026-05-20 已在代码中落地，文档更新为已完成状态。**

---

### ✅ [已完成] OP-09 TableRenderer 单元格 Position 写入时机前移

**文件**：`src/renderers/TableRenderer.hpp:282-316` (`updateCellWidgetLayouts`)  
**问题**：`updateCellWidgetLayouts` 在**渲染阶段**写入单元格控件坐标，但 `HitTestSystem` 使用**布局阶段**后的坐标，首帧可能点击失效（坐标为 (0,0)）。

**方案**：提取为独立方法，监听 `UpdateLayout` 事件，在 `LayoutSystem::update()` 之后、`RenderSystem::update()` 之前执行：

```cpp
// 新增 TableLayoutSystem（或在 LayoutSystem 中集成）
Dispatcher::Sink<events::UpdateLayout>().connect<&TableLayoutSystem::update>(this);
// update() 中遍历所有 TableTag 实体，调用 updateCellWidgetLayouts
```

> **2026-05-20 已在代码中落地，文档更新为已完成状态。**

---

### ✅ [已完成] OP-10 ScrollArea 组件拆分交互状态

**文件**：`src/common/Components.hpp:185-205`  
**问题**：`scrollbarHovered/scrollbarPressed/trackHovered` 是运行时 UI 状态，不应混入纯数据组件。

**方案**：拆出 `ScrollBarInteractionState` 组件（放 `Tags.hpp` 或独立结构体），仅当有交互时 `EmplaceOrReplace`：

```cpp
// 新增
struct ScrollBarInteractionState {
    using is_component_tag = void;
    bool scrollbarHovered = false;
    bool scrollbarPressed = false;
    bool trackHovered     = false;
};

// ScrollArea 中删除三个 bool 字段
```

> **2026-05-20 已在代码中落地，文档更新为已完成状态。**

---

### ✅ [已完成] OP-11 TextTextureCache 添加 GPU 设备 null 保护

**文件**：`src/managers/TextTextureCache.hpp:88`

```cpp
SDL_GPUTexture* getOrUpload(...) {
    SDL_GPUDevice* device = m_deviceManager.getDevice();
    if (device == nullptr || !m_fontManager.isLoaded()) return nullptr; // ✅ 已有检查

    // ✅ 新增：Fallback 模式下 SDL GPU 设备不可用，直接返回 null
    if (!SDL_GPUDeviceSupportsTextureFormat(device, SDL_GPU_TEXTUREFORMAT_R8_UNORM, ...))
        return nullptr;
    ...
}
```

实际上已有 `if (device == nullptr) return nullptr` 检查，需确认 Fallback 模式下 `getDevice()` 确实返回 null 而非无效指针。

> **2026-05-20 已在代码中落地，文档更新为已完成状态。**

---

### ✅ [已完成] OP-12 HitTestSystem 信号连接改为 Tag 驱动

**文件**：`src/systems/HitTestSystem.hpp:36-77`  
**问题**：约 20 对 `connect/disconnect` 信号注册，维护成本高，新增可交互组件必须手动添加信号。

**方案**：

1. 添加 `HitCacheInvalidateTag`（空 Tag）
2. 任何影响命中结果的组件变更时（`Clickable`、`VisibleTag`、`DisabledTag` 的 Construct/Destroy），自动打 `HitCacheInvalidateTag` 到对应实体
3. `HitTestSystem::update()` 开头检查 `Registry::View<HitCacheInvalidateTag>()` 是否非空，非空则重建缓存，然后批量移除 Tag

减少约 75% 的信号连接代码。

> **2026-05-20 已在代码中落地，文档更新为已完成状态。**

---

## 四、P3 — 长期重构

### ✅ [已完成] OP-13 TableInfo 回调迁移至事件驱动

**文件**：`src/common/Components.hpp:543`  
**问题**：`on_event<int, int> onCellClicked` 使 `TableInfo` 不可拷贝。

**方案**：

1. 在 `Events.hpp` 添加 `TableCellClicked { entt::entity table; int row; int col; }`
2. 用 `Dispatcher::Trigger<events::TableCellClicked>({entity, row, col})` 替代回调调用
3. 客户端改为 `Dispatcher::Sink<events::TableCellClicked>().connect<&Handler>(this)` 订阅
4. 从 `TableInfo` 中删除 `onCellClicked` 字段

> **2026-05-20 已在代码中落地，文档更新为已完成状态。**

---

### OP-14 Image::textureId 改为类型化 RAII 句柄

**文件**：`src/common/Components.hpp:345`  
**问题**：`void* textureId` 无类型、无生命周期管理。

**方案**：

```cpp
// 新增类型别名
using ImageHandle = std::shared_ptr<wrappers::UniqueGPUTexture>;

// Image 组件
struct Image {
    using is_component_tag = void;
    ImageHandle textureHandle; // 替代 void* textureId
    // ...
};
```

`ImageManager` 返回 `ImageHandle`，通过引用计数管理纹理生命周期。

---

### OP-15 FontManager FreeType/HarfBuzz 资源 RAII 化

**文件**：`src/managers/FontManager.hpp:62-82`

```cpp
// 当前裸指针
FT_Library m_ftLibrary = nullptr;
FT_Face    m_ftFace    = nullptr;
hb_font_t* m_hbFont   = nullptr;

// ✅ 改为 unique_ptr + 自定义 deleter
struct FtLibraryDeleter  { void operator()(FT_Library p) const { FT_Done_FreeType(p); } };
struct FtFaceDeleter     { void operator()(FT_Face    p) const { FT_Done_Face(p); } };
struct HbFontDeleter     { void operator()(hb_font_t* p) const { hb_font_destroy(p); } };

std::unique_ptr<std::remove_pointer_t<FT_Library>, FtLibraryDeleter> m_ftLibrary;
std::unique_ptr<std::remove_pointer_t<FT_Face>,    FtFaceDeleter>    m_ftFace;
std::unique_ptr<hb_font_t, HbFontDeleter>                            m_hbFont;
```

---

### OP-16 实现 ThemeSystem

**文件**：`src/systems/ThemeSystem.hpp`（当前纯存根）

**方案**：

1. 在 `registry.ctx()` 中存储 `ThemeData`（一组颜色 token，如 `primary/accent/background/text`）
2. `ThemeSystem` 监听 `ThemeChangedEvent`，收到后遍历相关实体批量 `EmplaceOrReplace<Background>(entity, theme.backgroundColor)`
3. 在 `src/api/` 层添加 `ui::theme::SetTheme(const ThemeData&)` 入口

---

### OP-17 建立 CI 管道

**当前状态**：无任何自动化构建/测试管道，所有验证依赖手动触发。

**建议**（`.github/workflows/ci.yml`）：

```yaml
# 最小可行 CI
on: [push, pull_request]
jobs:
  build-test:
    runs-on: windows-latest
    steps:
      - uses: actions/checkout@v4 --recursive
      - name: Configure
        run: cmake -G Ninja -B build -DENABLE_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug
      - name: Build
        run: cmake --build build
      - name: Test
        run: ctest --test-dir build -V --output-on-failure
```

---

### OP-18 补全核心系统测试覆盖

按优先级排序需补充的测试：

| 优先级 | 测试文件（新增） | 关键测试场景 |
|--------|----------------|-------------|
| 高 | `test_HitTestSystem.cpp` | Z-Order 排序、ScrollArea 偏移后命中坐标、禁用实体跳过 |
| 高 | `test_LayoutSystem.cpp` | Yoga Flexbox 计算、HFill/VFixed、嵌套布局 |
| 高 | `test_TableRenderer.cpp` | `computeColWidths`、`updateCellWidgetLayouts` 坐标计算 |
| 中 | `test_TimerSystem.cpp` | 单次/循环触发、取消、多任务竞争 |
| 中 | `test_StateSystem.cpp` | 焦点转移、hover 状态机、滚动条拖拽 |
| 中 | `test_IconRenderer.cpp` | Texture/Font/Default 三分支（回归 OP-01） |
| 低 | `test_TextInputSystem.cpp` | 键盘重复 delay/interval、多实例独立性 |
| 低 | `test_InteractionSystem.cpp` | SDL 事件 → Dispatcher 分发路径 |

---

## 改动优先级汇总

| ID | 优先级 | 预期工作量 | 文件 | 描述 |
|----|--------|-----------|------|------|
| OP-01 | 🔴 P0 | 0.5h | IconRenderer.hpp | 修复 `~Texture` 位运算 Bug |
| OP-02 | 🔴 P0 | 1h | Components.hpp + TableRenderer.hpp | 补 headerTextColor 字段 |
| OP-03 | 🟠 P1 | 0.5h | TextInputSystem.hpp | 静态键盘状态改为成员 |
| OP-04 | 🟠 P1 | 2h | TimerSystem | 任务表迁移到 registry.ctx() |
| OP-05 | 🟠 P1 | 0.5h | RenderSystem.cpp | firstUpdate 改为成员变量 |
| OP-06 | 🟠 P1 | 1h | StateSystem + Events.hpp | 去除 factory 反向依赖 |
| OP-07 | 🟠 P1 | 2h | EventLoop | 减少 CPU 空转（方案 B） |
| OP-08 | 🟡 P2 | 1h | TableRenderer | 拆分 .hpp → .hpp + .cpp |
| OP-09 | 🟡 P2 | 2h | TableRenderer / LayoutSystem | 单元格坐标写入时机前移 |
| OP-10 | 🟡 P2 | 1h | Components.hpp | ScrollArea 拆分交互状态 |
| OP-11 | 🟡 P2 | 0.5h | TextTextureCache | Fallback 模式 null 保护 |
| OP-12 | 🟡 P2 | 3h | HitTestSystem | 信号连接改为 Tag 驱动 |
| OP-13 | 🟢 P3 | 3h | TableInfo + Events | 回调迁移至事件驱动 |
| OP-14 | 🟢 P3 | 2h | Image / ImageManager | textureId 改为 RAII 句柄 |
| OP-15 | 🟢 P3 | 1h | FontManager | FreeType/HarfBuzz RAII 化 | ✅ 已完成 |
| OP-16 | 🟢 P3 | 4h | ThemeSystem | 主题系统实现 |
| OP-17 | 🟢 P3 | 2h | .github/workflows | 建立 CI 管道 |
| OP-18 | 🟢 P3 | 8h | tests/unittest/ | 补全核心系统测试 |

---

## v0.3 新增建议（2026-05-20）

> 关联：[ARCHITECTURE_CRITIQUE.md § v0.3](ARCHITECTURE_CRITIQUE.md)（C21–C31）。  
> 编号接续 OP-18，本次共新增 **9 条**（P0×3 / P1×2 / P2×3 / P3×1）。

### P0 — 立即修复

#### 🔴 OP-19 — TextInputSystem 用事件驱动替代 `static s_current` 路由

**关联**：C21  
**文件**：[src/systems/TextInputSystem.hpp:42-44](../src/systems/TextInputSystem.hpp#L42-L44), [src/systems/TextInputSystem.hpp:94](../src/systems/TextInputSystem.hpp#L94)；调用方 [src/core/TaskChain.hpp:121](../src/core/TaskChain.hpp#L121)

**问题**：`static void processKeyRepeat()` 入口靠 `static inline TextInputSystem* s_current` 指向"最后一个注册的实例"。多 UiRuntime 注册时后者覆盖前者，前一个运行时的键盘重复永远不会触发——把 v0.2 C4 的"状态串台"换成了"路由丢失"。

**改法**：废弃 `processKeyRepeat()` 静态入口与 `s_current`，改为订阅一个 `events::TickKeyRepeat`：

```cpp
// Events.hpp
struct TickKeyRepeat { /* 空事件 */ };

// TextInputSystem.hpp
void registerHandlersImpl() {
    Dispatcher::Sink<events::TickKeyRepeat>().connect<&TextInputSystem::doProcessKeyRepeat>(*this);
    // ... 其他订阅保持
}
// 移除 s_current / processKeyRepeat()

// TaskChain.hpp - InputTask
void operator()(uint32_t delta) {
    if (remainingTime > delta) { remainingTime -= delta; return; }
    remainingTime = delayTime;
    InteractionSystem::pollSdlEvents();
    Dispatcher::Trigger<events::TickKeyRepeat>();   // 替代静态调用
}
```

**验收**：新增 `test_TextInputSystem_MultiRuntime.cpp`，构造两个 UiRuntime 各自注册 TextInputSystem，分别在两个 scope 内推送 RawKeyInput + TickKeyRepeat，断言各自的 `m_heldKey` / 重复行为彼此独立。

---

#### 🔴 OP-20 — `Registry::current()` / `Dispatcher::current()` 增加 Release 防护

**关联**：C22  
**文件**：[src/singleton/Registry.hpp:38-43](../src/singleton/Registry.hpp#L38-L43), [src/singleton/Dispatcher.hpp:42-47](../src/singleton/Dispatcher.hpp#L42-L47)

**问题**：仅 `assert()` 防御，Release 下空指针解引用产生 UB；调用栈 inline 后难以诊断。

**改法**：

```cpp
// Registry.hpp
static Registry& current() {
    auto* instance = activeInstance();
    if (instance == nullptr) [[unlikely]] {
        // 既要在 Release 也生效，又要保留调用位置信息
        ui::Logger::error("[Registry] current() called outside UiRuntimeScope (at {})",
                          std::source_location::current());
        std::terminate();
    }
    return *instance;
}
// Dispatcher 同步修改
```

或者改抛 `std::logic_error`，由更上层的 `Application::exec()` 统一捕获并打印诊断后退出。

**验收**：补 `test_RegistryCurrent_NoActive.cpp`，用 `EXPECT_DEATH(Registry::current(), "outside UiRuntimeScope")` 校验 Release 路径也会终止。

---

#### 🔴 OP-21 — `pollSdlEvents` / `processKeyRepeat` 由 runtime 实例驱动

**关联**：C23（与 OP-19 协同推进）  
**文件**：[src/systems/InteractionSystem.hpp:75](../src/systems/InteractionSystem.hpp#L75), [src/core/TaskChain.hpp:119-126](../src/core/TaskChain.hpp#L119-L126)

**问题**：`TaskChain::InputTask` 直接调用 `InteractionSystem::pollSdlEvents()` / `TextInputSystem::processKeyRepeat()` 两个 `static` 入口；SDL 事件归属完全依赖"当前 thread_local 激活的 runtime"，多 runtime 同帧调度会错配。

**改法（最小落地）**：

1. `InteractionSystem::pollSdlEvents()` 改为成员方法，`SystemManager::pollInput()` 转发到实例方法。
2. `TaskChain::InputTask` 不再持有静态调用，改为持有 `SystemManager*`：

```cpp
struct InputTask {
    using is_task_tag = void;
    SystemManager* systems;          // 注入
    uint32_t remainingTime = 0;
    uint32_t delayTime = 32;

    void operator()(uint32_t delta) {
        if (remainingTime > delta) { remainingTime -= delta; return; }
        remainingTime = delayTime;
        systems->pollInput();        // 由具体 runtime 路由
    }
};
```

3. `SystemManager::pollInput()` 在内部按已注册顺序找到 `InteractionSystem` / `TextInputSystem` 实例并调用。

**验收**：双 runtime 测试中各自 emplace 一个 Button，向窗口 A 模拟点击，断言只有 runtime A 触发 ClickEvent。

---

### P1 — 重要改进

#### 🟠 OP-22 — `SystemManager` 引入显式 Phase

**关联**：C24  
**文件**：[src/core/SystemManager.hpp:64](../src/core/SystemManager.hpp#L64), [src/interface/Isystem.hpp](../src/interface/Isystem.hpp)

**问题**：系统执行顺序由 `addSystem` 调用顺序隐式决定，新增系统插错位置会破坏帧管线。

**改法**：在 `ISystem` polymorphic 接口上加 `getPhase()`，`SystemManager` 内部按 phase 分桶后再追加：

```cpp
enum class SystemPhase : uint8_t {
    Input = 0, Logic = 1, Animation = 2, Layout = 3, Render = 4, Frame = 5
};

// ISystem concept
struct ISystem {
    virtual SystemPhase getPhase() const noexcept = 0;
    // ... 其余
};

// SystemManager
template <typename T> void addSystem(T&& system) {
    m_systems.emplace_back(std::forward<T>(system));
    std::ranges::stable_sort(m_systems, {}, &ISystem::getPhase);
}
```

**验收**：把 `RenderSystem.addSystem` 调用挪到 `LayoutSystem` 之前，单测 `test_SystemPhaseOrder.cpp` 仍能验证 `RenderSystem` 在 `LayoutSystem` 后被执行。

---

#### 🟠 OP-23 — `EventLoop::quit()` 改为优雅 drain

**关联**：C28  
**文件**：[src/core/EventLoop.cpp:46-53](../src/core/EventLoop.cpp#L46-L53)

**问题**：`io_context::stop()` 立即丢弃未消费 handler，可能留下半提交的 GPU 帧。

**改法**：

```cpp
void EventLoop::quit() {
    if (!m_running.exchange(false)) return;

    // 1) 取消定时器，让最后一次 endFrame 任务消费完成
    m_frameTimer.cancel();
    // 2) 释放 work guard 后 io_context 自然 return；不再调用 stop()
    m_workGuard.reset();
    // 注意：调用方在 exec() 之后已经退出 run()，无需 stop()
}
```

并在 `Application::exec()` 退出后追加 `m_eventLoop /*natural drain done*/;` 注释。

**验收**：覆盖 `test_EventLoopQuitDrain.cpp`，断言 `quit()` 后 `m_ioContext->run()` 自然返回，且最后一次 `EndFrame` handler 被执行。

---

### P2 — 中等优化

#### 🟡 OP-24 — `ActionSystem` 统一走 `Dispatcher::Sink<>()`

**关联**：C25  
**文件**：[src/systems/ActionSystem.hpp:38-60](../src/systems/ActionSystem.hpp#L38-L60)

**问题**：唯一直访 `RuntimeFacade::current().enttDispatcher().sink<>()` 的系统，与全仓风格不一致，绕过 `traits::Events` 约束。

**改法**：

```cpp
void registerHandlersImpl() {
    Dispatcher::Sink<events::ClickEvent>().connect<&ActionSystem::onClickEvent>(*this);
    Dispatcher::Sink<events::HoverEvent>().connect<&ActionSystem::onHoverEvent>(*this);
    // ...
}
```

`unregisterHandlersImpl()` 镜像替换。

**验收**：编译期 `traits::Events` concept 通过；测试运行中 ClickEvent 仍正常触发动画。

---

#### 🟡 OP-25 — `Components.hpp` 按域拆分

**关联**：C26  
**文件**：[src/common/Components.hpp](../src/common/Components.hpp)（740+ 行）

**改法**：保留 `Components.hpp` 作为聚合 include，新增：

```
src/common/components/
  Visual.hpp      // Scale / RenderOffset / Alpha / Background / Image
  Layout.hpp      // Size / Position / Margin / Padding / LayoutInfo / Spacer
  Interaction.hpp // Clickable / Hoverable / Drag / ScrollArea / ScrollBarInteractionState
  Data.hpp        // TableInfo / TableCell / ListArea / TextEdit / SliderInfo / CheckBoxInfo
  Window.hpp      // Window / WindowTag / DialogTag / Geometry
```

`common/Components.hpp` 只剩：

```cpp
#pragma once
#include "components/Visual.hpp"
#include "components/Layout.hpp"
#include "components/Interaction.hpp"
#include "components/Data.hpp"
#include "components/Window.hpp"
```

**验收**：`cmake --build` 全绿；`tests` 90 用例不变。可顺便比对 PCH 增量编译时间。

---

#### 🟡 OP-26 — `TimerSystem` 用 `unordered_map` + slot/generation 句柄

**关联**：C29  
**文件**：[src/systems/TimerSystem.cpp:42-75](../src/systems/TimerSystem.cpp#L42-L75)

**改法**：

```cpp
// GlobalContext.hpp
struct TimerContext {
    std::unordered_map<uint32_t, TimerTask> tasks;
    uint32_t nextTaskId = 1;
};

// TimerSystem::cancelTask -> O(1) erase
void TimerSystem::cancelTask(uint32_t handle) {
    auto& ctx = RuntimeFacade::current().ensureContext<globalcontext::TimerContext>();
    ctx.tasks.erase(handle);
}

// update() 内：遍历 tasks，singleShot 触发后直接 erase；ID wrap 保护见下
```

可选：句柄打包 `uint32_t handle = (slot << 16) | generation`，过期 cancel 不再撞车。

**验收**：补 `test_TimerSystem_Cancel.cpp`，断言 cancel 后 `tasks.size()` 立刻收缩、`addTask + cancelTask` 1 万次循环不再持续增长。

---

### P3 — 长期重构

#### ✅ OP-27 — `RenderSystem.cpp` 拆分为 3 个子组件（已完成）

**关联**：C27  
**文件**：[src/systems/RenderSystem.cpp](../src/systems/RenderSystem.cpp)（720+ 行）

**改法**：

```
src/systems/render/
  RenderBackend.cpp     // ensureInitialized / Fallback / DeviceManager 装配
  RenderResources.cpp   // 白纹理、字体/图标缓存初始化、清理
  RenderFrame.cpp       // update() / collectRenderData() / 每帧 collect→batch→submit
```

`RenderSystem` 保留为外观类，转发到三个内部协作者；接口、对外测试用例不变。

**验收**：所有 90 个单测继续绿；新增 `test_RenderBackend_FallbackFlag.cpp` 单独覆盖后端选择分支。

---

#### 🟢 OP-28 — 组件层剩余 `on_event<>` 全部迁移到事件总线

**关联**：C31  
**文件**：[src/common/Components.hpp:32, 294-735](../src/common/Components.hpp#L32)（13 处使用）

**改法**：参考 OP-13 模板，逐个迁移 `TextEdit::onSubmit` → `events::TextEditSubmit`，`Button::onClick` → `events::ButtonClicked`，`Slider::onValueChanged` → `events::SliderValueChanged`，等等。删除 `on_event` 别名。

**验收**：`Components.hpp` 内 `move_only_function` 出现次数从 13 降到 0；组件全部可拷贝；entt snapshot 测试可启用。

---

### v0.3 改动优先级汇总（接续）

| ID | 优先级 | 预期工作量 | 文件 | 描述 |
|----|--------|-----------|------|------|
| OP-19 | 🔴 P0 | 2h | TextInputSystem + Events + TaskChain | 事件驱动替代 `static s_current` | ✅ 已完成 |
| OP-20 | 🔴 P0 | 0.5h | Registry / Dispatcher | `current()` 增加 Release 防护 | ✅ 已完成 |
| OP-21 | 🔴 P0 | 2h | InteractionSystem + TaskChain + SystemManager | static 入口实例化 | ✅ 已完成 |
| OP-22 | 🟠 P1 | 2h | SystemManager + ISystem | 显式 Phase 排序 | ✅ 已完成 |
| OP-23 | 🟠 P1 | 1h | EventLoop | `quit()` 改为自然 drain | ✅ 已完成 |
| OP-24 | 🟡 P2 | 0.5h | ActionSystem | 统一 `Dispatcher::Sink<>()` | ✅ 已完成 |
| OP-25 | 🟡 P2 | 3h | common/components/ | `Components.hpp` 按域拆分 | ✅ 已完成 |
| OP-26 | 🟡 P2 | 2h | TimerSystem | `unordered_map` + O(1) cancel | ✅ 已完成 |
| OP-27 | 🟢 P3 | 6h | systems/render/ | `RenderSystem.cpp` 拆 3 子组件 |
| OP-28 | 🟢 P3 | 8h | Components + Events | 剩余 `on_event<>` 全迁移 |
