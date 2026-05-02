# 测试构建闭环报告 - 2026-05-02 13:12

## 概览
| 阶段 | 结果 | 备注 |
|---|---|---|
| Configure | 通过 | 无 preset；`cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Debug -DENABLE_BUILD_TESTS=ON` |
| Build: ui | 通过 | CMake Tools `Build_CMakeTools`, target=`ui`；仅有既有 warning |
| Build: example_ui_demo | 通过 | 首次因 UI 公开依赖未传递失败；自修后通过 |
| Build: ui_tests | 通过 | 首次因 spdlog 公开依赖未传递失败；自修后通过 |
| Test: CTest | 失败 | CMake Tools `RunCtest_CMakeTools`；77 个 UI 测试中 76 通过，1 个失败 |
| Coverage | 跳过 | 本任务未要求覆盖率，未启用覆盖率配置 |
| Package | 跳过 | 未请求打包 |

## 配置结果
- 生成器：Ninja。
- 构建目录：`build/`。
- 构建类型：Debug。
- 测试开关：`ENABLE_BUILD_TESTS=ON`。
- 配置结论：通过。第三方配置输出包含 PkgConfig/LibUSB/ZLIB/PNG/BZip2/Brotli/Python3 等可选依赖未找到提示，不构成配置失败。

## 构建结果
| Target | 结果 | 说明 |
|---|---|---|
| `ui` | 通过 | CMake Tools 返回码 0；剩余 warning 包括未使用变量/参数等 |
| `example_ui_demo` | 通过 | 首次缺 `entt/entt.hpp`；修复 `ui` PUBLIC 依赖后通过 |
| `ui_tests` | 通过 | 首次缺 `spdlog/spdlog.h`；修复 `ui` PUBLIC 依赖后通过 |

## ui_tests / CTest 结果
| 项目 | 结果 | 备注 |
|---|---|---|
| `ui_tests` 构建 | 通过 | 测试可执行文件完成构建 |
| CTest 全量 UI 测试 | 失败 | `BatchManagerTest.ClearResetsAllBatchesAndSupportsReuse` 稳定失败 |
| 失败用例窄复现 | 失败 | 单独运行同一测试仍抛出 `0xc0000005` |

失败摘要：
```text
BatchManagerTest.ClearResetsAllBatchesAndSupportsReuse
unknown file: error: SEH exception with code 0xc0000005 thrown in the test body.
```

判定：该失败位于 `BatchManager::clear()`/PMR 复用相关运行时行为或对应测试假设，不属于本任务允许自动修复的 CMake/include/target 链接/测试入口笔误范围，已记录并升级待人工处理。

## 残留旧目录扫描结果
物理目录仍存在：
- `src/client`
- `src/server`
- `src/net`
- `src/shared`
- `src/utils`
- `third_party/abseil-cpp`
- `third_party/kcp`
- `third_party/json`
- `third_party/mimalloc`
- `third_party/HFSM2`
- `third_party/thorvg-main`
- `resource`
- `assets`

源码/CMake 文本残留扫描结论：旧残留主要来自仍存在的旧目录内部，例如 `src/client/CMakeLists.txt` 的 `PestManKillClient`、`src/server/CMakeLists.txt` 的 `PestManKillServer`、`src/net/CMakeLists.txt` 的 `kcp`/`nlohmann_json`、`tests/unittest/net/CMakeLists.txt` 的 `net_tests`。顶层当前配置入口未重新接入这些旧 target。

## 已自修项
- `src/ui/CMakeLists.txt`：将 `EnTT::EnTT`、`asio::asio` 从 `ui` 私有链接面移到 PUBLIC 链接面，修复示例包含 UI 公开头时找不到 EnTT/ASIO 头的问题。
- `src/ui/CMakeLists.txt`：将 `spdlog::spdlog_header_only` 移到 `ui` PUBLIC 链接面，修复测试/消费者包含 `Logger.hpp` 时找不到 spdlog 头的问题。

## 失败与处理
| 阶段 | 名称 | 摘要 | 问题日志 |
|---|---|---|---|
| Build | `example_ui_demo` | UI 公开头依赖未传递导致缺 `entt/entt.hpp`；已自修并复跑通过 | `问题日志.md` 2026-05-02 13:12 build |
| Unit Build | `ui_tests` | `Logger.hpp` 依赖 spdlog 未传递；已自修并复跑通过 | `问题日志.md` 2026-05-02 13:12 unit |
| CTest | `BatchManagerTest.ClearResetsAllBatchesAndSupportsReuse` | 稳定抛出 `0xc0000005`；未自动改业务逻辑 | `问题日志.md` 2026-05-02 13:12 unit |
| Residual Scan | 旧目录物理残留 | 旧源码/旧第三方/业务资源目录仍存在；未自动扩大删除 | `问题日志.md` 2026-05-02 13:12 build |

