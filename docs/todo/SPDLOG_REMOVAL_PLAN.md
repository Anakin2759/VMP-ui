# spdlog 移除与轻量日志迁移规划

> 日期：2026-10-08  
> 状态：2026-10-09 策略 A 已确认；核心源码已修改，构建与回归尚待验证  
> 目标：移除 VMP-ui 对 spdlog 及其内置 fmt 的依赖，保留必要的诊断能力和现有日志调用形态。

## 1. 决策摘要

**保留日志能力，移除 spdlog 实现依赖。**

VMP-ui 是可嵌入的 UI 库，需要保留 GPU 初始化失败、资源加载失败、渲染降级等诊断信息，但不必将第三方日志框架作为固定依赖。

迁移优先保留 `utils::Logger`、`UiRuntime::logger()` 以及 `debug/info/warn/error` 调用形态，只替换格式化和输出实现。不要把本次工作扩大为删除所有 Logger 注入、重写 Manager/System 错误处理或改变 Runtime 生命周期。

本文选择“通过现有 Logger 外观集中替换实现”的路线，调整了 `docs/architecture-review-2026-09-13.md` 第 3.3.2 节“不推荐当前阶段直接移除”的阶段性建议。原评审对线程安全、输出能力和职责边界的风险判断仍有效；实施完成前不得把原文当作已经失效的历史事实直接删除。

## 2. 迁移前基线（非当前实现）

| 位置 | 当前行为 | 迁移影响 |
| --- | --- | --- |
| `src/utils/Logger.hpp` | 包装 spdlog，提供四个等级方法、调用点位置、控制台和滚动文件输出 | 核心替换点 |
| `src/core/UiRuntime.hpp` | 每个 Runtime 持有独立 Logger | 保留所有权与访问接口 |
| `src/core/Application.cpp` | 使用内部 Logger，并配置日志文件路径 | 保持路径配置语义，核实失败处理 |
| `include/ui/api/Log.hpp` | 公共日志使用标准库格式化和函数指针回调 | 不需要依赖 spdlog |
| `src/api/Log.cpp` | 公共日志独立过滤并通知回调；`SetFilePath()` 当前为空实现 | 不重新绑定内部 Logger |
| `src/pch.hpp` | 包含 spdlog 和 sink 头 | 删除相关 include |
| 根及 `src/CMakeLists.txt` | 注册 spdlog 子目录，并配置内部 target 依赖 | 清理构建依赖 |
| `tests/unittest/CMakeLists.txt`、`tests/benchmark/CMakeLists.txt` | 链接 spdlog header-only target | 清理测试依赖 |
| `tests/unittest/test_FallbackRenderer.cpp` | 自定义 spdlog sink 统计日志 | 改为项目自己的测试输出回调 |

内部 Logger 当前默认行为：

- 控制台与文件双输出，文件路径为 `logs/vmpui.log`。
- 文件大小上限为 5 MiB，保留 1 个轮转备份。
- 默认等级为 debug；warn 及以上触发刷新。
- 文件记录包含时间、等级、源文件、行号和函数名。
- 默认构造即创建文件 sink；文件打开失败可能影响 Logger/Runtime 构造。
- 多个 Runtime 创建独立 sink，但默认指向同一日志路径；单个 sink 的锁不等于多个 Logger 之间的协调。

公共 `ui::log` 与内部 `utils::Logger` 是独立通道。当前 `ui::log::SetLevel()`、`SetCallback()`、`SetFilePath()` 不能被理解为内部 Logger 的配置入口。

## 3. 范围与非目标

### 3.1 本次范围

- 替换内部 Logger 的 spdlog/fmt 类型和实现。
- 保留常用日志调用与调用点 `std::source_location`。
- 提供等级过滤、标准错误输出、测试可捕获的输出接口。
- 处理现有文件输出与 `reconfigure()` 的兼容需求。
- 清理 PCH、CMake、测试及第三方目录中的项目依赖。
- 补齐日志专项测试，验证 Debug/Release 与发行消费者。

### 3.2 明确不做

- 不将所有日志替换成散落的 `std::cout` 或 `printf`。
- 不引入另一个第三方日志框架，也不为兼容而单独保留 fmt。
- 不重构 EventLoop 同步机制、Runtime 所有权或 System 注入结构。
- 不在同一批次强制将全部 Manager 日志迁移到 System。
- 不合并公共日志与内部日志通道。
- 不实现异步日志队列、网络 sink、复杂 pattern DSL 或完整 spdlog 替代品。
- 不承诺未经测量的构建时间、内存占用和产物大小收益。

## 4. 目标设计

### 4.1 保留 Logger 外观，隔离输出实现

