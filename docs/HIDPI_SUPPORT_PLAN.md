# PmkUi 高分屏（HiDPI）支持方案

> 文档版本：1.0  
> 日期：2026-05-24  
> 状态：待实现

---

## 一、问题诊断

### 1.1 坐标空间不一致（核心 Bug）

当前渲染管线存在贯穿全流程的坐标空间错位：

| 环节 | 使用的坐标空间 |
|------|--------------|
| Yoga 布局计算 → `components::Position/Size` | 逻辑像素 |
| 各 Renderer 构造顶点坐标 | 逻辑像素 |
| `RenderFrame.cpp` → `context.screenWidth/Height` → shader `screen_size` push constant | **物理像素** ← 错误 |
| `CommandBuffer` 裁剪矩形（scissor） | 逻辑像素（传入 GPU 时未缩放）← 错误 |
| Win32 `WM_NCHITTEST` lParam 坐标 | 物理屏幕像素 |
| `PlatformWindow.cpp` `borderWidth` | 逻辑像素 ← 错误 |

顶点着色器（`src/assets/shader/vert.hlsl`）的 NDC 映射：

```hlsl
float2 ndc = float2(
    (input.position.x / screen_size.x) * 2.0f - 1.0f,
    1.0f - (input.position.y / screen_size.y) * 2.0f
);
```

**当 DPI=2× 时的实际效果**：

- 顶点 `position` = 逻辑像素（如 400）
- `screen_size` = 物理像素（如 1600）
- 实际 NDC ≠ 预期 NDC，UI 元素被压缩至窗口左上角 1/4 区域

### 1.2 高分屏模式未激活

`Factory.cpp` 的 `SDL_CreateWindow()` 调用缺少 `SDL_WINDOW_HIGH_PIXEL_DENSITY` flag。
未设置此 flag 时 SDL3 不请求原生分辨率，`SDL_GetWindowSizeInPixels == SDL_GetWindowSize`，
高分屏完全失效（由操作系统做低质量放大）。

### 1.3 字体过采样固定值

`FontManager::TEXT_OVERSAMPLE_SCALE = 2.0F` 硬编码，无法适应非 2× DPI 比例（如 1.25×、1.5×）。

### 1.4 图标尺寸不感知 DPI

`IconManager::quantizeSize()` 以逻辑尺寸查找图标，高分屏下实际渲染尺寸是逻辑尺寸的 N 倍，
应以物理尺寸请求更大的图标素材以获得清晰度。

---

## 二、不受影响的部分（无需改动）

| 模块 | 原因 |
|------|------|
| **Yoga 布局系统** | 已在纯逻辑像素空间工作 |
| **HitTestSystem** | SDL3 鼠标事件默认为逻辑坐标，与 Yoga 匹配 |
| **frag.hlsl SDF 着色器** | `fwidth(dist)` 基于物理像素导数，高分屏下自动获得更细腻抗锯齿 |
| **WindowSync.hpp** | 已使用 `SDL_GetWindowSize`（逻辑），正确 |
| **HitTestSystem** | 已使用逻辑坐标比较，正确 |

---

## 三、修复方案

### P0 — 激活高分屏模式（前置条件）

**涉及文件**
- `src/core/Application.cpp`
- `src/api/Factory.cpp`

#### Application.cpp — 在 SDL_Init 前设置 DPI 感知提示

```cpp
// 在 SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) 之前插入：
SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2");
// 禁用 SDL 自身的逻辑缩放，让应用完全自主处理 DPI
SDL_SetHint(SDL_HINT_WINDOWS_DPI_SCALING, "0");
```

> `permonitorv2` 使进程在每个显示器上独立感知 DPI，是 Windows 10+ 推荐模式。

#### Factory.cpp — CreateSdlWindowOrRollback() 窗口创建 flags

```cpp
// 修改前（约第 57 行）：
SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN;

// 修改后：
SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN
                      | SDL_WINDOW_HIGH_PIXEL_DENSITY;
```

---

### P1 — 修复 shader screen_size 坐标空间（核心修复）

**涉及文件**
- `src/core/RenderContext.hpp`
- `src/systems/render/RenderFrame.cpp`

#### RenderContext.hpp — 新增 dpiScale 字段

```cpp
struct RenderContext
{
    // ... 已有字段 ...
    float screenWidth  = 0.0F;
    float screenHeight = 0.0F;
    float dpiScale     = 1.0F;  // 新增：物理像素 / 逻辑像素比值
    // ...
};
```

#### RenderFrame.cpp — 使用逻辑像素填充 screen_size