## 未解决问题
- [ ] `BatchManagerTest.ClearResetsAllBatchesAndSupportsReuse` 稳定失败，需由代码工厂或模块维护者定位 `BatchManager::clear()`/PMR 复用行为或测试假设。
- [ ] 物理删除残留未完成：旧 `src/client`、`src/server`、`src/net`、`src/shared`、`src/utils`，以及无用第三方目录和业务资源目录仍在工作树内。
- [ ] CMake Tools target 列表在重新 configure 后仍显示部分旧 target 名称，但实际指定构建 `example_ui_demo` 可成功执行；建议后续在清理旧目录后刷新/重配工作区验证。

---

# 复验追加 - 2026-05-02 13:19

## 概览
| 阶段 | 结果 | 备注 |
|---|---|---|
| Configure | 通过 | CMake Tools 构建过程中触发增量再生成；build/ Ninja Debug |
| Build: ui | 通过 | CMake Tools `Build_CMakeTools`, target=`ui`；返回码 0 |
| Build: example_ui_demo | 通过 | CMake Tools `Build_CMakeTools`, target=`example_ui_demo`；返回码 0 |
| Build: ui_tests | 通过 | CMake Tools `Build_CMakeTools`, target=`ui_tests`；返回码 0 |
| Narrow Test | 通过 | `BatchManagerTest.ClearResetsAllBatchesAndSupportsReuse` 1/1 通过 |
| CTest: UI 全量 | 通过 | CMake Tools `RunCtest_CMakeTools`；77/77 通过，0 失败 |
| Coverage | 跳过 | 本轮未要求覆盖率，未启用覆盖率配置 |
| Package | 跳过 | 未请求打包 |

## 构建结果
| Target | 结果 | 说明 |
|---|---|---|
| `ui` | 通过 | 成功链接 `src/ui/ui.lib` |
| `example_ui_demo` | 通过 | 成功链接 `example/ui_demo/example_ui_demo.exe` |
| `ui_tests` | 通过 | 成功链接 `tests/unittest/ui/ui_tests.exe` |

构建备注：本轮构建日志中仍出现既有 warning，例如 `Hierarchy.hpp` 未使用变量、`DeviceManager.hpp` 未使用参数、`IconRenderer.hpp` 未使用变量；CMake 诊断面板复查时无残留诊断。Windows 下测试发现阶段仍提示 `Building introspection files on Windows requires BUILD_SHARED_LIBS to be enabled.`，未影响构建返回码。

## BatchManager 修复复验
| 项目 | 结果 | 备注 |
|---|---|---|
| 窄测 | 通过 | `BatchManagerTest.ClearResetsAllBatchesAndSupportsReuse` 用时约 0.07 秒 |
| 全量 UI CTest 中的 BatchManager 组 | 通过 | `AddRectProducesSingleBatchAfterOptimize`、`DifferentScissorProducesDifferentBatches`、`DifferentPushConstantsBreakBatchMerging`、`ClearResetsAllBatchesAndSupportsReuse` 均通过 |

结论：原访问冲突复验点已恢复通过，当前未发现 BatchManager 相关新失败。

## UI CTest 全量结果
| 范围 | 结果 | 说明 |
|---|---|---|
| CMake Tools 可枚举 UI 测试 | 通过 | 77 个测试全部通过 |
| 失败用例 | 无 | 输出统计 `failLines=0` |

补充说明：CMake Tools 本轮按测试逐条执行并输出多段 `100% tests passed, 0 tests failed out of 1`，统计到 77 段通过摘要。输出中仍有 `DartConfiguration.tcl` 缺失提示，但 CTest 返回码为 0，不构成本轮失败。

## 残留旧目录扫描结果
物理目录仍存在：
- `src/client`
- `src/server`
- `src/net`
- `src/shared`
- `src/utils`
- `third_party/abseil-cpp`
- `third_party/kcp`
- `third_party/json`
- `third_party/mimalloc`
- `third_party/HFSM2`
- `third_party/thorvg-main`
- `resource`
- `assets`

有效入口扫描：在 `CMakeLists.txt`、`example/`、`src/`、`tests/` 范围内未扫描到重新接入上述旧模块的 `add_subdirectory(...)`。按本轮失败策略，未处理旧目录物理删除问题，仅重新扫描并报告状态。

## 失败与处理
| 阶段 | 名称 | 摘要 | 问题日志 |
|---|---|---|---|
| 无 | 无 | 本轮构建与测试均通过 | 未新增 |

## 自动修复
- 无。

## 待人工处理
- [ ] 物理删除残留仍未完成；本轮按要求不处理旧目录物理删除问题。