# Qt Quick Anchors 完整语义落地方案

- **日期**：2026-09-21
- **输入来源**：当前 `d:\code\VMP-ui` 源码；重点检查 `include/ui/Anchors.hpp`、`include/ui/api/Layout.hpp`、`src/api/Layout.cpp`、`src/common/components/Layout.hpp`、`src/systems/LayoutSystem.cpp/.hpp`、`src/helper/Helper.hpp`、`tests/unittest/test_Anchors.cpp` 与现有锚点规划文档。
- **作用范围**：公共锚点 API、ECS 锚点状态、Hierarchy/实体生命周期、Yoga 同步边界、LayoutSystem 求解顺序、脏传播和单元测试；不改变公共 API 暴露 `UiRuntime`，不引入多 Runtime 选择。

## 1. 结论与推荐架构

推荐将锚点从“Yoga absolute position 的若干快捷映射”升级为**独立的 Anchor Constraint Solver**，Yoga 只负责非锚点 Flex/Layout 子树的 intrinsic/implicit size 和普通 Flex 排列。每个布局根在一次布局帧内按以下阶段执行：

1. 同步实体树和约束引用，建立本层 anchor graph。
2. 校验约束、清理失效目标、检测 SCC/环；记录诊断并按确定性规则禁用受影响约束。
3. 先由 Yoga 计算普通 Flex 节点和所有锚点节点的 implicit size；锚点节点从 Flex 排列中排除。
4. 对锚点图做拓扑求解：父/已求解 sibling 的几何先可用，再解目标节点的位置和由双边约束决定的尺寸。
5. 将结果写回 `Position`/`Size`，若尺寸反馈影响 implicit size 或父节点，再有限次数重算；达到稳定或报告未收敛。
6. 统一比较几何后向下游传播 dirty，避免在求解过程中递归触发即时布局。

这比继续向 Yoga 注入 left/right/margin 更符合 Qt Quick：Qt anchors 是几何约束关系，不是 Flex item margin；Yoga 不支持同层 sibling 约束、baseline 约束图和可靠的环诊断。

## 2. Qt Quick 语义范围（本期完整契约）

### 2.1 Anchor lines 与目标

支持七类 line：`left`、`right`、`top`、`bottom`、`horizontalCenter`、`verticalCenter`、`baseline`。目标只能是：

- `parent`：父 Item 的对应 line；
- **同层 sibling**：与当前节点拥有同一直接 parent 的实体，不能跨层、不能指向后代；
- `baseline` 目标须具备可用 baseline（文本/显式 baseline provider），否则约束无效并产生诊断。

每条 line 是“本节点 line = target line + offset”。公共 API 的目标使用 `ui::entity`，由 Application 内部当前上下文解析；不公开 `UiRuntime`。进程约束仍为单 `Application`/单默认上下文，调用前必须处于 Application UI 执行期。

### 2.2 属性模型

`AnchorSet` 应分别保存 line binding（target entity、target line、启用状态）和 offset：

- `margins`：统一覆盖四个 edge line 的默认 margin；不影响 center/baseline；
- `leftMargin`、`rightMargin`、`topMargin`、`bottomMargin`：对应 edge 的显式 margin，优先于 `margins`；
- `horizontalCenterOffset`、`verticalCenterOffset`、`baselineOffset`：对应 center/baseline 的有符号偏移；
- 建议保留 `baseline` 独立于 top/bottom，不把它降级为普通 edge；
- 所有值必须 finite，margin 按 Qt 语义为非负；offset 可正可负。

覆盖规则：`anchors.margins` 是默认值；任一显式 side margin 设置后只覆盖对应 side，不改变其他 side。清除显式 side margin 后回退到 `margins`。不能把组件 `Margin` 与 anchor margin 静默相加；锚点求解时 anchor margin 优先，普通外边距只在未参与该轴 anchor 的 Flex 节点中生效。

### 2.3 fill / centerIn

