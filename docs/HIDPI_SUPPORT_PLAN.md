# PmkUi 高分屏（HiDPI）支持方案

> 文档版本：2.0  
> 日期：2026-05-25  
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

### 1.5 Linux X11 链接缺失（现存构建 Bug）

`PlatformWindow.cpp` 在 `__linux__` 分支中直接调用 `XChangeProperty`、`XInternAtom` 等 X11
ABI，但 `src/CMakeLists.txt` 缺少对 `X11` 库的 `find_package` + `target_link_libraries`，
Linux 下链接阶段必然报 undefined reference。Windows 当前不受影响。

### 1.6 `WM_DPICHANGED` 未处理（Windows）

当用户把窗口拖到不同 DPI 的显示器时，Windows 发送 `WM_DPICHANGED`，
但 `CustomFrameProc`（`PlatformWindow.cpp`）没有处理该消息，
导致 DPI 变化时 `screen_size`、字体缓存、NCHITTEST 边框宽度都不更新。

### 1.7 Wayland 装饰层缺失

Wayland 原生协议（`xdg-toplevel`）不支持 Motif Hints，
当前代码在 Wayland 会话下完全回退为空操作，无法实现自绘标题栏边框。
`libdecor` 或 `xdg-decoration` 协议可解决此问题，但需要额外依赖。

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
| `src/core/PlatformWindow.cpp` | P3 | Win32 border 按 DPI 缩放 + WM\_DPICHANGED | 低 |
| `src/CMakeLists.txt` | **P3** | **X11 链接修复**（现存 Bug） | 低 |
| `src/managers/FontManager.hpp` | P4 | DPI 感知过采样 + 缓存失效 | 中 |
| `src/systems/PlatformWindowSystem.hpp` | P4 | 响应 DPI 变化通知 FontManager | 低 |
| `src/managers/IconManager.hpp` / Renderer | P5 | 物理尺寸请求图标 | 低 |

**最小可用改动（P0+P1+P2+X11链接修复）**：涉及 6 个文件，可在高分屏上得到正确渲染结果并修复 Linux 构建错误。

---

## 五、平台层详细规划

### 5.1 各平台 DPI 机制与逻辑/物理像素映射

#### Windows

| 项目 | 详情 |
|------|------|
| DPI 感知模式 | `DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2`（每显示器独立） |
| SDL3 设置入口 | `SDL_HINT_WINDOWS_DPI_AWARENESS = "permonitorv2"` 在 SDL_Init 前 |
| DPI 值语义 | `GetDpiForWindow(hwnd)` 返回 96（=100%）到 384（=400%） |
| 缩放关系 | `scale = dpi / 96.0f` |
| 逻辑像素 | `SDL_GetWindowSize` = Win32 逻辑像素（DWM 打折后） |
| 物理像素 | `SDL_GetWindowSizeInPixels` = Win32 物理像素 |
| DPI 变化事件 | Win32: `WM_DPICHANGED`；SDL3: `SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED` |
| NCHITTEST lParam | **物理屏幕像素**（DPI 感知进程下），需将 borderWidth 居正 |
| SDL 缩放控制 | `SDL_HINT_WINDOWS_DPI_SCALING="0"` 要求 SDL 不做逻辑缩放，应用自己处理 DPI |

**WM_DPICHANGED 处理（需补充到 `CustomFrameProc`）**：

```cpp
case WM_DPICHANGED:
{
    // 新 DPI 在 wParam 高 16 位
    UINT const newDpi = HIWORD(wParam);
    // SDL3 会自动调整窗口到建议的新大小（lParam 中）
    auto const* suggestedRect = reinterpret_cast<RECT const*>(lParam); // NOLINT
    SetWindowPos(hWnd, nullptr,
        suggestedRect->left, suggestedRect->top,
        suggestedRect->right  - suggestedRect->left,
        suggestedRect->bottom - suggestedRect->top,
        SWP_NOZORDER | SWP_NOACTIVATE);
    // WM_DPICHANGED 后 SDL3 会发出 SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED，
    // ECS 层在该事件中重新计算 dpiScale 并刷新布局。
    (void)newDpi;
    break;
}
```

#### Linux / X11

