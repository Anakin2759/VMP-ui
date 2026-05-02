# 修改规划：ui-doc-cleanup-optimize（来源：仓库根目录用户需求）

- 生成日期：2026-04-27
- 输入来源：仓库根目录用户需求“删除无关文档，优化 ui 模块”
- 作用范围：1）docs/ 下与当前项目目标无关或冗余的文档；2）仓库根目录下无关说明文件；3）src/ui/docs/ 内无关或过时文档；4）src/ui/ 模块内部用于结构整理/去冗余的代码热点

## 影响摘要

- 改动类型：文档清理 + 局部重构（UI 模块内）
- 影响范围：4 个区域（docs/pm、仓库根目录说明文件、src/ui/docs/、src/ui/ 内部结构热点），建议拆为 8 个可独立派发批次
- 是否需要数据迁移：否
- 是否涉及跨模块破坏性接口：原则上否；仅 ui 模块公共聚合头和静态入口若收口过快，存在轻微兼容风险，需通过兼容层控制

## 文档清单

### 建议删除

| 路径 | 理由 | 依赖/影响 |
|---|---|---|
| docs/pm/run-20260427-1312-clean-docs-and-ui-optimize.md | 属于临时协调记录，不是项目长期文档；内容已被本规划和后续实现记录覆盖 | 仅影响过程留痕，不影响构建和代码 |
| docs/pm/run-20260427-1415-ui-doc-cleanup-and-optimize.md | 与上条同类，且信息重复于本轮正式规划产物 | 仅影响过程留痕，不影响构建和代码 |
| test_embed.txt | 根目录孤立测试文件，非项目说明文档，也不属于 UI 模块资产 | 不参与构建；删除可降低仓库噪音 |

### 建议保留

| 路径 | 保留理由 | 后续动作 |
|---|---|---|
| README.md | 仓库根唯一项目入口说明，尽管当前内容编码异常，但职责仍然成立 | 不纳入本轮删除；若后续确认内容损坏，单独开“README 修复”工单 |
| src/ui/docs/README.md | UI 文档索引，当前 4 份主题文档均从此处集中入口 | 在 UI 重构完成后同步校正引用说明 |
| src/ui/docs/ARCHITECTURE_REVIEW.md | 直接描述当前 UI 结构边界，与本轮“结构整理/去冗余”强相关 | 在 Runtime/StateSystem 重构后同步更新边界表述 |
| src/ui/docs/DPI_AND_COORDINATES.md | 与当前 UI 主线目标直接相关，且未见重复来源 | 保留 |
| src/ui/docs/RENDERING_PIPELINE.md | 渲染职责划分清晰，仍可作为 RenderSystem 收敛依据 | 在渲染辅助逻辑抽取后同步更新“核心角色” |
| src/ui/docs/EXTENSION_AND_ROADMAP.md | 虽含路线图语句，但仍是新增控件/系统的最低约束文档，不属于无关文档 | 保留，并在批次 8 中同步剔除与新结构不一致的措辞 |

### 待用户确认

- 无

## 修改规划表