- `fill: target` 等价于 left/right/top/bottom 四边绑定到同一 target，使用对应 side margin；fill 不自动绑定 baseline/center。
- `centerIn: target` 等价于 horizontalCenter/verticalCenter 绑定到同一 target；使用两个 center offset，不使用 side margins。
- `fill`/`centerIn` 仅是批量设置 API，不应成为与单条 line 并存的第三种求解模型；设置任一显式 line 后应更新同一份 binding 状态。
- 本期公共 convenience API 至少提供 `SetAnchor`, `ClearAnchor`, `SetFill`, `SetCenterIn`, `ClearAllAnchors`；避免只支持现有的 `ParentAnchors::Fill/CenterIn` 值对象。

### 2.4 尺寸、implicit size 与优先级

每轴按以下规则确定：

1. 两条相对 edge（left+right 或 top+bottom）同时存在：由目标几何、margin 和 parent/sibling 坐标求出位置及尺寸；显式 width/height 不再主导该轴，但仍受 min/max 限制。
2. 只有一条 edge：保持声明 width/height；没有显式尺寸则使用 implicit size。
3. center 存在：保持声明/implicit size，以目标中心和 offset 定位。
4. 没有 anchor：使用 Flex/Layout 原有规则。
5. `fill` 在目标尺寸不可用时等待目标求解；不得读取上一帧位置作为正常结果。

`implicit size` 必须与声明 `Size` 分离保存：文本、图片、组件内容提供 intrinsic/implicit size；Yoga 测量结果可作为候选值，但不得写入用户声明值再用“快照”恢复。显式 width/height、min/max、anchor 双边约束的优先级必须在 solver 中统一裁剪并有测试定义；冲突后尺寸至少为零，不产生 NaN。

### 2.5 冲突与错误规则

- 同一轴不能同时设置 center 与任一 edge；`horizontalCenter` 与 left/right 冲突，`verticalCenter` 与 top/bottom 冲突。
- 同一 line 只能有一个 binding；重复设置应替换，而不是叠加。
- `fill` 与手动同轴 edge 是同一组 binding，重复设置按最后一次 API 写入；`centerIn` 与手动 center 同理。
- anchor 目标必须有效、为 parent 或同层 sibling；self、跨层、后代、不同 root、无效句柄直接返回 `INVALID_ARGUMENT`，且保持原状态不变。
- 同一轴的双边约束与显式尺寸不是 API 错误，而是确定性约束优先级（双边决定尺寸）；记录 debug 诊断即可。
- 检测到 cycle：整条强连通分量的 anchor 约束本帧禁用，节点回退到无该约束的 intrinsic/Flex 几何；返回值 API 可在设置时拒绝已可检测环，运行时因层级/尺寸变化形成的环必须每帧诊断。不得使用“上一帧几何”打破环。

### 2.6 删除、重挂载与目标变更

目标删除、目标失效、目标移出同层或 reparent 后，binding 必须自动变为 inactive/清除，目标节点不得留下悬空 entity。推荐保留 binding 元数据但标记 `Unresolved`，供调试查询；布局按无该 binding 回退，且一次性产生 `AnchorTargetLost` 诊断。重新绑定只能通过显式 API，避免 entity index 重用误绑定。

删除锚点节点时同步移除反向依赖索引；删除/重挂载 parent 时使受影响 root 和依赖后代 dirty。`Hierarchy::AddChild` 还必须拒绝把节点挂到自身/后代，以免同时破坏层级树和 anchor graph。

### 2.7 Layout/Flex、RTL、transform/scale

- 有任一 active anchor 的节点退出父 Flex 排列，但仍保留 Yoga 测量/intrinsic size；不可把锚点转译成普通 absolute child 后再让 Yoga决定 sibling 关系。
- `LayoutInfo` 与 anchors 不能共同决定同一节点的布局。推荐：active anchors 与 Flex item 属性互斥；设置锚点时从 Flex participant 集合排除，保留 LayoutInfo 仅用于该节点的子树。若 parent 自身是 anchor 节点，两者可在不同实体层共存。
- RTL 通过逻辑映射处理：公共物理线 `left/right` 的语义必须固定，若后续增加 `start/end`，只能在解析层映射到 left/right；不能依赖 Yoga `YGDirection` 误改变已有物理 line。现阶段 root 计算方向不应固定为 `YGDirectionLTR` 而忽略布局方向。
- anchors 在**未变换的 parent local coordinate** 中求解。节点自身 rotation/scale 不改变其 anchor line；parent transform/scale 需要在坐标空间转换层处理，不能把 scale 当 margin/尺寸乘法。若当前渲染变换只存在动画辅助函数而无持久 Transform component，则本期明确“不支持 transform 参与 anchor 几何”，API/文档应拒绝或按 local geometry 处理，不能声称 Qt 等价。