- 保留 `debug/info/warn/error` 方法和 `UiRuntime::logger()`。
- `LogLocation` 改用标准库字符串视图与 `std::source_location`，不暴露第三方类型。
- 格式化采用 `std::format` / `std::vformat`；根据现有调用中的动态格式字符串决定适配方式，不机械地全部改成编译期 `std::format_string`。
- 先判断日志等级，再格式化消息。注意：这不能避免调用方参数表达式在进入函数前求值。
- 非模板输出、文件管理与同步逻辑尽量放入 `Logger.cpp`，避免继续扩张重型头文件。
- 移除接受 `std::shared_ptr<spdlog::logger>` 的构造函数和 `getLogger()`；先核实所有使用点，再以项目内部输出接口替换。

### 4.2 轻量输出接口

内部日志记录应包含等级、消息和调用点。允许注入同步输出回调，以便单元测试捕获记录或由内部配置接管输出。

契约要求：

- 消息视图只在本次调用期间有效；异步保存必须复制。
- 不持有 Logger 配置锁调用外部回调，防止回调重入死锁。
- 明确回调是否可被并发调用，以及替换/解除回调时的生命周期要求。
- 防止日志回调再次记录日志造成无限递归。
- 格式化或输出失败不能替代原有业务错误、改变 `Result` 传播路径。
- 异常路径采用尽力而为的最小降级输出；不通过再次调用 Logger 报告 Logger 自身失败。
- 析构和刷新路径不得向外传播异常。

内部输出接口不直接复用进程级 `ui::log::SetCallback()`，以免无意合并两套通道。

### 4.3 文件输出策略：A 已确认（2026-10-09）

用户已明确批准 A：取消内部落盘与轮转，默认 stderr，独立回调可接管输出；宿主负责文件。
`Logger::reconfigure()` 已移除，不保留静默 no-op。AppConfig 路径字段仅兼容保存旧值，
Application 对非空旧配置输出废弃提示；空值不会创建默认文件。公共 `ui::log` 不修改、不合并。
以下为决策时的备选范围，当前仅实施 A：

| 策略 | 内容 | 代价 |
| --- | --- | --- |
| A：最小诊断方案（推荐） | 内部默认标准错误输出，允许回调接管；文件及轮转由应用负责 | 实现简单，但改变默认落盘和内部 `reconfigure()` 行为，需明确批准 |
| B：兼容文件方案 | 保留默认路径、5 MiB 上限、1 个备份、warn 刷新和路径重配置 | 需维护文件轮转、目录创建、并发与失败降级 |

**尚未确认时，不得擅自删除现有落盘能力。** 如果仍要求完整保留复杂文件输出行为，应重新评估移除收益，避免重新实现一个日志框架。

选择 B 时必须补充以下约定：

- 多 Logger 写同一路径的进程内同步和共享文件状态；每实例 mutex 不足以解决此问题。
- 暂不承诺跨进程安全轮转；明确限制并提供应用侧自定义路径能力。
- 路径重配置与正在进行的输出互斥；打开新路径失败时保留可用的旧输出或降级到标准错误。
- 文件打不开、目录不可写、写入失败和轮转失败不能使正常 UI 构造或业务流程失败。

### 4.4 兼容性边界

必须保留：

- 常规内部日志调用形态及 source location 的调用点语义。
- 公共日志 API 的回调、等级过滤和无 Runtime 可用性。
- 原有 GPU/资源/渲染诊断信息及关键告警计数语义。
- 公共头零第三方依赖的发行约束。

可在明确说明后变化：

- 控制台是否彩色、时间戳排版和输出到 stdout/stderr 的选择。
- 内部 Debug/Release 默认等级；本轮默认保持原行为，调整等级作为显式决策。
- 文件输出行为仅按已批准的 A/B 策略变化。

## 5. 分阶段实施

| 阶段 | 工作内容 | 交付与退出条件 |
| --- | --- | --- |
| P0：冻结契约 | 核实日志调用、动态格式字符串、自定义格式类型、底层 Logger 访问、路径配置；确认 A/B 策略 | 完整使用清单与已确认行为表 |
| P1：实现替代 | 新增轻量输出实现；保留 Logger 外观；添加独立日志专项测试；替换 spdlog 专用测试 sink | Logger 本身及日志捕获测试通过 |
| P2：清理依赖 | 删除 PCH include、各 target 链接及根 CMake 子目录配置；检查遗漏的间接头依赖 | 新构建目录可配置并构建相关目标 |
| P3：回归验证 | 运行日志、fallback、Runtime、API 等测试；验证完整构建和发行消费者 | 测试结果与退出码有记录 |
| P4：清理交付 | 移除第三方目录/子模块登记（若存在），更新约定及文档，确认仓库无生产依赖残留 | 源码、构建、安装和文档一致 |

