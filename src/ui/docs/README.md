# UI Docs Index

日期：2026-03-26

## 目的

本索引用于给 `src/ui/docs` 目录建立稳定的分层规则，避免文档继续平铺堆积，也避免“主线文档”和“专题记录”混在一起使用。

从现在开始，UI 文档按以下层级理解：

1. 主线基线文档
2. 分阶段设计草案
3. 评审与覆盖记录
4. 专题研究文档
5. 参考说明文档

## 使用规则

### 规则 1

当需要确定 UI 模块当前的官方设计方向时，优先阅读主线基线文档，而不是历史评审记录。

### 规则 2

当需要推进某个阶段的改造时，优先阅读对应的 phase 草案，而不是直接从评审文档反推实现方案。

### 规则 3

专题研究文档可以保留，但默认不作为当前重构主线的优先输入。

### 规则 4

历史评审文档只负责记录判断过程和问题发现，不再承担“最新目标定义”的职责。

## 第一层：主线基线文档

这类文档定义当前 UI 模块的统一目标语言，优先级最高。

目录：`baseline/`

1. [baseline/ui-design-baseline-2026-03-26.md](baseline/ui-design-baseline-2026-03-26.md)
   定位：UI 模块 Phase 0 统一设计基线
   用途：统一保留项、重构项、runtime 职责图、系统迁移表

## 第二层：分阶段设计草案

这类文档定义某个阶段的边界、目标和迁移策略，优先级仅次于基线文档。

目录：`phase/`

1. [phase/ui-phase1-runtime-facade-draft-2026-03-26.md](phase/ui-phase1-runtime-facade-draft-2026-03-26.md)
   定位：Phase 1 Runtime Facade 草案与已落地范围说明
2. [phase/ui-runtime-roadmap-2026-03-26.md](phase/ui-runtime-roadmap-2026-03-26.md)
   定位：长期路线图
   说明：这是长期规划文档，不是阶段实现细则

## 第三层：评审与覆盖记录

这类文档保留历史判断过程，帮助追踪为什么会形成当前基线，但不直接替代基线文档。

目录：`reviews/`

1. [reviews/ui-architecture-review-2026-03-24.md](reviews/ui-architecture-review-2026-03-24.md)
   定位：第一阶段止血导向的早期评审
2. [reviews/ui-architecture-review-2026-03-26.md](reviews/ui-architecture-review-2026-03-26.md)
   定位：批判性架构评审记录
3. [reviews/ui-coverage-review-2026-03-26.md](reviews/ui-coverage-review-2026-03-26.md)
   定位：覆盖率与第二阶段输入记录

## 第四层：专题研究文档

这类文档保留对某个方向的专题判断，可作为未来候选输入，但默认不属于当前重构主线。

目录：`research/`

1. [research/ui-hfsm2-feasibility-draft-2026-03-26.md](research/ui-hfsm2-feasibility-draft-2026-03-26.md)
   定位：hfsm2 引入可行性专题评估
2. [research/cpo-suggestions.md](research/cpo-suggestions.md)
   定位：CPO 方向专题建议
3. [research/fallback-renderer-design.md](research/fallback-renderer-design.md)
   定位：fallback renderer 方案设计

## 第五层：参考说明文档

这类文档是实现说明、模块说明、能力状态表，适合作为开发参考，但不直接承担架构路线定义。

目录：`reference/`

1. [reference/BatchManager.md](reference/BatchManager.md)
   定位：BatchManager 说明文档
2. [reference/ResourceProvider.md](reference/ResourceProvider.md)
   定位：资源提供器说明文档
3. [reference/textedit-implementation-2026-02-07.md](reference/textedit-implementation-2026-02-07.md)
   定位：TextEdit 输入框功能实现总结
4. [reference/ui-components-status.md](reference/ui-components-status.md)
   定位：组件与控件实现状态分析
5. [reference/interaction-system-split-implementation-2026-03-26.md](reference/interaction-system-split-implementation-2026-03-26.md)
   定位：InteractionSystem 最小职责拆分实现记录

## 当前已清理项

以下文档已不再保留：

1. `ui-next-steps.md`

原因：

1. 该文档是功能待办视角，不再适合作为当前 UI 重构主线输入。
2. 其优先级和术语体系与 Phase 驱动文档冲突。

## 后续新增文档约定

新增文档建议遵守以下命名方式：

1. 主线基线：`ui-design-baseline-YYYY-MM-DD.md`
2. 阶段草案：`ui-phaseN-<topic>-draft-YYYY-MM-DD.md`
3. 评审记录：`ui-<topic>-review-YYYY-MM-DD.md`
4. 专题研究：`ui-<topic>-feasibility-draft-YYYY-MM-DD.md`
5. 参考说明：`<module>-design.md`、`<module>.md`、`<feature>-implementation-YYYY-MM-DD.md`

## 推荐阅读顺序

如果你第一次进入这个目录，推荐按以下顺序阅读：

1. [baseline/ui-design-baseline-2026-03-26.md](baseline/ui-design-baseline-2026-03-26.md)
2. [phase/ui-phase1-runtime-facade-draft-2026-03-26.md](phase/ui-phase1-runtime-facade-draft-2026-03-26.md)
3. [phase/ui-runtime-roadmap-2026-03-26.md](phase/ui-runtime-roadmap-2026-03-26.md)
4. [reviews/ui-architecture-review-2026-03-26.md](reviews/ui-architecture-review-2026-03-26.md)
5. [reviews/ui-coverage-review-2026-03-26.md](reviews/ui-coverage-review-2026-03-26.md)

这样可以先理解当前基线，再理解阶段目标，最后再看历史判断和专题分析。
