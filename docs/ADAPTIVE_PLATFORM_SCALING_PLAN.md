# VMP-ui 自适应平台缩放规划

> 文档版本：1.0  
> 日期：2026-05-31  
> 状态：规划中  
> 目标平台：Windows / Linux X11 / Linux Wayland

---

## 一、目标

自适应平台缩放的目标是让 VMP-ui 默认跟随操作系统显示缩放，并在窗口跨显示器、显示器缩放变化、窗口后端切换时保持 UI 尺寸、命中测试、裁剪、字体和图标清晰度一致。

本规划采用以下默认策略：

1. 默认启用平台缩放：新建窗口默认请求高像素密度 framebuffer。
2. 布局坐标保持逻辑像素：Yoga、组件尺寸、鼠标命中和 UI API 不暴露物理像素。
3. 渲染目标使用物理像素：swapchain、viewport、scissor 和资源光栅化按平台缩放换算。
4. 平台缩放变化可热更新：窗口移动到不同 DPI/scale 显示器时，重新计算 scale 并触发布局/渲染刷新。
5. 优先依赖 SDL3 跨平台抽象：Windows 特殊处理仅限 DPI awareness、无边框 hit-test 和 WM_DPICHANGED。

---

## 二、当前实现评估

### 2.1 已具备的基础

| 能力 | 当前状态 | 说明 |
|------|----------|------|
| 默认高像素密度窗口 | 已具备 | `CreateWindow` / `CreateDialog` 已使用 `SDL_WINDOW_HIGH_PIXEL_DENSITY`。 |
| Windows DPI awareness | 已具备 | `Application` 在 `SDL_Init` 前设置 `SDL_WINDOWS_DPI_AWARENESS=permonitorv2` 和 `SDL_WINDOWS_DPI_SCALING=0`。 |
| 逻辑/物理像素分离 | 已具备 | `RenderFrame` 使用 `SDL_GetWindowSize` 填充 shader screen size，使用 `SDL_GetWindowSizeInPixels` 作为 GPU viewport。 |
| Scissor 物理像素换算 | 已具备 | `CommandBuffer::execute` 接收 `dpiScale`，设置 GPU scissor 前进行缩放。 |
| 字体 DPI 感知 | 部分具备 | `FontManager::setDpiScale` 已接入渲染帧，文本缓存键包含 oversample scale。 |
| Windows 无边框缩放热区 | 已具备 | `WM_NCHITTEST` 使用 `GetDpiForWindow` 将边框宽度换算到物理像素。 |
| Windows DPI 变化窗口建议矩形 | 已具备 | `WM_DPICHANGED` 中调用 `SetWindowPos` 应用 suggested rect。 |
| Linux X11 链接 | 已具备 | CMake 可按 `UI_LINUX_WINDOW_SYSTEM` 链接 `X11::X11` 并定义 `UI_LINUX_X11`。 |
| Wayland 后端开关 | 已具备 | CMake 可定义 `UI_LINUX_WAYLAND`，装饰能力仍依赖 SDL/ compositor。 |

### 2.2 主要缺口

| 缺口 | 风险 | 建议优先级 |
|------|------|------------|
| 缺少显式平台缩放状态组件 | `dpiScale` 只在渲染帧临时计算，其他系统无法稳定读取当前 scale。 | P0 |
| 未监听 `SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED` | Windows 跨屏、Wayland fractional scale、X11 scale 变化可能无法立即刷新。 | P0 |
| `dpiScale` 计算只用物理宽 / 逻辑宽 | 非等比、边界时序、SDL 后端差异下不如 `SDL_GetWindowDisplayScale` 明确。 | P0 |
| 字体缓存由全局 `FontManager` 持有单一 scale | 多窗口位于不同 DPI 显示器时可能互相清缓存或复用不理想。 | P1 |
| 图标仍未按物理尺寸选型 | 高缩放下图标可能模糊。 | P1 |
| fallback / CPU 渲染路径未统一确认 scale 语义 | GPU 路径已闭合，软件路径需验收。 | P1 |
| 缺少用户可控开关 | 默认启用是目标，但需要调试/兼容时能关闭平台缩放。 | P2 |
| Wayland 装饰能力不完整 | 自绘无边框在 Wayland 下无法获得与 X11/Windows 完全一致的窗口管理行为。 | P2 |

