# 渲染子系统架构锐评与优化建议

> 审查日期：2026-05-18  
> 审查范围：`src/renderers/`、`src/systems/RenderSystem.cpp`、`src/managers/`、`src/core/`

---

## 一、现有架构概览

```
实体遍历（collectRenderData 递归）
    ↓
每实体逐一询问所有渲染器 canHandle()
    ↓
RenderItem 写入 m_renderQueue（带 sortKey = Z-Order << 32 | submissionIndex）
    ↓
std::sort 全量排序
    ↓
逐项调用 renderer->collect()  →  BatchManager::beginBatch() / addRect()
    ↓
BatchManager::optimize() 合批
    ↓
CommandBuffer::execute()  →  SDL_GPU Draw Call
```

12 个渲染器，优先级 0–30，脏标记驱动跳帧（`RenderDirtyTag` 为空时整帧跳过）。

---

## 二、做得好的地方

| 亮点 | 说明 |
|------|------|
| **脏标记跳帧** | `RenderDirtyTag` 为空直接 `return`，静态 UI 零开销 |
| **Z-Order 稳定排序** | sortKey 高 32 位编码 Z-Order，低 32 位保持提交顺序，保证同层级实体顺序稳定 |
| **双帧在途** | `MAX_FRAMES_IN_FLIGHT = 2`，CPU 与 GPU 流水线重叠，减少等待 |
| **合批机制** | 相同纹理 + 裁剪 + 推送常量才合并，原则正确 |
| **TextTextureCache LRU** | 最多 256 条缓存，避免每帧重新上传文本纹理 |
| **ImageManager 缓存** | 路径级缓存，防止重复上传同一图片到 GPU |
| **SDF 着色器** | `draw_mode` 区分纯色矩形 / 纹理采样 / 描边，减少管线切换 |

---

## 三、问题与锐评

### 3.1 ⚠️ `canHandle()` O(N×R) 线性扫描——最大热点

```
每帧每实体调用 12 次 canHandle()
```

`canHandle()` 内部调用 `Registry::AnyOf<...>()` 做 ECS 组件检测，成本低但乘以实体数后积累明显。当 UI 树有数百实体时，每帧约有 **数千次** ECS 查询仅用于分发。

**建议**：换为 EnTT **Group/View 预分组**。为每个渲染器维护一个 `entt::view` 或 `entt::group`，`collect()` 时只遍历真正匹配的实体，避免全量扫描。

```cpp
// 现状
for (auto& renderer : m_renderers) {
    if (renderer->canHandle(entity)) { ... }  // 每实体 × 12 次查询
}

// 优化方向
// ShapeRenderer 自持视图
auto shapeView = Registry::View<components::Background>();  // 预缓存
for (auto entity : shapeView) { /* 直接处理，跳过 canHandle */ }
```

---

### 3.2 ⚠️ `RenderDirtyTag` 粒度过粗——整帧全量重绘

当前 `RenderDirtyTag` 标在 **窗口实体** 上，触发整个窗口的全量重绘。哪怕只有一个按钮 Hover 状态改变，整棵 UI 树也会重新 collect。

**建议**：引入 **Dirty Rect / Dirty Region** 机制（短期可先做"只 collect 脏子树"）：

```
InteractionSystem 触发 Hover → MarkDirty(entity) → 
RenderSystem 只 re-collect entity 及其子树 → 其余实体复用上帧 RenderItems
```

即便仅实现"记录哪些实体脏了，只重新 collect 这些"也能大幅降低 CPU 侧 collect 开销。

---

### 3.3 ⚠️ `BatchManager` 合批条件过于严格——大量 Draw Call

当前合批需要**推送常量完全一致**（包括 `rect_size`、`radius` 等逐实体参数）。这意味着几乎每个有圆角背景的元素都会单独开一个批次，实际合批率极低。

**根本原因**：SDF 圆角参数（`rect_size`、`radius`）作为 Push Constants 传入，而不是作为顶点属性。这导致不同大小的圆角矩形必须分批。

**建议**：将 `rect_size`、`radius`、`opacity` 等**逐实体参数下沉到顶点属性**（Per-Vertex Data），只保留真正全局的参数（`screen_size`）在 Push Constants 里。这样同一着色器的所有 Shape 可以合入一个 Draw Call。

```glsl
// 现状：Push Constants 包含逐实体数据（无法合批）
layout(push_constant) uniform UiPushConstants {
    vec2 screen_size;
    vec2 rect_size;   // ← 逐实体，导致无法合批
    vec4 radius;      // ← 逐实体，导致无法合批
    float opacity;
};

// 优化方向：逐实体数据移入顶点属性
layout(location = 2) in vec2 in_rect_size;
layout(location = 3) in vec4 in_radius;
layout(location = 4) in float in_opacity;
```

这是**影响最大的单点优化**，可以将 Shape 类 Draw Call 从 O(N) 降至接近 O(1)。

---