| # | 批次 | 模块/文件范围 | 当前状态 | 目标状态 | 改动类型 | 优先级 | 预估复杂度 | 依赖项 | 风险 | 备注 |
|---|---|---|---|---|---|---|---|---|---|---|
| 1 | 文档清理 A | docs/pm/run-20260427-1312-clean-docs-and-ui-optimize.md；docs/pm/run-20260427-1415-ui-doc-cleanup-and-optimize.md；test_embed.txt | 临时或孤立文件仍留在仓库根范围内 | 删除无关/冗余文件，仅保留正式规划与长期文档 | 删除 | P0 | S | 无 | 极低；仅过程留痕减少 | 可由代码工厂独立执行，不必等待 UI 代码批次 |
| 2 | Runtime 边界收口 | src/ui/core/UiRuntime.hpp；src/ui/core/RuntimeFacade.hpp；src/ui/core/RuntimeFacade.cpp；src/ui/singleton/Registry.hpp；src/ui/singleton/Dispatcher.hpp；src/ui/core/Application.hpp；src/ui/core/Application.cpp | UiRuntimeScope、RuntimeFacade、Registry/Dispatcher 静态入口并存，读写路径混杂 | 统一“受控入口”约定：新增/迁移代码优先走 RuntimeFacade；Registry/Dispatcher 仅保留兼容层与底层封装职责 | 局部重构 | P0 | M | 无 | 中；若一次性删掉旧入口会伤及大量调用点 | 这是允许重构的核心理由：同一运行时边界已有双轨实现，继续并存会放大后续整理成本 |
| 3 | 帧驱动链路收口 | src/ui/core/Application.cpp；src/ui/core/Application.hpp；src/ui/core/TaskChain.hpp；src/ui/common/Events.hpp；src/ui/systems/InteractionSystem.hpp | TaskChain 仅被 Application 主循环单点消费，但事件语义注释和实现分散在多处 | 将“常规帧阶段”定义收口到 Application/FramePipeline 一侧；TaskChain 若保留则下沉为私有实现，若不保留则由 Application 显式驱动阶段调用 | 局部重构 | P1 | M | #2 | 中；事件时序改动若无回归测试容易引入输入/渲染顺序问题 | 触发理由成立：单点调用却跨多文件扩散语义，属于真实重复和边界不清 |
| 4 | StateSystem 拆分 | src/ui/systems/StateSystem.hpp；src/ui/common/Events.hpp；src/ui/common/GlobalContext.hpp；src/ui/systems/HitTestSystem.hpp | StateSystem 头文件同时承担焦点、悬停、滚动条拖拽、窗口生命周期、滑块拖拽等职责，单文件过大 | 以“状态写入/滚动行为/窗口状态同步/滑块拖拽”拆成若干私有 helper 或 detail 文件，保留 StateSystem 作为唯一注册入口 | 局部重构 | P0 | L | #2 | 中；拆分时若直接改变外部事件契约会扩大影响面 | 本轮 UI 优化的首要热点，应优先做“结构去冗余”，不是算法改写 |
| 5 | 空间/滚动辅助去冗余 | src/ui/systems/StateSystem.hpp；src/ui/systems/RenderSystem.cpp；src/ui/systems/HitTestSystem.hpp；src/ui/api/Utils.hpp；src/ui/api/Utils.cpp；src/ui/common/Components.hpp | 滚动区域 padding、绝对位置、视口/内容边界计算在状态、命中、渲染侧重复出现 | 抽出共享辅助函数或私有 utility，保证“滚动容器几何语义”单点定义 | 局部重构 | P1 | M | #4 | 中；抽取过度会把简单逻辑再次抽象过头 | 仅抽真实重复的几何/滚动计算，不引入新框架或新层 |
| 6 | LayoutSystem 实现整理 | src/ui/systems/LayoutSystem.hpp；src/ui/CMakeLists.txt | LayoutSystem 目前在头文件中承载大量资源管理、节点缓存、递归同步细节，公共编译面偏大 | 将非模板实现迁移到 .cpp，头文件仅保留接口和必要内联；同步更新 ui 模块构建清单 | 局部重构 | P1 | M | #2 | 低到中；主要是编译边界调整 | 这是典型“结构整理”，不涉及布局算法替换 |
| 7 | 公共入口头精简 | src/ui/ui/ui.hpp；src/ui/CMakeLists.txt；src/ui/common/Components.hpp；src/ui/common/Tags.hpp；src/ui/common/Policies.hpp | ui/ui.hpp 聚合过重，且仓库内未发现实际 include 调用；公共头组合关系偏粗 | 将 ui/ui.hpp 降为兼容入口或最小聚合入口；避免默认暴露全部重量级头；必要时补过渡注释 | 接口扩展（向后兼容） | P2 | M | #2 | 中；外部潜在消费者可能依赖现有全量 include 行为 | 由于仓库内无直接调用，适合在 UI 模块内先做兼容式收口 |
| 8 | 文档同步 B | src/ui/docs/README.md；src/ui/docs/ARCHITECTURE_REVIEW.md；src/ui/docs/RENDERING_PIPELINE.md；src/ui/docs/EXTENSION_AND_ROADMAP.md；docs/architecture/change-ui-doc-cleanup-optimize-20260427.md | 代码结构整理后，现有 UI 文档会出现“旧边界/旧术语/旧入口”残留 | 以实现结果为准同步更新 UI 文档，不保留过时草案措辞 | 修改 | P1 | S | #2、#3、#4、#5、#6、#7 | 低；主要是文档与实现不一致风险 | 已同步 RuntimeFacade 受控入口、Application 单点消费 TaskChain、StateSystem 当前职责边界、LayoutSystem 实现收口到 `.cpp` |

## 分批派发建议

### 第一批：可立即独立派发

| 批次 | 显式文件范围 | 交付要求 |
|---|---|---|
| A1 | docs/pm/run-20260427-1312-clean-docs-and-ui-optimize.md；docs/pm/run-20260427-1415-ui-doc-cleanup-and-optimize.md；test_embed.txt | 仅删除文件，不触碰代码 |
| A2 | src/ui/core/UiRuntime.hpp；src/ui/core/RuntimeFacade.hpp；src/ui/core/RuntimeFacade.cpp；src/ui/singleton/Registry.hpp；src/ui/singleton/Dispatcher.hpp；src/ui/core/Application.hpp；src/ui/core/Application.cpp | 输出 Runtime 边界收口后的兼容策略说明 |