## 3. 当前实现的明确问题

1. `ParentAnchors` 只有六个 `optional<float>`，没有目标、`baseline`、四种 margin 覆盖、center/baseline offset，也无法表示 sibling。
2. `SetAnchors` 强制存在 parent，直接排除了 sibling 和未来 `fill: target`/`centerIn: target`。
3. `ConfigureAbsolutePosition` 把 center 转成 `left: 50%`，随后通过 `YGNodeStyleSetMargin(left, offset - width/2)` 修正；这不是约束求解，且依赖 Yoga 本帧测量与二次布局，无法支持 sibling、baseline 或完整双向依赖。
4. center offset 为 `0` 时使用 `if (value.horizontalCenter)` 判断，零偏移不会进入二次修正；`CenterIn()` 的默认零偏移因此语义错误，现有测试与实现意图不一致。
5. 现有 center 使用 `value - width/2` 的 margin 公式并没有明确 parent content/padding 坐标空间，和 Qt Item 坐标语义不等价。
6. edge anchor 直接写 Yoga absolute position；只设置 left/right 时尺寸行为交给 Yoga，而显式 `Size`、implicit size、min/max 的优先级未建立 Qt 契约。
7. `ParentAnchors::Fill(margin)` 只把同一个数写入四条 edge，没有 `margins` 覆盖层，也未区分 anchor margin 与组件 `Margin`。
8. `Anchors::originalSize` 是设置时快照；锚定期间修改尺寸后 `ClearAnchors` 恢复旧尺寸，破坏声明尺寸/计算尺寸分离，也不能表达 implicit size。
9. `LayoutSystem` 的 Yoga child 排列仍以普通树为中心；当前“退出 Flex”只靠 absolute style，且 sibling anchor 不能成为 Yoga 的可见目标。
10. `findRoot` 只有最大深度保护，没有 anchor graph 的 cycle/SCC 检测；层级 AddChild 也没有 self/descendant 环检测。
11. 实体销毁只释放 Yoga node；没有 anchor 反向依赖清理和目标失效传播。目标删除后不存在明确回退/诊断契约。
12. 布局计算固定传 `YGDirectionLTR`；没有 RTL 语义。源码中 `Transform2D`/动画 scale 存在，但 anchor solver 没有 transform/scale 坐标处理。
13. `MarkLayoutDirty` 的现有传播需要核对：锚点目标几何变化应传播到所有反向依赖节点及其布局 root，而不是只标记被直接修改的实体；当前代码没有 anchor dependency index。
14. `SetAnchors` / `ClearAnchors` 通过 `UiRuntime::current()` 满足“不暴露 Runtime”，但仍要求默认上下文和 UI 线程；这符合当前单 Application 约束，不能在新 API 中引入 runtime 参数或多实例路由。

## 4. API 与数据结构改造计划

### 公共头（P0）

- `include/ui/Anchors.hpp`：新增 `AnchorLine`、`AnchorTarget`、`AnchorRef`、`AnchorSet`、`AnchorMargins`；提供 `ParentAnchors` 兼容构造或标记 deprecated，避免直接暴露内部 registry/runtime。
- `include/ui/api/Layout.hpp`：新增返回 `Result<void>` 的按 line 设置/清除、`SetAnchorFill(entity,target,margins)`、`SetAnchorCenterIn(entity,target,offsets)`、`ClearAllAnchors`；target 仅为 public `ui::entity`/`AnchorTarget`。所有 mutator 做参数验证并保证失败不改状态。
- `include/ui/api/Hierarchy.hpp`：必要时将 Add/Remove 改为可返回 `Result`（若保持 void，则内部至少拒绝非法环并产生统一诊断）；补充 target 变更触发约定。
- 不在任何公共头暴露 `UiRuntime`、`Registry`、Yoga 类型、solver 类型或内部 entity。

