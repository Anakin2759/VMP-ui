# VMP-ui 架构锐评与优化路线（2026-05）

> 基于 `src/` 全量扫描：139 个源文件 / ≈14,000 行 / 49 处 `RuntimeFacade::current()`。  
> 评估对象：UI 静态库 `src/`（不含 example / tests / third_party）。  
> **最后更新：2026-05-29 rev8** — StateSystem 的窗口/下拉框销毁路径已切到 `m_reg/m_disp`；Factory.cpp 已开始按告警热点分段迁移到 `RuntimeFacade::current().enttRegistry()/enttDispatcher()`。

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

### 5. ~~`ThemeSystem.hpp` 从「存根」悄悄膨胀为 1178 行全内联头文件~~ ✅ 已完成（OP-A2）

- [ThemeSystem.hpp](../src/systems/ThemeSystem.hpp) 已瘦身至 **29 行**（纯类声明），[ThemeSystem.cpp](../src/systems/ThemeSystem.cpp) 承载全部 50+ 辅助函数（匿名命名空间）。
- `SystemManager.cpp` 增量重编触发范围恢复正常，主题逻辑改动只重编 ThemeSystem.cpp。
- 旧「全内联」现象已消除，OP-A2 兑现。

### 6. ~~`Components.hpp` 伞头未废止~~ ✅ 已完成

- `src/common/Components.hpp` 聚合入口**已删除**，子头按域直接 include，OP-A 已兑现。
- `StateSystem.cpp`(1078 行) / `LayoutSystem.cpp`(1050 行) 仍然是大文件，但独立域 include 已生效。

---

## 二、次要硬伤

