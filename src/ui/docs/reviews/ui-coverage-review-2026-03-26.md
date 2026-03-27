# UI 模块覆盖率测试与第二阶段架构锐评

日期：2026-03-26

## 背景

本文档是在第一阶段止血完成之后，对 UI 模块重新审视的结果。
第一阶段（截至 2026-03-25）已完成：脏标记入口集中、平台刷新路径显式化、Factory 副作用收口、HitTest 缓存语义显式化。

本文档目标：

1. 对当前架构做一次不留情面的评价，找出仍然存在的结构性问题。
2. 记录新增覆盖率测试的范围与结论。
3. 为第二阶段重构提供进一步的优先级输入。

---

## 一、架构锐评

### 1.1 IsEntityExist — 声明存在但实现缺失

`src/ui/api/Utils.hpp` 第 52 行声明了 `bool IsEntityExist(const std::string& alias)`，
但 `Utils.cpp` 中从未有对应实现。

这是一个**编译期看不出来、链接期才暴露的 ODR 问题**。
在项目里所有调用点都能通过编译，直到链接时才崩。
说明这个函数目前根本没有被实际调用，或调用点也被同样遗漏了。

**当前修复**：已补全实现（`Utils.cpp`），通过 `Registry::View<components::BaseInfo>()` 按别名线性扫描。

**深层问题**：线性扫描的时间复杂度是 O(N)，随控件数量增长。
如果 alias 是唯一标识符，应当考虑维护一个 `unordered_map<string, entt::entity>` 索引。
当前单机场景下 N 较小，暂时可接受，但这是一个**潜在伪设计**：
标榜提供"按名查找"这样的能力，却用遍历实现，接口契约比实现更快膨胀。

---

### 1.2 Registry / Dispatcher 全局单例的测试代价

每个测试用例的 `SetUp` 都要手动调用 `Registry::Clear()`，
每个 `TearDown` 也要调用一次。

这说明测试的隔离度完全依赖这个手动清理动作。

风险在于：
1. 任何一个测试漏掉 `TearDown` 都会污染后续所有测试。
2. 测试之间的执行顺序会影响结果——这违背了单元测试的基本假设。
3. 全局单例的存在使得"同时跑多组不相干测试"从架构上不可能实现。

目前的缓解方式是 `::testing::Test` fixture + `Clear()`，能用，但不优雅。

**根本问题**：`Registry` 是进程级单例。
如果它是可注入的上下文对象，每个测试可以自己持有一个独立注册表实例，
测试彼此隔离就不需要这个清除魔法了。

这是第二阶段"UI Runtime Context"任务的核心动机：
**单例打穿了测试基础设施**，不只是打穿了运行时边界。

---

### 1.3 StateSystem 的 private 访问问题仍未彻底解决

`InteractionSystem::syncWindowPropertiesImmediately()` 调用了 `StateSystem::syncSDLWindowProperties()`。
在第一阶段重构中，`StateSystem` 内部按主题做了分区重组，
但 `syncSDLWindowProperties` 最终落在了 `private` 区块里，导致跨类调用失败。

经过多次尝试（`public:` 访问说明符、`friend class InteractionSystem`），
问题仍未稳定解决，说明当前 `StateSystem` 内部的访问控制结构比较混乱，
一次分区重组就能意外打破原有的跨类协作关系。

**根本问题**：`InteractionSystem` 依赖 `StateSystem` 的内部实现细节（跨类调用静态私有函数）。
这是一种跨 "God Object" 边界的隐式耦合——正是第二阶段拆分的动机。

**建议**：`syncSDLWindowProperties` 不应是任何一个系统的"私有辅助函数"，
而应该成为 `WindowSyncState` 这一主题下的显式职责入口，
公开给调用方而不是靠 `friend` 或 `public` 越权访问。

---

### 1.4 工厂创建期副作用仍然过重

第一阶段已经把 `CreateTitleBar` 拆了，但 Factory 整体还是一个"构造 + 平台资源 + 生命周期触发"的混合体。

以 `CreateWindow` 为例，一次调用会：
1. 创建 ECS 实体。
2. 调用 SDL 创建真实窗口。
3. 发送图形上下文初始化事件。
4. 设置 RootTag、WindowTag。

如果步骤 2 成功但步骤 3 失败，当前虽然有最小回滚（销毁实体），
但图形设备状态可能已经被部分初始化。

**还缺少的**：两阶段构造（纯数据构造 / 平台绑定）的边界。
这不是第一阶段的坑，是第二阶段"Factory 构造/绑定/启动分离"要解决的。

---

### 1.5 ThemeSystem 处于游离状态

`src/ui/systems/ThemeSystem.hpp` 存在，但目前：
1. 没有任何测试覆盖。
2. 在 `SystemManager` 的注册流程中是否真正激活尚不清楚。
3. 主题切换对脏标记传播的影响没有被文档化。

