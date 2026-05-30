# VMP-ui 架构再评估与优化路线（2026-05-30）

> 日期：2026-05-30  
> 输入来源：`docs/ARCHITECTURE.md`(v0.4)、`docs/ARCHITECTURE_REVIEW_2026-05.md`(rev8)、`docs/UI_FRAMEWORK_GAP_ANALYSIS_2026-05.md` 与 `src/` 全量源码核对。  
> 作用范围：UI 静态库 `src/`（不含 example / tests / third_party）。  
> 方法：本文所有结论以**真实源码 + 行号**为准，纠正历史文档中与代码不一致的「已完成」标记。

> 执行进展（同日）：**OP-K1 已落地**。`src/core/Batcher.hpp` 已在确认全工程零引用后删除；后续路线以此为基线继续推进。
> 执行进展（同日）：**OP-E 已进入下一阶段**。`RenderSystem` 已把注入的 `Registry&` 传给全部 renderer；经构建验证与全文检索，`src/renderers/` 中 `Registry::` 已清零，renderers 层的 legacy 入口清理完成。
> 执行进展（同日）：`src/systems/render/` 中 `Registry::` 已清零；`InteractionSystem` 与 `PlatformWindowSystem` 也已切离静态 `Registry::/Dispatcher::` 门面。`RenderFrame.cpp` 的文件级语义检查通过，但整库构建在该 TU 上遭遇 `clang-cl` OOM，故当前对该文件采用“语义检查 + 邻域 grep”作为验证基线。
> 执行进展（同日）：`src/api/Factory.cpp` 中 `Registry::/Dispatcher::` 已清零，工厂热点已切到 `RuntimeFacade::current().registry()/dispatcher()/enttRegistry()` 路径。
> 执行进展（同日）：`TweenSystem` 已切离 `Registry::current()/Dispatcher::current()` fallback，改为仅走注入的 `Registry&/Dispatcher&` 订阅路径；文件级语义检查通过。`ActionSystem` 也已移除源码中的 `Registry::current()/Dispatcher::current()/Dispatcher::Trigger` 文本残留，但当前文件级诊断仍报告一组疑似陈旧的 deprecated warning，需以后续 TU/整库验证再最终确认。

---

## 0. 文档整理建议（只建议，不删除）

当前 `docs/` 下评审/规划类文档已出现重叠与时间漂移，建议如下治理：

| 文档 | 现状判断 | 建议 |
|------|----------|------|
| `ARCHITECTURE.md` | 主线文档，但 §3.3 仍把 `ThemeSystem` 标「存根，待实现」，与代码严重不符（已 ~1157 行实现）。§11 测试矩阵只列 3 个文件，实际 15 个。 | **更新而非新建**：修正 ThemeSystem 状态、补全测试矩阵、补 `RuntimeFacade`/PIMPL 现状。保持「单一主线」定位。 |
| `ARCHITECTURE_REVIEW_2026-05.md` (rev8) | 增量 rev 累积，部分 TOP 行数、`RuntimeFacade::current()` 计数已过时（49 vs 真实 82）。 | **按日期滚动归档**：将其标记为「历史快照」，以本文（-05-30）为最新评审入口。后续评审继续按 `ARCHITECTURE_REVIEW_YYYY-MM-DD.md` 滚动，旧版只读。 |
| `UI_FRAMEWORK_GAP_ANALYSIS_2026-05.md` | 能力缺口盘点，与 REVIEW 是**正交**关系（GAP=功能广度，REVIEW=结构质量）。但「Clipboard 未规划」已与代码不符（`TextEditingService` 已实现复制/剪切/粘贴/全选）。 | **保留并小修**：GAP 负责「还缺什么功能」，REVIEW 负责「现有结构怎么改」。修正 Clipboard、ThemeSystem 两处过时判断。 |
| `THEME_STYLE_SYSTEM_PLAN.md` / `RICH_TEXT_PLAN.md` / `SVG_SUPPORT_PLAN.md` / `HIDPI_SUPPORT_PLAN.md` / `CPU_RENDER_ISSUES.md` | 单主题 PLAN 文档。THEME 计划已部分落地（ThemeSystem + `api/Theme.cpp` + 测试）。 | **PLAN 与 GAP 建立引用关系**：GAP 表中每个 P0/P1 项链接到对应 PLAN，PLAN 顶部标注「实现进度」。THEME_STYLE_SYSTEM_PLAN 应从「规划」改为「部分落地 / 进行中」。 |

**核心建议**：确立三层文档职责 —— `ARCHITECTURE.md`（事实主线，常更）→ `ARCHITECTURE_REVIEW_<date>.md`（按日期滚动的结构评审快照）→ `*_PLAN.md`/`GAP`（功能路线）。避免把「结构债务」和「功能缺口」混在同一张表里反复抄写。

