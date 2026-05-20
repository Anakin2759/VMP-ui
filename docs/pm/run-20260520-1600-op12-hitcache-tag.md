# 项目经理协调记录 - OP-12 HitTest 缓存失效改为 Tag 驱动

- 时间：2026-05-20 16:00
- 输入来源：用户请求“整理文档，继续优化”
- 本轮范围：实现 OP-12 + 构建验收 + 文档同步
- 验收标准：
  1. HitTestSystem 不再维护大量 OnConstruct/OnDestroy 对称断连代码
  2. 使用 HitCacheInvalidateTag 触发缓存失效重建
  3. ui 目标构建通过
  4. OP-12 / C7 文档状态同步为已完成

## 工作包
| # | 工作包 | Agent | 输入 | 产物 | 状态 |
|---|---|---|---|---|---|
| 1 | OP-12 代码落地 | 代码工厂 | HitTestSystem.hpp + Tags.hpp | 源码变更报告 | ✅ 完成 |
| 2 | 定向构建验收 | 测试构建闭环 | Debug + target=ui | 构建报告 | ✅ 完成 |
| 3 | 文档状态同步（OP-12/C7） | 代码工厂 | 两份 docs | 文档变更报告 | ✅ 完成 |

## 调度时间线
- 16:00 创建协调记录
- 16:02 派发工作包 #1，完成 HitCacheInvalidateTag 与 Tag 驱动改造
- 16:05 派发工作包 #2，ui 目标构建通过
- 16:07 派发工作包 #3，完成 OP-12/C7 文档状态同步

## 待用户决策
- [ ] 无

## 结论
- 状态：✅ 完成
- 关键产物：
  - src/common/Tags.hpp（新增 HitCacheInvalidateTag）
  - src/systems/HitTestSystem.hpp（统一打标记函数 + 批量清理调用点）
  - docs/test-reports/run-20260520-1422.md（构建验收报告）
  - docs/OPTIMIZATION_SUGGESTIONS.md（OP-12 标记已完成）
  - docs/ARCHITECTURE_CRITIQUE.md（C7 标记已完成）
- 下一工作包：OP-13（TableInfo 事件化）或 OP-14（Image 句柄 RAII）
