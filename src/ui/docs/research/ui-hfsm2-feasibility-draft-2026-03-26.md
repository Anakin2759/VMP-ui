# UI 模块引入 hfsm2 可行性草案

日期：2026-03-26

## 目的

本文档用于记录 UI 模块引入 hfsm2 的初步可行性评估，明确其适用边界、预期收益、主要风险，以及推荐的最小接入路径。

本文档的结论不是“是否要把整个 UI 模块改写成状态机框架”，而是回答一个更实际的问题：

在现有 ECS + Dispatcher + TaskChain 架构下，hfsm2 是否适合用于收敛交互 runtime 的复杂状态管理。

## 结论

### 结论摘要

hfsm2 可以引入，但只适合局部引入，不适合替代整个 UI 模块的现有架构。

更准确地说：

1. 适合用于收敛交互 runtime 中的复杂状态迁移。
2. 不适合替代 ECS 组件模型、事件总线和渲染管线。
3. 最适合的接入点是 PointerState 和 FocusState，而不是 LayoutSystem、RenderSystem 或整个控件树。

因此，hfsm2 在本项目中更适合作为“交互状态协调器”，而不是“UI 框架核心骨架”。

## 当前架构背景

UI 模块当前采用的是以下运行时结构：

1. ECS 组件存储：Registry
2. 事件分发：Dispatcher
3. 固定帧阶段：TaskChain
4. 交互输入接入：InteractionSystem
5. 命中测试：HitTestSystem
6. 状态推进：StateSystem
7. 抽象动作回调：ActionSystem

关键位置参考：

1. [src/ui/core/TaskChain.hpp](src/ui/core/TaskChain.hpp)
2. [src/ui/common/Events.hpp](src/ui/common/Events.hpp)
3. [src/ui/systems/InteractionSystem.hpp](src/ui/systems/InteractionSystem.hpp)
4. [src/ui/systems/HitTestSystem.hpp](src/ui/systems/HitTestSystem.hpp)
5. [src/ui/systems/StateSystem.hpp](src/ui/systems/StateSystem.hpp)
6. [src/ui/systems/ActionSystem.hpp](src/ui/systems/ActionSystem.hpp)
7. [src/ui/common/GlobalContext.hpp](src/ui/common/GlobalContext.hpp)

从代码现状看，UI 模块已经存在若干“手写状态机”，只是它们目前主要通过：

1. 布尔标记
2. entt::entity 当前激活对象
3. 事件顺序
4. 帧尾合并逻辑

来维持正确性，而不是通过显式状态图表达。

## 为什么 hfsm2 在局部上是可行的

### 1. 当前交互链路天然存在状态机主题

[src/ui/systems/StateSystem.hpp](src/ui/systems/StateSystem.hpp) 中已经可以明确识别出多条状态机主题：

1. PointerState：Hover / Active / Press / Release / Click / Drag
2. FocusState：焦点切换、输入法启停、Caret 显示
3. ScrollInteractionState：滚动条命中、轨道点击、滑块拖拽、滚轮滚动
4. SliderInteractionState：按下、拖拽、释放
5. WindowSyncState：窗口位置、尺寸、显示状态同步

这说明引入状态机不是强行改变问题模型，而是把现有隐式状态迁移显式化。

### 2. 事件输入已经具备统一入口

[src/ui/common/Events.hpp](src/ui/common/Events.hpp) 已经把原始输入事件与抽象交互事件区分开了。

例如：

1. RawPointerMove / RawPointerButton / RawPointerWheel
2. HitPointerMove / HitPointerButton / HitPointerWheel
3. HoverEvent / UnhoverEvent / MousePressEvent / MouseReleaseEvent / ClickEvent

这意味着状态机可以稳定地挂在“命中后的语义层”上，而不必直接接触 SDL 原始事件。

### 3. 当前最复杂的问题属于离散状态迁移问题

StateSystem 当前最混乱的部分，不是数学计算，也不是大规模数据处理，而是：

