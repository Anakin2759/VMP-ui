# CPU 软件渲染路径缺陷记录

> 版本：v1.0 · 日期：2026-05-25  
> 范围：`FallbackBackendRenderer`（SDL_Renderer 后端）下的圆角、图片、Canvas 渲染异常  
> 测试基线：GPU 路径（D3D12/Vulkan/Metal）正常；触发 `UI_FORCE_CPU_RENDER=ON` 或 GPU 初始化失败时复现

---

## 一、背景

GPU 渲染路径通过 HLSL SDF 着色器（`src/assets/shader/frag.hlsl`）在片元级别实时计算圆角、
阴影、描边、纹理采样。`Vertex` 结构体携带完整 SDF 参数（`rect_size / radius[4] / shadow_params[4] / mode_params[4]`），
着色器消费这些属性产出像素级精确效果。

`FallbackBackendRenderer`（`src/renderers/FallbackBackendRenderer.hpp`）使用 SDL_Renderer 
绕过 GPU 管线，当前实现的 `drawBatch()` 仅读取顶点 AABB + 首顶点颜色，调用 
`SDL_RenderFillRect()` 绘制纯色矩形。所有 SDF 参数均被静默丢弃。

---

## 二、问题清单

### BUG-1 — 圆角完全消失

**现象**：CPU 渲染路径下，所有设置了 `BorderRadius` 的控件（按钮、卡片、输入框等）
均以直角矩形渲染，圆角被忽略。

