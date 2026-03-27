# UI 模块架构评审与第一阶段整改清单

日期：2026-03-24

## 目的

记录当前 UI 模块的架构评审结论，以及一套不推倒重来的“第一阶段止血改造”方案。

本文档的目标不是追求一次性重构，而是先把当前最危险的耦合点、隐式规则和副作用收口，让后续迭代有稳定落脚点。

## 总评

当前 UI 模块的方向并不差：

1. 对外 API 有明显的声明式倾向。
2. RenderSystem 内部的拆分比大部分模块健康。
3. ECS、Yoga、SDL3 GPU 的组合本身具备扩展空间。

但从架构实现看，模块当前更像“工程化很强的 UI Runtime 原型”，还不是“边界稳定、能长期扩展的 UI 框架”。

核心问题不在于功能缺失，而在于：

1. 依赖方向不稳。
2. 生命周期归属不清。
3. 系统行为依赖隐式时序。
4. 脏标记和刷新语义依赖人工约定。

一句话概括：复杂度没有被真正消掉，而是被转移到了全局状态、事件顺序和分散副作用里。

## 主要问题

### 1. 全局单例打穿了运行时边界

当前 Registry、Dispatcher 和 ctx 都是进程级单例入口，Application 只是向全局上下文中注入状态。

这会直接带来以下问题：

1. UI 上下文几乎不具备多实例能力。
2. 测试隔离困难，状态泄漏风险高。
3. 代码签名看起来干净，但真实依赖被藏在静态访问里。
4. 生命周期归属模糊，谁创建、谁清理、谁保证顺序都不够明确。

这类设计短期开发顺手，长期会显著抬高维护成本。

### 2. System 抽象较薄，真实模型依赖隐式时序

SystemManager 当前更像“系统注册器”，而不是具有显式 phase 的调度器。

实际运行顺序依赖：

1. Application 中的 handler 注册顺序。
2. TaskChain 每帧手动触发的事件顺序。
3. 各系统对 Dispatcher 的订阅关系。

问题在于：

1. 系统间依赖没有被显式声明。
2. 改一个系统的注册顺序就可能改变行为。
3. 很难做局部推理，排查 bug 时需要同时理解多个事件链条。

从工程角度看，这是一种“事件驱动外观下的时序脚本管线”。

### 3. InteractionSystem 已膨胀为 God Object

InteractionSystem 当前同时承担：

1. SDL 事件读取。
2. 输入事件翻译。
3. 文本输入编辑。
4. 键盘重复逻辑。
5. 平台窗口事件桥接。
6. 即时触发布局和渲染。

这已经超出了“交互系统”的职责范围。

更关键的是，它同时存在两条语义不同的路径：

1. 常规路径：Enqueue 后由每帧统一处理。
2. 特例路径：平台事件里直接 Trigger，立即刷新。

这种双轨事件语义很容易让窗口刷新、文本焦点和输入状态在边界条件下出现不一致。

### 4. Factory 越权，创建期副作用过重

Factory 当前不仅负责创建 ECS 实体，还直接跨入：

1. SDL_Window 创建。
2. 图形上下文初始化触发。
3. RootTag 的临时赋值与后置修正。

这带来两个直接问题：

1. 创建 API 不再是纯构造，而是“构造 + 平台资源分配 + 生命周期启动”。
2. 一旦中间步骤失败，容易出现半初始化对象。

尤其是 RootTag 默认发放、后续再摘除的做法，说明树结构语义并没有在创建阶段被稳定建模。

### 5. 脏标记模型分散，靠经验维持

当前 LayoutDirtyTag 和 RenderDirtyTag 的设置逻辑分散在 API 层和多个系统里。

结果是：

1. 改属性时需要靠作者自己判断该打什么脏标记。
2. 同类语义可能在不同文件里有不同处理方式。
3. 漏打脏标记会直接变成难排查的 UI 异常。

这不是优化模型，而是隐式约定扩散。

## 现阶段值得保留的部分

### 1. 链式 DSL API 方向是对的

基于 Chain 和 EntityAction 的外部配置方式让 UI 书写体验比较顺手，也便于复用样式组合。

这层 API 值得保留，不建议在第一阶段改造中动它。

### 2. RenderSystem 的内部拆层相对健康

设备管理、字体、图标、批次、命令缓冲和 renderer 列表已经有较清晰边界。

这说明项目并不是没有分层能力，而是核心 runtime 和状态流的边界没立住。

## 第一阶段目标

第一阶段不做大重构，只做止血。

目标是：