| 项目 | 详情 |
|------|------|
| DPI 来源 | Xft.dpi 资源（xrdb）、`GDK_SCALE`、`QT_SCALE_FACTOR` 环境变量 |
| SDL3 读取 | `SDL_GetWindowDisplayScale(window)` 内部读 Xft.dpi |
| 逻辑像素 | `SDL_GetWindowSize` = X11 逻辑尺寸 |
| 物理像素 | `SDL_GetWindowSizeInPixels`（开启 HIGH_PIXEL_DENSITY 后） |
| DPI 变化事件 | SDL3: `SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED` |
| 裁剪窗口边框 | `_MOTIF_WM_HINTS` 广播属性到 WM（已实现） |
| 运行时依赖 | **`X11`** 库（调用 `XChangeProperty` 等 API） |

**现存构建 Bug**：`PlatformWindow.cpp` 在 Linux 下直接调用 `XChangeProperty`、`XInternAtom` 等 X11
ABI，但 `src/CMakeLists.txt` 缺少对 `X11` 库的 `find_package` + `target_link_libraries`，
Linux 下链接阶段必然报 undefined reference。修复方案见 §5.2。

#### Linux / Wayland

| 项目 | 详情 |
|------|------|
| DPI 来源 | `wl_output.scale`（整数）或 `wp_fractional_scale_v1`（分数） |
| SDL3 读取 | `SDL_GetWindowDisplayScale(window)` 自动处理 |
| 裁剪窗口边框 | Wayland 协议不支持 Motif Hints，当前空操作，需 `libdecor` 或 `xdg-decoration` |
| DPI 变化事件 | SDL3: `SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED` |
| 窗口裁剪 | 适用 `xdg-toplevel-decoration-v1` 协议，需 `wayland-protocols` |

---

### 5.2 CMakeLists.txt 平台链接汇总

#### 当前状态（src/CMakeLists.txt）

```cmake
# 现在的平台链接块（仅 Windows）
if(WIN32)
    target_link_libraries(ui PRIVATE dwmapi comctl32)
endif()
```

| 库 | 平台 | 用途 | 当前状态 |
|-----|------|------|----------|
| `dwmapi` | Windows | DWM 透明合成 (`DwmExtendFrameIntoClientArea`) | ✅ 已链接 |
| `comctl32` | Windows | 窗口子类化 (`SetWindowSubclass`) | ✅ 已链接 |
| `user32` | Windows | `GetDpiForWindow`、`GetWindowRect` 等 | ✅ Windows 默认链接 |
| `shcore` | Windows | `GetDpiForMonitor`（当前未使用，`GetDpiForWindow` 已足够） | — 暂不需要 |
| `X11` | Linux/X11 | `XChangeProperty`、`XInternAtom` | ❌ **缺失**，需新增 |
| `Xrandr` | Linux/X11 | XRandR 读取物理 DPI（SDL3 已替代） | — 暂不需要 |
| `wayland-client` | Wayland | `libdecor` / 分数缩放协议 | — P6 扩展点 |

#### 修复后的平台链接块

```cmake
# Windows 平台链接
if(WIN32)
    target_link_libraries(ui PRIVATE dwmapi comctl32)
endif()

# Linux X11 平台链接（修复已存在的链接缺失）
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(X11 REQUIRED)          # 提供目标 X11::X11
    target_link_libraries(ui PRIVATE X11::X11)
endif()
```

> `find_package(X11 REQUIRED)` 在各发行版上的依赖包：
> - Ubuntu/Debian：`sudo apt install libx11-dev`
> - Fedora/RHEL：`dnf install libX11-devel`
> - Arch：`pacman -S libx11`

---

### 5.3 平台层扩展点（可选，未来迭代）

> 以下为 P6/P7 方向，不在最小可用范围内。

#### P6：Wayland 原生裁剪窗口边框

当前 Wayland 会话下 `SetupCustomTitleBar` 是空操作，两个实现路径：

**方案 A：SDL3 + libdecor（最简单）**
- 依赖：`libdecor-0-dev`
- SDL3 自动加载，展示原生 Wayland 边框装饰
- CMake：无需额外处理，SDL3 自将检测

**方案 B：xdg-decoration 协议（更可控）**
- `xdg-toplevel-decoration-v1`：请求 WM 移除边框，类似 Motif Hints 在 X11 的作用
- 需要 Wayland 协议头文件：`wayland-protocols` 包提供

```cmake
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(WAYLAND_CLIENT wayland-client)
    if(WAYLAND_CLIENT_FOUND)
        target_link_libraries(ui PRIVATE ${WAYLAND_CLIENT_LIBRARIES})
        target_include_directories(ui PRIVATE ${WAYLAND_CLIENT_INCLUDE_DIRS})
        target_compile_definitions(ui PRIVATE UI_WAYLAND_DECORATION=1)
    endif()
endif()
```

