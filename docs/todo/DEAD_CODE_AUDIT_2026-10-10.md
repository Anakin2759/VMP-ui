# VMP-ui 死代码审计记录

> **状态**：RECORD
> **日期**：2026-10-10
> **最后复核**：2026-10-10
> **责任范围**：仓库自有生产/测试源码（`src/`、`include/`、`tests/`、`example/`、`CMake`）的可消除死代码
> **不在范围**：`third_party/**`、`build*/**`、公共 API 设计层面的冗余（属 API 演进，非死代码）
> **验收**：每条结论附「引用证据 + 移除前置条件 + 验证方式」，可直接作为删除工单依据

---

## 1. 审计范围与方法

1. **可达性判定**：对候选文件/符号，在 `src/`、`include/`、`tests/`、`example/` 内检索其 include 与符号引用；零外部引用即判为死代码。
2. **构建登记比对**：`src/CMakeLists.txt` 的 OBJECT 目标源列表（`UI_OBJECT_*_SOURCES`）与 `tests/unittest/CMakeLists.txt` 实际源文件比对，找出「文件存在但未参与构建」的孤儿。
3. **标记检索**：`TODO/FIXME/废弃/历史/legacy` 等标记 + 空实现 + 注释掉的代码块。
4. **反向确认**：对所有「疑似死」项逐一探查，避免误删（见 §5）。

---

## 2. 确认可消除（零引用，可直接删除）

| # | 文件 | 规模 | 性质 | 引用证据 | 移除前置 |
| --- | --- | --- | --- | --- | --- |
| 1 | `src/systems/RenderSystem.cpp` | 20 行 / 882 B | 空壳历史文件 | 仅含 `@file` 注释 + `// 实现分散于 render/ 子目录，此文件仅保留作历史记录。`；**未登记**在任何 OBJECT 目标源列表 | 无 |
| 2 | `src/traits/MyPFR.hpp` | 610 行 / 21.8 KB | 未采用的反射工具 | 全仓库零 `#include`；仅自身 `@file` 命中 | 无 |
| 3 | `src/renderers/RendererRegistry.hpp` | 157 行 / 5.6 KB | 被取代的动态渲染器注册表 | 全仓库零 `#include`；现行动态注册在 `RenderResources.cpp::initializeRenderers`，本文件不再使用 | 需同步删除 `tools/check_architecture_boundaries.py` 的 baseline 条目 |
| 4 | `src/api/export.hpp` | 16 行 / 567 B | 未启用的导出宏占位（模板残留） | 全仓库零 `#include`；`VMP_UI_API` 零使用；使用占位宏 `MYLIB_BUILD_DLL`（非本项目宏） | 无 |
| 5 | `src/utils/Logger.hpp` 的 `NormalizePath()` | 约 14 行 | 未调用工具函数 | 全仓库仅定义处 1 处命中，零调用点 | 无 |

**合计**：约 **803 行 / 29 KB**。

### 2.1 移除前置详解

- **#3 `RendererRegistry.hpp`**：该路径被架构门禁扫描（`SOURCE_EXTENSIONS` 含 `.hpp`），删除文件时必须同时删除 `tools/check_architecture_boundaries.py` 中：
  ```python
  ALLOWED_RAW_ACCESS_COUNTS = Counter(
      {
          ("src/renderers/RendererRegistry.hpp", ".raw()"): 2,
      }
  )
  ```
  否则门禁会因 baseline 指向不存在的文件而失败/产生噪声。

- **#4 `src/api/export.hpp`**：`VMP_UI_API` 导出宏属于 P1 未完成工作（见 `.github/copilot-instructions.md`）。当前文件既未被包含、宏名也来自模板占位，**不是**有效实现；删除不影响动态库（现依赖 `WINDOWS_EXPORT_ALL_SYMBOLS`）。后续做显式导出时应新建合规实现，而非复活本文件。

---

## 3. 暂不删除（被活动规划保留，须先决策）

| 文件 | 规模 | 现状 | 保留理由 |
| --- | --- | --- | --- |
| `src/managers/FontAtlasManager.hpp` | 220 行 / 7.9 KB | 零外部 `#include`（仅 `TextRenderHelper.hpp` 包含它） | `docs/todo/RICH_TEXT_PLAN.md`（状态 **PLANNED**）将其列为富文本 Atlas 路径的实现基础，并记为「只读参考」 |
| `src/managers/TextRenderHelper.hpp` | 135 行 / 5.1 KB | 零外部 `#include`（无任何文件包含它） | 同上，属 RICH_TEXT 计划的顶点生成层 |

**决策点**：先确认富文本是否仍走「逐字符 Atlas + `FontAtlasManager`」方案。
- 若采纳 → 保留，但应在 RICH_TEXT 实施时接入并补齐测试（当前完全未接入）。
- 若改为其他方案 → 与 `FontAtlasManager.hpp` 一并删除，约减 **355 行 / 13 KB**。

**连带影响**：`src/managers/TextureAtlas.hpp` 现仅被**测试**（`tests/unittest/test_GPUTextureOwner.cpp`）与 `FontAtlasManager.hpp` 使用；生产路径（`FontManager` + `TextTextureCache`）已不使用。若删除上表两项，需同时评估 `TextureAtlas.hpp` 与 `test_GPUTextureOwner.cpp` 的存废（本项本身**不是死代码**，见 §5）。