### ECS 与内部组件（P0/P1）

- `src/common/components/Layout.hpp`：将 `components::Anchors` 改为完整声明状态（bindings、margin overrides、unresolved 状态、revision），删除 `originalSize` 快照；新增 `ImplicitSize`/`LayoutResolution`（可合并到现有内部组件，但必须区分 declared/computed geometry）。
- 新增内部 `AnchorDependencyIndex`（建议放 `src/systems/AnchorSolver.hpp/.cpp` 或 `src/common` 的 runtime context）：source -> target 依赖、target -> dependents 反向索引、binding revision、诊断信息。
- 所有存储使用内部 `entt::entity`，公共 API 只在边界转换；实体版本校验由 `ui::entity` 转换层完成。

### 布局实现（P0/P1）

- 新增 `src/systems/AnchorSolver.hpp/.cpp`：解析有效 target，验证同层/parent，构建 line graph，Tarjan/Kosaraju SCC，拓扑求解，双边尺寸、center、baseline、margin 解析，有限次 implicit-size feedback。
- `src/systems/LayoutSystem.cpp/.hpp`：拆出 `syncYogaForFlex` 与 `applyAnchorResolution`；Yoga 只配置普通布局和测量，active anchor 节点不参与 parent Flex；删除当前 center 的“margin + 二次 Yoga”补丁。
- `src/helper/Helper.hpp` 与 `src/api/Layout.cpp`：设置/清除时更新 dependency index、校验冲突、标记 source、target dependents 和共同 root；`ClearAnchors` 不再恢复快照，而是恢复声明尺寸/布局策略。
- `src/helper/Helper.hpp` 的 `AddChild/RemoveChild`：维护锚点有效性、禁止层级环、reparent 时让旧/新 root 和 anchor dependents dirty。
- `src/core/UiRuntime` 的内部 context：增加单实例 anchor index/diagnostics 生命周期；不改变公共接口，不允许跨 Application entity 使用。

### 测试与构建（P1）

- 扩展 `tests/unittest/test_Anchors.cpp`，必要时新增 `tests/unittest/test_AnchorSolver.cpp`、`test_AnchorLifecycle.cpp`。
- 在 `tests/install_consumer` 增加公共 API 编译测试，确认不需要 `UiRuntime`/EnTT/Yoga。
- 更新 `src/CMakeLists.txt` 的源文件和测试 target；保持公共头自包含门禁通过。

## 5. 可执行里程碑与优先级

| 阶段 | 交付 | 依赖 | 验收重点 |
|---|---|---|---|
| P0-A | API/数据模型、声明/计算尺寸分离、兼容层 | 无 | 公共头不泄漏内部类型，旧 Fill/Center 调用可迁移 |
| P0-B | parent-only 完整七 line + margins/fill/centerIn | P0-A | 无 Yoga 二次 margin 补丁；双边、center、baseline 正确 |
| P1-A | sibling target、依赖索引、拓扑/SCC | P0-B | 顺序无关、环稳定回退、target delete/reparent |
| P1-B | implicit size feedback、Layout/Flex 互斥、脏传播 | P1-A | 内容尺寸改变只重算受影响 root/依赖闭包 |
| P1-C | RTL、transform/scale contract、诊断和兼容文档 | P1-B | RTL 物理/逻辑规则、local coordinate 和不支持项明确 |

不推荐的备选方案是继续扩展 `ConfigureAbsolutePosition` 并依赖 Yoga 二次计算：改动短期少，但无法自然表达 sibling graph、baseline、环检测和目标生命周期，最终会形成不可维护的隐式求解器。

## 6. 验收测试矩阵