P1 可以暂时保留 spdlog 构建依赖便于分步验证，但不能将“双日志后端长期并存”作为最终状态。删除第三方目录前，先确认其不是其他依赖的实际构建输入。

## 6. 预计修改文件

| 文件 | 预期修改 |
| --- | --- |
| `src/utils/Logger.hpp` | 去除第三方类型，保留模板入口，定义内部等级与输出契约 |
| `src/utils/Logger.cpp`（新增） | 输出、同步、失败降级，以及按策略选择的文件实现 |
| `src/CMakeLists.txt` | 注册新实现，清理内部依赖和聚合 target 的相关配置 |
| `src/pch.hpp` | 删除 spdlog include，补齐必要标准库直接包含 |
| `CMakeLists.txt` | 删除 spdlog 子目录及相关配置 |
| `src/core/Application.cpp` | 按确认后的路径策略调整配置/失败处理，必要时修改 |
| `tests/unittest/test_FallbackRenderer.cpp` | 用项目内部捕获接口替代 spdlog sink |
| `tests/unittest/test_Logger.cpp`（新增） | 内部日志专项测试 |
| `tests/unittest/test_Log.cpp` | 验证公共日志语义不变，必要时补充用例 |
| `tests/unittest/CMakeLists.txt` | 注册测试并移除依赖 |
| `tests/benchmark/CMakeLists.txt` | 移除依赖 |
| `tools/check_architecture_boundaries.py` | 移除过时的 spdlog target 白名单；不放宽门禁 |
| `src/api/Log.cpp` | 清理与现实现不符的 spdlog/Runtime 历史注释，不改变通道职责 |
| `.github/copilot-instructions.md` | 更新依赖与日志格式化约定 |
| `third_party/spdlog/` | 确认无依赖后移除 |

`UiRuntime`、Manager、System、Renderer 原有注入关系原则上不改；如遇格式化类型不兼容，仅做必要的局部转换。公共头检查器中的 spdlog 禁用规则继续保留，避免依赖回流。

## 7. 验证矩阵

### 7.1 Logger 专项

- 无参数、多个参数、整数、字符串、浮点精度、转义大括号和中文消息。
- 现有错误对象 `ToString()`、枚举转换及动态格式字符串兼容性。
- 等级关闭或过滤时，不进入格式化与输出后端。
- source location 指向业务调用点，而不是 Logger 包装函数内部。
- 多线程消息输出不发生记录级交错；测试回调并发契约符合实现。
- 回调异常、回调重入、格式化失败不导致递归或业务异常替换。
- 多 Runtime 创建/销毁不产生悬空回调或输出资源引用。
- 选择 B 时：目录创建、不可写路径、文件轮转、刷新、重配置失败、多实例同路径输出。

### 7.2 项目回归

- `test_Log.cpp`：公共回调、等级、无 Runtime 场景。
- fallback renderer：原日志计数、告警限次、降级诊断测试。
- Runtime 生命周期、GPU/资源失败路径相关单元测试。
- 全量单元测试；具备图形环境时运行 interaction/screenshot 回归。
- Debug/Release 构建；启用测试和 benchmark 时不存在悬空链接 target。
- 至少一次关闭 PCH 的相关目标构建，排查隐式 include 依赖。
- 公共头自包含和架构边界门禁均通过。
- 静态与共享安装消费者验证；发行配置不再要求 spdlog/fmt 包。

### 7.3 构建与依赖检查

- 优先使用新的独立构建目录做干净配置，不直接删除用户现有 `build/`。
- 如需复用旧目录，应重新配置；仅当缓存问题确认存在时再清理相应缓存。
- 搜索生产源码、测试、CMake、安装导出中的 spdlog/fmt 残留，区分历史文档和禁止依赖的门禁规则。
- 检查新生成的 target、include path 与安装清单，不能用旧 build 产物证明迁移结果。
- 如报告性能收益，应在一致工具链、配置、并行度与资源后端下比较构建耗时、峰值内存和产物大小。

## 8. 风险与回退

| 风险 | 应对 |
| --- | --- |
| fmt 与标准库格式化不兼容 | P0 收集真实类型与格式；P1 编译调用点并补充专项测试 |
| 自研输出实现范围失控 | 先确认 A/B；不加入异步队列或复杂 sink 系统 |
| 回调重入、锁竞争或悬空 | 定义回调生命周期，不持配置锁调用回调，增加并发和销毁测试 |
| 多 Logger 同路径轮转冲突 | 选择 B 时采用进程内共享协调，不误认为每实例 mutex 足够 |
| 删除 PCH 依赖后暴露缺失 include | 补直接包含并做关闭 PCH 验证 |
| 日志失败影响正常初始化 | 输出失败降级，不把日志文件打开失败变成 UI 致命错误 |
| 测试通过但安装包仍带旧依赖 | 新构建目录与独立安装前缀验证消费者和安装清单 |

