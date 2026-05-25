# PmkUi 优化建议清单

> 版本：v1.0 · 日期：2026-05-25  
> 基准：v0.4 架构锐评（C1–C31 全量审查）+ 90 个测试全绿  
> 关联文档：[架构锐评](ARCHITECTURE_CRITIQUE.md) · [HiDPI 方案](HIDPI_SUPPORT_PLAN.md) · [SVG 方案](SVG_SUPPORT_PLAN.md)

---

## 总体进度快照

| 类别 | 总条目 | 已完成 | 待办 |
|------|--------|--------|------|
| 架构缺陷修复（C1–C31） | 31 | 23 | 8 |
| 工作包（WP） | 4 | 0 | 4 |
| 功能扩展（HiDPI / SVG） | 2 | 0 | 2 |

---

## 一、待修复缺陷（来自架构锐评）

### 🔴 严重（影响正确性 / 稳定性）

#### C18 — `Registry` / `Dispatcher` 跨线程竞争无防护

`thread_local` 单例路由仅保证当前线程活跃上下文正确，但 `entt::registry` 本身不是线程安全的。
`UI_ENABLE_MULTITHREAD=ON` 场景下 `ThreadPool` worker 线程访问 `Registry::View<>()` 或 `Registry::Emplace<>()` 会产生数据竞争。

**建议**：
1. 在所有跨线程访问路径前加 `std::shared_mutex` 读写锁，或
2. 将多线程操作限定为"只写 mailbox，主线程统一消费"，彻底规避共享 registry 的竞争窗口。

**涉及文件**：`src/singleton/Registry.hpp`、`src/core/ThreadPool.hpp`

---

#### C19 — 核心系统测试覆盖率为 0

`LayoutSystem`、`RenderSystem`、`HitTestSystem`、`StateSystem`、`TimerSystem`、`TextInputSystem` 均无单测，占系统总数约 65%。任何重构均为盲飞。

**建议**：
1. 优先为 `LayoutSystem` 编写 Yoga 布局输出的确定性测试（不依赖 GPU）。
2. 用 Fake/Stub `IBackendRenderer` 解耦 `RenderSystem` 的 GPU 依赖，测试 collect → batch 逻辑。
3. 利用现有 `UiRuntime` fixture 模式为 `HitTestSystem` 添加坐标 → 实体命中用例。

**测试目录**：`tests/unittest/`，需 `-DENABLE_BUILD_TESTS=ON`

---

#### C20 — 无持续集成管道

仓库根无 `.github/workflows/`，回归测试完全依赖手动触发。历史上 C9（`~Texture` 位运算错误）若有 CI 早已被发现。

**建议**：
1. 添加 `ci.yml`：`cmake -B build -DENABLE_BUILD_TESTS=ON && cmake --build build && ctest --test-dir build --output-on-failure`
2. 矩阵覆盖：Windows (MSVC + clang-cl)，后续可扩展 Ubuntu (Clang)。
3. 触发条件：`push` 到 `main` / `dev` 分支及所有 PR。

---

### ⚠️ 中等（影响可维护性 / 扩展性）

#### C17 — `Image::textureId` 是裸 `void*`，无生命周期管理

纹理销毁完全依赖外部手动管理，`ImageRenderer` 无法感知 `ImageManager` 侧纹理是否已释放。

**建议**：引入 `ImageHandle`（`SDL_GPUTexture*` + 引用计数），`Image::textureId` 改为 `std::shared_ptr<ImageHandle>`，
`ImageManager` 内部 LRU 析构时通过弱引用感知外部是否仍有持有者。

**前置条件**：SVG 支持规划（`Image::textureId` 生命周期问题会随 SVG 解码一并放大）

**涉及文件**：`src/common/components/Visual.hpp`、`src/managers/ImageManager.hpp/.cpp`

---

#### C30 — 大型头文件内联实现编译负担重

| 文件 | 行数 | 问题 |
|------|------|------|
| `systems/StateSystem.hpp` | **1233 行** | 全部逻辑内联；每个 TU 重复编译 |
| `managers/TextTextureCache.hpp` | 430 行 | GPU 上传逻辑内联在头文件中 |
| `systems/HitTestSystem.hpp` | 371 行 | 命中测试逻辑内联 |