```cpp
// 物理像素：给 GPU 视口（CommandBuffer::execute）使用
int width = 0, height = 0;
SDL_GetWindowSizeInPixels(sdlWindow, &width, &height);
if (width <= 0 || height <= 0) continue;

// 逻辑像素：给 shader screen_size 使用
int logW = 0, logH = 0;
SDL_GetWindowSize(sdlWindow, &logW, &logH);
if (logW <= 0 || logH <= 0) { logW = width; logH = height; }  // 降级保护

float const dpiScale = static_cast<float>(width) / static_cast<float>(logW);

m_screenWidth  = static_cast<float>(logW);   // 逻辑像素
m_screenHeight = static_cast<float>(logH);

// 构建 rootContext（改动部分）：
rootContext.screenWidth  = m_screenWidth;
rootContext.screenHeight = m_screenHeight;
rootContext.dpiScale     = dpiScale;          // 新增

// CommandBuffer::execute 仍然传物理像素（用于 GPU 视口）
m_commandBuffer->execute(sdlWindow, width, height, dpiScale, batches);
```

**原理**：
- 顶点坐标（逻辑像素） ÷ `screen_size`（逻辑像素） = 正确 NDC
- GPU 视口（物理像素） 将 NDC 映射到完整物理分辨率
- DPI 缩放在 NDC → 物理像素的视口变换中自动完成，无需修改着色器

---

### P2 — 修复裁剪矩形坐标空间

**涉及文件**
- `src/managers/CommandBuffer.hpp`

`batch.scissorRect` 存储的是逻辑像素坐标，但 `SDL_SetGPUScissor` 接受物理像素。

#### CommandBuffer.hpp — execute() 接收 dpiScale 并缩放 scissor

```cpp
// execute() 签名变更：
void execute(SDL_Window* window, int physWidth, int physHeight,
             float dpiScale,  // 新增参数
             const std::pmr::vector<render::RenderBatch>& batches)

// recordRenderPass() 同步增加 dpiScale 参数，
// 在设置裁剪矩形时缩放：
auto scaleScissor = [dpiScale](SDL_Rect r) -> SDL_Rect {
    return {
        static_cast<int>(std::floor(r.x * dpiScale)),
        static_cast<int>(std::floor(r.y * dpiScale)),
        static_cast<int>(std::ceil(r.w  * dpiScale)),
        static_cast<int>(std::ceil(r.h  * dpiScale))
    };
};

if (batch.scissorRect.has_value()) {
    SDL_Rect r = scaleScissor(batch.scissorRect.value());
    SDL_SetGPUScissor(renderPass, &r);
} else {
    SDL_Rect full = {0, 0, physWidth, physHeight};
    SDL_SetGPUScissor(renderPass, &full);
}
```

> 使用 `floor` 扩大裁剪区起点、`ceil` 扩大尺寸，避免物理像素舍入导致边缘像素被意外裁掉。

---

### P3 — 修复 Win32 无边框窗口缩放感应区

**涉及文件**
- `src/core/PlatformWindow.cpp`

Win32 `WM_NCHITTEST` 的 lParam 坐标是物理屏幕像素（DPI 感知进程），但 `borderWidth` 存储逻辑像素，
导致高分屏下拖拽缩放感应区实际只有正常宽度的 `1/dpiScale`，极难触发。

```cpp
// HandleNcHitTest() 中：
int const border = (data != nullptr) ? data->borderWidth : 6;

// 新增：用 Win32 API 获取当前 DPI，将逻辑 border 转为物理像素
UINT const dpi = GetDpiForWindow(hWnd);
int  const scaledBorder = border * static_cast<int>(dpi) / 96;

// 以下所有 border 比较改用 scaledBorder：
bool const onLeft   = (cursorX >= windowRect.left && cursorX < windowRect.left + scaledBorder);
bool const onRight  = (cursorX < windowRect.right  && cursorX >= windowRect.right  - scaledBorder);
bool const onTop    = (cursorY >= windowRect.top    && cursorY < windowRect.top    + scaledBorder);
bool const onBottom = (cursorY < windowRect.bottom  && cursorY >= windowRect.bottom - scaledBorder);
```

---

### P4 — 字体过采样 DPI 感知（推荐优化）

**涉及文件**
- `src/managers/FontManager.hpp`
- `src/systems/PlatformWindowSystem.hpp`（或事件处理侧）

#### FontManager — 新增 setDpiScale() / 动态过采样

