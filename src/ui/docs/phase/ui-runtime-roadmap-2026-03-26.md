# UI 模块设计评审与长期改进规划

日期：2026-03-26

## 目的

本文档用于统一记录 UI 模块当前的设计评审结论、保留项、主要结构性问题，以及面向长期演进的改进路线。

现有 UI 模块已经具备可用的 ECS 分层、链式 DSL、渲染器拆分和基础测试能力，但 runtime 边界、事件语义和交互链路仍然依赖较多隐式约定。后续工作的重点不应是继续堆叠功能，而应是收敛运行时边界，让模块从“可运行”走向“可长期维护”。

本文档在以下已有材料基础上进一步整合并扩展：

1. [../reviews/ui-architecture-review-2026-03-26.md](../reviews/ui-architecture-review-2026-03-26.md)
2. [../reviews/ui-coverage-review-2026-03-26.md](../reviews/ui-coverage-review-2026-03-26.md)

## 总体判断

### 结论

当前 UI 模块的主要问题不是渲染能力不足，也不是 DSL 表达能力不够，而是运行时层的边界不够显式。

对外 API 已经比内部 runtime 更健康。链式 DSL、工厂函数、组件模型和渲染器分层都具备保留价值；真正需要重构的是 runtime 的依赖关系、帧阶段语义、窗口生命周期和交互状态管理。

一句话总结：

1. 保留 DSL 与渲染体系。
2. 收敛 Runtime 边界与事件语义。
3. 拆分交互链路与窗口生命周期职责。

## 当前可保留资产

以下设计方向不建议推倒重来，而应作为后续重构的稳定边界：

1. 链式 DSL 与语义化 UI API。
位置参考：[Chains.hpp](src/ui/api/Chains.hpp)

2. 基于 EnTT 的组件数据模型。
位置参考：[Components.hpp](src/ui/common/Components.hpp)

3. 渲染器分层与 RenderSystem 的总体组织方式。
位置参考：[RenderSystem.hpp](src/ui/systems/RenderSystem.hpp)

4. 现有 UiRuntime 多运行时切换雏形。
位置参考：[UiRuntime.hpp](src/ui/core/UiRuntime.hpp)

5. Window 属性同步能力的初步抽离。
位置参考：[WindowSync.hpp](src/ui/common/WindowSync.hpp)

## 主要结构性问题

### 1. Runtime 边界偏隐式

[Registry.hpp](src/ui/singleton/Registry.hpp) 和 Dispatcher 仍以静态入口为主，虽然 [UiRuntime.hpp](src/ui/core/UiRuntime.hpp) 已经引入活跃实例切换机制，但模块内部大多数代码仍然把依赖隐藏在全局入口后面。

这会带来几个直接问题：

1. 系统依赖不透明。
2. 测试隔离依赖手工清场。
3. 多实例运行时能力难以真正稳定落地。

### 2. 帧阶段与事件语义不够显式

[TaskChain.hpp](src/ui/core/TaskChain.hpp) 当前定义了 Queue、Input、Render 的固定顺序，但并未把 phase 语义、事件触发规则和平台补救路径抽象成正式合约。

结果是正确性大量依赖执行顺序，而不是依赖可推理的阶段边界。

### 3. InteractionSystem 和 StateSystem 职责过重

[InteractionSystem.hpp](src/ui/systems/InteractionSystem.hpp) 同时负责：

1. SDL 事件采集
2. 输入翻译
3. 键盘重复输入
4. 平台窗口阻塞场景的即时刷新补救

[StateSystem.hpp](src/ui/systems/StateSystem.hpp) 同时负责：

1. Hover 状态
2. Active 状态
3. Focus 状态
4. 滚动条拖拽与滑块拖拽
5. 部分窗口生命周期处理

这两个系统都已经超出单一职责范围，且通过事件顺序形成隐式耦合。

### 4. HitTestSystem 的缓存失效策略维护成本高

[HitTestSystem.hpp](src/ui/systems/HitTestSystem.hpp) 通过大量组件构造、更新、销毁监听来保证交互缓存有效。这种方式短期可行，但监听面过大，扩展可交互组件时容易遗漏失效条件。

核心问题不是缓存本身，而是缓存正确性依赖过多分散入口。

