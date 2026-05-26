# PmkUi 架构锐评与优化路线（2026-05）

> 基于 `src/` 全量扫描：137 个源文件 / 23,036 行 / 49 处 `RuntimeFacade::current()`。
> 评估对象：UI 静态库 `src/`（不含 example / tests / third_party）。

---

## 一、核心痛点（按"伤害值"排序）

### 1. 隐式全局依赖泛滥 — `RuntimeFacade::current()` 是被 Scope 包装的线程本地单例

- 现状：`UiRuntimeScope` 把 `Registry / Dispatcher / Mailbox` 注入 `RuntimeFacade` 的活跃槽，所有 System 通过 `RuntimeFacade::current().enttRegistry()` 拉数据。
- 量化：`src/` 内共 **49 处** `RuntimeFacade::current()` 直调；[TweenSystem.hpp](../src/systems/TweenSystem.hpp) 单文件 8 处。
- 后果：
  - 单元测试必须先 `UiRuntimeScope`，否则 `std::terminate`。
  - System 的真实依赖不在签名里，靠口传。
  - 多 runtime 并行（多窗口隔离 / 并发测试夹具）受限于 process-wide `activeInstance`。

### 2. 双轨 API 并存

- 旧门面：`Registry::Create() / Dispatcher::Sink<>()`（[Registry.hpp:53](../src/singleton/Registry.hpp#L53) 标注 `Legacy PascalCase entrypoints`）。
- 新门面：`RuntimeFacade::current().enttRegistry() / .enttDispatcher()`。
- 同一仓库混用：[RenderSystem.hpp:88-105](../src/systems/RenderSystem.hpp#L88-L105) 用旧；[TweenSystem.hpp:45](../src/systems/TweenSystem.hpp#L45) 用新。
- 代价：阅读时持续维护"这是不是已迁移"的心智负担。

### 3. RenderSystem 是 God Class，头文件未瘦身

- [RenderSystem.hpp](../src/systems/RenderSystem.hpp) 头部直接持有 **9 个 unique_ptr 成员**：`DeviceManager / FontManager / IconManager / ImageManager / PipelineCache / TextTextureCache / BatchManager / CommandBuffer / IBackendRenderer`。
- 实现已物理拆分到 [systems/render/RenderBackend.cpp](../src/systems/render/RenderBackend.cpp)、[RenderFrame.cpp](../src/systems/render/RenderFrame.cpp)，但头文件未拆。
- 后果：任何 manager 头改动 → 所有 include `RenderSystem.hpp` 的 TU 全量重编。

### 4. ECS System 接口双层抽象冗余

- `entt::poly<ISystem>` 已做类型擦除（[Isystem.hpp:42-52](../src/interface/Isystem.hpp#L42-L52)）；又叠 `EnableRegister<Derived>` CRTP 强制 `registerHandlersImpl` 命名。
- `pollInput()` 每个 System 付 vtable 调用，但**只有 InteractionSystem 重写**，违反 ISP。
- `getPhase()` 每次注册都走 vtable，本可编译期常量。
- 文件名 `Isystem.hpp` 大小写不符合项目 PascalCase 约定，应为 `ISystem.hpp`。

### 5. `Components.hpp` 伞头未废止 → 巨型 TU

- 物理拆分到 [src/common/components/](../src/common/components/) 6 个子头，但 [Components.hpp](../src/common/Components.hpp) 还是 6-include 伞头。
- 被 `StateSystem.cpp`(1063 行) / `LayoutSystem.cpp`(1054 行) / `Factory.cpp`(842 行) 等大文件 include。
- 改 `Window.hpp` 一个字段 → StateSystem.cpp 全文件重编。
- `repo memory OP-25` 标记"P2 待办" — 实际只完成物理拆分，逻辑收益没兑现。

---

## 二、次要硬伤

| 问题 | 位置 | 后果 |
|------|------|------|
| `"../singleton/..."` 相对路径 include | 全工程 | 文件移动即全员改 include |
| `Chain<F>` 每次 `\|` 生成新模板实例 | [Chains.hpp:64-76](../src/api/Chains.hpp#L64-L76) | 链长 O(n²) 编译时间 |
| `FontManager / IconManager` 未迁 `ui_errc` | WP-A3 | 错误码二元化（`FontErrc/IconErrc` + `ui_errc`） |
| `FallbackBackendRenderer` 直接耦合 SDL3 Surface | renderers/ | 无法单元测试 |
| `pch.hpp` 与 `Components.hpp` 内容重叠未知 | src/pch.hpp | 可能 PCH 加速失效 |
| View 层 `inline void Create*()` 自由函数 | client/View | 无法 DI / mock |

---

## 三、TOP 大文件清单（编译时间嫌疑）

| 行数 | 文件 |
|-----:|------|
| 1063 | [systems/StateSystem.cpp](../src/systems/StateSystem.cpp) |
| 1054 | [systems/LayoutSystem.cpp](../src/systems/LayoutSystem.cpp) |
|  842 | [api/Factory.cpp](../src/api/Factory.cpp) |
|  814 | [managers/FontManager.hpp](../src/managers/FontManager.hpp) |
|  754 | [renderers/TextRenderer.hpp](../src/renderers/TextRenderer.hpp) |
|  603 | [managers/IconManager.cpp](../src/managers/IconManager.cpp) |

---

## 四、优化路线（按 ROI 排序）

### Phase 1 — 高 ROI / 低风险

#### OP-A：废止 `Components.hpp` 伞头
- 将 `Components.hpp` 改为 `#pragma message` deprecation，过渡 1 个版本后删除。
- 系统按需 include `components/Visual.hpp` / `components/Window.hpp` 等子头。
- **预期**：StateSystem.cpp / LayoutSystem.cpp 增量编译 -40%。

#### OP-B：RenderSystem 头文件 PIMPL 化
- `RenderSystem.hpp` 仅暴露 `class RenderSystem` + 事件回调签名 + `getStats()`。
- 9 个 manager unique_ptr → `class RenderSystemImpl`（前置声明 + .cpp 定义）。
- **预期**：上游 TU 少包 8 个 manager 头。

#### OP-C：ISystem 接口瘦身
- 删 `EnableRegister<Derived>` CRTP，让 `entt::poly<ISystem>` 直接调具体类。
- `pollInput()` 拆出独立接口 `IInputPoller`，仅 InteractionSystem 实现。
- `getPhase()` 改编译期 `static constexpr SystemPhase PHASE`，注册期 `if constexpr (requires{ T::PHASE; })` 读取。
- 重命名 `Isystem.hpp` → `ISystem.hpp`。

#### OP-F：include 路径根化
- `target_include_directories(ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})` 已存在则免改。
- 全工程 `"../xxx/Yyy.hpp"` → `"xxx/Yyy.hpp"`，一次性脚本替换。

### Phase 2 — 中 ROI / 中风险

#### OP-D：System 显式依赖注入
1. 每个 System 加构造注入：`TweenSystem(entt::registry&, entt::dispatcher&)`。
2. `update()` 方法签名带 `const FrameContext&`，不再调 `tryFrame()`。
3. `SystemManager::addSystem` 时从当前 Runtime 拿引用注入。
4. `RuntimeFacade::current()` 仅保留给 API 层（Factory / Chains 等"用户便利门面"）。
5. **测试收益**：可直接 `TweenSystem tw{reg, disp}; tw.update(FrameContext{16});`，无 `UiRuntimeScope`。

#### OP-E：双轨 API 收口
- `Registry::Create / Emplace / View` 静态方法标 `[[deprecated]]`。
- `Dispatcher::Sink<>` 同样标 deprecated。
- 全部迁到 `RuntimeFacade::current().enttRegistry()` 或 OP-D 注入。
- 待无引用后删除 legacy 方法。

### Phase 3 — 架构演进

#### OP-G：渲染流水线类型化
- 引入 `struct FrameGraph { std::vector<RenderPass> passes; }`。
- RenderSystem 改为 `m_pipeline.execute(cmdBuf, batchMgr)`。
- manager 不再被 God class 持有，按 pass 注入。

#### OP-H：Chain DSL 节点类型擦除
- `Chain<F> operator|` 立即 `std::move_only_function<void(entt::entity)>`，避免模板实例爆炸。

#### OP-I：错误码统一（WP-A3 收口）
- `FontErrc / IconErrc` 并入 `ui_errc`，删 `UiErrors.hpp`。

#### OP-J：View 层可测试化
- `inline void CreateXxx()` → `struct XxxView { entt::entity build(entt::registry&); };`，自由函数仅保留为薄包装。

---

## 五、执行优先级建议

| ID | 类型 | 风险 | 预期收益 | 建议工时档位 |
|----|------|-----|---------|------------|
| OP-A | 机械重构 | 低 | 编译时间 ↓↓ | S |
| OP-B | 头文件 PIMPL | 低 | 编译时间 ↓ | S |
| OP-C | 接口瘦身 | 中 | 代码清晰度 ↑↑ | M |
| OP-F | include 规整 | 低（脚本可回滚） | 维护成本 ↓ | S |
| OP-D | 依赖注入 | 中-高 | 可测试性 ↑↑↑ | L（需 architect 出规划表） |
| OP-E | API 收口 | 低（紧跟 OP-D） | 心智负担 ↓ | M |
| OP-G | 渲染重构 | 高 | 长期可维护性 ↑↑ | XL |
| OP-H | DSL 优化 | 低 | 编译时间 ↓ | S |
| OP-I | 错误码统一 | 低 | 一致性 ↑ | M |
| OP-J | View 重塑 | 中 | 可测试性 ↑ | M |

---

## 六、风险提示

- **OP-D 是骨干手术**，会触动所有 System 的构造签名与 `SystemManager`。建议先经"架构师 Agent"产出修改规划表后再落地。
- **OP-C** 编译期 phase 化要先确认 `entt::poly` 类型擦除后仍能通过 `if constexpr (requires{...})` 读取静态成员；不行就退化为注册期手动表。
- **OP-A** 不要一次性删伞头，先 deprecate 一个迭代周期，观察 `example/` 与 `client/View/` 影响面。
- 任何涉及 `Registry::current()` 改造的 PR 都需要 `cmake --build d:/test/PMK/build --config Debug` 全绿 + 90/90 测试用例通过基线。

---

## 七、参考索引

- `pch.hpp` 与组件头关系：[src/pch.hpp](../src/pch.hpp)
- 现有 Result / 错误码：[src/common/Result.hpp](../src/common/Result.hpp)、[src/common/ErrorCodes.hpp](../src/common/ErrorCodes.hpp)
- Runtime 生命周期：[src/core/UiRuntime.hpp](../src/core/UiRuntime.hpp)、[src/core/RuntimeFacade.hpp](../src/core/RuntimeFacade.hpp)
- System Phase 排序：`SystemManager::registerAllHandlers`（index-sort 实现）
- 已完成基线：repo memory OP-19~OP-26 + OP-15