```cpp
class FontManager
{
public:
    void setDpiScale(float scale) noexcept
    {
        if (m_dpiScale == scale) return;
        m_dpiScale = scale;
        clearGlyphCache();  // DPI 变化后失效所有缓存字形
    }

private:
    float m_dpiScale = 1.0F;

    // 光栅化分辨率 = requestedSize * max(TEXT_OVERSAMPLE_SCALE, m_dpiScale)
    // 当前 TEXT_OVERSAMPLE_SCALE = 2.0F，在 2× DPI 时仍为 2×，无额外开销
    // 在 3× DPI（如部分笔记本）时会提升到 3×，保证清晰度
    static constexpr float TEXT_OVERSAMPLE_SCALE = 1.0F;  // 基准值改为 1，由 dpiScale 主导
};
```

#### PlatformWindowSystem — 响应 DPI 变化事件

```cpp
// 现有代码已处理 SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED → events::WindowPixelSizeChanged
// 在该事件的处理器中，额外更新 FontManager 的 DPI scale：
void onWindowPixelSizeChanged(const events::WindowPixelSizeChanged& ev)
{
    // ... 现有逻辑 ...
    if (SDL_Window* win = SDL_GetWindowFromID(ev.windowID))
    {
        int logW = 0, logH = 0;
        SDL_GetWindowSize(win, &logW, &logH);
        if (logW > 0 && ev.width > 0)
        {
            float newScale = static_cast<float>(ev.width) / static_cast<float>(logW);
            m_fontManager->setDpiScale(newScale);
        }
    }
}
```

---

### P5 — 图标尺寸 DPI 感知（可选优化）

**涉及文件**
- `src/managers/IconManager.hpp`（或调用 `quantizeSize` 的 Renderer 处）

在请求图标时，以**物理像素尺寸**作为查询参数，渲染时目标矩形仍保持逻辑像素
（修正后的 `screen_size` 会将逻辑尺寸正确映射到物理分辨率）：

```cpp
// Renderer 中请求图标前：
float const physicalIconSize = logicalIconSize * context.dpiScale;
auto* icon = context.imageManager->getIcon(quantizeSize(physicalIconSize));
// 渲染目标矩形仍用 logicalIconSize（交由 shader 完成缩放）
```

---

## 四、改动文件汇总

| 文件 | 优先级 | 改动性质 | 风险评估 |
|------|--------|---------|---------|
| `src/core/Application.cpp` | P0 | 新增 SDL Hints（2 行） | 低 |
| `src/api/Factory.cpp` | P0 | flags 加 HIGH\_PIXEL\_DENSITY | 低 |
| `src/core/RenderContext.hpp` | P1 | 新增 `dpiScale` 字段（1 行） | 低 |
| `src/systems/render/RenderFrame.cpp` | **P1** | 改用逻辑像素 + 计算 dpiScale | **中** |
| `src/managers/CommandBuffer.hpp` | **P2** | execute() 签名 + scissor 缩放 | **中** |
| `src/core/PlatformWindow.cpp` | P3 | Win32 border 按 DPI 缩放 | 低 |
| `src/managers/FontManager.hpp` | P4 | DPI 感知过采样 + 缓存失效 | 中 |
| `src/systems/PlatformWindowSystem.hpp` | P4 | 响应 DPI 变化通知 FontManager | 低 |
| `src/managers/IconManager.hpp` / Renderer | P5 | 物理尺寸请求图标 | 低 |

**最小可用改动（仅 P0+P1+P2）**：涉及 5 个文件，可以在高分屏上得到正确的渲染结果，
Win32 边框感应区和字体清晰度留作后续迭代。

---

## 五、回归测试要点

1. **1× DPI 显示器**：行为与修改前完全一致（`dpiScale=1.0`，逻辑像素=物理像素）
2. **2× DPI 显示器（如 4K@200%）**：UI 元素完整铺满窗口，不再压缩至左上角
3. **裁剪区域**：滚动列表、Table 组件的溢出内容不超出容器边界
4. **无边框窗口拖拽缩放**：4 条边 + 4 个角的热区宽度在各 DPI 下保持视觉一致
5. **字体渲染**：文字边缘在高分屏上明显更清晰，无模糊
6. **DPI 变化**：将窗口从 1× 显示器拖到 2× 显示器后，布局和字体正确更新

---

## 六、参考

- SDL3 高分屏文档：`SDL_WINDOW_HIGH_PIXEL_DENSITY`、`SDL_GetWindowDisplayScale`
- Win32 Per-Monitor DPI：`GetDpiForWindow`（需 `<shellscalingapi.h>` 或 `<winuser.h>`）
- Yoga 布局引擎：所有尺寸输入/输出均为设备无关像素（逻辑像素），无需修改