1. 收口脏标记入口。
2. 显式标识平台即时刷新通道。
3. 收紧 RootTag 语义。
4. 降低 Factory 的副作用扩散。
5. 不破坏现有对外 DSL 用法。

## 第一阶段执行原则

1. 不追求一步到位。
2. 不引入大规模 API 破坏。
3. 优先把“靠记忆维持”的规则改成“靠入口函数维持”。
4. 先做最小闭环，让后续拆分有稳定基础。

## 第一阶段改造清单

### 1. 统一脏标记入口

优先文件：

1. `src/ui/api/Utils.hpp`
2. `src/ui/api/Utils.cpp`

建议新增统一入口函数：

1. `MarkLayoutChanged`
2. `MarkVisualChanged`
3. `MarkLayoutAndVisualChanged`

要求：

1. 布局脏和渲染脏的传播逻辑统一写在 Utils 中。
2. 其他模块尽量不再直接 `EmplaceOrReplace<...DirtyTag>`。
3. 保留旧接口作为过渡层，避免一次性破坏太大。

完成标准：

1. 后续新增属性变更时，开发者只调用语义化函数。
2. 脏标记规则从“散点约定”收口为“集中约定”。

### 2. 替换 API 层散点脏标记

优先文件：

1. `src/ui/api/Controls.cpp`
2. `src/ui/api/Icon.cpp`
3. `src/ui/api/Visibility.cpp`
4. `src/ui/api/Hierarchy.cpp`

要求：

1. 控件 setter 不再直接打脏标签。
2. 同类属性修改统一走 Utils 的语义化入口。
3. 可见性变化要明确是否影响布局、渲染、命中缓存。
4. 层级结构变化要统一补布局脏和必要的缓存失效逻辑。

完成标准：

1. API 层不再继续扩散“手打脏标签”的习惯。
2. 同类变更行为一致，可被统一推理。

### 3. 收紧 RootTag 语义

优先文件：

1. `src/ui/api/Factory.cpp`
2. `src/ui/api/Hierarchy.cpp`

要求：

1. 普通 widget 默认不再携带 RootTag。
2. 只有真正的拓扑根实体，如 Window、Dialog，才持有 RootTag。
3. 现有依赖“先挂 RootTag，后面再摘”的路径要逐步收缩，不能继续扩散。

完成标准：

1. RootTag 回归真实语义，不再作为默认占位标记。
2. 树结构规则更加稳定，减少后置修正。

### 4. 控制 Factory 的副作用扩散

优先文件：

1. `src/ui/api/Factory.cpp`

要求：

1. 将 SDL 窗口创建和图形上下文事件触发收口到少数局部辅助函数。
2. 即使暂时不做两阶段构造，也要先把平台副作用集中。
3. 给窗口创建失败路径补最基本的保护，避免半初始化对象继续流入系统。

完成标准：

1. Factory 的职责仍然偏重，但副作用路径集中、可审查。
2. 后续再拆成“构造阶段 / 绑定阶段”时改动会小很多。

### 5. 标识平台即时刷新通道

优先文件：

1. `src/ui/systems/InteractionSystem.hpp`
2. `src/ui/core/TaskChain.hpp`
3. `src/ui/common/Events.hpp`

要求：

1. 将 `SDL_AddEventWatch` 这条路径明确标记为平台阻塞补救通道。
2. 把直接 `Trigger(UpdateLayout / UpdateRendering)` 的逻辑抽成局部辅助函数。
3. 在文档和注释层面明确区分：
   1. 正常帧路径
   2. 平台即时刷新路径
4. 对现有事件注释做现实对齐，避免注释与实际行为不一致。

完成标准：

1. 开发者肉眼能区分“常规事件流”和“平台特例流”。
2. 后续拆 InteractionSystem 时能清楚识别边界。

### 6. 审查命中测试缓存的失效条件

优先文件：

1. `src/ui/systems/HitTestSystem.hpp`

要求：

1. 明确命中缓存当前依赖哪些组件变化失效。
2. 检查布局变化、可见性变化是否足以触发命中缓存更新。
3. 如果当前规则不完整，至少先补注释与 TODO，避免行为继续隐式化。

完成标准：

1. 命中测试缓存与布局/可见性变化关系被文档化。
2. 不再完全依赖作者对缓存边界的口头记忆。

## 第一阶段建议执行顺序

建议按以下顺序落地：

1. `src/ui/api/Utils.hpp`
2. `src/ui/api/Utils.cpp`
3. `src/ui/api/Controls.cpp`
4. `src/ui/api/Icon.cpp`
5. `src/ui/api/Visibility.cpp`
6. `src/ui/api/Hierarchy.cpp`
7. `src/ui/api/Factory.cpp`
8. `src/ui/systems/StateSystem.hpp`
9. `src/ui/systems/InteractionSystem.hpp`
10. `src/ui/systems/HitTestSystem.hpp`
11. `src/ui/core/TaskChain.hpp`
12. `src/ui/common/Events.hpp`