### 5. 窗口生命周期尚未独立成稳定服务

[WindowSync.hpp](src/ui/common/WindowSync.hpp) 已经把一部分 SDL 窗口属性同步逻辑抽出来，但窗口创建、图形上下文 set 和 unset、关闭、位置和尺寸回写、异常回滚仍然散落在多个系统与工厂实现中。

目前还不能算真正稳定的 Window Lifecycle 抽象。

### 6. 测试覆盖仍未触达高风险交互链路

当前测试已经覆盖了一部分骨架与 API 层行为，例如：

1. [test_TaskChain.cpp](tests/unittest/ui/test_TaskChain.cpp)
2. [test_UiRuntime.cpp](tests/unittest/ui/test_UiRuntime.cpp)
3. [test_UtilsCoverage.cpp](tests/unittest/ui/test_UtilsCoverage.cpp)
4. [test_HierarchyCoverage.cpp](tests/unittest/ui/test_HierarchyCoverage.cpp)

但以下高风险区域仍需重点补强：

1. StateSystem 的 Hover、Active、Focus 竞态
2. InteractionSystem 的平台补救刷新路径
3. HitTestSystem 的缓存失效规则
4. Window 生命周期与异常回滚行为

## 重构原则

为了避免把 UI 模块变成长期重写工程，后续改进应遵守以下原则：

1. 先固化语义，再替换实现。
2. 先收敛 runtime 边界，再处理局部性能优化。
3. 尽量保留 DSL、组件模型和渲染器分层。
4. 每个阶段都必须有可验证的测试与回归边界。
5. 不通过继续堆特例来修正系统设计缺陷。

## 长期改进路线

### Phase 0：冻结设计基线

目标：形成统一设计基线，停止评审结论继续分叉。

工作内容：

1. 合并现有评审文档结论。
2. 明确保留项与重构项。
3. 产出 runtime 职责图和系统迁移表。

完成标准：

1. UI 模块的重构目标统一到一份文档。
2. 不再出现“局部止血”和“长期蓝图”相互冲突的情况。

### Phase 1：建立显式 Runtime 边界

目标：把当前线程活跃 Registry、Dispatcher、Context 的获取方式正式收敛到 Runtime 边界。

建议方向：

1. 基于 [UiRuntime.hpp](src/ui/core/UiRuntime.hpp) 增加 Runtime facade 或 RuntimeServices 层。
2. 把 Registry、Dispatcher、FrameContext、StateContext 的获取方式统一收口。
3. 后续新增系统优先通过 Runtime 边界取依赖，而不是继续扩散静态入口。

收益：

1. 依赖关系更可见。
2. 测试隔离更自然。
3. 为多实例和后续调度模型铺路。

### Phase 2：显式化 Phase 与事件规则

目标：把主循环和平台补救路径从“约定”变成“规则”。

建议方向：

1. 在 [TaskChain.hpp](src/ui/core/TaskChain.hpp) 之上定义明确 phase。
2. 在 [SystemManager.hpp](src/ui/core/SystemManager.hpp) 中引入与 phase 对应的系统组织规则。
3. 明确哪些事件必须立即触发，哪些事件必须入队，哪些事件只能出现在平台补救通道。

收益：

1. 系统顺序依赖变成显式约束。
2. 交互路径更容易推理与测试。
3. 后续拆分系统时不会重新靠隐式时序粘起来。

### Phase 3：拆分交互主链路

目标：消除 InteractionSystem 和 StateSystem 的 God Object 趋势。

建议方向：

1. 将 InteractionSystem 拆为 SDL 采集层、输入翻译层、平台补救层。
2. 将 StateSystem 拆为 PointerState、FocusState、ScrollInteraction、WindowLifecycle 或等价主题。
3. 保持事件边界清晰，避免跨主题私有辅助函数互相调用。

收益：

1. 单个系统职责更清晰。
2. 交互问题排查范围更小。
3. 后续新增控件时更容易落到正确层级。

### Phase 4：收敛命中测试与缓存失效策略

目标：降低 HitTest 缓存维护复杂度。

建议方向：

1. 减少分散的组件监听入口。
2. 评估基于拓扑版本号或窗口级版本号的缓存失效。
3. 把“可交互成员资格变更”和“几何变化”区分开来处理。

收益：