---

## 三、设计原则

### 3.1 坐标空间定义

| 坐标空间 | 使用场景 | 说明 |
|----------|----------|------|
| 逻辑像素 | UI API、Yoga 布局、组件尺寸、鼠标事件、文本排版、hit-test | 对用户和控件稳定；不随平台 scale 改变布局语义。 |
| 物理像素 | GPU viewport、swapchain、scissor、字体/图标光栅化尺寸 | 对渲染清晰度负责；由平台 scale 从逻辑像素换算。 |
| 平台缩放因子 | `displayScale = physical / logical` | 每个 SDL 窗口独立维护，允许多窗口不同 scale。 |

### 3.2 缩放因子来源优先级

统一新增平台缩放查询函数，返回可用于 UI 渲染的 `float`：

```cpp
namespace ui::platform
{
    [[nodiscard]] float GetWindowDisplayScale(SDL_Window* window) noexcept;
}
```

建议取值顺序：

1. `SDL_GetWindowDisplayScale(window)` 返回有效正数时优先使用。
2. 回退到 `SDL_GetWindowSizeInPixels / SDL_GetWindowSize`。
3. 两者都不可用时返回 `1.0F`。

Windows 可在该函数内使用 `GetDpiForWindow(hwnd) / 96.0F` 作为 SDL 异常时的强回退；X11 和 Wayland 默认委托 SDL3。

### 3.3 默认启用策略

新增 CMake / 运行时两层开关：

```cmake
option(UI_ENABLE_PLATFORM_SCALING "Enable platform display scaling by default" ON)
```

```cpp
struct PlatformScalePolicy
{
    bool enabled = true;
    float forcedScale = 0.0F; // <= 0 表示自动
};
```

默认行为：

1. `UI_ENABLE_PLATFORM_SCALING=ON` 时，窗口 flags 自动包含 `SDL_WINDOW_HIGH_PIXEL_DENSITY`。
2. Windows 在 `SDL_Init` 前启用 per-monitor v2 DPI awareness。
3. `forcedScale > 0` 仅用于测试和调试，正常用户路径不建议暴露。
4. `enabled=false` 时退回 `scale=1.0F`，并可不请求 high pixel density。

---

## 四、架构改造规划

### P0：引入窗口缩放状态并接入事件

#### 4.1 新增组件字段

在 `components::Window` 中维护当前缩放状态：

```cpp
struct Window
{
    // existing fields...
    float displayScale = 1.0F;
    Vec2 logicalSize{0.0F, 0.0F};
    Vec2 pixelSize{0.0F, 0.0F};
};
```

说明：

1. `displayScale` 是该窗口当前平台缩放。
2. `logicalSize` 来自 `SDL_GetWindowSize`。
3. `pixelSize` 来自 `SDL_GetWindowSizeInPixels`。
4. `RenderFrame` 不再重复推导状态，而是读取 `Window` 组件；如果组件未初始化则现场补齐。

#### 4.2 新增窗口缩放变化事件

在 `common/Events.hpp` 中新增：

```cpp
struct WindowDisplayScaleChanged
{
    using is_event_tag = void;
    uint32_t windowID = 0;
    float oldScale = 1.0F;
    float newScale = 1.0F;
};
```

#### 4.3 扩展 PlatformWindowSystem

`PlatformWindowSystem` 的 relevant event 增加：

```cpp
SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED
SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED
SDL_EVENT_WINDOW_RESIZED
SDL_EVENT_WINDOW_MOVED
```

处理流程：