## 第一阶段当前进展

截至 2026-03-25，第一阶段已经完成的部分明显多于文档初稿时的预期，但仍未彻底收尾。

### 已完成项

1. `src/ui/api/Utils.hpp` / `src/ui/api/Utils.cpp`
   - 已补齐 `MarkLayoutChanged`、`MarkVisualChanged`、`MarkLayoutAndVisualChanged`。
   - 旧接口仍保留为过渡层，语义入口已经集中。
2. `src/ui/api/Controls.cpp`
   - 滑块、进度条、滚动区域相关 setter 已改走语义化脏标记入口。
3. `src/ui/api/Icon.cpp`
   - 图标增删改已统一走 `MarkLayoutAndVisualChanged`。
4. `src/ui/api/Visibility.cpp`
   - 可见性、透明度、背景、边框相关变更已区分布局+视觉和纯视觉刷新。
5. `src/ui/api/Hierarchy.cpp`
   - 父子节点增删时已补齐统一的布局/视觉失效处理。
6. `src/ui/api/Factory.cpp`
   - `CreateTitleBar` 已拆分为数个局部辅助函数，复杂度热点已移除。
   - `WindowGraphicsContextSetEvent` 的日志和调用语义已与实际行为对齐。
7. `src/ui/systems/StateSystem.hpp`
   - 滚动、焦点、窗口尺寸变更等路径已改走语义化脏标记入口。
8. `src/ui/systems/InteractionSystem.hpp`
   - 已明确区分常规帧刷新与平台即时刷新补救通道。
   - SDL 轮询入口、平台窗口事件、文本编辑快捷键、删除、导航、回车处理已拆成具名 helper。
9. `src/ui/core/TaskChain.hpp`
   - 常规帧路径注释已与实际行为对齐。
   - 预存的主要风格告警已清理，当前文件已无诊断。
10. `src/ui/common/Events.hpp`
    - 事件注释已改为反映真实语义：常规帧触发、平台特例通道、输入事件队列派发。
11. `src/ui/systems/HitTestSystem.hpp`
   - 已补齐命中缓存的主要失效触发：Z 顺序、层级、可见性、交互资格、TextEdit 只读状态。
   - 已明确当前缓存只保存“可交互实体集合 + Z 顺序”，不缓存几何位置和尺寸。

### 部分完成项

1. `src/ui/api/Factory.cpp`
   - Factory 的副作用路径已经比初始状态更集中，窗口创建失败时也已补上最小回滚：SDL_Window 创建失败或窗口 ID 获取失败都会立即销毁实体并返回 `entt::null`。
   - 但它仍然没有进入“构造阶段 / 绑定阶段”分离，平台资源分配与生命周期启动依旧耦合在创建入口中。

### 当前结论

第一阶段已经完成了最关键的“止血动作”：

1. 脏标记入口已集中。
2. 平台即时刷新路径已显式化。
3. InteractionSystem 和 TaskChain 的隐式耦合已明显降低。
4. Factory 中最突出的局部复杂度热点已拆散。
5. RootTag 已回归拓扑语义：普通 widget 默认不再持有 RootTag，Window / Dialog 创建期显式设根，层级断开后子树重新成为根。
6. HitTest 缓存边界已显式化，缓存失效不再只靠少数零散 hook 维持。
7. Factory 的最小失败路径保护已落地，窗口创建不再留下半初始化实体。

因此，第一阶段的“止血目标”现在可以认为已经完成。
剩下的问题属于第二阶段的结构性改造，而不是第一阶段的缺口。

## 第一阶段完成标准

如果达到以下 5 条，可以认为第一阶段已经成功止血：

1. 除 `Utils.cpp` 外，业务代码和系统代码中几乎不再直接设置脏标签。
2. RootTag 只表示真实拓扑根，不再是默认占位语义。
3. 平台即时刷新路径已显式命名并有清晰注释。
4. Factory 的创建期副作用集中在少数函数中。
5. 命中测试缓存、可见性、布局脏之间的关系已经被说明清楚。

截至 2026-03-25，上述 1、2、3、4、5 已达到。

## 暂时不要动的范围

第一阶段不建议优先碰以下部分：

1. `src/ui/singleton/Registry.hpp`
2. `src/ui/singleton/Dispatcher.hpp`
3. `src/ui/core/SystemManager.cpp`
4. `src/ui/systems/RenderSystem.hpp`