迁移按 Logger 实现、构建清理、第三方目录清理组织可回退变更。发现阻断时只回退本次相关修改，不覆盖用户其他工作，也不通过关闭现有测试或放宽门禁完成验收。

## 9. 完成标准

- [x] 已确认文件输出采用 A，并记录有意改变的行为。
- [ ] 生产代码和构建 target 不再依赖 spdlog 或其内置 fmt。
- [ ] 现有 Logger 常规调用形态与 Runtime 注入关系保持稳定。
- [ ] 公共日志保持独立、无 Runtime 依赖，不意外接管内部日志。
- [ ] 关键错误与降级诊断仍可观察，日志失败不破坏业务流程。
- [ ] spdlog 专用测试替换完成，新增 Logger 专项测试通过。
- [ ] Debug/Release、PCH 检查、架构门禁及发行消费者验证完成。
- [ ] 第三方目录、构建配置、安装清单与工程约定清理一致。
- [ ] 验证结果注明实际执行范围；未执行的平台或测试不标记为通过。

## 10. 2026-10-09 实施记录（Debug 核心回归通过，完整发行验收待完成）

- `Logger.hpp/.cpp`：标准库格式化，默认 debug，可配置至 off，过滤早于格式化；
	`make_format_args(args...)` 使用命名左值，保留业务调用点 source_location。
- 非模板输出在 cpp：跨实例共用 stderr 记录锁；无动态分配的失败输出路径；
	格式化失败、回调抛出及配置失败不穿透业务调用，失败诊断尽力写 stderr。
- 回调采用拥有捕获状态的 `move_only_function`，共享快照维持在途所有权；
	调用及旧捕获对象释放均不持配置锁。回调可并发执行，捕获状态由宿主同步。
	同线程嵌套日志绕过所有内部 Logger 回调写 stderr，防止跨实例递归；替换不等待在途调用。
- 删除文件轮转常量与双 sink 注释，AppConfig/Application 明确说明废弃路径行为。
- fallback 计数改为局部捕获，取消全局可变计数。
- 注册 `Logger.cpp` 和 `test_Logger.cpp`；新增格式化、过滤、调用点、异常、并发、
	回调替换/所有权/重入及多实例 stderr 完整记录测试，9 项全部通过。
- 恢复架构门禁 `spdlog::spdlog_header_only` PUBLIC link 检测，并覆盖 `spdlog::spdlog`；
	未增加白名单或放宽债务基线。
- 前轮 CMake/PCH 依赖移除由用户说明，本轮不覆盖前轮及其他用户改动。
	`third_party/spdlog/` 暂留；不删除旧 build、日志，不提交/push。

### 实际验证证据与限制

- 已读取规划、设计原则、相关实现/调用点、格式配置及编辑器诊断。
- 主会话已核对 Git status/diff；工作区另有锚点相关用户改动，未回退或覆盖。
- 独立 `build-spdlog-removal`：Debug、tests ON、PCH OFF、clang++22，`ui_unit_tests` 构建完成；独立任务复核报告 `ninja: no work to do`。
- 专项测试：`LoggerTest.*:RenderCapabilityDiagnosticsTest.*` 共 10 项通过，退出码 0。
- 核心单元回归：`ui_unit_tests` 共 33 个测试套件、121 项测试全部通过，退出码 0；包含公共日志、Runtime、资源及渲染相关测试。
- 架构边界门禁与公共头自包含门禁通过，公共头检查 0 违规。
- 项目源码、测试和 CMake 的 spdlog 搜索仅剩防止依赖回流的门禁及说明，不存在实际 include/link 使用。
- 专用 clangd 工具不可用，使用可用编辑器诊断查询；新增告警按查询结果修正。
- 预存诊断：fallback 的相邻同类型参数、数值常量和 reinterpret_cast；
	AppConfig 的无大括号语句；Application 的可 static 方法。未作无关修整。
- 尚未验证 Release、启用 PCH、ECS/API 独立测试目标、interaction/screenshot、benchmark 与静态/共享安装消费者；不得将核心单元回归称为全项目测试通过。
- `third_party/spdlog/` 尚未物理移除，工程约定及发行清理仍待收尾。
- P0 已冻结；P1 与构建依赖移除已通过上述 Debug 验证；P3 仅完成上述子集，P4 未完成。