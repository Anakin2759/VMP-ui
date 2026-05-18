# 渲染管线与性能策略

本文档描述 UI 渲染主链路，以及优化开销时应优先关注的节点。

## 渲染核心角色

1. RenderSystem
- 管线调度中心：在 `UpdateRendering` 阶段负责收集、排序、提交。

2. IRenderer
- `ui::core::IRenderer` 是渲染扩展接口；各类型渲染器（文本、形状、图标、滚动条等）把组件数据转为可提交几何。

3. BatchManager
- 合并满足条件的绘制请求，减少 Draw Call。

4. CommandBuffer
- 把批次转为 SDL GPU 提交命令。

5. RenderContext
- 递归遍历中传递位置、透明度、裁剪区等上下文。

## 每帧流程

当前常规帧由 `Application` 主循环单点消费 `QueuedTask -> InputTask -> RenderTask`，其中渲染相关阶段固定为 `UpdateLayout -> UpdateRendering -> EndFrame`。

1. Collection
- 从根实体遍历 UI 树。
- 继承并合并父节点上下文。
- 收集可渲染项到队列。

2. Sorting
- 以 Z 序和提交顺序稳定排序，确保遮挡关系正确。

3. Geometry
- 调用对应渲染器生成顶点/索引与材质参数。

4. Batching
- 按纹理、裁剪、常量等状态进行批次合并。

5. Execution
- 上传缓冲。
- 绑定管线和资源。
- 执行绘制并提交。

## 性能关注点

1. 合批命中率
- 若频繁切换纹理或裁剪区，会显著增加批次数。

2. 文本开销
- 文本栅格化与纹理缓存失效是高频热点。

3. 内存分配
- 每帧临时容器分配会放大抖动，优先复用和预分配。

4. 裁剪层级深度
- 深层嵌套裁剪会增加上下文计算与状态切换成本。

## 渲染改动准则

1. 新增渲染类型优先实现 `ui::core::IRenderer` 并接入 RenderSystem 的现有收集链路，不直接改写 RenderSystem 的主调度职责。
2. 对批处理规则的改动必须附带统计对比（批次数、顶点数、帧时）。
3. 文本相关改动必须考虑 DPI 和缓存键设计。
