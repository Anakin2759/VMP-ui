# BatchManager 说明文档

**文件**: src/ui/managers/BatchManager.hpp

## 概述

`BatchManager` 是 UI 渲染流水线中的批次组装与管理组件，负责在一帧内：

- 收集渲染顶点与索引，按渲染状态组织为 `RenderBatch`。
- 在可能情况下合并连续的批次（相同纹理、相同裁剪矩形并且 PushConstants 完全匹配），以减少绘制调用和状态切换。
- 提供批次列表供上层 `RenderSystem` 或 `CommandBuffer` 提取并提交到 GPU。

设计目标是减少帧内分配（使用 `std::pmr::monotonic_buffer_resource`）、降低纹理切换、并为后续优化（排序、合并、透明度处理）留出扩展点。

## 主要成员与接口

- 构造函数
  - `BatchManager()`：初始化 `m_bufferResource` 和 `m_batches`（使用 PMR 内存池）。

- 管理方法
  - `void clear()`：清空当前批次和已收集批次，释放 PMR 资源。
  - `void beginBatch(SDL_GPUTexture* texture, const std::optional<SDL_Rect>& scissor, const render::UiPushConstants& pushConstants)`：开始或尝试合并到当前批次（比较纹理、裁剪和 PushConstants）。若无法合并则先 `flushBatch()`。
  - `void addVertex(const render::Vertex& vertex)`：向当前批次添加顶点（如果存在）。
  - `void addIndex(uint16_t index)`：向当前批次添加索引（如果存在）。
  - `void addRect(const Eigen::Vector2f& pos, const Eigen::Vector2f& size, const Eigen::Vector4f& color, const Eigen::Vector2f& uvMin = {0,0}, const Eigen::Vector2f& uvMax = {1,1})`：便捷添加四边形（4 顶点 + 6 索引）。
  - `void flushBatch()`：将当前批次推入 `m_batches` 并重置当前批次。
  - `void optimize()`：触发批次优化（当前实现仅 flush 当前批次，未来可扩展排序/合并逻辑）。

- 查询方法
  - `const std::pmr::vector<render::RenderBatch>& getBatches() const`：获取已组装的批次列表。
  - `size_t getBatchCount() const`：批次数量。
  - `size_t getTotalVertexCount() const`：统计所有批次的顶点数。

## 实现要点

1. 内存管理
   - 使用 `std::pmr::monotonic_buffer_resource m_bufferResource` 来减少帧内分配开销，`m_batches` 与 `m_currentBatch` 都使用该资源进行容器内分配。
   - `clear()` 会调用 `m_bufferResource.release()` 并重建 `m_batches`，以避免保留对已释放内存的引用。

2. 批次合并策略
   - 合并条件（`beginBatch` 中）:
     - 相同纹理指针 `texture`。
     - 裁剪矩形 `scissor` 一致（包括有/无的对齐）。
     - `pushConstants` 字段逐项浮点近似比较（EPSILON = 0.001f），包括屏幕尺寸、矩形尺寸、四个圆角半径、阴影参数、透明度等关键字段。
   - 若任一条件不满足，则在新批次前先 `flushBatch()`，将当前批次存入 `m_batches`。

3. 添加矩形的索引计算
   - 顶点按顺序加入：左上、右上、右下、左下。
   - 索引构成两个三角形： (0,1,2) 和 (0,2,3)，索引以当前批次顶点数为基准偏移。

4. 安全与限制
   - `addVertex` / `addIndex` / `addRect` 若在无 `m_currentBatch` 时调用会直接返回，不做错误抛出。
   - `flushBatch()` 仅在当前批次不为空且包含顶点与索引时才会将其推入集合。

## 使用示例（伪代码）

```cpp
BatchManager bm;

bm.beginBatch(textureA, std::nullopt, pushConstsA);
bm.addRect({0,0}, {100,50}, colorA);
// 尝试开始一个可以合并的批次（相同 textureA 与 pushConstsA）
bm.beginBatch(textureA, std::nullopt, pushConstsA);
bm.addRect({100,0}, {100,50}, colorB);

// 不可合并的批次（例如不同 texture）会导致 flush
bm.beginBatch(textureB, std::nullopt, pushConstsB);
...

bm.flushBatch();
auto& batches = bm.getBatches();
// 将 batches 提交到 CommandBuffer / GPU
```

## 已知问题与 TODO

- 当前 `optimize()` 未实现完整优化流程，仅调用 `flushBatch()`；后续应实现：
  - 按纹理排序以减少切换。
  - 合并相邻相同纹理的批次（跨 frame 或同帧内碎片化合并）。
  - 透明对象的 Z-order 排序与分组。

- PushConstants 的比较采用逐字段浮点差值比较，成本较高且易受精度影响；建议：
  - 为 `UiPushConstants` 提供稳定的哈希或整型签名（例如量化后 memcmp 或使用 std::uint64_t 哈希），以便快速比较。

- PMR 资源使用注意:
  - `clear()` 中通过 `m_bufferResource.release()` 清理后立即重建 `m_batches` 是必要的，但如果跨帧长期保留大量数据，需注意内存增长策略与上限控制（例如防止单帧生成巨量顶点导致 OOM）。

- 缺少线程安全性保障：当前类假定在单一 UI 线程中使用；若计划在多线程中收集渲染命令，需要添加锁或采用线程局部批次汇总。

## 建议改进清单

- 实现 `optimize()`：
  - 首先按 `texture` 分组排序，再尝试合并可连续合并的批次。
  - 在合并前校验 scissor 与 pushConstants 一致性。

- 提升 PushConstants 比较效率：引入签名/哈希或使用整数量化以避免逐字段浮点比较。

- 增加单元测试：覆盖合并逻辑、索引计算与 PMR 清理行为。

- 考虑对 `addVertex/addIndex/addRect` 在无当前批次调用时记录断言或返回错误码，便于调试。

## 参考

- 源文件: `src/ui/managers/BatchManager.hpp`

---

文档生成时间: 2026-02-10