| 问题 | 位置 | 后果 |
|------|------|------|
| `"../singleton/..."` 相对路径 include | 全工程 | 文件移动即全员改 include |
| `Chain<F>` 每次 `\|` 生成新模板实例 | [Chains.hpp:64-76](../src/api/Chains.hpp#L64-L76) | 链长 O(n²) 编译时间 |
| `FontManager / IconManager` 未迁 `ui_errc` | WP-A3 | 错误码二元化（`FontErrc/IconErrc` + `ui_errc`） |
| `FallbackBackendRenderer` 直接耦合 SDL3 Surface | renderers/ | 无法单元测试 |
| `pch.hpp` 与组件头内容重叠未知 | src/pch.hpp | 可能 PCH 加速失效 |
| View 层 `inline void Create*()` 自由函数 | client/View | 无法 DI / mock |
| ~~`Isystem.hpp` 文件名大小写不符 PascalCase 约定~~ ✅ | ~~interface/~~ | 已重命名为 `ISystem.hpp` |
| [traits/MyPFR.hpp](../src/traits/MyPFR.hpp) 671 行编译期反射库放在 traits/ 内 | ⚠️ NEW | 应作为 third_party 隔离，全量修改拖慢所有使用者 |
| [core/TextEditingService.hpp](../src/core/TextEditingService.hpp) 459 行 | ⚠️ NEW | core 层混入业务级服务，与「core 只放骨架」设计原则冲突 |
| [core/Batcher.hpp](../src/core/Batcher.hpp)(352行) 与 [managers/BatchManager.hpp](../src/managers/BatchManager.hpp)(347行) 并存 | ⚠️ NEW | 职责重叠待对齐，新人无从判断应用哪个 |

---

## 三、TOP 大文件清单（编译时间嫌疑）

> ⚠️ 2026-05-29 重测，排名已变化。

| 行数 | 文件 | 备注 |
|-----:|------|------|
| ~~1178~~ **29** | [systems/ThemeSystem.hpp](../src/systems/ThemeSystem.hpp) | ✅ OP-A2 完成，实现移至 ThemeSystem.cpp |
| 1078 | [systems/StateSystem.cpp](../src/systems/StateSystem.cpp) | — |
| 1050 | [systems/LayoutSystem.cpp](../src/systems/LayoutSystem.cpp) | — |
|  828 | [api/Factory.cpp](../src/api/Factory.cpp) | — |
|  809 | [managers/FontManager.hpp](../src/managers/FontManager.hpp) | 头文件偏大 |
|  754 | [renderers/TextRenderer.hpp](../src/renderers/TextRenderer.hpp) | 全内联渲染器 |
|  671 | [traits/MyPFR.hpp](../src/traits/MyPFR.hpp) | ⚠️ 应隔离为 third_party |
|  602 | [managers/IconManager.cpp](../src/managers/IconManager.cpp) | — |
|  459 | [core/TextEditingService.hpp](../src/core/TextEditingService.hpp) | ⚠️ core 层业务服务 |

---

## 四、优化路线（按 ROI 排序）

### Phase 1 — 高 ROI / 低风险

#### OP-A：废止 `Components.hpp` 伞头 ✅ 已完成
- `src/common/Components.hpp` 已删除，子头按域直接引用。
- 增量编译收益已兑现。

#### OP-A2：ThemeSystem 头/实现分离 ✅ 已完成
- `ThemeSystem.hpp` 已缩至 **29 行**（纯类声明）。
- 所有 50+ 个辅助函数移入 `ThemeSystem.cpp` 的匿名命名空间。
- SystemManager.cpp 不再因主题逻辑改动而全量重编。

#### OP-B：RenderSystem 头文件 PIMPL 化 ✅ 已完成
- `RenderSystem.hpp` 仅暴露 `class RenderSystem` + 事件回调签名 + `getStats()` + `std::unique_ptr<RenderSystemImpl> m_impl`。
- 9 个 manager unique_ptr 全部隔离到 `systems/render/RenderSystemImpl.hpp`（仅供 render/*.cpp 包含）。
- **已兑现**：上游 TU 不再传递包含 8 个 manager 头。

#### OP-C：ISystem 接口瘦身
- 删 `EnableRegister<Derived>` CRTP，让 `entt::poly<ISystem>` 直接调具体类。
- `pollInput()` 拆出独立接口 `IInputPoller`，仅 InteractionSystem 实现。
- `getPhase()` 改编译期 `static constexpr SystemPhase PHASE`，注册期 `if constexpr (requires{ T::PHASE; })` 读取。
- ~~重命名 `Isystem.hpp` → `ISystem.hpp`~~ ✅ 已完成。

#### OP-F：include 路径根化 ✅ 已完成
- `src/` 已作为 include 根（`target_include_directories` PRIVATE/PUBLIC）。
- 全 327 处 `"../xxx/Yyy.hpp"` / `"../../xxx/Yyy.hpp"` → `"xxx/Yyy.hpp"` 脚本替换完成，UTF-8 安全处理，双层嵌套目录（`systems/render/`、`common/components/`）自动正规化。

### Phase 2 — 中 ROI / 中风险

#### OP-D：System 显式依赖注入 ⚠️ 部分完成
1. `SystemManager(entt::registry&, entt::dispatcher&)` 已具备注入通路，多个 System 已保存 `m_reg/m_disp`。
2. `registerHandlersImpl/unregisterHandlersImpl` 的 `Dispatcher::Sink<E>()` 已基本迁到 `m_disp->sink<E>()`。
3. 但系统内部仍残留大量 `Registry::...` 旧门面调用，说明“显式依赖注入”尚未收口完成。
4. 已完成六个收口切片：`TimerSystem` 的 caret 闪烁遍历已改为 `m_reg->view(...)`；`HitTestSystem` 内部的 `Registry::...` / `Dispatcher::Enqueue` 已全部切到 `m_reg` / `m_disp`；`StateSystem` 的 `SliderStateHelpers` 已不再依赖 `Registry::...`；`PointerStateHelpers` 的悬停、焦点与点击投递路径已切到 `m_reg/m_disp`；`ScrollbarStateHelpers` 的滚动区域查找、滚轮、拖拽与命中判定已切到 `m_reg`；窗口关闭 / 下拉框外部点击 / 实体销毁路径也已切到 `m_reg/m_disp`。
5. `RuntimeFacade::current()` 目前仍承担 frame/context 等上下文访问；后续应继续把 registry/dispatcher 读取从 System 实现里剥离。
6. **当前结论**：测试初始化负担已下降，但“System 可完全脱离全局 Registry 单例”这一目标尚未达成。

#### OP-E：双轨 API 收口
- `Registry` 与 `Dispatcher` 的 Legacy PascalCase 入口已补 `[[deprecated]]` 标记，用编译告警阻止新增调用继续走旧门面。
- `Factory.cpp` 已按告警热点开始迁移到 `RuntimeFacade::current().enttRegistry()/enttDispatcher()`，当前已覆盖窗口、布局、标题栏、文本浏览器、复选框与下拉框弹层相关创建路径。
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
| OP-A | ~~废止伞头~~ | — | ✅ 已完成 | — |
| OP-A2 | ~~ThemeSystem 头/实现分离~~ | — | ✅ 已完成 | — |
| OP-B | ~~头文件 PIMPL~~ | — | ✅ 已完成 | — |
| OP-C | 接口瘦身 | 中 | 代码清晰度 ↑↑ | M |
| OP-F | include 规整 | 低（脚本可回滚） | 维护成本 ↓ | S |
| OP-D | 依赖注入收口 | 中 | 隐式全局依赖 ↓ | M |
| OP-E | API 收口 | 低（紧跟 OP-D） | 心智负担 ↓ | M |
| OP-G | 渲染重构 | 高 | 长期可维护性 ↑↑ | XL |
| OP-H | DSL 优化 | 低 | 编译时间 ↓ | S |
| OP-I | 错误码统一 | 低 | 一致性 ↑ | M |
| OP-J | View 重塑 | 中 | 可测试性 ↑ | M |

---

## 六、风险提示

- **OP-D 仍在进行中**。虽然构造注入骨架已存在，但只要 System 内还保留 `Registry::...` 调用，就仍然受线程本地单例约束。
- **OP-C** 编译期 phase 化要先确认 `entt::poly` 类型擦除后仍能通过 `if constexpr (requires{...})` 读取静态成员；不行就退化为注册期手动表。
- **OP-A** 伞头已删，无需操作。
- **OP-A2** ✅ 已完成。ThemeSystem 头 29 行，实现在 ThemeSystem.cpp 匿名命名空间，122 测试全绿。
- 任何涉及 `Registry::current()` 改造的 PR 都需要 `cmake --build d:/test/VMP-ui/build --config Debug` 全绿 + 单元测试基线通过。

---

## 七、参考索引

- `pch.hpp` 与组件头关系：[src/pch.hpp](../src/pch.hpp)
- 现有 Result / 错误码：[src/common/Result.hpp](../src/common/Result.hpp)、[src/common/ErrorCodes.hpp](../src/common/ErrorCodes.hpp)
- Runtime 生命周期：[src/core/UiRuntime.hpp](../src/core/UiRuntime.hpp)、[src/core/RuntimeFacade.hpp](../src/core/RuntimeFacade.hpp)
- System Phase 排序：`SystemManager::registerAllHandlers`（index-sort 实现）
- 已完成基线：repo memory OP-19~OP-26 + OP-15
