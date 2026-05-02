# 纯 UI 库化改造规划

- 日期：2026-05-02
- 输入来源：用户需求；协调文档 `docs/pm/run-20260502-0000-pure-ui-library.md` 工作包 #1
- 作用范围：仓库构建拓扑、`src/ui` 静态库、旧 `src/client` View 示例迁移、非 UI 模块物理迁移或删除、单元测试入口与第三方依赖裁剪
- 目标状态：仓库成为可独立构建的 UI 库项目；旧客户端界面作为 `example` 下的可构建示例程序；不保留完整客户端网络/游戏逻辑、服务端逻辑、共享协议与 KCP 网络模块。

## 结论

推荐做一次构建拓扑收敛，而不是在原有 `client/server/net/shared/utils` 上继续空壳化。当前 `src/ui` 已内置 UI 运行需要的 `Logger`、`Registry`、`Dispatcher` 单例头，精确扫描未发现 UI 源码 include `src/utils`、`src/shared`、`src/net`、`src/server`。因此纯 UI 库化的主线是：保留并整理 `src/ui`，删除/迁移非 UI target，把旧客户端 View 和最小 main 移到 `example/`，并把示例中残留的 `src/utils` 依赖替换为 UI 自身能力或示例本地代码。

改动类型：架构重构 + CMake 构建拓扑调整 + 示例迁移。该改动会破坏旧的 `PestManKillClient`、`PestManKillServer`、`net`、`shared`、`utils` target 对外可见性，是有意的破坏性变更。

## 影响范围

| 范围 | 当前状态 | 目标处理 | 优先级 | 说明 |
|---|---|---|---|---|
| 顶层 `CMakeLists.txt` | 同时添加 `src/utils`、`src/shared`、`src/ui`、`src/net`、`src/client`、`src/server`；无条件加入多余第三方库 | 只添加 UI 必需第三方、`src/ui`、可选 `example`、可选 UI 测试 | P0 | 这是纯 UI 库化的入口；必须先断开旧 target。 |
| `src/ui` | 静态库 `ui`；自带 `singleton/Logger.hpp`、`Registry.hpp`、`Dispatcher.hpp`；链接 SDL3、EnTT、asio、Yoga、spdlog、freetype、harfbuzz、Eigen、cmrc | 保留为核心库；必要时重命名 project/target 元信息，但不改 UI 行为 | P0 | UI 没有直接 include `src/utils`/`src/shared`，可独立保留。 |
| `src/client/View` | `menu.h`、`Mainwindow.h` 是纯界面声明，但 `Mainwindow.h` include `src/utils/Logger.h` | 迁移到 `example/client-view/` 或 `example/pestmankill-view/`；namespace 可保留或改为 example；日志改用 `ui::Logger`/UI 日志宏或示例本地包装 | P0 | 是示例主体。不得带入网络/游戏逻辑。 |
| `src/client/main.cpp` | 最小 UI app 启动；include `src/utils/Functions.h` 只用于控制台 UTF-8 | 迁移为 `example/pestmankill_view/main.cpp`；去掉 `utils::functions::SetConsoleToUTF8()` 或在示例本地实现 Windows 控制台 UTF-8 | P0 | 示例可执行程序入口。 |
| `src/client/CMakeLists.txt` | 生成 `PestManKillClient`，链接 `utils`、`shared`、json、absl、ui、mimalloc | 删除；新建 `example/.../CMakeLists.txt`，只链接 `ui` 和示例实际需要的最小依赖 | P0 | 旧客户端 target 不应保留。 |
| `src/utils` | header-only；含 `Logger`、`Registry`、`Dispatcher`、`Functions`、`ThreadPool`；依赖 asio、EnTT、spdlog | 默认删除或迁出仓库；若只为示例需要 `Functions.h`，改为示例本地小函数，不保留整个 target | P1 | UI 已有同名支撑代码，保留 `src/utils` 会让仓库仍像通用游戏项目。 |
| `src/shared` | 网络消息协议和共享游戏类型；依赖 json、EnTT，部分消息依赖 `src/net` | 删除或迁出仓库 | P0 | UI 与旧 View 未使用；保留会引入协议/业务噪声。 |
| `src/net` | KCP/ASIO 网络库；测试仍覆盖 net | 删除或迁出仓库；顶层和测试入口移除 net | P0 | 非 UI 模块。 |
| `src/server` | 游戏逻辑和服务端可执行程序 | 删除或迁出仓库 | P0 | 非 UI 模块。 |
| `tests/unittest` | 同时包含 `net` 与 `ui` 测试；UI 测试链接 `utils` | 删除 `tests/unittest/net`；保留 `tests/unittest/ui`，去除 `utils` 链接并确认 include 不再需要 | P1 | 纯 UI 库仍建议保留 UI 单测。 |
| `third_party` | 包含 UI 依赖和非 UI 依赖：abseil、kcp、json、mimalloc、HFSM2、thorvg-main 等 | 第一阶段只从顶层 CMake 断开非 UI 第三方；第二阶段再物理删除未被 UI/example/test 使用的目录 | P1/P2 | 先保证构建，后裁剪目录，降低误删风险。 |
| `assets/`、`resource/` | 根部 `assets/font` 与游戏 `resource/Character/Pest.json`；UI 资源在 `src/ui/assets` | 保留 `src/ui/assets`；根部 `resource/` 删除或迁出；根部 `assets/` 仅在确认无 UI/example 引用后删除 | P2 | UI CMake 使用 `src/ui/assets`，不是根部 `assets`。 |
| `cmake/` | 含 `FindDXC.cmake`、资源嵌入辅助 | 保留 | P0 | UI shader 编译和资源嵌入仍需要。 |
| `docs/pm`、`docs/architecture`、`docs/test-reports` | 过程文档 | 保留 | P2 | 作为迁移记录。 |