1. 当前到底是 hover 还是 active
2. 当前按下是否已经升级为拖拽
3. 当前释放是否应该触发 click
4. 当前滚动条拖拽是否消费后续输入
5. 当前 focus 切换是否应立即启动输入法

这些都属于有限状态迁移问题，hfsm2 在这类问题上是合适的。

## 为什么不适合全量引入

### 1. ECS 仍然应该是单一事实来源

UI 模块当前依赖 ECS 组件和标签表达可观察状态，例如：

1. HoveredTag
2. ActiveTag
3. FocusedTag
4. Caret
5. ScrollArea

这些状态最终要被渲染系统、动作系统和其他系统读取，因此它们必须继续作为 ECS 数据存在。

如果把 hfsm2 当成新的事实来源，而 ECS 只是附属镜像，会带来双源状态问题。

### 2. Layout 和 Render 不是有限状态问题

[src/ui/systems/LayoutSystem.hpp](src/ui/systems/LayoutSystem.hpp) 和 [src/ui/systems/RenderSystem.hpp](src/ui/systems/RenderSystem.hpp) 的核心问题分别是：

1. 布局脏标记传播与回写
2. 批次收集、资源管理和渲染时序

这两类问题本质上不是状态图问题。强行用 hfsm2 表达，通常只会增加额外复杂度，而不会提升可维护性。

### 3. HitTestSystem 的核心问题是缓存策略，不是状态表达

[src/ui/systems/HitTestSystem.hpp](src/ui/systems/HitTestSystem.hpp) 当前的主要设计债是缓存失效规则复杂，而不是少了状态机。

因此不建议把 HitTestSystem 整体改造成状态机系统。

### 4. 整个控件树不应演化成“一控件一状态机”

如果后续把每个 UI 实体都绑定一个 hfsm2 实例，虽然理论上可以成立，但会带来明显问题：

1. 管理成本急剧上升
2. 调试路径变复杂
3. 与 ECS 组件状态产生职责重叠
4. 对简单控件而言建模过度

因此更合理的粒度是 runtime 级、窗口级或交互域级状态机，而不是实体级全覆盖状态机。

## 推荐的引入边界

### 推荐边界 1：PointerMachine

这是最优先的接入点。

建议让一台 PointerMachine 负责协调以下逻辑：

1. Idle
2. Hovering
3. Pressed
4. DragCandidate
5. Dragging
6. ScrollbarDragging
7. SliderDragging

输入来源：

1. HitPointerMove
2. HitPointerButton
3. HitPointerWheel
4. EndFrame 或等价 phase 信号

输出副作用：

1. 写回 HoveredTag / ActiveTag
2. 更新 StateContext 中当前 active 和 hovered 实体
3. 触发 MousePressEvent / MouseReleaseEvent / ClickEvent
4. 更新 ScrollArea 或 SliderInfo 组件

### 推荐边界 2：FocusMachine

建议让 FocusMachine 负责：

1. 无焦点
2. 有焦点但非文本输入
3. 文本输入焦点
4. Caret 可见性或输入法激活相关切换

输入来源：

1. HitPointerButton
2. TextEdit 命中判定
3. 窗口切换或关闭事件

输出副作用：

1. 写回 FocusedTag
2. 更新 Caret 组件
3. 启停 SDL_StartTextInput / SDL_StopTextInput

### 可选边界 3：WindowInteractionMachine

这不是第一阶段重点，但后续可以考虑：

1. 常规帧驱动
2. 平台窗口事件补救刷新
3. expose / resize / moved 导致的即时刷新路径

适合位置主要在 [src/ui/systems/InteractionSystem.hpp](src/ui/systems/InteractionSystem.hpp) 和窗口同步职责边界之间。

## 不推荐的接入方式

### 1. 不要用 hfsm2 替代 Dispatcher

Dispatcher 负责事件总线语义，hfsm2 负责状态迁移语义，两者职责不同。

### 2. 不要让 hfsm2 替代 ECS 组件真值

状态机只应负责“决定怎么变”，不应成为系统最终共享状态的唯一来源。

### 3. 不要一开始就按实体级部署状态机

第一阶段应避免“一控件一台状态机”的方案，优先以 runtime 级或交互域级协调器的形式落地。

