# UI 模块架构锐评与测试覆盖记录

日期：2026-03-26

## 结论

UI 模块现在能跑，也已经长出了一套有表达力的 DSL 和可复用的系统拆分，但底层运行时的边界仍然不稳。它更像一个已经工程化的运行时原型，而不是一套约束清晰、可长期扩展的 UI 框架。

问题不在于“功能少”，而在于“功能是靠隐式规则粘起来的”。这会让模块在规模继续增长时，维护成本按组合爆炸，而不是线性增长。

一句话总结：

1. 对外 API 比内部 runtime 干净。
2. 渲染子系统比交互与调度层健康。
3. 当前最大的债不是性能，而是生命周期、依赖方向和事件语义不够显式。

## 架构锐评

### 1. 单例把运行时边界打穿了

核心入口 Registry 和 Dispatcher 都是进程级全局单例，FrameContext 之类的运行态状态也塞在全局上下文里。

相关文件：

1. src/ui/singleton/Registry.hpp
2. src/ui/singleton/Dispatcher.hpp
3. src/ui/common/GlobalContext.hpp

这类设计的直接后果是：

1. UI 上下文天然不支持多实例。
2. 测试天然需要自己清场，否则状态和订阅就会串味。
3. 函数签名看起来很纯，真实依赖却被静态入口隐藏掉了。

这不是抽象，而是把依赖挪到了看不见的地方。

### 2. TaskChain 是固定脚本，不是真正可推理的调度器

TaskChain 当前定义了固定帧管线：QueuedTask -> InputTask -> RenderTask。这个顺序本身没问题，问题在于它并没有把 phase、依赖和副作用显式建模出来。

相关文件：

1. src/ui/core/TaskChain.hpp
2. src/ui/core/SystemManager.hpp

现状更接近“按顺序执行的一组脚本步骤”，而不是一个具有明确阶段语义的调度器：

1. QueuedTask 修改 FrameContext，并顺带驱动 UpdateTimer 和 Dispatcher::Update。
2. InputTask 直接轮询 SDL，并夹带键盘重复输入逻辑。
3. RenderTask 决定布局、渲染和 EndFrame 的触发顺序。

这意味着：

1. 逻辑时序写死在任务对象里，难以组合和替换。
2. 系统行为依赖“谁在什么时候触发了哪个事件”，而不是显式 phase 合约。
3. 出问题时要同时读任务链、系统注册和事件订阅，局部推理成本很高。

### 3. InteractionSystem 仍然是 God Object

InteractionSystem 负责 SDL 事件轮询、窗口事件监听、输入翻译、键盘重复输入、即时刷新补救和一部分窗口同步逻辑。

相关文件：

1. src/ui/systems/InteractionSystem.hpp

这不是一个“交互系统”，而是一坨粘合层：

1. 既处理正常输入路径，又处理平台阻塞时的特例刷新路径。
2. 既负责 enqueue 原始输入，又会直接 trigger 布局和渲染。
3. 既做输入语义转换，又夹带窗口属性同步。

结果就是它同时维护两套语义：

1. 常规帧路径：先入队，下一帧统一处理。
2. 平台补救路径：当场触发布局和渲染。

这两套语义并存不是不能接受，但如果不把边界写死，后面很容易继续往这个类里堆“再加一个特例”。

### 4. Factory 仍然越权

Factory 不只是创建实体，它还承担 SDL_Window 创建、窗口 ID 绑定、标题栏拖拽配置、RootTag 调整和图形上下文相关副作用。

相关文件：

1. src/ui/api/Factory.cpp

这说明创建阶段没有被拆成纯构造和平台绑定两层，而是直接混在一起。

坏处很实际：

1. 创建失败路径复杂，容易出现半初始化状态。
2. 工厂函数的副作用半径太大，不适合作为稳定 API 边界。
3. RootTag 的添加与移除还在创建逻辑里来回修正，说明拓扑语义没有被稳定建模。

### 5. API 外观已经比 runtime 更健康

这算是当前 UI 模块最值得保留的一点。

相关文件：

1. src/ui/api/Chains.hpp
2. src/ui/api/Utils.hpp
3. src/ui/api/Controls.cpp
4. src/ui/api/Hierarchy.cpp

链式 DSL 和语义化脏标记入口说明模块已经有“高层写法要声明式、低层副作用要收口”的意识。问题不是方向错，而是内部运行时没有完全跟上这套边界设计。

换句话说：

1. 这套 API 值得保留。
2. 真正需要继续收敛的是内部 runtime，而不是对外 DSL。

## 这次新增的覆盖率测试

本次没有去碰高噪音的 SDL 平台分支，而是优先补了 TaskChain 这一层的单测。原因很简单：这里是 UI 主帧路径的骨架，之前几乎没有直接覆盖。

新增测试文件：

1. tests/unittest/ui/test_TaskChain.cpp

覆盖点包括：

1. Combined 的广播语义，确保前后任务收到同一组原始参数，且返回值来自第二个任务。
2. WrapArgs 的参数绑定语义，确保上下文参数会在真正执行任务前固化下来。
3. QueuedTask 的帧上下文推进逻辑，确认 intervalMs 和 frameSlot 会先更新，再派发 UpdateTimer 和队列事件。
4. RenderTask 的固定阶段顺序，确认 UpdateLayout -> UpdateRendering -> EndFrame 的顺序不被破坏。
5. RenderTask 的节流行为，确认 remainingTime 会按 delayTime 工作，而不是每次调用都强制重绘。

这批测试的价值不在于 case 数量，而在于它们把“UI 主循环靠什么顺序维持正确性”变成了显式约束。

## 现在仍然缺的覆盖

下面这些地方仍然是测试薄弱区：

1. InteractionSystem 的平台事件补救路径，没有被稳定隔离测试。
2. Factory 的失败回滚路径，尤其是 SDL_Window 创建失败和 windowID 绑定失败分支。
3. SystemManager 的注册顺序依赖，没有显式测试保证。
4. 脏标记传播和命中缓存失效之间的关系，仍然缺系统性覆盖。

这几个点里，优先级最高的是第 2 和第 4 项。它们最容易在重构时悄悄回归，但用户层面会直接看到 UI 异常。

## 后续建议

如果继续收敛 UI 架构，建议按下面顺序推进：

1. 先把 InteractionSystem 再拆一层，至少把平台补救刷新路径从常规输入路径里抽出来。
2. 给 Factory 补一个更明确的两阶段模型：实体构造和平台资源绑定分开。
3. 给 SystemManager 引入显式 phase 概念，而不是继续依赖注册顺序和任务链隐式约定。
4. 围绕 MarkLayoutChanged / MarkVisualChanged 增加更细的传播测试，防止布局脏和渲染脏的规则再次散开。

## 总结

UI 模块最大的问题不是“写得乱”，而是“很多正确性依赖默认约定”。

约定在小规模阶段可以跑得很快，但一旦模块继续长大，约定会比代码本身更难维护。当前最合理的策略不是推倒重写，而是继续把关键路径的隐式规则变成显式边界、显式入口和显式测试。