原因：

1. 这些属于第二阶段及之后的问题。
2. 当前阶段的目标是止血，不是开胸。
3. 先动这些容易把风险从“可控整理”升级成“大范围运行时回归”。

## 第二阶段重构清单草案

第一阶段解决的是“失控的自由度”，第二阶段要解决的是“隐藏的依赖关系”。

第二阶段不建议再以“文件告警清理”或“局部 helper 拆分”为主，而应围绕 runtime 边界、phase 语义和系统职责来推进。

### 第二阶段目标

第二阶段的核心目标有 4 个：

1. 将输入、平台窗口、文本编辑、状态机的职责边界重新拉直。
2. 将当前依赖隐式事件顺序的运行模型，提升为显式 phase 模型。
3. 逐步把 UI 实例级状态从进程级静态入口中剥离出来。
4. 保持现有 DSL 和大部分控件 API 不破坏，避免重构范围外溢到业务层。

### 第二阶段建议拆分主题

#### 1. 拆 InteractionSystem 的多重职责

优先文件：

1. `src/ui/systems/InteractionSystem.hpp`
2. `src/ui/systems/StateSystem.hpp`
3. `src/ui/common/Events.hpp`

建议拆出 4 类子职责：

1. SDL 输入读取与原始事件转发。
2. 文本输入编辑控制。
3. 平台窗口事件桥接与即时补救刷新。
4. 键盘重复和输入节流控制。

要求：

1. `pollSdlEvents()` 保留为统一入口，但内部只负责分派，不再承载大块交互语义。
2. 文本输入逻辑应形成独立的 helper 组或独立子模块，避免继续和平台窗口事件共处一层。
3. 平台即时刷新路径必须继续保留显式命名，并与常规帧路径严格分开。
4. 键盘 repeat 逻辑不要继续以静态局部约定的方式扩散到更多输入处理代码里。

完成标准：

1. InteractionSystem 回到“输入接入层”的角色，而不是继续承担输入策略和 UI 行为实现。
2. 新增输入能力时，开发者能明确知道应改哪一层，而不是继续往单个头文件堆逻辑。

#### 2. 把 StateSystem 从“大状态桶”拆成几条清晰状态机

优先文件：

1. `src/ui/systems/StateSystem.hpp`
2. `src/ui/common/GlobalContext.hpp`
3. `src/ui/common/Events.hpp`

当前 StateSystem 至少同时承担：

1. Hover / Active / Focus 状态。
2. Scrollbar 拖拽状态。
3. Slider 拖拽状态。
4. 窗口 ECS ←→ SDL 同步。
5. 部分输入副作用处理。

建议按状态机主题拆组：

1. PointerState：hover / active / click / drag 手势。
2. FocusState：焦点、输入法、Caret 可见性。
3. ScrollInteractionState：滚动条命中、拖拽、滚轮滚动。
4. WindowSyncState：窗口尺寸、位置、标题、透明度、可见性同步。

要求：

1. 不要求一次拆成多个类，但至少先在文件内按主题形成稳定分区。
2. `StateContext` 中的字段要按主题归组，而不是继续平铺增长。
3. SDL window 同步逻辑应逐步从“状态系统的附属职责”提升为明确的窗口同步职责。

完成标准：

1. StateSystem 的新增逻辑能归属于某条已有状态机，而不是继续随意堆叠。
2. 未来真要拆文件时，不需要重新发明边界。

#### 3. 给运行时建立显式 phase 语义

优先文件：

1. `src/ui/core/TaskChain.hpp`
2. `src/ui/core/Application.cpp`
3. `src/ui/core/SystemManager.cpp`
4. `src/ui/common/Events.hpp`

当前真实 phase 已经存在，但主要靠 TaskChain 和事件注释维持。

建议显式定义的 phase 至少包括：

1. QueueUpdate：推进帧上下文、Timer、缓冲事件。
2. InputPump：SDL 输入轮询与原始输入入队。
3. LayoutPhase：布局刷新。
4. RenderPhase：渲染刷新。
5. EndFramePhase：批量应用延迟状态。

要求：

1. 不一定要立刻引入新的调度框架，但要把 phase 名字固定下来。
2. `SystemManager` 的角色描述需要与现实对齐，不再暗示自己负责调度顺序。
3. 文档、注释、事件语义要围绕 phase，而不是围绕“某个系统碰巧什么时候触发”。

完成标准：

1. 任意一个事件或系统行为，都能明确回答它属于哪个 phase。
2. 后续若要做多线程渲染或输入抽样优化，已经有可承载的语义骨架。

#### 4. 引入显式 UI runtime context