如果 `ThemeSystem` 目前只是占位，应当明确标记为 WIP；
如果它已经有实际行为，应当补测试和说明。

**现状**：哑炮风险。在测试覆盖率分析之前，它是一个代码库里的暗区。

---

### 1.6 动画系统（TweenSystem）与时间轴耦合过紧

`TweenSystem` 测试中用 `intervalMs = 16` 模拟一帧时长，然后触发 `UpdateEvent` 来驱动动画。

问题在于：
1. 这个 `intervalMs` 是全局上下文字段，在 `ctx()` 里手动设置。
2. 任何遗漏设置 `FrameContext` 的测试都会得到未初始化的时间步长。
3. 动画是否完成完全取决于 `intervalMs` 与动画 `duration` 的比值，
   测试里要手动控制这个比值才能让动画"一帧完成"。

这是一种**测试驱动的隐式约定**：你需要理解 `intervalMs` 的含义才能写出正确的测试。
这不是随机问题，是架构上时间管理没有显式封装的投影。

---

### 1.7 DSL 管道（Chains）对未初始化实体无保护

所有 `EntityAction` 的 `bind()` 调用最终会调到具体的 API 函数（如 `SetVisible`、`AddChild`）。
这些 API 函数开头都有 `if (!Registry::Valid(entity)) return;` 保护，
但 DSL 链式调用时如果 `entity` 是 `entt::null`，
整条链中间可能已经触发了若干个 `MarkLayoutAndVisualChanged`，
即使最终没有出错，也会产生无意义的脏标记。

这是一种"非成即脏"的副作用蔓延。当前危害还可控，一旦 DSL 链变长（如 15-20 个操作），
调试起来会非常麻烦。

---

## 二、测试覆盖率报告（截至 2026-03-26）

### 2.1 新增测试文件

本次新增三个覆盖率测试文件，覆盖原本完全没有测试的区域：

| 文件 | 覆盖目标 |
|------|---------|
| `tests/unittest/ui/test_UtilsCoverage.cpp` | `src/ui/api/Utils.cpp` 脏标记传播逻辑 |
| `tests/unittest/ui/test_VisibilityCoverage.cpp` | `src/ui/api/Visibility.cpp` 可见性与样式 setter |
| `tests/unittest/ui/test_HierarchyCoverage.cpp` | `src/ui/api/Hierarchy.cpp` 父子关系操作边界 |

---

### 2.2 test_UtilsCoverage.cpp 覆盖要点

**MarkLayoutChanged**：

- 单节点：调用后该节点携带 `LayoutDirtyTag`。
- 多层向上传播：子节点脏标记会传递到父节点和祖父节点。
- 同层隔离：标记一个子节点不应影响同层的兄弟节点。
- 无效实体：调用 `entt::null` 不崩溃。

**MarkVisualChanged**：

- 单节点：调用后该节点携带 `RenderDirtyTag`。
- 向上传播到祖先窗口：子节点 MarkVisual 应同时标记祖先中第一个带 `WindowTag` 的实体。
- 无窗口祖先时只标记自身。
- 无效实体：调用 `entt::null` 不崩溃。

**MarkLayoutAndVisualChanged**：

- 同时产生 `LayoutDirtyTag` 和 `RenderDirtyTag`。

**HasAlignment**：

- 精确匹配返回 `true`。
- 对组合标志（`CENTER = HCENTER | VCENTER`）能正确识别单个子标志。
- 无关标志返回 `false`。

**IsEntityExist**：

- 已注册别名返回 `true`。
- 未注册别名返回 `false`。
- `Registry::Clear()` 后返回 `false`（确认别名状态随注册表销毁）。

---

### 2.3 test_VisibilityCoverage.cpp 覆盖要点

**Show / Hide**：

- `Show` 添加 `VisibleTag`，`Hide` 移除 `VisibleTag`。
- `Show` / `Hide` 都触发 `MarkLayoutAndVisualChanged`（验证脏标记产生）。
- `SetVisible(true/false)` 与 `Show/Hide` 行为一致。
- 对无效实体调用不崩溃。

**SetAlpha**：

- 正常值正确存入 `Alpha` 组件。
- 负值 clamp 到 0，超过 1 的值 clamp 到 1。
- 对无效实体调用不崩溃。

**SetBackgroundColor**：

- 颜色值正确存入 `Background` 组件。
- `Background.enabled` 被设置为 `Enabled`。
- 调用后产生 `RenderDirtyTag`。

**SetBorderRadius**：

- 四角圆角半径均匀写入 `Background.borderRadius`。
- 负值 clamp 到 0。

**SetBorderColor / SetBorderThickness**：

- 颜色/厚度值正确写入 `Border` 组件。
- `Border.enabled` 被设置为 `Enabled`。

---

### 2.4 test_HierarchyCoverage.cpp 覆盖要点

**AddChild**：