#### P7：Wayland 分数缩放协议

SDL3 3.x 已通过 `wp_fractional_scale_v1` 协议内部支持分数 DPI，
`SDL_GetWindowDisplayScale()` 可直接返回 1.25、1.5 等非整数缩放比，**应用层无需额外处理**。

#### 平台层 API 扩展建议（`PlatformWindow.hpp`）

建议在 `ui::platform` 命名空间新增统一的 DPI 查询与变化通知接口：

```cpp
namespace ui::platform
{
    /**
     * @brief 查询窗口当前的 DPI 缩放比例
     *
     * Windows : GetDpiForWindow(hwnd) / 96.0f
     * X11     : Xft.dpi 对应的比例（由 SDL3 读取）
     * Wayland : wl_output.scale 或 fractional scale
     * 通用    : SDL_GetWindowDisplayScale(sdlWindow)
     */
    [[nodiscard]] float GetDisplayScale(SDL_Window* sdlWindow) noexcept;

    /**
     * @brief 当 DPI 变化时更新平台窗口状态
     *
     * Windows : 重新设置 SetWindowRgn 的圆角区域（按新 DPI 缩放）
     * X11     : 空操作
     * Wayland : 空操作
     */
    void OnDisplayScaleChanged(SDL_Window* sdlWindow, float newScale);
}
```

各平台实现示例：

```cpp
// Windows 实现
float GetDisplayScale(SDL_Window* sdlWindow) noexcept {
    HWND hwnd = GetHwndFromSDL(sdlWindow);
    if (hwnd == nullptr) return SDL_GetWindowDisplayScale(sdlWindow);
    return static_cast<float>(GetDpiForWindow(hwnd)) / 96.0F;
}

void OnDisplayScaleChanged(SDL_Window* sdlWindow, float /*newScale*/) {
    // 重新同步圆角裁剪区域
    HWND hwnd = GetHwndFromSDL(sdlWindow);
    if (hwnd == nullptr) return;
    RECT r{}; GetWindowRect(hwnd, &r);
    // ... 调用 SyncRoundedRegion 逻辑 ...
}

// Linux / 通用实现（委托给 SDL3）
float GetDisplayScale(SDL_Window* sdlWindow) noexcept {
    return SDL_GetWindowDisplayScale(sdlWindow);
}
void OnDisplayScaleChanged([[maybe_unused]] SDL_Window*, [[maybe_unused]] float) {}
```

---

## 六、回归测试要点

1. **1× DPI 显示器**：行为与修改前完全一致（`dpiScale=1.0`，逻辑像素=物理像素）
2. **2× DPI 显示器（如 4K@200%）**：UI 元素完整铺满窗口，不再压缩至左上角
3. **裁剪区域**：滚动列表、Table 组件的溢出内容不超出容器边界
4. **无边框窗口拖拽缩放（Windows）**：4 条边 + 4 个角的热区宽度在各 DPI 下保持视觉一致
5. **字体渲染**：文字边缘在高分屏上明显更清晰，无模糊
6. **DPI 变化（拖动窗口）**：窗口从 1× 显示器拖到 2× 显示器后，布局和字体正确更新
7. **Linux 构建验证**：`cmake -B build-linux ... && cmake --build build-linux` 无 `undefined reference` 错误

---

## 七、参考

- SDL3 高分屏：[`SDL_WINDOW_HIGH_PIXEL_DENSITY`](https://wiki.libsdl.org/SDL3/SDL_WindowFlags)、[`SDL_GetWindowDisplayScale`](https://wiki.libsdl.org/SDL3/SDL_GetWindowDisplayScale)
- Win32 Per-Monitor DPI V2：[`SetProcessDpiAwarenessContext`](https://learn.microsoft.com/windows/win32/api/winuser/nf-winuser-setprocessdpiawarenesscontext)、[`GetDpiForWindow`](https://learn.microsoft.com/windows/win32/api/winuser/nf-winuser-getdpiforwindow)
- Win32 DPI 变化事件：[`WM_DPICHANGED`](https://learn.microsoft.com/windows/win32/hidpi/wm-dpichanged)
- X11 链接：[CMake FindX11](https://cmake.org/cmake/help/latest/module/FindX11.html)
- Wayland fractional scale：[wp-fractional-scale-v1](https://wayland.app/protocols/fractional-scale-v1)
- Wayland xdg-decoration：[xdg-toplevel-decoration-v1](https://wayland.app/protocols/xdg-decoration-unstable-v1)
- Yoga 布局引擎：所有尺寸均为逻辑像素，无需修改