优先文件：

1. `src/ui/core/Application.hpp`
2. `src/ui/core/Application.cpp`
3. `src/ui/singleton/Registry.hpp`
4. `src/ui/singleton/Dispatcher.hpp`
5. `src/ui/common/GlobalContext.hpp`

这一项不建议一步到位推翻单例，而应采用渐进式收缩：

1. 先定义“哪些状态属于某个 UI 实例”。
2. 再定义这些状态如何由 Application 持有并向系统暴露。
3. 最后再考虑 Registry / Dispatcher 的实例化或包装层。

第一步建议先明确归属：

1. 帧上下文。
2. 输入与焦点状态。
3. 窗口集合与窗口生命周期。
4. 定时任务。
5. 渲染设备与缓存资源的所有权边界。

要求：

1. 第二阶段只做“显式归属建模”，不强求彻底消灭静态调用。
2. Application 需要逐渐从“初始化器”变成“runtime owner”。
3. 新增全局状态时，必须先判断它是 UI 实例级还是进程级。

完成标准：

1. 代码中能明显看出哪些状态属于某个 UI runtime。
2. 后续测试隔离、多实例运行、嵌入式 UI 场景才有实现基础。

#### 5. 收缩 Factory 与窗口生命周期的耦合

优先文件：

1. `src/ui/api/Factory.cpp`
2. `src/ui/core/PlatformWindow.cpp`
3. `src/ui/systems/RenderSystem.cpp`
4. `src/ui/common/Events.hpp`

第一阶段已经补上最小失败回滚；第二阶段应继续做“构造阶段 / 绑定阶段”拆分。

建议的职责划分：

1. 构造阶段：只创建 ECS 实体与默认组件。
2. 绑定阶段：创建 SDL_Window、初始化平台窗口定制、触发图形上下文事件。
3. 启动阶段：使窗口进入可见和可交互运行状态。

要求：

1. 构造 API 尽量保持纯度，不再默认混入全部平台副作用。
2. 失败路径必须能明确回答“当前回滚到哪一层”。
3. 渲染系统对窗口图形上下文的认领，继续通过清晰的生命周期事件驱动。

完成标准：

1. Window / Dialog 的创建过程可以分段调试和验证。
2. 后续支持无窗口预构造、延迟显示、测试替身窗口时不会再次把 Factory 撑爆。

### 第二阶段建议执行顺序

建议按以下顺序推进：

1. `src/ui/systems/InteractionSystem.hpp`
2. `src/ui/systems/StateSystem.hpp`
3. `src/ui/common/GlobalContext.hpp`
4. `src/ui/core/TaskChain.hpp`
5. `src/ui/common/Events.hpp`
6. `src/ui/core/Application.cpp`
7. `src/ui/core/Application.hpp`
8. `src/ui/api/Factory.cpp`
9. `src/ui/core/PlatformWindow.cpp`
10. `src/ui/singleton/Registry.hpp`
11. `src/ui/singleton/Dispatcher.hpp`

原因：

1. 先拆最重的运行时职责，再固化 phase 语义。
2. phase 稳定后，再去定义 runtime context 的归属。
3. Factory 和单例入口放在后面，避免在边界尚未定型时过早动核心底座。

### 第二阶段完成标准

如果达到以下 6 条，可以认为第二阶段完成：

1. InteractionSystem 不再同时承担文本编辑、平台桥接和输入策略实现。
2. StateSystem 的逻辑可以明确分配到少数几条状态机主题。
3. TaskChain / Events / 注释对 phase 的定义一致且稳定。
4. Application 已具备明显的 runtime owner 角色，而不只是初始化入口。
5. Window / Dialog 的创建与平台绑定可以分阶段推理。
6. 新增系统或状态时，开发者不必继续依赖隐式全局顺序做猜测。

### 第二阶段不建议同时做的事

为了避免范围外溢，第二阶段不建议和以下事项并行推进：

1. 重写 DSL 链式 API。
2. 重做 RenderSystem 内部渲染器架构。
3. 一次性把 Registry / Dispatcher 全部替换成实例对象。
4. 同时引入跨线程渲染调度改造。

原因：

1. 这些改动会把“运行时边界整理”变成“整套 UI 框架重构”。
2. 第二阶段的目标是把 runtime 模型立住，而不是同时重写所有实现层。

## 总结

当前 UI 模块最大的问题不是“技术栈不对”，也不是“功能不够”，而是核心规则没有被关进少数稳定入口。

所以第一阶段最值钱的动作不是再设计一套更高级的架构，而是先把当前架构里最危险的自由度关掉。

先收口，再拆分，最后才谈抽象升级。