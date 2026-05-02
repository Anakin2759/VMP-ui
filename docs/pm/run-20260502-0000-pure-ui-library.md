# 项目经理协调记录 - 纯 UI 库化

- 时间：2026-05-02 00:00
- 输入来源：用户请求：把项目中其他部分先剔除，做出纯 UI 库，原本客户端界面部分用 example 文件夹展示
- 本轮范围：全流程：规划、实现、验证
- 验收标准：仓库物理迁移/删除非 UI 业务模块后，CMake 可构建纯 UI 库；原客户端界面迁移到 example 文件夹并作为可构建示例程序；构建验证完成或给出完整问题日志。

## 工作包
| # | 工作包 | Agent | 输入 | 产物 | 状态 |
|---|---|---|---|---|---|
| 1 | 影响分析与迁移规划 | 架构师 (Architect) | 用户需求、当前仓库结构、CMake/UI/client 边界 | docs/architecture/change-pure-ui-library-20260502.md | 完成 |
| 2 | 源码与构建结构落地 | 代码工厂 (Code Factory) | 工作包 #1 规划文档、用户补充决策 | 源码/CMake 变更与交付报告 | 部分完成 |
| 3 | 构建与示例验证 | 测试构建闭环 (Test-Build Loop) | 工作包 #2 交付结果与残留删除问题 | docs/test-reports/pure-ui-library-20260502.md | 完成 |
| 4 | 物理残留目录清理 | 命令行执行 | 用户明确授权直接命令行删除 | 删除命令与复扫结果 | 完成 |

## 调度时间线
- 00:00 创建协调记录。
- 00:00 派发工作包 #1 给架构师。
- 00:00 工作包 #1 完成，产物：docs/architecture/change-pure-ui-library-20260502.md。
- 00:00 用户确认：项目名 PmkUi；非 UI 源码/无用第三方直接删除；example 命名 example_ui_demo；保留 UI 单元测试。
- 00:00 派发工作包 #2 给代码工厂。
- 00:00 工作包 #2 部分完成：构建入口、example、README、UI 测试入口已调整；物理递归删除旧目录/无用第三方因工具删除未生效而阻塞。
- 00:00 派发工作包 #3 给测试构建闭环，验证当前构建状态并记录残留目录问题。
- 13:12 工作包 #3 部分完成：configure、ui、example_ui_demo、ui_tests 构建通过；CTest 中 BatchManagerTest.ClearResetsAllBatchesAndSupportsReuse 访问冲突；旧目录仍物理残留。报告：docs/test-reports/pure-ui-library-20260502.md。
- 13:12 回派代码工厂修复 BatchManagerTest 访问冲突。
- 13:19 复验完成：ui、example_ui_demo、ui_tests 构建通过；BatchManager 窄测通过；UI 全量 CTest 77/77 通过。报告追加到 docs/test-reports/pure-ui-library-20260502.md。
- 13:19 派发物理残留目录清理；第一次因网络中断失败，第二次确认当前工具无法递归删除目录树。
- 13:19 用户授权直接命令行删除；已用 PowerShell 删除旧源码、旧第三方、旧测试、resource 与根部 assets，复扫确认无残留路径、无旧有效入口引用。

## 待用户决策
- [x] 剔除方式：物理迁移或删除源码。
- [x] 示例形态：可构建 example 可执行程序。
- [x] 项目命名：PmkUi。
- [x] 删除策略：直接删除非 UI 源码和无用第三方。
- [x] 示例命名：example_ui_demo。
- [x] 测试保留：保留 UI 单元测试与 googletest。

## 结论
- 状态：完成
- 关键产物：docs/architecture/change-pure-ui-library-20260502.md；docs/test-reports/pure-ui-library-20260502.md；example/ui_demo
- 已完成：有效 CMake 拓扑已收敛为 PmkUi + ui + example_ui_demo + ui_tests；构建和 UI CTest 全绿。
- 已完成：旧源码、旧第三方、旧网络测试、resource、根部 assets 已物理删除；复扫确认无旧有效入口引用。
- 下一步：无。