**建议**：按 `TableRenderer` 的拆分范式，将实现逐步迁移到对应 `.cpp`。
优先级：`StateSystem`（收益最大）→ `TextTextureCache` → `HitTestSystem`。

---

#### C31 — 14 处 `on_event<>` 仍嵌在数据组件中（OP-28 P3）

`onClick`、`onDragStart`、`onDragEnd`、`onDragMove`、`onHover`、`onUnhover`、`onPress`、`onRelease`（`Interaction.hpp`）  
`onSubmit`、`onTextChanged`、`onValueChanged`、`onScroll`、`onChanged`×2（`Data.hpp`）  

`std::move_only_function` 使组件不可拷贝，阻碍 entt `snapshot/reload`、热重载和序列化。

**建议**：按 `TableInfo::onCellClicked` → `events::TableCellClicked` 的已有范式，
将每个 `on_event<>` 替换为对应 `Dispatcher::Sink<>()` 订阅，逐步迁移。
**风险**：回调注册方式变化会影响 `example/` 和客户端代码，需同步更新示例。

---

### ℹ️ 轻微（建议改进）

#### C3 — `DslPaser.hpp` 空存根占用 `core/` 槽位

文件仅有注释声明，无任何实现，已加 TODO，但与正式模块同目录放置会对代码阅读者产生误导。

**建议**：移入 `docs/ideas/DslParser.md` 作为设计草稿，从 `src/core/` 删除。

---

#### C8 — `ThemeSystem` 是 18 行纯存根（WP-B P1 未启动）

已挂载到 `SystemManager` 注册名额但无实现，每帧触发空调用开销。

**建议**：WP-B 启动前，从 `SystemManager::addSystem()` 注册列表中注释掉 `ThemeSystem`，保留头文件存根。

---

## 二、工作包（Work Packages）

### WP-A3 — 错误处理迁移（P2，遗留过渡期尾项）

**目标**：将 `FontManager` / `IconManager` 中仍使用 `FontErrc` / `IconErrc`（`src/common/UiErrors.hpp`）的错误路径迁移到统一 `ui_errc`。

**现状**：`src/common/UiErrors.hpp` 中的旧枚举处于并存过渡期。

**涉及文件**：`src/managers/FontManager.hpp/.cpp`、`src/managers/IconManager.hpp/.cpp`、`src/common/UiErrors.hpp`（过渡完成后可删除）

---

### WP-B — Theme 系统（P1）

**目标**：实现主题/样式表系统，支持颜色、字体、间距等全局样式配置，替换当前硬编码的颜色常量（如 `headerTextColor` 等历史遗留点）。

**参考缺陷**：C8（ThemeSystem 存根）、C11（表头颜色硬编码，已修复但缺乏主题接入点）

**设计建议**：
- `ThemeToken` 枚举（Primary / Secondary / Background / OnSurface / …）
- `Theme` 组件挂在 root entity，系统通过 `Registry::Get<Theme>()` 全局读取
- `ThemeSystem` 监听 `ThemeChangedEvent` 触发全量视觉刷新

---

### WP-C — Canvas Painter 曲线支持（P2）

**目标**：`CanvasRenderer` / `Canvas API` 补充贝塞尔曲线、圆弧等矢量绘制原语。

**现状**：`src/api/Canvas.hpp/.cpp` 已有直线和矩形，曲线接口未实现。

**技术路径**：顶点细分 + CPU 曲线采样，结果作为普通三角形条带提交 `BatchManager`，不新增 shader pass。

---

### WP-D — JS 脚本层（P3）

**目标**：通过内嵌 JavaScript 引擎（QuickJS 或 V8 轻量模式）支持 UI 逻辑脚本化，降低客户端与 C++ 的耦合度。

**现状**：`src/core/DslPaser.hpp` 是空存根，设计思路待明确。  
**建议**：先完成 WP-B / WP-C，再进行 WP-D 的可行性原型；直接在 C++ 侧先稳定 DSL API 后，脚本绑定工作量更可控。

---

## 三、功能扩展

### F1 — 高分屏（HiDPI）支持

> 详细方案见 [HIDPI_SUPPORT_PLAN.md](HIDPI_SUPPORT_PLAN.md)