| 类别 | 必测场景 | 断言 |
|---|---|---|
| API/验证 | parent、sibling、self、跨层、后代、无效 target；NaN/Inf/负 margin；重复绑定 | 错误码准确、失败不改旧状态、重复设置最后写入 |
| 七条 line | 每条 line 单独绑定；baseline 文本到文本、无 baseline target | 几何 line 等式满足，baseline 不错误降级为 top |
| 组合 | left/right、top/bottom、center、baseline；同轴冲突 | 双边决定尺寸；center 与 edge 冲突；baseline offset 有符号 |
| margin | `margins`、各 side override、清除 override、anchor margin 与组件 Margin | 覆盖优先级稳定，不重复相加 |
| 批量 | fill parent/sibling、centerIn parent/sibling、offset 0/正/负 | 零 offset 仍生效；目标不是隐含固定 parent |
| 顺序/图 | sibling 前后顺序、链式 A->B->C、跨 root、多个 SCC、长链 | 结果与插入顺序无关；环仅禁用 SCC；无栈溢出 |
| 尺寸 | fixed、auto、implicit text/image、双边拉伸、min/max、父 resize | declared/implicit/computed 分离；不依赖上一帧错误几何 |
| 生命周期 | target delete、source delete、reparent、entity index reuse、parent delete | binding inactive/诊断、反向索引无悬挂、受影响节点重算 |
| Flex/Layout | anchor child 混合 Flex siblings、anchor 节点有 LayoutInfo 子树、clear anchor | 锚点节点不占 Flex slot；子树仍布局；清除回普通 Flex |
| 脏传播 | target move/resize、implicit size 改变、anchor API 改变、无关节点改变 | 依赖闭包更新；无关 root 不重算；同帧合并一次 |
| 坐标方向 | RTL、parent padding、负 offset、scale/rotation | 物理 line/逻辑映射明确；local coordinate；不发生 margin 冒充 scale |
| 回归 | 现有 `AnchorsTest.*`、完整 unit/interaction/screenshot、install consumer | 现有行为中未被兼容契约保留的差异必须显式更新测试 |

建议使用 property-based/fuzz 生成有限 anchor graph，验证：无环图拓扑结果满足所有有效约束；有环图不崩溃且输出 finite；删除任意节点后无悬挂 target。

## 7. 风险与验证建议

- **Yoga 边界风险**：Yoga 的 absolute/flex 结果不能直接当 Qt anchors；solver 必须拥有最终权威。先以 parent/sibling local geometry 测试锁定契约，再接入渲染。
- **implicit size 反馈风险**：文本测量可能改变目标尺寸并形成反馈环；限制为最多两轮稳定求解，未稳定时保留上轮声明/implicit 输入并发诊断，不读上一轮位置作为锚点结果。
- **生命周期风险**：EnTT entity index 复用可能误绑定；binding 必须带 entity generation 或通过 public handle/version 校验，并在销毁信号前清理反向索引。
- **兼容风险**：旧 `ParentAnchors` 只支持 parent edge/center；保留迁移适配层，但把 zero-center、clear snapshot 等错误行为改为新契约并更新测试。
- **RTL/transform 风险**：当前实现固定 `YGDirectionLTR`，且 Transform2D 不是布局组件；在没有坐标空间模型前不要宣称完全 Qt 等价。应先落地 local-space 语义，另设 transform 集成验收门。
- **性能风险**：依赖索引 + dirty closure，复杂度目标为 $O(V+E)$ 建图/拓扑，避免每帧扫描所有实体；需要基准覆盖大树和长 sibling 链。

验证顺序：先构建公共头自包含检查和锚点 solver 单元测试，再运行 `ui_ecs_tests` 锚点/布局回归，随后执行完整 Debug build、CTest interaction/screenshot/unit、install consumer；最后运行架构边界检查和 clang-tidy。

## 8. 待确认问题

1. `left/right/top/bottom` 是否作为物理线固定语义，还是项目希望同时提供 Qt 的 `start/end` 逻辑线？
2. baseline provider 首版是否仅支持 Text，还是允许公共 API 注册自定义 baseline？推荐首版仅 Text + 明确 provider 接口。
3. 对 cycle 是“设置时立即拒绝”还是“保留 unresolved 并在本帧回退”？推荐可静态检测的直接拒绝、运行时环保留诊断并禁用 SCC。
4. 当前动画 `Transform2D` 是否要纳入布局坐标空间？若无明确需求，P1 只承诺 local geometry，不把渲染 scale 纳入 anchors。
5. 是否接受对 `SetAnchors`/`ClearAnchors` 的旧快照恢复行为做破坏性修正？完整 Qt 语义要求接受，并以声明/implicit size 测试替代旧快照测试。