1. 根据 `windowID` 找到根窗口实体。
2. 调用 `platform::GetWindowDisplayScale(sdlWindow)` 更新 `Window::displayScale`。
3. 同步 `logicalSize` / `pixelSize`。
4. 如果 scale 变化超过 epsilon，触发 `WindowDisplayScaleChanged`。
5. 标记根窗口 `LayoutDirtyTag` 和 `RenderDirtyTag`。
6. 清理或迁移依赖物理像素的缓存：字体、文本纹理、图标纹理。

#### 4.4 渲染路径改造

`RenderFrame` 中的 scale 获取建议调整为：

```cpp
auto& windowComp = windowView.get<components::Window>(windowEntity);
const float displayScale = windowComp.displayScale > 0.0F ? windowComp.displayScale : 1.0F;

rootContext.screenWidth = windowComp.logicalSize.x();
rootContext.screenHeight = windowComp.logicalSize.y();
rootContext.dpiScale = displayScale;

m_commandBuffer->execute(sdlWindow,
                         static_cast<int>(windowComp.pixelSize.x()),
                         static_cast<int>(windowComp.pixelSize.y()),
                         displayScale,
                         batches);
```

收益：

1. 所有系统读取同一份窗口缩放状态。
2. display-scale 事件可提前更新缓存，渲染帧只消费结果。
3. 多窗口不同 scale 的问题更容易拆分治理。

---

### P1：资源按窗口 scale 管理

#### 4.5 字体缓存从全局 scale 过渡到按 scale 分桶

当前 `FontManager` 只有一个 `m_dpiScale`，多窗口不同 scale 时可能频繁清缓存。建议改为缓存键包含 quantized scale：

```cpp
struct GlyphKey
{
    char32_t codepoint;
    int fontSize;
    int oversamplePercent;
};
```

策略：

1. `RenderContext` 继续传递 `dpiScale`。
2. 字形请求时使用 `context.dpiScale` 计算 oversample。
3. cache key 带上 oversample percent，而不是依赖全局 `FontManager::m_dpiScale`。
4. 保留 `setDpiScale` 作为单窗口兼容路径，后续收敛到无状态查询。

#### 4.6 图标按物理尺寸选型

图标渲染保持逻辑尺寸，但素材请求使用物理尺寸：

```cpp
const float physicalIconSize = logicalIconSize * context.dpiScale;
```

建议将 `IconManager` 缓存键扩展为：

```cpp
iconName + quantizedPhysicalSize + styleVariant
```

#### 4.7 图片和 NinePatch 后续扩展

图片资源暂不强制多分辨率；规划扩展点：

1. `@1x` / `@2x` / `@3x` 命名约定。
2. `ImageManager` 根据 `context.dpiScale` 选择最接近资源。
3. 目标矩形仍使用逻辑像素。

---

### P2：配置、诊断与验收工具

#### 4.8 配置入口

建议增加命令行或 AppConfig 项：

| 配置 | 默认值 | 作用 |
|------|--------|------|
| `--ui-platform-scaling=auto` | auto | 默认跟随系统。 |
| `--ui-platform-scaling=off` |  | 强制 scale=1，用于兼容或截图对比。 |
| `--ui-platform-scale=1.5` |  | 测试用强制缩放。 |

#### 4.9 日志诊断

窗口创建和 scale 变化时记录：

```text
[PlatformScale] window=3 backend=wayland logical=800x600 pixel=1200x900 scale=1.50 source=SDL
```

日志字段建议包含：

1. SDL video driver：`SDL_GetCurrentVideoDriver()`。
2. window id。
3. logical size / pixel size。
4. display scale。
5. scale 来源：SDL / Win32 / size-ratio / forced / disabled。

#### 4.10 测试辅助视图

示例程序可增加调试面板，展示当前窗口：

1. 逻辑尺寸。
2. 物理尺寸。
3. display scale。
4. video driver。
5. 当前字体 oversample。

---

## 五、平台适配细节

### 5.1 Windows

目标行为：

