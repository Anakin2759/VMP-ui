# TODO 文档索引与管理规则

> 更新日期：2026-08-27
>
> 本文件是当前状态与优先级的唯一索引；执行摘要见 [`TODO_ROADMAP_SUMMARY_2026-08-27.md`](TODO_ROADMAP_SUMMARY_2026-08-27.md)。架构文档中的早期风险分析和归档报告只用于追溯。

## 当前状态

| 范围 | 状态 | 说明 |
|---|---|---|
| P0-1～P0-3 | DONE | EventLoop 发布协议、Runtime ownership、回调重入 |
| P1-1～P1-9 | DONE | dirty、事件目录、多窗口、调度、Runtime、SDL 唤醒、构建与 fixture 治理 |
| P2-1～P2-3 | DONE | System phase、ImageManager 缓存、fallback 能力矩阵 |
| P2-4 | DONE | TODO 分类、状态、重复规划和历史记录已治理 |
| 阶段 D 产品能力 | PARTIAL | Theme 最小范围已完成；Rich Text、SVG、完整 Style/DSL 仍规划中 |

## 活动规划

| 优先级 | 文档 | 状态 | 主题 |
|---|---|---|---|
| P1 | `HIDPI_SUPPORT_PLAN.md` | PARTIAL | P5 跨平台/多窗口验收 |
| P1 | `ADAPTIVE_PLATFORM_SCALING_PLAN.md` | PLANNED | 自适应平台缩放边界 |
| P1 | `RICH_TEXT_PLAN.md` | PLANNED | 富文本解析、布局和渲染 |
| P1 | `SVG_SUPPORT_PLAN.md` | NEEDS-REVIEW | SVG 资源、缓存和渲染 API |
| P2 | `SCALING_WHITE_BORDER_OPTIMIZATION_PLAN.md` | PLANNED | 缩放像素基线与修复 |
| P2 | `TEST_MODULE_REVIEW_PLAN_2026-06-04.md` | NEEDS-REVIEW | 测试分类和 CI 隔离 |
| P2 | `THEME_STYLE_SYSTEM_PLAN.md` | DONE（最小范围） | 完整 CSS/Style 能力需另立规划 |

## 背景与历史记录

- `UI_FRAMEWORK_GAP_ANALYSIS_2026-05.md`：`REFERENCE`，历史能力缺口快照，不直接产生任务。
- `problem-log-wp4a-gpu-generation-20260729.md`、`problem-log-wp4b-gpu-lifecycle-20260730.md`：`RECORD`，问题与环境事实，不是执行规划。
- `PUBLIC_EVENT_API_BOUNDARY_PLAN_2026-06-04.md`：基础公开事件边界已 `DONE`；未来跨线程协议须新建规划。
- `样式系统规划.md`：`SUPERSEDED`，统一以 `THEME_STYLE_SYSTEM_PLAN.md` 为准。
- `PESTMANKILL_BRAND_RESIDUE.md`：`DONE` 历史改名记录，后续遗留项须新建规划。
- `DEAD_CODE_AUDIT_2026-10-10.md`：`RECORD`，死代码审计（本轮已消除 3 文件 + 1 函数；`MyPFR.hpp` 按决策保留；待决策项见文档 §3）。
- 已完成验证记录位于 `docs/archive/todo-completed-2026-08-25/`。

## 状态与维护规则

规划文档首部应包含：`状态`、`最后复核`、`责任范围`、`验收`。允许状态：`PLANNED`、`ACTIVE`、`BLOCKED`、`NEEDS-REVIEW`、`PARTIAL`、`DONE`、`SUPERSEDED`、`REFERENCE`、`RECORD`。

1. 每个 P0/P1 主题只保留一个活动主规划。
2. 需求规划、问题日志、测试报告分开保存。
3. 完成项移入 `docs/archive/` 或明确标记 `DONE`，不得从历史证据中制造新的待办。
4. 被替代文档必须写明唯一事实源；禁止保留互相矛盾的状态。
5. 新遗留问题应新建规划文件，并在本索引登记。