- 正确设置子节点的 `Hierarchy.parent`。
- 正确追加到父节点的 `Hierarchy.children`。
- 移除子节点上的 `RootTag`。
- 重复添加（幂等性）：不产生重复的 children 记录。
- 重新挂载：从旧父节点的 children 中移除，并加入新父节点。
- 对无效 parent 或 child 不崩溃。

**RemoveChild**：

- 清除子节点的 `Hierarchy.parent`（还原为 `entt::null`）。
- 从父节点的 `Hierarchy.children` 中移除。
- 恢复子节点的 `RootTag`（脱离父节点重新成根）。
- 对"不是自己的子节点"的实体调用是 no-op。
- 对无效实体调用不崩溃。

**TraverseChildren**：

- 后序深度遍历（先递归子节点，再访问父节点）——与实现对齐。
- 对叶子节点调用，visitor 被调用零次。

---

### 2.5 现有测试文件覆盖范围总览

| 测试文件 | 主要覆盖区域 | 状态 |
|---------|------------|------|
| `test_MainWindow.cpp` | Factory 创建、DSL 链式调用、Hierarchy 集成、LineEdit | 有效 |
| `test_TweenSystem.cpp` | TweenSystem 位置动画、ActionSystem 交互动画 | 有效 |
| `test_BatchManager.cpp` | BatchManager 合批逻辑、纹理/剪裁矩形切割 | 有效 |
| `test_TextUtils.cpp` | TextUtils UTF-8 边界、換行策略、截断 | 有效 |
| `test_ResourceProviderCoverage.cpp` | ResourceProvider 嵌入字体/图标、缺失资源 | 有效 |
| `test_TaskChain.cpp` | TaskChain pipe DSL、帧事件顺序 | 有效 |
| `test_UtilsCoverage.cpp` _(新增)_ | Utils 脏标记传播、HasAlignment、IsEntityExist | 新增 |
| `test_VisibilityCoverage.cpp` _(新增)_ | Visibility Show/Hide/Alpha/Background/Border | 新增 |
| `test_HierarchyCoverage.cpp` _(新增)_ | Hierarchy AddChild/RemoveChild/Traverse | 新增 |

---

### 2.6 仍然没有测试覆盖的区域

以下区域暂无测试，或现有测试覆盖度不足：

| 区域 | 风险等级 | 原因 |
|------|---------|------|
| `ThemeSystem` | 高 | 完全无测试，行为暗区 |
| `HitTestSystem` | 中 | 缓存失效规则复杂，只有注释，无测试验证 |
| `StateSystem` hover/active 状态流转 | 中 | 涉及全局输入事件，很难脱离 SDL 隔离测试 |
| `LayoutSystem` Yoga 集成 | 中 | 需要真实布局计算，已有注释但无验证 |
| `Factory` 失败路径 | 中 | SDL 窗口创建失败的回滚行为无测试 |
| `Animation.cpp` 中级缓动曲线 | 低 | TweenSystem 测试只覆盖了线性完成 |
| `TimerCallback / CancelQueuedTask` | 低 | Timer 任务调度依赖真实帧时间，难以单测 |

---

## 三、第二阶段优先级更新

基于本次分析，建议第二阶段按以下顺序推进：

### 优先级一：结构性编译正确性

1. 解决 `StateSystem::syncSDLWindowProperties` 的访问控制问题——这是当前的构建 blocker。
2. 明确 `InteractionSystem` 与 `StateSystem` 的边界，避免再次出现跨类静态函数调用。

### 优先级二：ThemeSystem 状态澄清

1. 确认 `ThemeSystem` 是否真正激活。
2. 如果激活，补覆盖测试；如果是占位，明确标注 WIP 并隔离。

### 优先级三：HitTestSystem 测试

1. 构造最小测试场景验证命中缓存失效条件：布局变化、可见性变化、Z 顺序变化。
2. 确认文档化的失效规则与实际代码行为一致。

### 优先级四：IsEntityExist 性能设计

1. 评估是否需要在 `Registry` 上下文中维护 `alias → entity` 的 `unordered_map`。
2. 如果 `FindEntityByAlias` 是高频路径，当前 O(N) 扫描需要替换。

### 优先级五（持续）：InteractionSystem 职责拆分

继续第二阶段路线图中已规划的 InteractionSystem 拆分：
SDL 泵、文本编辑、平台窗口桥、键盘重复分别独立为明确职责模块。

---

## 四、不要动的范围（仍然有效）

以下内容第二阶段仍然不建议主动触碰：

1. `src/ui/singleton/Registry.hpp` / `Dispatcher.hpp` — 单例架构改造属于第三阶段。
2. `src/ui/renderers/` 下的所有渲染器实现 — 这一层当前分层合理。
3. 外部 DSL（`Chains.hpp` 和 `actions/` 命名空间）— 用户侧 API 稳定性比内部整洁更重要。
4. `test_MainWindow.cpp` 中的集成测试 — 这是唯一一个端到端的 smoke test，不要破坏它。