---

## 4. 判定为「兼容契约 / 有意空实现」（保留，禁止直接删）

| 位置 | 现状 | 结论 |
| --- | --- | --- |
| `ui::log::SetFilePath()`（`src/api/Log.cpp`） | 空实现 `static_cast<void>(path)` | 公共头已声明，属既有 ABI；删除为破坏性变更。保留，走 API 弃用流程 |
| `config::AppConfig::logFilePath()`（`src/common/AppConfig.hpp`，`@deprecated`） | 仅保存旧配置，`Application` 对非空值打废弃提示 | 策略 A 兼容路径，仍被 `Application.cpp` 读取，非死代码 |
| `managers::BatchManager::optimize()`（`src/managers/BatchManager.hpp`） | 仅调用 `flushBatch()`，正文为 `// TODO: 排序/合并/Z-order` | **未完成**而非死代码；有测试覆盖（`test_BatchManager.cpp`），保留 |
| `src/managers/StdEmbedResourceTable.cpp` | 仅当 `UI_RESOURCE_BACKEND=CMRC` 时参与编译 | 条件编译，默认 STD_EMBED 下不编译 ≠ 死代码 |

---

## 5. 反向确认：检查后判定仍存活（勿误删）

| 候选 | 实际引用 |
| --- | --- |
| `src/utils/EventLoop.hpp` | `src/core/EventLoop.hpp` 包含；`tests/unittest/test_EventLoopStress.cpp` 使用 |
| `src/utils/MpscQueue.hpp` | `src/utils/EventLoop.hpp` + `test_EventLoopStress.cpp` |
| `src/systems/TextInputSystem.hpp` | `src/core/SystemManager.cpp` 注册 |
| `src/core/UiRuntimeScope.hpp` | 生产 + 大量测试 fixture |
| `src/core/ApplicationLifecycle.hpp` | `Application.cpp` + `test_ApplicationLifecycle.cpp` |
| `src/core/TaskChain.hpp` | `Application.cpp` + `test_TaskChain.cpp` |
| `src/core/TextUtils.hpp` | `TextRenderer.hpp`、`TextEditingService.cpp`、`test_TextUtils.cpp` |
| `src/core/WindowSync.hpp` | `Helper.hpp`、`RenderFrame.cpp`、`StateSystem.cpp`、`test_Visibility.cpp` |
| `src/core/WindowEntityLookup.hpp` | `src/api/Factory.cpp` |
| `src/traits/Contains.hpp` / `PoliciesTraits.hpp` / `ComponentsTraits.hpp` / `EventTraits.hpp` | 分别被逐级 include 链引用（`PoliciesTraits` ← `LayoutSystem.cpp`；`Contains` ← `PoliciesTraits`；`ComponentsTraits` ← `Registry.hpp`；`EventTraits` ← `Dispatcher.hpp`） |
| `src/common/Result.hpp` / `ErrorCodes.hpp` / `Animation.hpp` | 兼容转发头，分别被 `CommandBuffer.hpp`、`ErrorCodes.cpp`、`test_PublicLeafHeaders.cpp` 等使用 |
| `src/helper/Helper.hpp` / `HelperAnimation.cpp` | 被 20+ 文件引用 |
| `src/managers/TextureAtlas.hpp` | 仅测试 + `FontAtlasManager.hpp`（见 §3 连带影响） |
| `src/CMakeLists.txt` 的 `UI_SOURCES` | 被 `tools/check_architecture_boundaries.py` 正则解析（**不是**死变量） |
| `tests/unittest/*.cpp` | 42 个文件全部已在 `tests/unittest/CMakeLists.txt` 注册，无孤儿测试 |

---

## 6. 附带清理项（非死代码，冗余/一致性）

1. `src/CMakeLists.txt` 的 `UI_HEADERS` 中 `systems/ShortcutSystem.hpp` **重复登记**（约 271 行与 350 行），建议去重。

---

## 7. 建议执行顺序与验证

```text
1. 删除 §2 的 4 个文件 + Logger.hpp 的 NormalizePath()
2. 同步删除 check_architecture_boundaries.py 中 RendererRegistry.hpp 的 raw() baseline
3. UI_HEADERS 去重（§6）
4. 验证：
   cmake --build build --config Debug
   ctest --test-dir build -C Debug --output-on-failure
   python tools/check_architecture_boundaries.py
   python tools/check_public_headers_self_contained.py
5. §3 决策：确认 RICH_TEXT 方案后再处理 FontAtlasManager/TextRenderHelper
```

**风险**：低。§2 全部为零引用文件/符号，删除不改变任何编译单元的可达代码；唯一需要留意的是 §2.1 的门禁 baseline 同步。

---

## 8. 结论摘要

- **可立即消除**：4 个文件 + 1 个函数，约 **803 行 / 29 KB**。
- **待决策可消除**：`FontAtlasManager.hpp` + `TextRenderHelper.hpp`，约 **355 行 / 13 KB**（阻塞于 RICH_TEXT 规划去留）。
- **需保留**：`SetFilePath` 空实现、`AppConfig::logFilePath`、`BatchManager::optimize`、CMRC 专用资源表（均为契约/条件编译/未完成，非死代码）。
- spdlog / fmt 生产依赖已清零（`src/` 无残留）。