### 3.4 ⚠️ `ImageRenderer` 渲染帧内懒加载——卡顿风险

```cpp
// ImageRenderer::collect() 内部——在渲染帧中做 GPU Upload
SDL_GPUTexture* tex = managers::ImageManager::instance().loadTexture(src->path, device);
```

`loadTexture()` 内部含 stb_image 解码 + SDL_GPUTransferBuffer 上传。这在渲染帧的 collect 阶段（CPU 关键路径）同步执行，会造成可见卡顿。

**建议**：把图片加载移到异步预加载阶段（`ThreadPool` 已存在于 `utils/`），collect 阶段只查缓存：

```cpp
// 异步预热（在创建 Image 实体时触发）
ThreadPool::Submit([path, device] {
    managers::ImageManager::instance().preload(path, device);
});

// collect 阶段只读缓存，不阻塞
if (!src->loaded) return;  // 首帧跳过，异步加载中
```

---

### 3.5 ⚠️ `CanvasRenderer` 用矩形近似圆/线——精度差且顶点多

- **Circle**：24 段折线 → 每段一个矩形 → 24 次 `addRect()` = 96 个顶点
- **Line**：扩展为细矩形 → 存在端点拼接缝隙

**建议**：
1. **短期**：Circle/Line 通过 SDF 在片元着色器内绘制（已有 SDF 管线，复用即可），消除分段近似误差
2. **长期**：引入 ThorVG（项目 `third_party/thorvg` 已存在）处理矢量图形，Canvas 命令直接委托给 ThorVG 光栅化

---

### 3.6 ⚠️ `TextTextureCache` key 缺少 `color` 维度——潜在渲染错误

```cpp
// 当前 key 构建（推测）
buildCacheKey(text, fontSize) → "Helloworld_240"
```

如果相同文本内容以不同颜色显示（例如 Hover 变色的按钮），会命中同一缓存 key，返回错误颜色的纹理。

**建议**：key 加入颜色哈希：

```cpp
std::string buildCacheKey(const std::string& text, float fontSize, const Eigen::Vector4f& color) {
    return fmt::format("{}_{:.0f}_{:02X}{:02X}{:02X}{:02X}",
        text, fontSize,
        static_cast<int>(color.x() * 255),
        static_cast<int>(color.y() * 255),
        static_cast<int>(color.z() * 255),
        static_cast<int>(color.w() * 255));
}
```

---

### 3.7 💡 `FontManager` 单字体 / 单尺寸——可扩展性瓶颈

当前 `FontManager` 只持有一个 `FT_Face` 和一个 `m_fontSize`，多字号渲染每次调用 `FT_Set_Pixel_Sizes()` 切换，HarfBuzz font 对象也是单个。

**建议**：维护 `std::unordered_map<float, FaceCache>` 按字号缓存 FreeType + HarfBuzz 实例，避免频繁切换。

---

### 3.8 💡 `collectRenderData` 递归深度——大 UI 树的栈风险

深层 Hierarchy 树（如嵌套 ScrollArea + Table + 多行）会产生较深递归。当前无深度保护。

**建议**：改为显式栈（`std::stack<ContextFrame>`）迭代遍历，消除栈溢出风险，同时更便于添加剪枝（视口外实体跳过 collect）。

---

## 四、优化优先级汇总

| 优先级 | 问题 | 改动量 | 收益 |
|--------|------|--------|------|
| 🔴 P0 | 逐实体参数下沉顶点属性（3.3） | 大（需改 HLSL + BatchManager） | Draw Call 从 O(N) → O(1) |
| 🔴 P0 | canHandle 改为 View 预分组（3.1） | 中 | 消除每帧 12×N 次 ECS 查询 |
| 🟠 P1 | 脏子树增量 collect（3.2） | 大 | 静态区域零 collect 开销 |
| 🟠 P1 | 图片异步预加载（3.4） | 小 | 消除首帧加载卡顿 |
| 🟡 P2 | TextTextureCache key 加 color（3.6） | 小 | 修正潜在渲染错误 |
| 🟡 P2 | Circle/Line SDF 化（3.5） | 中 | 减少顶点数，提升精度 |
| 🟢 P3 | FontManager 多字号缓存（3.7） | 小 | 多字号文本性能 |
| 🟢 P3 | collectRenderData 改迭代栈（3.8） | 小 | 大 UI 树安全性 |

---

## 五、结论

当前架构**整体方向正确**（ECS + 批渲染 + 脏标记跳帧），核心短板集中在：

1. **合批率低**——Push Constants 携带逐实体数据是根本原因，解决后 GPU 侧性能可有数量级提升
2. **全量 collect 无剪枝**——每帧无论脏的范围多小都重遍历整棵树，随 UI 复杂度线性劣化
3. **两处正确性风险**——文本缓存 key 缺色彩维度、渲染帧内同步 GPU Upload

建议按 P0 → P1 顺序推进，P0 两项投入产出比最高。