1. 默认 per-monitor v2。
2. 窗口移动到不同缩放显示器后，自动应用 suggested rect。
3. UI 逻辑尺寸不突变，物理 framebuffer 和资源清晰度跟随新 DPI。
4. 无边框缩放热区视觉宽度稳定。

实现要点：

| 项 | 方案 |
|----|------|
| 进程 DPI 感知 | `SDL_WINDOWS_DPI_AWARENESS=permonitorv2`，必须在 `SDL_Init` 前。 |
| 禁用 SDL 自动 DPI 缩放 | `SDL_WINDOWS_DPI_SCALING=0`，由 UI 自己处理逻辑/物理映射。 |
| 查询 scale | 首选 `SDL_GetWindowDisplayScale`，回退 `GetDpiForWindow / 96.0F`。 |
| DPI 变化 | `WM_DPICHANGED` 应用 suggested rect；SDL 事件层处理 `SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED`。 |
| 自绘边框 | hit-test 边框宽度按 `GetDpiForWindow` 换算。 |
| 圆角透明 | scale 变化后重新同步 `SetWindowRgn`，避免圆角尺寸停留在旧 DPI。 |

验收场景：

1. 100% 和 150% 双屏之间拖动窗口。
2. 150% 和 200% 双屏之间拖动窗口。
3. 运行中修改系统显示缩放后窗口刷新。
4. 4 条边和 4 个角的 resize hit-test 宽度一致。

### 5.2 Linux X11

目标行为：

1. 默认请求 high pixel density。
2. 读取 X11 桌面环境提供的 scale/DPI。
3. 支持 `UI_LINUX_WINDOW_SYSTEM=AUTO` 下 X11 优先、Wayland 兜底。
4. X11 自绘无边框继续使用 Motif hints。

实现要点：

| 项 | 方案 |
|----|------|
| 查询 scale | 首选 `SDL_GetWindowDisplayScale`。 |
| 回退 scale | `pixelSize / logicalSize`，必要时读取 `Xft.dpi` 只作为诊断，不作为主路径。 |
| 构建依赖 | `find_package(X11 QUIET)` 成功时链接 `X11::X11` 并定义 `UI_LINUX_X11=1`。 |
| 事件 | 监听 `SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED`、`PIXEL_SIZE_CHANGED`、`RESIZED`。 |
| 环境变量兼容 | 验证 `GDK_SCALE`、`QT_SCALE_FACTOR`、`Xft.dpi` 对 SDL3 返回值的影响。 |

风险：

1. X11 对 fractional scale 的支持高度依赖桌面环境，可能只体现为字体 DPI 而非 framebuffer scale。
2. 不同 WM 对 Motif hints 支持不一致，缩放策略不能绑定到装饰成功与否。
3. `SDL_GetWindowDisplayScale` 和 size-ratio 不一致时，需要日志暴露而不是静默覆盖。

验收场景：

1. KDE/GNOME X11 100% / 150% / 200%。
2. `UI_LINUX_WINDOW_SYSTEM=X11` 强制后端构建和运行。
3. `UI_LINUX_WINDOW_SYSTEM=AUTO` 在无 X11 dev 包环境下能给出清晰 CMake 诊断或 Wayland 兜底。
4. 滚动区域 scissor 在 1.25 / 1.5 scale 下不漏裁剪。

### 5.3 Linux Wayland

目标行为：

1. 默认请求 high pixel density。
2. 支持 Wayland integer scale 和 fractional scale。
3. 依赖 SDL3 获取 `wl_output.scale` / `wp_fractional_scale_v1` 后的 display scale。
4. 自绘窗口装饰不阻塞平台缩放。

实现要点：

| 项 | 方案 |
|----|------|
| 查询 scale | `SDL_GetWindowDisplayScale` 是主路径。 |
| fractional scale | 由 SDL3 处理；应用层只消费 float scale。 |
| 事件 | 必须监听 `SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED` 和 `PIXEL_SIZE_CHANGED`。 |
| 装饰 | 当前 Wayland 下 `PlatformWindow` 退化为空操作；后续通过 libdecor / xdg-decoration 解决。 |
| 构建 | 不直接链接 `wayland-client`，除非引入自管装饰协议。 |