## 依赖关系

### 目标依赖

| Target | 目标状态 | 必要依赖 | 可移除依赖 |
|---|---|---|---|
| `ui` | 保留 | `SDL3::SDL3`、`EnTT::EnTT`、`asio::asio`、`yogacore`、`spdlog::spdlog_header_only`、`freetype`、`harfbuzz`、`Eigen3::Eigen`、`cmrc`/`ui_fonts`、Windows 下 `dwmapi`/`comctl32` | `utils`、`shared`、`net`、`server`、`kcp`、`absl`、`nlohmann_json`、`mimalloc-static` |
| `ui_fonts` | 保留，条件构建 | `cmrc`、UI shader/font/icon 资源 | 非 UI 资源 |
| `example_pestmankill_view` | 新增 | `ui`；必要时 Windows 控制台 UTF-8 本地函数 | `utils`、`shared`、`net`、`absl`、`json`、`mimalloc` |
| `ui_tests` | 保留，可选 | `ui`、`GTest::gmock`、`GTest::gtest`；如测试直接 include 第三方则保留对应链接 | `utils`、`net_tests` |

### 第三方目录保留建议

必须保留：`third_party/asio`、`third_party/cmrc`、`third_party/entt`、`third_party/SDL`、`third_party/stb`、`third_party/yoga`、`third_party/eigen-5.0.0`、`third_party/freetype`、`third_party/harfbuzz`、`third_party/spdlog`、`third_party/googletest`（若保留测试）。

可在构建断开后删除或迁出：`third_party/abseil-cpp`、`third_party/kcp`、`third_party/json`、`third_party/mimalloc`、`third_party/HFSM2`、`third_party/thorvg-main`。其中 `json` 当前只被 `shared/net/server/client` 旧 target 使用；若未来 UI 示例需要 JSON 配置，再作为新需求引入。

## 分步骤实施规划