### 4. 不要在 phase 未收敛前直接接入

如果 [src/ui/core/TaskChain.hpp](src/ui/core/TaskChain.hpp) 中的 phase 语义、[src/ui/common/Events.hpp](src/ui/common/Events.hpp) 中的 Trigger 和 Enqueue 规则仍然不稳定，那么 hfsm2 接入后只会把旧的时序问题搬进状态机内部。

## 主要风险

### 风险 1：双重驱动

当前 [src/ui/systems/ActionSystem.hpp](src/ui/systems/ActionSystem.hpp) 和 [src/ui/systems/StateSystem.hpp](src/ui/systems/StateSystem.hpp) 都会响应交互事件。

如果引入 hfsm2 后，旧逻辑和新逻辑同时推进状态，就会出现双重驱动问题。

表现为：

1. Click 触发两次
2. Hover 状态来回抖动
3. ActiveTag 或 FocusedTag 重复写入

### 风险 2：立即应用与延迟应用混用

当前 StateSystem 里：

1. Hover / Active 倾向于帧尾合并
2. Focus 倾向于立即应用

如果不先明确 phase 规则，hfsm2 接入后依然会遇到“当前事件是否应立刻生效”的歧义。

### 风险 3：过度建模

如果把简单的标签切换也强行抽象成层级状态节点，会让系统比现在更难理解。

### 风险 4：实体粒度过细导致状态机泛滥

若未来每个控件都拥有自己的状态机实例，将极大增加调试和序列化难度。

## 前置条件

在正式引入 hfsm2 之前，建议至少完成以下约束整理：

1. 明确 TaskChain 中的 phase 语义。
2. 明确 Events 中哪些事件只能 Trigger，哪些只能 Enqueue。
3. 明确 StateSystem 和 ActionSystem 的边界，避免引入状态机后重复推进。
4. 明确 ECS 组件仍然是最终可观察结果，而不是状态机自身。

## 最小接入方案

### Stage 1：仅替换 PointerState

目标：

只替换 [src/ui/systems/StateSystem.hpp](src/ui/systems/StateSystem.hpp) 中最复杂的 PointerState 逻辑，不动 Layout、Render 和 HitTest 的主体架构。

范围：

1. Hover
2. Press
3. Release
4. Click 判定
5. Drag 判定
6. ScrollbarDragging
7. SliderDragging

保持不变：

1. Registry / Dispatcher 入口
2. ECS 组件和标签
3. ActionSystem 的回调层

### Stage 2：接入 FocusState

目标：

把文本输入焦点、输入法启停、Caret 初始可见性从 StateSystem 中拆出来。

范围：

1. FocusedTag
2. Caret
3. TextEdit 聚焦判定
4. SDL_StartTextInput / SDL_StopTextInput

### Stage 3：评估平台窗口补救路径是否状态机化

目标：

仅在 Phase 语义稳定之后，再评估 [src/ui/systems/InteractionSystem.hpp](src/ui/systems/InteractionSystem.hpp) 中平台窗口补救路径是否需要单独的小型状态机。

## 验证建议

如果后续尝试接入 hfsm2，建议优先补以下测试：

1. Hover 和 Active 的迁移顺序测试
2. Press 后拖动是否升级为 Dragging 的测试
3. Dragging 释放后是否抑制 Click 的测试
4. Scrollbar 拖拽与 Slider 拖拽互斥测试
5. Focus 切换和文本输入启停测试
6. 同帧事件下的立即应用与延迟应用一致性测试

## 最终建议

当前判断是：

1. hfsm2 适合引入，但应定位为交互 runtime 的局部协调器。
2. 最合适的切入点是 PointerMachine 和 FocusMachine。
3. 不建议把整个 UI 模块、整个 ECS、整个控件树都迁移到 hfsm2。
4. 真正的前提不是先写状态图，而是先稳定 phase 和事件语义。

只要坚持“ECS 是状态真值，hfsm2 只负责迁移协调”这条边界，hfsm2 在当前 UI 模块中是有现实价值的。