**根因**（[FallbackBackendRenderer.hpp:144–175](../src/renderers/FallbackBackendRenderer.hpp#L144)）：

```cpp
// 当前实现：只取 AABB，radius[4] 从未被读取
const float minX = std::min({ ... v[0].position[0], ... });
// ...
SDL_RenderFillRect(m_renderer, &rect);  // ← 纯矩形，丢弃 radius
```

`ShapeRenderer` 正确地将圆角写入顶点（`src/renderers/ShapeRenderer.hpp` 的 
`renderBackground()` 写 `vertex.radius[0-3]`），但 `FallbackBackendRenderer` 完全不消费这些数据。

**影响范围**：所有使用 `BorderRadius()` 链式操作的控件、`DropDownRenderer`、`ProgressBarRenderer` 圆头等。

---

### BUG-2 — 图片不显示（黑屏 / 完全透明）

**现象**：CPU 渲染路径下，`Image` 控件区域空白，图片内容不渲染。

**根因**（[FallbackBackendRenderer.hpp:130–133](../src/renderers/FallbackBackendRenderer.hpp#L130)）：

```cpp
void drawBatch(const render::RenderBatch& batch, SDL_GPUTexture* whiteTextureTag) override
{
    // ...
    if (batch.texture != nullptr && batch.texture != whiteTextureTag)
    {
        return;  // ← 图片纹理（SDL_GPUTexture*）不等于 whiteTextureTag，直接丢弃整批
    }
    // ...
}
```

`ImageRenderer` 调用 `ImageManager::loadTexture()` 拿到 `SDL_GPUTexture*` 并存入 
`Image::textureId`（[ImageRenderer.hpp:58–66](../src/renderers/ImageRenderer.hpp#L58)）。
但 SDL_Renderer 无法直接消费 SDL_GPUTexture，`FallbackBackendRenderer` 对此类批次
以静默 `return` 跳过，无任何警告日志。

**次级问题**：`ImageManager::loadTexture()` 在 CPU fallback 下仍尝试走 GPU 纹理上传路径；
若 GPU 设备为 `nullptr`（纯软件降级），`loadTexture()` 将返回错误，触发 
`ImageSource::loadFailed = true`，图片永久标记失败，后续帧不再重试。

---

### BUG-3 — Canvas 圆形退化为矩形

**现象**：CPU 渲染路径下，`DrawCircle` / `FillCircle` 绘制结果为正方形包围盒，
而非圆形。

**根因**（[CanvasRenderer.hpp:162–185](../src/renderers/CanvasRenderer.hpp#L162)）：

```cpp
// renderFilledCircle — 依赖 SDF 参数
pushConst.radius[0] = radius;   // 写入 pushConst.radius（PushConstants 层），
pushConst.draw_mode = 0.0F;     // 非 Vertex 层！
context.batchManager->beginBatch(context.whiteTexture, ...);
context.batchManager->addRect(...);  // 最终生成一个正方形批次
```

`pushConst.radius` 属于 `UiPushConstants`（批次级），**不是** `Vertex::radius[4]`（顶点级）。
GPU 路径中着色器直接读取 push constants 中的 `radius` 和 `draw_mode` 实现 SDF 圆形。
`FallbackBackendRenderer::drawBatch()` 不读取 `batch.pushConstants.radius`，
因此圆形退化为同尺寸矩形。

同理，`renderCircleOutline` 中的 `draw_mode = 1.0F`（描边模式）也被忽略。

**影响范围**：`CanvasRenderer` 的 `FILLED_CIRCLE` 和 `CIRCLE` 命令；
`CubicBezier` 中的端点标记圆若有类似路径也受影响。

---

### BUG-4 — Canvas 抗锯齿缺失（轻微）

**现象**：CPU 路径下线条、折线均为矩形块状，斜线出现明显锯齿。

**根因**：`renderLine` / `renderSegment` 用矩形近似有角度的线段，
无次像素精度，也无 SDL_RenderGeometry 的三角形插值。此为 Fallback 
设计的固有限制，不构成功能性错误，但影响视觉质量。

---

## 三、现有保护（未受影响的部分）

| 模块 | 保护措施 | 位置 |
|------|----------|------|
| 文本渲染 | `TextRenderer` 检查 `isUsingFallbackBackend()`，改用 `drawCachedBitmap()` | `TextRenderer.hpp:154-157` |
| TextTextureCache | `device == nullptr` 提前返回 `nullptr`，不触发 GPU API | `TextTextureCache.hpp:94-96` |
| FallbackBackendRenderer 初始化 | `m_renderer == nullptr` 判断 | `FallbackBackendRenderer.hpp:127` |

---

## 四、修复方案

### 方案 A — ShapeRenderer 圆角（BUG-1）

**核心思路**：CPU fallback 路径中，用多边形近似替代 SDF 矩形。

#### A1. `FallbackBackendRenderer` 增加软件圆角矩形方法

在 `drawBatch()` 内读取 `batch.vertices[i].radius[0]`（左上角半径代表，取均值兜底），
若半径 > 0，调用新增的 `renderRoundedRect()` 方法；若半径为 0，保持原有 `SDL_RenderFillRect`。

```cpp
// FallbackBackendRenderer.hpp — 新增私有方法
static void renderRoundedRect(SDL_Renderer* renderer,
                               const SDL_FRect& rect,
                               float radius,
                               SDL_FColor color);
```

`renderRoundedRect` 实现：
1. 中心矩形 + 左/右/上/下四条边延伸矩形（共 5 个 `SDL_RenderFillRect`）
2. 四个角用 `CORNER_SEGMENTS`（建议 8）段三角形扇形近似，调用 `SDL_RenderGeometry`

**改动文件**：`src/renderers/FallbackBackendRenderer.hpp`  
**估计工作量**：半天

#### A2. `drawBatch()` 读取顶点 radius

```cpp
// drawBatch() 内，取四个顶点 radius[0] 均值作为代表圆角
const float r = 0.25F * (v[0].radius[0] + v[1].radius[1] + v[2].radius[2] + v[3].radius[3]);
if (r > 0.5F) {
    renderRoundedRect(m_renderer, rect, r, ...);
} else {
    SDL_RenderFillRect(m_renderer, &rect);
}
```

> **注意**：`Vertex::radius` 存储的是四角各自的半径（左上/右上/右下/左下）；
> CPU fallback 可采用"四角取最大值"或"全部使用 [0]"的简化策略。

---

### 方案 B — 图片显示（BUG-2）

**核心思路**：绕开 SDL_GPUTexture，让 `ImageManager` 在 CPU 模式下提供像素数据，
复用 `drawCachedBitmap()` 链路（与文本渲染路径一致）。

#### B1. `ImageManager` 新增 `loadPixels()` 方法

```cpp
// ImageManager.hpp — 新增接口
struct PixelBuffer {
    std::vector<uint8_t> rgba;
    int width = 0;
    int height = 0;
};
Result<PixelBuffer> loadPixels(std::string_view path);
```

实现：复用 stb_image 解码逻辑，不调用任何 GPU API，直接返回 RGBA 字节缓冲。

#### B2. `ImageRenderer` 检测 fallback 并走 bitmap 路径

```cpp
// ImageRenderer.cpp::collect()
if (context.backendRenderer->getType() == BackendType::FALLBACK) {
    // CPU 路径：加载像素数据，通过 drawCachedBitmap 绘制
    auto pixResult = context.imageManager->loadPixels(src->path);
    if (pixResult) {
        context.backendRenderer->drawCachedBitmap(
            src->path,                     // 以 path 为 cacheKey
            { pixResult->rgba.data(), pixResult->rgba.size() },
            pixResult->width, pixResult->height,
            dstRect, context.currentScissor, alphaMod);
    }
} else {
    // GPU 路径（现有逻辑不变）
    // ...
}
```

`drawCachedBitmap` 已在 `FallbackBackendRenderer` 中实现，有完整的 
`SDL_CreateTexture / SDL_UpdateTexture` 缓存逻辑（[FallbackBackendRenderer.hpp:183–220](../src/renderers/FallbackBackendRenderer.hpp#L183)）。

**注意事项**：
- `loadPixels()` 与 `loadTexture()` 共用 stb_image 解码，建议抽取 `decodeFile()` 私有方法复用
- `FallbackBackendRenderer::getOrCreateBitmapTexture()` 已有宽高变化检测，无需额外处理
- `ImageSource::loadFailed` 标记需区分"GPU 失败"与"像素数据失败"，建议加 `cpuLoadFailed` 字段

**改动文件**：`src/managers/ImageManager.hpp/.cpp`、`src/renderers/ImageRenderer.hpp`  
**估计工作量**：1 天

---

### 方案 C — Canvas 圆形（BUG-3）

**核心思路**：`CanvasRenderer` 检测 fallback 模式，圆形/圆弧改用多边形近似。

#### C1. `renderFilledCircle` 增加 fallback 分支

```cpp
static void renderFilledCircle(const components::CanvasDrawCommand& cmd,
                                const Eigen::Vector2f& origin,
                                core::RenderContext& context)
{
    if (context.backendRenderer != nullptr &&
        context.backendRenderer->getType() == BackendType::FALLBACK)
    {
        // CPU fallback：三角扇形（CIRCLE_SEGMENTS 段）
        renderFilledCircleFallback(cmd, origin, context);
        return;
    }
    // GPU 路径（SDF batch，现有代码不变）
    // ...
}
```

`renderFilledCircleFallback` 实现：将圆分为 `CIRCLE_SEGMENTS`（24）个扇形三角形，
每个三角形用 `addRect()` 近似（或引入 `addTriangle()` 批次方法，精度更高）。

> **简单可行的近似**：24 段细分，每段用一个细矩形（宽 = 弦长，高 = 线宽）拼接圆弧；
> 视觉上在 CPU fallback 场景下是可接受的质量。

#### C2. `renderCircleOutline` 同步修改

同 C1，`draw_mode=1.0F` 在 CPU 路径中无效，用分段矩形近似描边圆弧。

**改动文件**：`src/renderers/CanvasRenderer.hpp`（新增 fallback 分支）  
**估计工作量**：半天

---

## 五、实施优先级

| 优先级 | Bug | 方案 | 理由 |
|--------|-----|------|------|
| P1 | BUG-2 图片不显示 | 方案 B | 功能性缺失，图片控件在 CPU 路径下完全无效 |
| P1 | BUG-1 圆角消失 | 方案 A | 所有圆角控件视觉异常，影响可用性 |
| P2 | BUG-3 Canvas 圆形退化 | 方案 C | Canvas 是可选功能，退化为矩形不影响基本使用 |
| P3 | BUG-4 锯齿 | — | 固有限制，Fallback 场景可接受 |

---

## 六、测试建议

1. **复现环境**：设置 CMake 选项 `-DUI_FORCE_CPU_RENDER=ON` 编译，或在 GPU 初始化后手动调用
   `DeviceManager::claimWindow()` 触发 Fallback 降级。
2. **验收标准**：
   - BUG-1：`example/ui_demo` 中所有按钮在 CPU 模式下显示圆角（与 GPU 模式视觉近似）
   - BUG-2：Image 控件在 CPU 模式下正确显示图片内容，不再黑屏
   - BUG-3：Canvas `FillCircle` 在 CPU 模式下渲染近似圆形，不退化为正方形
3. **回归保护**：修复后在 `tests/unittest/` 增加 `test_FallbackRenderer.cpp`，
   mock `FallbackBackendRenderer` 验证三类批次的处理路径。

---

## 七、不受本次修复影响的部分

| 模块 | 原因 |
|------|------|
| 文本渲染 | 已有 `isUsingFallbackBackend()` 分支，走 `drawCachedBitmap()` |
| 纯色矩形背景（无圆角） | `drawBatch()` 现有逻辑正确 |
| ScrollBar 轨道 | 无圆角，不受 BUG-1 影响 |
| ProgressBar 矩形部分 | 同上 |
| 阴影效果 | Fallback 路径预期不支持阴影，非 bug |