| 步骤 | 优先级 | 可派发任务 | 涉及文件/目录 | 前置依赖 | 验收点 |
|---|---|---|---|---|---|
| 1 | P0 | 建立纯 UI 构建入口：顶层 project 名称可改为 UI 库名；顶层只 `add_subdirectory(src/ui)`，增加 `option(BUILD_EXAMPLES ON)` 与 `option(ENABLE_BUILD_TESTS OFF)` 控制 example/test | `CMakeLists.txt` | 无 | 配置阶段不再生成 `PestManKillClient`、`PestManKillServer`、`net`、`shared`、`utils` target。 |
| 2 | P0 | 裁剪顶层第三方加载：仅保留 UI/example/test 实际需要的 `add_subdirectory(third_party/...)`；先不要物理删除目录 | `CMakeLists.txt` | 步骤 1 | `cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Debug` 能配置到 UI target；无旧网络/服务端 target 报错。 |
| 3 | P0 | 新建 example 结构与 CMake：添加 `example/pestmankill_view/CMakeLists.txt`，生成示例可执行程序，链接 `ui` | `example/pestmankill_view/CMakeLists.txt`、顶层 `CMakeLists.txt` | 步骤 1 | `cmake --build build --target example_pestmankill_view` 有可构建目标。 |
| 4 | P0 | 迁移旧客户端入口：把 `src/client/main.cpp` 迁到 example；去除 `src/utils/Functions.h` 依赖，改为示例本地 Windows 控制台 UTF-8 初始化或直接删除该调用 | `src/client/main.cpp` -> `example/pestmankill_view/main.cpp` | 步骤 3 | 示例入口只 include 标准库、`ui.hpp`、迁移后的 view 头。 |
| 5 | P0 | 迁移旧 View 层：把 `src/client/View/menu.h`、`Mainwindow.h` 迁到 example；把 `src/utils/Logger.h` 替换为 UI 自带日志能力或示例本地轻量宏；保留界面 DSL 行为 | `src/client/View/*` -> `example/pestmankill_view/View/*` | 步骤 3、4 | View 不再 include `src/utils`、`src/shared`、`src/net`、`src/server`。 |
| 6 | P0 | 删除旧客户端 target：移除 `src/client/CMakeLists.txt` 入口；物理删除或迁出 `src/client` 剩余目录，包括 `Interface` 业务接口 | `src/client`、顶层 `CMakeLists.txt` | 步骤 4、5 | 仓库内不再有 `PestManKillClient` target；example 替代其展示职责。 |
| 7 | P0 | 删除非 UI 模块 target 与源码：物理删除或迁出 `src/server`、`src/net`、`src/shared`；同步移除任何 CMake 引用 | `src/server`、`src/net`、`src/shared`、顶层 `CMakeLists.txt` | 步骤 1、2 | `rg "add_subdirectory\(src/(server|net|shared)"` 无结果；配置不引用这些目录。 |
| 8 | P1 | 处理 `src/utils`：确认 UI/example/test 无 include 后物理删除或迁出；如需日志/注册表/事件，统一使用 `src/ui/singleton/*`；如需控制台 UTF-8，只放在 example 私有实现 | `src/utils`、`example/...`、`tests/unittest/ui/CMakeLists.txt` | 步骤 4、5 | `rg "src/utils|target_link_libraries\(.*utils"` 在项目源码和测试中无有效结果。 |
| 9 | P1 | 调整 UI 测试入口：`tests/unittest/CMakeLists.txt` 只添加 `ui`；删除 `tests/unittest/net`；`ui_tests` 去掉 `utils` 链接，保留必要 include 目录 | `tests/unittest/CMakeLists.txt`、`tests/unittest/ui/CMakeLists.txt`、`tests/unittest/net` | 步骤 7、8 | `ENABLE_BUILD_TESTS=ON` 时只构建 `ui_tests`。 |
| 10 | P1 | 清理第三方目录：在构建通过后物理删除或迁出非 UI 第三方；更新 `.gitmodules` 或相关说明 | `third_party/abseil-cpp`、`kcp`、`json`、`mimalloc`、`HFSM2`、`thorvg-main`、`.gitmodules` | 步骤 2、7、9 | `rg "absl::|kcp|nlohmann_json|mimalloc" CMakeLists.txt src tests example` 无旧依赖。 |
| 11 | P2 | 清理业务资源与文档入口：删除或迁出 `resource/`；确认根部 `assets/` 是否仍被引用；README 改为 UI 库使用、构建、example 说明 | `resource`、`assets`、`README.md` | 步骤 3、7 | README 不再描述 PestManKill 客户端/服务端架构；资源目录只剩 UI/example 必需内容。 |
| 12 | P2 | 最终验证与残留扫描：执行配置、构建 UI、构建 example、可选测试；扫描旧模块名与 target 名残留 | 全仓 | 步骤 1-11 | 构建成功；旧业务模块仅在历史文档或迁移记录中出现。 |