1. 降低遗漏风险。
2. 提高多窗口和复杂控件场景的可维护性。
3. 缓存规则更容易通过测试固定下来。

### Phase 5：窗口生命周期服务化

目标：让窗口相关逻辑脱离 StateSystem、InteractionSystem 和 Factory 的夹缝生长状态。

建议方向：

1. 以 [WindowSync.hpp](src/ui/common/WindowSync.hpp) 为基础建立 WindowLifecycle 服务层。
2. 统一窗口属性同步、关闭、Graphics Context set 和 unset、位置和尺寸回写逻辑。
3. 明确触发时机、失败处理和回滚策略。

收益：

1. 平台相关逻辑集中管理。
2. Factory 与交互系统的副作用半径缩小。
3. 窗口问题调试边界更清晰。

### Phase 6：Factory 两阶段化

目标：把实体构造与平台资源绑定分离。

建议方向：

1. 先创建 ECS 实体与组件拓扑。
2. 再进行 SDL_Window、图形上下文与窗口属性绑定。
3. 失败时以阶段边界为单位回滚，避免半初始化状态扩散。

收益：

1. 创建路径更可测试。
2. Headless 场景更容易复用纯构造逻辑。
3. 平台资源失败回滚更清晰。

### Phase 7：把测试改造成架构保护网

目标：测试不再只是覆盖行为，而是保护关键设计约束。

优先增加以下测试：

1. Hover、Active、Focus 同帧切换规则
2. 滚动条与滑块拖拽行为
3. 平台窗口阻塞场景的补救刷新路径
4. Phase 顺序与事件语义约束
5. HitTest 缓存失效规则
6. Window 生命周期失败回滚

完成标准：

1. 隐式规则被显式测试覆盖。
2. 重构时能快速识别语义回归。

### Phase 8：次级优化与性能收尾

目标：在边界稳定后再处理局部性能和工具化问题。

建议候选项：

1. alias 查找索引优化
2. 缓存粒度精调
3. 系统注册顺序可视化
4. 文档与测试基线联动

说明：

此阶段不应抢在 Runtime 边界和交互链路重构之前进行，否则容易用局部优化掩盖结构问题。

## 优先级建议

如果按长期蓝图推进，但希望每一阶段仍有可交付成果，建议优先级如下：

1. Phase 1：Runtime 边界
2. Phase 2：Phase 与事件规则
3. Phase 3：交互主链路拆分
4. Phase 5：窗口生命周期服务化
5. Phase 6：Factory 两阶段化
6. Phase 4：HitTest 缓存策略重做
7. Phase 7：测试保护网持续补齐
8. Phase 8：局部性能与工具化优化

说明：

HitTest 缓存重做虽然重要，但它更依赖前面的 phase 和交互职责边界先稳定下来；否则只是把旧规则换一种写法继续保存。

## 验证策略

每个阶段都应同时包含设计验证、构建验证和行为验证。

### 设计验证

1. 依赖关系是否从隐式全局入口变成显式 Runtime 边界。
2. 系统职责是否从大类混合变成按主题分层。
3. 是否减少跨系统私有辅助函数调用。

### 构建验证

1. 保持 UI 模块可独立通过 Debug 构建。
2. 在启用测试构建后能稳定运行 UI 单测集。

### 行为验证

建议维护以下回归清单：

1. 窗口 resize 与 expose
2. 文本输入焦点切换
3. 滚动条拖拽
4. Slider 拖拽
5. 多窗口命中测试
6. 长按重复输入

## 不建议的做法

以下方向不建议作为主线：

1. 直接推倒重写 UI 模块。
2. 在 Runtime 边界没收敛前优先做局部性能优化。
3. 继续把平台补救逻辑塞进 InteractionSystem。
4. 继续扩大 StateSystem 文件尺寸并依赖更多内部布尔状态维持正确性。

## 最终目标

长期目标不是把 UI 模块拆得更多，而是让模块形成以下性质：

1. Runtime 边界清晰。
2. Phase 和事件语义清晰。
3. 系统职责单一且可组合。
4. 平台窗口生命周期稳定可控。
5. 关键交互路径有测试保护。

达到以上五点后，UI 模块才算真正具备继续扩展复杂控件、复杂交互和多窗口场景的工程基础。