风险：

1. Wayland 下客户端不能随意操作顶层窗口边框，不能把 X11 Motif hints 的能力等价迁移。
2. Fractional scale 可能导致物理尺寸不是逻辑尺寸的整数倍，scissor 必须继续使用 floor/ceil 策略。
3. 多显示器之间移动时，scale 和 pixel size 事件顺序可能不同，状态同步必须幂等。

验收场景：

1. GNOME Wayland 100% / 125% / 150% / 200%。
2. KDE Wayland fractional scale。
3. 窗口跨不同 scale 输出移动。
4. `SDL_VIDEODRIVER=wayland` 强制后端运行。

---

## 六、实施顺序

### 阶段 A：最小闭环（P0）

1. 新增 `platform::GetWindowDisplayScale(SDL_Window*)`。
2. 扩展 `components::Window`，保存 `displayScale` / `logicalSize` / `pixelSize`。
3. 扩展 `PlatformWindowSystem`，监听 display-scale / pixel-size / resized / moved。
4. `WindowSync` 或新 helper 统一同步窗口 scale 和尺寸。
5. `RenderFrame` 改为消费 `Window` 组件中的 scale 和尺寸。
6. scale 变化时标记布局和渲染 dirty，并清理文本缓存。

### 阶段 B：资源清晰度（P1）

1. 字体缓存 key 纳入 scale bucket，减少多窗口互相清缓存。
2. 图标按物理尺寸请求和缓存。
3. fallback 渲染路径确认 scissor、文本和图片 scale 语义。
4. 增加日志诊断。

### 阶段 C：配置与平台完善（P2）

1. 增加 `UI_ENABLE_PLATFORM_SCALING` CMake 选项，默认 ON。
2. 增加运行时 `auto/off/forced` 调试入口。
3. 示例程序增加缩放诊断面板。
4. Wayland 装饰能力独立评估 libdecor / xdg-decoration。

---

## 七、验收矩阵

| 平台 | 后端 | Scale | 必测项 |
|------|------|-------|--------|
| Windows 10/11 | SDL windows | 100% | 行为与 scale=1 一致。 |
| Windows 10/11 | SDL windows | 150% | UI 不压缩，文字清晰，scissor 正确。 |
| Windows 10/11 | SDL windows | 200% | 无边框 resize hit-test 宽度稳定。 |
| Windows 10/11 | SDL windows | 跨屏 | `WindowDisplayScaleChanged` 触发，缓存刷新。 |
| Linux | X11 | 100% | 构建链接正常，Motif hints 不影响 scale。 |
| Linux | X11 | 150% | `SDL_GetWindowDisplayScale` 与 size-ratio 日志可见。 |
| Linux | X11 | 跨屏 | pixel-size / display-scale 事件顺序不导致状态错乱。 |
| Linux | Wayland | 100% | 基础窗口、布局、渲染正常。 |
| Linux | Wayland | 125% / 150% | fractional scale 下裁剪边缘无缺口。 |
| Linux | Wayland | 跨屏 | scale 热更新，窗口装饰退化不影响 UI 渲染。 |

---

## 八、完成标准

1. 默认构建和默认窗口创建路径启用平台缩放。
2. `Window` 组件可准确反映当前逻辑尺寸、物理尺寸和 display scale。
3. Windows / X11 / Wayland 均通过 SDL 事件热更新 scale。
4. GPU viewport、shader screen size、scissor、文本和图标使用一致的逻辑/物理像素规则。
5. 多窗口位于不同 DPI 显示器时不会互相污染资源缓存。
6. 日志可以解释当前 scale 来源和后端。
7. 提供关闭或强制 scale 的调试入口，但默认保持自动平台缩放开启。
