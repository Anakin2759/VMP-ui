---
name: lint-and-format
description: '批量对项目自有 C/C++ 源码运行 clang-format 与 clang-tidy，自动跳过 third_party / extern 等第三方目录，并生成 clang-tidy 汇总报告。Use when: 跑全量格式化, 全量 clang-tidy, 静态检查, lint 检查, format 全项目, tidy 报告, 代码风格统一, 提交前检查, pre-commit lint, repo-wide clang-format, repo-wide clang-tidy.'
argument-hint: '可选: format | tidy | all (默认 all)；附加 -Check 仅做检查不落盘'
---

# Lint & Format (clang-format + clang-tidy)

## 适用场景
- 一次性把整个仓库的自有源码用 `clang-format` 规整到 `.clang-format` 风格
- 在 CMake 构建产物基础上对自有源码跑 `clang-tidy`，并把诊断汇总成 Markdown 报告
- CI 之前的本地预检（`-Check` 模式：format 走 `--dry-run --Werror`，tidy 走只读模式）
- 始终跳过 `third_party/`、`extern/`、`build/`、`out/` 等非自有目录

## 不适用场景
- 第三方库自身的格式/检查（它们各自带 `.clang-format` / `.clang-tidy`，不要污染）
- 单文件格式化（直接在编辑器里保存即可，无需走这个流程）

## 前置条件
1. PATH 中可用 `clang-format` 与 `clang-tidy`（本机一般在 `D:\LLVM\bin\`）
2. tidy 需要 `compile_commands.json`：先用 CMake 配置过项目，默认在 `build/compile_commands.json`
3. 仓库根目录存在 `.clang-format` 与 `.clang-tidy`（本仓库已具备）

## 操作步骤

1. 确认构建目录已生成 `compile_commands.json`：
   - 若缺失，先运行 `cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Debug`（不需要 `-DENABLE_CLANG_TIDY=ON`，脚本独立调用 tidy）。
2. 运行 [run-lint-format.ps1](./scripts/run-lint-format.ps1)：
   - `pwsh ./.github/skills/lint-and-format/scripts/run-lint-format.ps1` — 默认 `all`，原地格式化 + 生成 tidy 报告
   - `... -Mode format` — 仅 clang-format
   - `... -Mode tidy` — 仅 clang-tidy 并生成报告
   - `... -Check` — 不落盘：format 走 `--dry-run --Werror`，tidy 仅打印诊断、报告仍写出
   - `... -BuildDir build-clang` — 自定义 compile_commands 所在目录
   - `... -ReportPath docs/clang-tidy-report.md` — 自定义汇总报告路径（默认即此）
3. 阅读 [docs/clang-tidy-report.md](../../../docs/clang-tidy-report.md)：
   - 顶部「Summary」按 check 名聚合计数
   - 「Findings by file」按文件列出 warning/error 行号与 check
4. 修复后重复 `... -Mode tidy` 直至 Summary 清零，或将剩余项写入 [问题日志.md](../../../问题日志.md) 跟进。

## 文件过滤规则（脚本内置，不可绕过）
- 来源：`git ls-files`（避免漏未跟踪垃圾文件，同时尊重 `.gitignore`）
- 扩展名：`c, cc, cpp, cxx, h, hh, hpp, hxx`
- 排除前缀（不区分大小写）：`third_party/`、`extern/`、`external/`、`vendor/`、`build/`、`out/`、`cmake-build*/`、`.git/`
- format 与 tidy 共用同一份过滤后的文件清单

## 完成判据
- `-Check` 模式：format 与 tidy 步骤的退出码均为 0
- 落盘模式：`git diff --check` 无空白告警，tidy 报告 Summary 段无新增 error 级别项目

## 相关
- 根配置：[.clang-format](../../../.clang-format)、[.clang-tidy](../../../.clang-tidy)
- CMake 中的 tidy 集成开关：`-DENABLE_CLANG_TIDY=ON`（构建期注入，与本 skill 互补）
