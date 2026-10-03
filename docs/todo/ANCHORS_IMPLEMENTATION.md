# Qt Quick Anchors 完整语义实施要求

> 本文档中的“第一阶段”仅描述历史落地状态，不代表最终目标。项目最终要求是实现完整的 Qt Quick anchors 语义；在完整语义完成前，不得将当前父节点锚点实现宣传为 Qt Quick anchors 兼容实现。

## 强制目标

锚点系统最终必须支持：

- `left`、`right`、`top`、`bottom`；
- `horizontalCenter`、`verticalCenter`；
- `baseline`；
- `margins`、`leftMargin`、`rightMargin`、`topMargin`、`bottomMargin`；
- `horizontalCenterOffset`、`verticalCenterOffset`、`baselineOffset`；
- `fill` 与 `centerIn`；
- 父节点和同层 sibling 作为目标；
- 目标有效性、目标删除、重挂载和实体索引复用处理；
- 同轴冲突检测、循环依赖检测和确定性回退；
- implicit size、声明尺寸、双边约束、最小/最大尺寸的明确优先级；
- 锚点节点与 Flex 布局互斥，但其子树仍可正常布局；
- 脏传播、依赖索引、同帧拓扑求解；
- parent local coordinate 语义，以及明确的 RTL、transform/scale 支持边界。

完整方案见：`docs/architecture/change-qt-quick-anchors-20260921.md`。

当前 `ParentAnchors` 仅为过渡模型，不能视为最终 API。

新增 `ui::ParentAnchors`，锚点配置由 Application 内部的默认 Runtime 绑定。公共接口为 `ui::layout::SetAnchors(entity, anchors)` 和 `ui::layout::ClearAnchors(entity)`，不暴露 `UiRuntime`；安装消费者只能看到 Application、实体句柄、窗口句柄和公共数据类型。`AnchorCenterIn()`、`AnchorFill()` 和 `ClearAnchor()` 提供链式入口。

支持 left/right/top/bottom 内边距、horizontalCenter/verticalCenter 有符号偏移，以及 `ParentAnchors::Fill(margin)`、`ParentAnchors::CenterIn(x, y)`。

锚点实体退出 Flex 排列。双边锚点决定轴尺寸，中心锚点使用本帧 Yoga 测量后再次布局，不读取上一帧几何。非有限参数、无效实体、无父节点及中心/边缘冲突返回错误，不修改原配置。清除恢复设置锚点时的尺寸快照与原布局策略。

## 当前限制

- 仅支持父节点，不支持兄弟线到线关系、baseline 或完整 Qt Quick anchors。
- 采用 Yoga 的 containing-block、padding、min/max 规则，尚不承诺所有边界行为与 Qt 一致。
- UI 线程调用是前置条件。
- 项目不支持多 Runtime；每个进程只应存在一个 `Application` 实例，`Runtime` 只是其内部组件，因此锚点 API 不需要 Runtime 选择或跨 Runtime 校验。
- 当前锚点 API 依赖 Application 生命周期内的默认内部上下文；调用前必须已经创建 Application，并且在其 UI 执行上下文中使用。
- 锚定期间修改尺寸后再清除，仍恢复最初尺寸快照；声明尺寸与布局结果分离留待后续完善。
- 已移除锚点组件中的 `sizeBeforeAnchors` 快照恢复逻辑；清除锚点不会覆盖用户当前声明尺寸。
- 已新增 `components::ImplicitSize`，为文本/图片等内容保留独立的隐式尺寸输入；锚点求解在未提供声明尺寸时可使用该输入，且不会将其写回 `components::Size`。
- `components::Size` 现在保留 `declaredSize` 与轴级 `hasDeclaredWidth/hasDeclaredHeight`，公共 `SetSize`/`SetFixedSize` 会同步声明值；锚点清除恢复最新声明值，而不是锚定期间的计算结果。
- 未覆盖复杂嵌套、滚动容器、父节点销毁及所有零尺寸边界。

## 验证

`ui_ecs_tests` 中 `AnchorsTest.*` 16 项通过：填充、中心偏移、边缘内距、父节点 resize、声明尺寸恢复、显式零尺寸、隐式尺寸分离、非法/冲突参数、长链和循环等。尚未执行全套回归。