---

## 1. 声明核对表（7 条关键声明 × 真实代码）

| # | 声明（来源文档） | 判定 | 证据 |
|---|------------------|------|------|
| 1 | OP-A：`src/common/Components.hpp` 伞头已删除 | ✅ **属实** | `file_search` 全工程无 `Components.hpp`；子头按域 include。遗留：`tests/unittest/test_UmbrellaHeader.cpp` 仍存在（应清理或改名）。 |
| 2 | OP-A2：`ThemeSystem.hpp` 已瘦身（声称 29 行），实现移到 `.cpp` | ✅ **属实**（行数略有出入） | [src/systems/ThemeSystem.hpp](../src/systems/ThemeSystem.hpp#L1) 实为 **~37 行**纯声明；[src/systems/ThemeSystem.cpp](../src/systems/ThemeSystem.cpp#L1) **~1157 行**承载全部实现（匿名命名空间辅助函数）。 |
| 3 | OP-B：`RenderSystem.hpp` 已 PIMPL，9 个 manager 移到 `RenderSystemImpl.hpp` | ✅ **属实** | [src/systems/RenderSystem.hpp](../src/systems/RenderSystem.hpp#L131-L137) 只剩 `Dispatcher* m_disp` + `std::unique_ptr<RenderSystemImpl> m_impl`；9 个 manager unique_ptr 全在 [src/systems/render/RenderSystemImpl.hpp](../src/systems/render/RenderSystemImpl.hpp#L57-L65)。 |
| 4 | OP-F：include 路径已根化，无 `"../xxx/Yyy.hpp"` 残留 | ✅ **属实** | `#include "../` 在 `src/` 内 **0 命中**（仅 third_party/entt 自身使用相对 include）。 |
| 5 | OP-C：接口瘦身（文件名 `ISystem.hpp`、删 CRTP、`getPhase` 编译期常量） | ⚠️ **部分** | 文件名已是 [src/interface/ISystem.hpp](../src/interface/ISystem.hpp#L1) ✅；但 `EnableRegister<Derived>` CRTP **仍在**且 12 个系统全继承（见 [ISystem.hpp#L57](../src/interface/ISystem.hpp#L57)）；`getPhase()` **仍是 vtable** `poly_call<3>`（[ISystem.hpp#L46](../src/interface/ISystem.hpp#L46)），未改编译期常量；`pollInput()` 仍是全员 vtable。**仅完成改名一项。** |
| 6 | OP-D/OP-E：`RuntimeFacade::current()` 计数、legacy 门面残留、`[[deprecated]]` | ⚠️ **部分** | `RuntimeFacade::current()` 在 `src/` 真实 **82 处**（旧文档记 49，已过时）；legacy `Registry::*` 调用仍 **200+ 处**（封顶，遍布 [Factory.cpp](../src/api/Factory.cpp)、[LayoutSystem.cpp](../src/systems/LayoutSystem.cpp)、全部 `renderers/*.hpp`、[TextEditingService.hpp](../src/core/TextEditingService.hpp)）；`[[deprecated]]` 已加在 [Registry.hpp#L192-L334](../src/singleton/Registry.hpp#L192) 与 [Dispatcher.hpp#L91-L121](../src/singleton/Dispatcher.hpp#L91) ✅。**结论：deprecated 标记到位，但迁移远未收口。** |
| 7 | `ARCHITECTURE.md` 称 `ThemeSystem` 为「存根，待实现」 | ❌ **不符** | ThemeSystem 已重度实现：[ThemeSystem.cpp](../src/systems/ThemeSystem.cpp#L1) ~1157 行 + [src/api/Theme.cpp](../src/api/Theme.cpp#L1) + `tests/unittest/test_ThemeSystem.cpp`。已支持默认样式注入、交互态主题、调色板切换、`ThemedTag` 增量重应用。 |

---

## 2. 更新后的核心痛点排序（基于真实代码）

> 已剔除旧文档中「已完成」的 OP-A/A2/B/F；按真实「伤害值 × 残留规模」重排。

### 痛点 1 —— 双轨 API 名义收口、实际仍以 legacy 为主（最高优先）
- `[[deprecated]]` 已挂满 legacy 入口，但 **200+ 处 `Registry::*` 调用仍在产线路径**：`Factory.cpp`(~974 行) 几乎全程用 `Registry::Emplace/Get`，全部 `renderers/*.hpp` 的 `canHandle`/`collect` 用 `Registry::AnyOf/TryGet`，`TextEditingService.hpp`、`LayoutSystem.cpp`、`render/RenderFrame.cpp` 同样。
- 现状等于「一边贴 deprecated 警告、一边自己持续踩警告」。若开了 `-Werror` 将无法编译；若没开，则警告噪声淹没真实问题。
- **本质**：OP-E 的「标记」与「迁移」严重脱节，是当前最大的认知与维护负担。

### 痛点 2 —— 隐式全局依赖仍是 System 的事实数据源
- `RuntimeFacade::current()` 真实 82 处，承载 registry/dispatcher/state/frame/windowLookup/context 多种访问。
- `renderers/*.hpp` 完全绕过注入、直接 `Registry::` 静态门面，渲染器无法脱离线程本地单例做单测。
- OP-D 的注入骨架（`m_reg/m_disp`）只在部分 System 内部生效，**渲染器层和 Factory 层是注入盲区**。

### 痛点 3 —— core/ 层混入业务服务
- **OP-K1 已完成**：零引用死代码 `core/Batcher.hpp` 已删除，`core/` 与 `managers/` 的批处理双轨入口已消除。
- [core/TextEditingService.hpp](../src/core/TextEditingService.hpp)(~600 行) 是业务级文本编辑服务（含剪贴板/选区/导航），放在「只放骨架」的 core/ 违反分层；且全内联 + 全 legacy `Registry::`。
- **OP-K3 已完成**：零引用 TODO 存根 `core/DslPaser.hpp` 已删除，避免继续暴露一个拼写错误且无实现的假入口。后续若重启 DSL 方案，应以明确的设计文档和正式模块重新落地。

### 痛点 4 —— ISystem 双层抽象冗余（OP-C 未动实质）
- `entt::poly<ISystem>` 已类型擦除，却仍叠 `EnableRegister` CRTP；`getPhase()`/`pollInput()` 全员付 vtable，只有 `InteractionSystem` 真正重写 `pollInput`，违反 ISP。

### 痛点 5 —— 巨型文件 + 全内联重头，编译扇出大
- 多个文件突破千行（见 §3）；`TextRenderer.hpp`(~887)、`FontManager.hpp`(~948) 全内联在头文件，任何改动触发所有 include 方重编。`traits/MyPFR.hpp`(~723) 编译期反射库放在 traits/，本质是 third_party。

---

## 3. 更新后的 TOP 大文件清单（真实行数，2026-05-30 重测）

> 行数为按文件尾部实测的近似值（±2 行）。与旧 REVIEW 清单对比，多数文件**已增长**，旧清单已偏小。

| 真实行数 | 文件 | 类型 | 旧文档值 | 备注 |
|-----:|------|------|------|------|
| ~1316 | [systems/StateSystem.cpp](../src/systems/StateSystem.cpp) | .cpp | 1078 | 状态机持续膨胀，建议按 Pointer/Scrollbar/Slider/Window helper 拆 .cpp |
| ~1216 | [systems/LayoutSystem.cpp](../src/systems/LayoutSystem.cpp) | .cpp | 1050 | Yoga 桥接 + 居中回退逻辑混杂 |
| ~1157 | [systems/ThemeSystem.cpp](../src/systems/ThemeSystem.cpp) | .cpp | 29(头) | 实现已落地（非存根），体量偏大 |
| ~974 | [api/Factory.cpp](../src/api/Factory.cpp) | .cpp | 828 | 全程 legacy `Registry::`，迁移与拆分主战场 |
| ~948 | [managers/FontManager.hpp](../src/managers/FontManager.hpp) | .hpp | 809 | 全内联，光栅化/混合逻辑应下沉 .cpp |
| ~887 | [renderers/TextRenderer.hpp](../src/renderers/TextRenderer.hpp) | .hpp | 754 | 全内联渲染器，编译扇出大 |
| ~723 | [traits/MyPFR.hpp](../src/traits/MyPFR.hpp) | .hpp | 671 | 编译期反射库，应隔离为 third_party |
| ~602 | [managers/IconManager.cpp](../src/managers/IconManager.cpp) | .cpp | 602 | 持平 |
| ~600 | [core/TextEditingService.hpp](../src/core/TextEditingService.hpp) | .hpp | 459 | core 层业务服务 + 全内联，已增长 |

---

## 4. 更新后的优化路线（OP-x，区分真实状态）

### 4.1 已真实完成（验证通过，建议从滚动 backlog 摘除）
- **OP-A** 伞头删除 ✅（遗留：清理 `test_UmbrellaHeader.cpp`）
- **OP-A2** ThemeSystem 头/实现分离 ✅
- **OP-B** RenderSystem PIMPL ✅
- **OP-F** include 根化 ✅
- **OP-C 子项**：`Isystem.hpp` → `ISystem.hpp` 改名 ✅
- **OP-K1** 删除死代码 `core/Batcher.hpp` ✅（已确认全工程零引用后落地）
- **OP-K3** 删除零引用 TODO 存根 `core/DslPaser.hpp` ✅

### 4.2 进行中（标记到位但未收口）
- **OP-E** 双轨 API 收口：`[[deprecated]]` 已挂，但 200+ legacy 调用未迁，**完成度低**。
- **OP-D** System 显式依赖注入：`m_reg/m_disp` 注入通路存在，部分 System 已切；**渲染器层 / Factory 层未覆盖**。
- **OP-I**（错误码统一 `FontErrc/IconErrc`→`ui_errc`）：未在本轮核对范围，按历史状态视为进行中。

### 4.3 未开始 / 实质未动
- **OP-C 主体**：删 `EnableRegister` CRTP、`getPhase()` 编译期常量化、`pollInput()` 拆 `IInputPoller` —— 全未动。
- **OP-G** 渲染流水线类型化（FrameGraph）。
- **OP-H** Chain DSL 节点类型擦除。
- **OP-J** View 层可测试化。
- **新增 OP-K**：迁移 `TextEditingService.hpp` 出 core/。

### 4.4 ROI × 风险 优先级表

| ID | 动作 | ROI | 风险 | 工时档 | 建议次序 |
|----|------|-----|------|--------|---------|
| **OP-E** | legacy `Registry::*` 批量迁移到注入/Facade（先 renderers，再 Factory） | 高 | 中（量大但机械） | L | **立即起步** |
| **OP-C** | ISystem 去 CRTP + `getPhase` 编译期常量 | 中高 | 中（需验证 `entt::poly` + `if constexpr requires`） | M | 高 |
| **OP-K2** | `TextEditingService.hpp` 移出 core/ 并下沉 .cpp | 中 | 低 | S | 高 |
| **OP-D** | System/渲染器依赖注入收口 | 中 | 中 | L | 中（与 OP-E 并行） |
| **大文件拆分** | StateSystem/LayoutSystem/Factory 按域拆 .cpp | 中 | 中 | L | 中 |
| OP-H | Chain DSL 类型擦除 | 中（编译时间） | 低 | S | 中 |
| OP-I | 错误码统一 | 中 | 低 | M | 中 |
| OP-G | FrameGraph 渲染重构 | 高（长期） | 高 | XL | 后 |
| OP-J | View 可测试化 | 中 | 中 | M | 后 |

---

## 5. 风险提示与验证基线

**风险**
- **OP-E 是最大体量项**：200+ legacy 调用分布在产线渲染/工厂路径，迁移需分批 + 每批回归，切忌一次性大改。建议按目录切片：`renderers/` → `api/` → `core/` → `systems/`。
- **OP-C 编译期 phase 化**：需先验证 `entt::poly` 类型擦除后能否经 `if constexpr (requires{ T::PHASE; })` 读静态成员；不行则退化为注册期手工表。
- **大文件拆分**：StateSystem/LayoutSystem 内部 helper 命名空间耦合紧，拆分需保持事件订阅签名不变。

**验证基线（每个改动 PR 必须全绿）**
```bash
# 配置
cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Debug -DENABLE_BUILD_TESTS=ON
# 构建
cmake --build build --config Debug
# 单元测试（当前 15 个测试文件）
ctest --test-dir build -V
# 可选：静态检查（clang-cl 下注意 OOM）
cmake -B build -DENABLE_CLANG_TIDY=ON
```

**当前测试覆盖快照（`tests/unittest/`，15 个文件）**

| 已覆盖 | 明显缺口 |
|--------|----------|
| BatchManager、Result、TaskChain、TweenSystem、UiRuntime、ThemeSystem、DragDrop、Hierarchy、MainWindow、ResourceProvider、TextUtils、Utils、Visibility、UmbrellaHeader(待清理) | **LayoutSystem / RenderSystem / HitTestSystem / StateSystem / InteractionSystem 无直接单测**；渲染器层因强耦合 `Registry::` 静态门面而难以单测（与 OP-D/E 相互绑定）。 |

---

## 6. 待确认问题

1. 构建是否开启 `-Werror`？若开启，§2 痛点 1 的 200+ legacy 调用应已无法编译——需确认 legacy 警告是否被局部 `#pragma` 抑制，这决定 OP-E 是「阻断级」还是「债务级」。
2. ThemeSystem 既已实现，`ARCHITECTURE.md` §3.3 是否同步修正（本文只评审，不改主线文档）？
3. 大文件拆分（StateSystem 等）是否纳入本季度，还是仅作为债务登记？