**核心问题**：shader `screen_size` push constant 使用物理像素，顶点坐标使用逻辑像素，两者不匹配导致 DPI=2× 时 UI 压缩到左上角 1/4 区域。

**实施优先级**：

| 阶段 | 改动 | 涉及文件 |
|------|------|---------|
| P0 | 激活 `SDL_WINDOW_HIGH_PIXEL_DENSITY`，设置 `permonitorv2` DPI 感知 | `Application.cpp`、`Factory.cpp` |
| P1 | `screen_size` 改用逻辑像素，`RenderContext` 新增 `dpiScale` 字段 | `RenderContext.hpp`、`RenderFrame.cpp` |
| P2 | scissor rect 在提交 GPU 前乘以 `dpiScale` | `CommandBuffer.hpp` |
| P3 | `FontManager::TEXT_OVERSAMPLE_SCALE` 动态感知 DPI | `FontManager.hpp` |
| P4 | `IconManager::quantizeSize()` 以物理尺寸请求图标素材 | `IconManager.hpp` |
| P5 | 处理 `WM_DPICHANGED`（Windows），清理字体缓存并刷新布局 | `PlatformWindow.cpp` |

---

### F2 — SVG 图像支持

> 详细方案见 [SVG_SUPPORT_PLAN.md](SVG_SUPPORT_PLAN.md)

**核心思路**：在 `ImageManager` 内增加 SVG 专用解码路径（推荐 `lunasvg` 或 `nanosvg`），先栅格化为 RGBA 位图再复用现有 `uploadToGpu()` 链路。`ImageRenderer`、`BatchManager`、shader、Yoga 布局系统均**无需改动**。

**前置条件**：建议在 C17（`Image::textureId` 生命周期重构）落地后再推进，避免两项改动在同一管路交叉。

---

## 四、代码整洁 Checklist

以下是独立可操作的小改动，任意顺序推进均可：

| 项 | 文件 | 工作量 |
|----|------|--------|
| 将 `DslPaser.hpp` 移出 `core/` | `src/core/DslPaser.hpp` → `docs/ideas/` | 5 min |
| 注释掉 `ThemeSystem` 注册直到 WP-B 启动 | `src/core/Application.cpp` | 5 min |
| `StateSystem.hpp` 实现拆出到 `.cpp` | `src/systems/StateSystem.hpp` (1233 行) | ~1 天 |
| `TextTextureCache.hpp` 实现拆出到 `.cpp` | `src/managers/TextTextureCache.hpp` (430 行) | 半天 |
| `HitTestSystem.hpp` 实现拆出到 `.cpp` | `src/systems/HitTestSystem.hpp` (371 行) | 半天 |
| OP-28: 迁移 8 处 `Interaction.hpp` on_event<> | `src/common/components/Interaction.hpp` | ~1 天 |
| OP-28: 迁移 6 处 `Data.hpp` on_event<> | `src/common/components/Data.hpp` | ~1 天 |
| 补全 Linux X11 链接（已知构建 Bug） | `src/CMakeLists.txt` | 10 min |

---

## 五、优先级汇总

| 优先级 | 条目 | 理由 |
|--------|------|------|
| **P0（立刻）** | C20 CI 管道 | 无 CI 意味着所有回归测试靠人工 |
| **P0（立刻）** | C19 核心系统测试 | C18 根因未动前，测试是唯一安全网 |
| **P1（近期）** | F1 HiDPI P0–P2 | 影响所有用户可见质量 |
| **P1（近期）** | WP-A3 错误处理迁移 | 清理过渡期遗留，减少维护面 |
| **P2（计划）** | C17 textureId 生命周期 | F2 SVG 的前置依赖 |
| **P2（计划）** | C30 StateSystem.hpp 拆分 | 降低编译时间，提升可维护性 |
| **P2（计划）** | WP-B Theme 系统 | P1 特性，设计先行 |
| **P3（未来）** | C31 on_event<> 迁移 | 破坏性改动，需同步更新示例 |
| **P3（未来）** | F2 SVG 支持 | 依赖 C17 |
| **P3（未来）** | WP-C Canvas 曲线 | 独立，不阻塞其他 |
| **P3（未来）** | WP-D JS 脚本层 | 依赖 WP-B / WP-C 稳定 API |
| **P3（未来）** | C18 跨线程根因 | 需要架构层级决策 |

