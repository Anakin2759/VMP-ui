# 扩展指南与演进路线

本文档给出"怎么加功能不把架构弄乱"的最小流程，以及接下来一个阶段的优先级。

## 新增控件的最小流程

1. 定义数据组件
- 只放数据，不放行为。

2. 工厂层创建默认组件
- 在 `factory` 中提供可预测默认值。

3. 行为接入
- 由 Interaction/State/Action 链路消费事件并更新组件；新增路径优先复用 `RuntimeFacade` 暴露的上下文和事件总线访问。

4. 布局接入
- 需要参与容器布局时，补充 LayoutSystem 处理；当前 LayoutSystem 的非模板实现已收口到 `.cpp`，新增行为优先放实现文件侧。

5. 渲染接入
- 新增或扩展 `ui::core::IRenderer` 实现。

6. 补测试
- 至少包含一个集成测试，覆盖从输入到可视结果的主路径。

## 新增系统的准入规则

1. 先写职责声明：输入、输出、禁止做什么。
2. 只订阅必须事件，避免"万能系统"。
3. 不跨层写数据：状态系统不直接做渲染决策。
4. 若需要访问运行时上下文、窗口映射或事件总线，优先走 `RuntimeFacade`，不要再把静态入口写成新的默认示例。

## 默认值策略

1. 工厂默认值必须"可解释 + 可测试"。
2. 默认对齐、滚动、尺寸策略改动必须同步更新测试。
3. 破坏性默认值变更需记录迁移说明。

## 近两期路线图

### P1（稳定性优先）

1. 输入链路时序稳定性提升。
2. 关键系统边界约束继续落地，尤其是 RuntimeFacade 入口和常规帧路径说明的一致化。
3. **渲染正确性兜底**（详见 [渲染评审 §3.6](./RENDERING_ARCHITECTURE_REVIEW.md)）：`TextTextureCache` 缓存键补 color 维度，消除同文本不同色错命中。
4. **首帧卡顿消除**（[渲染评审 §3.4](./RENDERING_ARCHITECTURE_REVIEW.md)）：`ImageRenderer::collect()` 内的同步 `loadTexture()` 改为异步预热 + 缓存只读。

### P2（扩展性优先）

1. 多窗口运行时隔离强化。
2. 文本与图标缓存策略再优化（含 `FontManager` 多字号缓存，[渲染评审 §3.7](./RENDERING_ARCHITECTURE_REVIEW.md)）。
3. 渲染统计可视化与性能回归基线。
4. **合批率提升（高收益项）**（[渲染评审 §3.3](./RENDERING_ARCHITECTURE_REVIEW.md)）：把 `rect_size`、`radius`、`opacity` 等逐实体参数从 Push Constants 下沉到顶点属性，目标 Shape Draw Call 从 O(N) 降至接近 O(1)。
5. **`canHandle` 改 EnTT View/Group 预分组**（[渲染评审 §3.1](./RENDERING_ARCHITECTURE_REVIEW.md)）：消除每帧 N×R 次 ECS 查询。

### P3（演进型，可延后）

1. 脏子树增量 collect（[渲染评审 §3.2](./RENDERING_ARCHITECTURE_REVIEW.md)），将 `RenderDirtyTag` 粒度从窗口下沉到实体子树。
2. Canvas 矢量图形委托给 ThorVG（仓库已含 `third_party/thorvg`，[渲染评审 §3.5](./RENDERING_ARCHITECTURE_REVIEW.md)）；过渡期先用现有 SDF 管线绘制 Circle/Line。
3. `collectRenderData` 改显式栈迭代，并加视口剪枝（[渲染评审 §3.8](./RENDERING_ARCHITECTURE_REVIEW.md)）。

### 新增工程化建议

1. **静态分析常态化**：clangd / clang-tidy 已对外暴露大量隐式耦合与未使用代码。CI 加 `-DENABLE_CLANG_TIDY=ON` 校验，新增代码不允许新增警告。命名规则（`UPPER_CASE` 枚举、`camelBack` 局部常量、最小标识符长度 3）以 `.clang-tidy` 为准。
2. **clangd 头文件可解析性**：`include/ui.hpp` 这类聚合头无编译单元，`.clangd` 已增加 `PathMatch` 兜底；新增聚合头时同步补充编译标志，避免 IDE 报"找不到头"。
3. **第三方 CMake 警告隔离**：`CMakeLists.txt` 已用 `CMAKE_WARN_DEPRECATED OFF` 包裹 eigen 子目录；后续接入其他可能触发 CMP0xxx 的依赖时，沿用同一模板，不要全局关闭。
4. **测试覆盖跟随评审落地**：每条 P1/P2 优化条目落地必须附"批次数 / Draw Call / 帧时"三项基线对比，写入 `docs/test-reports/` 同日报告中。

## 不做事项（当前阶段）

1. 不做一次性去全局静态入口的大重构。
2. 不引入复杂资源管线改造阻塞主线。
3. 不接受没有测试的跨系统行为改动。
4. 不在未量化基线的前提下重写渲染管线；任何"性能改进"必须先有 baseline。