## CMake 调整重点

- 顶层 `CMakeLists.txt` 应从“游戏全项目聚合”变成“UI 库项目入口”。建议新增 `BUILD_EXAMPLES` 控制 `example`，保留 `ENABLE_BUILD_TESTS` 控制 UI 单测。
- `src/ui/CMakeLists.txt` 目前无需依赖 `src/utils` 或 `src/shared`，但建议把 `target_compile_features(ui INTERFACE cxx_std_23)` 这类客户端侧误写配置从旧 `src/client/CMakeLists.txt` 移回 UI target 或删除重复定义。
- example target 不应链接 `shared`、`utils`、`nlohmann_json`、`absl`、`mimalloc-static`。它只负责演示 UI 控件与 DSL。
- 测试 target 若继续直接 include `src/ui/...`，可保留 `${CMAKE_SOURCE_DIR}` include；长期可改为只 include 安装/公开头，但这不是本工作包必要范围。

## 风险与缓解

| 风险 | 影响 | 缓解 |
|---|---|---|
| 误删 UI 间接依赖 | 配置或编译失败 | 先断开 CMake target，再物理删除目录；每删一类依赖后执行窄构建。 |
| example 迁移后 include 路径失效 | 示例不可构建 | example 使用相对 include 或私有 include 目录；不要继续引用 `src/client`。 |
| 日志宏替换不一致 | View 示例编译失败或日志不可用 | 优先使用 `src/ui/singleton/Logger.hpp` 已有能力；若 UI 未公开宏，则在 example 私有头中做最小包装。 |
| `utils::functions::SetConsoleToUTF8()` 删除后 Windows 中文输出变化 | 控制台日志中文显示可能异常 | 示例本地实现 Windows 控制台 UTF-8 初始化，或接受示例不保证控制台编码；UI 窗口文本不依赖该函数。 |
| 第三方目录物理删除过早 | 难以定位缺失依赖 | 第三方先从 CMake 断开，构建通过后再删除目录。 |
| 测试仍链接 `utils` 或构建 `net_tests` | `ENABLE_BUILD_TESTS=ON` 失败 | 同步调整 `tests/unittest`，将 net 测试归档/删除。 |
| 文档/README 仍描述客户端/服务端 | 项目定位混乱 | 最后阶段更新 README 和架构说明，明确这是 UI 库和 example。 |

## 验证建议

1. 配置验证：`cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Debug`。
2. UI 库构建：`cmake --build build --target ui --config Debug`。
3. 示例构建：`cmake --build build --target example_pestmankill_view --config Debug`。
4. 测试配置：`cmake -G Ninja -B build-tests -DCMAKE_BUILD_TYPE=Debug -DENABLE_BUILD_TESTS=ON`。
5. UI 测试：`cmake --build build-tests --target ui_tests --config Debug`，再用 `ctest --test-dir build-tests --output-on-failure`。
6. 残留扫描：检查 `src/net`、`src/server`、`src/shared`、`src/utils`、`PestManKillClient`、`PestManKillServer`、`target_link_libraries(... utils/shared/net)` 是否仍在源码/CMake 中有效出现。

## 待确认问题

1. 项目/target 命名：是否将 CMake project 从 `PestMan` 改为更中性的 UI 库名，例如 `PmkUi` 或 `UiLib`？
2. 非 UI 源码处理方式的落点：物理删除即可，还是需要迁到 `archive/`、单独分支或外部备份目录？用户已确认“物理迁移或删除源码”，但尚未指定迁移位置。
3. example 命名：是否保留 `PestManKill` 品牌作为 `example_pestmankill_view`，还是改为通用 `ui_demo`？
4. 根部 `assets/` 是否有计划作为公共 UI 资源入口？当前 UI 构建使用 `src/ui/assets`，根部 `assets` 未在本次扫描中发现必要性。
5. `third_party/googletest` 是否随测试保留；若仓库只交付库和 example、暂不保留测试，可进一步移除。