### 第二批：依赖 Runtime 约定后执行

| 批次 | 显式文件范围 | 交付要求 |
|---|---|---|
| B1 | src/ui/core/Application.cpp；src/ui/core/Application.hpp；src/ui/core/TaskChain.hpp；src/ui/common/Events.hpp；src/ui/systems/InteractionSystem.hpp | 明确常规帧时序，不改变对外行为 |
| B2 | src/ui/systems/StateSystem.hpp；src/ui/common/Events.hpp；src/ui/common/GlobalContext.hpp；src/ui/systems/HitTestSystem.hpp | 将 StateSystem 变成薄协调层，拆出私有细分实现 |

### 第三批：去冗余与编译面收缩

| 批次 | 显式文件范围 | 交付要求 |
|---|---|---|
| C1 | src/ui/systems/StateSystem.hpp；src/ui/systems/RenderSystem.cpp；src/ui/systems/HitTestSystem.hpp；src/ui/api/Utils.hpp；src/ui/api/Utils.cpp；src/ui/common/Components.hpp | 只抽重复几何/滚动辅助，不顺手改业务语义 |
| C2 | src/ui/systems/LayoutSystem.hpp；src/ui/CMakeLists.txt | 将实现搬到 .cpp 后保持现有行为和注册方式 |
| C3 | src/ui/ui/ui.hpp；src/ui/CMakeLists.txt；src/ui/common/Components.hpp；src/ui/common/Tags.hpp；src/ui/common/Policies.hpp | 以兼容优先方式收缩公共入口头 |

### 第四批：文档回写

| 批次 | 显式文件范围 | 交付要求 |
|---|---|---|
| D1 | src/ui/docs/README.md；src/ui/docs/ARCHITECTURE_REVIEW.md；src/ui/docs/RENDERING_PIPELINE.md；src/ui/docs/EXTENSION_AND_ROADMAP.md | 只保留与实现一致的边界说明，不保留历史措辞 |

## #8 文档同步 B 已回写的实现变化

1. Runtime 入口说明已统一为：`UiRuntime` 持有实例，`UiRuntimeScope` 负责作用域切换，`RuntimeFacade` 是新增和迁移代码的受控访问入口。
2. 常规帧路径说明已统一为：`Application` 主循环单点消费 `QueuedTask -> InputTask -> RenderTask`，渲染相关阶段为 `UpdateLayout -> UpdateRendering -> EndFrame`。
3. StateSystem 边界说明已改为当前事实：它仍是唯一注册入口和状态写入落点，但内部 helper 尚未完全下沉到独立实现文件。
4. LayoutSystem 说明已改为当前事实：头文件保留接口，非模板实现已迁移到 `.cpp`。

## 重构提议

- 重构点：RuntimeFacade 与 Registry/Dispatcher 双轨入口收口
  - 触发理由：已有真实重复，两套入口都在表达“当前活跃运行时”，且语义边界稳定
  - 不重构的代价：后续 StateSystem、LayoutSystem、TaskChain 整理时会继续引入混合调用，导致新增代码再次分叉
  - 推迟到何时可接受：不可再推迟到 StateSystem 拆分之后，否则拆分结果会被迫同时兼容两套写法

- 重构点：StateSystem 内部职责拆分
  - 触发理由：单文件承载多个真实职责，且已有重复几何/滚动逻辑
  - 不重构的代价：滚动、悬停、焦点、窗口生命周期问题将继续耦合，局部修改仍需通读千行文件
  - 推迟到何时可接受：最多推迟到 Runtime 边界收口完成后立即执行

- 重构点：LayoutSystem 与公共聚合头的编译面收缩
  - 触发理由：公共头暴露过重、头文件内实现偏多，属于稳定边界上的结构冗余
  - 不重构的代价：后续每次 UI 公共头修改都容易放大编译依赖和接口误用面
  - 推迟到何时可接受：可放到行为型重构完成后，但不建议再跨一个迭代周期

## 执行顺序建议

1. 先做 #1，因为它完全独立，且能立即清掉本轮无关文档噪音。
2. 再做 #2，因为后续所有 UI 结构整理都依赖统一运行时入口约定。
3. 然后做 #3 和 #4，因为帧驱动语义与 StateSystem 拆分共同决定 UI 行为边界。
4. 在行为边界稳定后做 #5 和 #6，集中处理重复计算与头文件实现冗余。
5. 最后做 #7 和 #8，以兼容优先方式收缩公共入口并把文档同步到最终实现。

## 待确认问题

- 无