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

- 父节点与同层 sibling 目标、七条 line（含 baseline）、margin 覆盖、fill/centerIn 已落地并有单测；RTL 与 transform/scale 坐标空间尚未纳入。
- 同轴冲突已按 Qt Quick 语义拒绝（center 与同轴任一 edge 冲突、baseline 与垂直方向 edge/center 冲突）。
- 双边约束派生尺寸与用于定位的有效尺寸已统一经 min/max 裁剪，且恒为非负有限值。
- 目标销毁/重挂载后，指向它的 sibling binding 会被清除（公共实体句柄无版本号，避免索引复用误绑定），并在 `components::Anchors::lastDiagnostic` 记录 `AnchorTargetLost`。
- 尚未落地独立的 `AnchorDependencyIndex` 反向索引；脏传播目前依赖 `MarkLayoutDirty` + 每帧重解整棵脏 root。
- `baseline` 当前为节点几何（position.y + baselineOffset），尚未接入文本实际 baseline provider。
- RTL 语义未实现：布局仍固定传 `YGDirectionLTR`，且没有 public 的 root 布局方向 API。
- 采用 Yoga 的 containing-block、padding、min/max 规则，尚不承诺所有边界行为与 Qt 一致。
- UI 线程调用是前置条件。
- 项目不支持多 Runtime；每个进程只应存在一个 `Application` 实例，`Runtime` 只是其内部组件，因此锚点 API 不需要 Runtime 选择或跨 Runtime 校验。
- 当前锚点 API 依赖 Application 生命周期内的默认内部上下文；调用前必须已经创建 Application，并且在其 UI 执行上下文中使用。
- 已移除锚点组件中的 `sizeBeforeAnchors` 快照恢复逻辑；清除锚点不会覆盖用户当前声明尺寸。
- 已新增 `components::ImplicitSize`，为文本/图片等内容保留独立的隐式尺寸输入；锚点求解在未提供声明尺寸时可使用该输入，且不会将其写回 `components::Size`。
- `components::Size` 现在保留 `declaredSize` 与轴级 `hasDeclaredWidth/hasDeclaredHeight`，公共 `SetSize`/`SetFixedSize` 会同步声明值；锚点清除恢复最新声明值，而不是锚定期间的计算结果。
- 未覆盖复杂嵌套、滚动容器、父节点销毁及所有零尺寸边界。

## 验证

`ui_ecs_tests` 中 `AnchorsTest.*` 20 项通过，覆盖：填充、中心偏移（含零偏移）、边缘内距与 margin 覆盖、父节点 resize、声明尺寸恢复、显式零尺寸、隐式尺寸分离、非法/冲突/非有限参数、长链与静态循环、sibling 目标 parent-local 几何、baseline 偏移、双边尺寸 min/max 裁剪、目标销毁与重挂载失效诊断。

回归：`ui_ecs_tests` 163 项、`ui_unit_tests` 112 项全部通过；`check_architecture_boundaries.py` 与 `check_public_headers_self_contained.py` 均通过。

### 2026-10-03 增量

- 同轴冲突规则修正为 Qt Quick 语义（center 与同轴任一 edge 冲突、baseline 与垂直方向 edge/center 互斥），失败不改状态并更新对应单测预期。
- 锚点派生尺寸统一经 `[minSize, maxSize]` 裁剪（`LayoutSystem::applyAnchorLayout` 内 `clampSize`），双边/边缘+中心派生尺寸与单边/中心定位的有效尺寸都受裁剪，恒为非负有限值。
- `AnchorSet` 新增逐线 `unresolved` 标记与 `AnchorDiagnostic` 枚举；`components::Anchors` 记录 `lastDiagnostic`/`targetLostReported`。目标失效或跨层时该 line 本帧按无 binding 回退。
- 目标实体销毁时，`LayoutSystem` 通过 `on_destroy<Hierarchy>` 登记待失效目标，在下一帧 `update()` 中清除指向它的 sibling binding 并记录 `AnchorTargetLost`（延迟处理避免在信号内改注册表）；重挂载（`AddChild`/`RemoveChild`）同样记录诊断。
- 架构守卫基线补齐 `src/api/Layout.cpp` 的 8 处 `entt::entity`（锚点 API 边界校验/环检测），待迁移至 `src/systems/AnchorSolver` 后移除。

### 待推进

- `AnchorDependencyIndex` 反向索引 + 依赖闭包脏传播（同帧合并一次）。
- implicit size 反馈闭环（内容测量 → implicit size → 有限轮重算）。
- RTL 与 transform/scale 坐标空间契约、文本 baseline provider。