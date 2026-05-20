# 项目经理协调记录 - OP-13 TableInfo 回调迁移事件驱动

- 时间：2026-05-20 16:30
- 输入来源：用户选择继续优化项 `1`（OP-13）
- 本轮范围：OP-13 落地 + 构建验收 + 文档同步
- 验收标准：
  1. `TableInfo` 不再持有 `on_event<int,int> onCellClicked`
  2. 新增 `events::TableCellClicked { table,row,col }`
  3. 在点击表格时由系统触发 `Dispatcher::Trigger<events::TableCellClicked>`
  4. `ui` 目标构建通过
  5. OP-13 / C16 文档同步为已完成

## 工作包
| # | 工作包 | Agent | 输入 | 产物 | 状态 |
|---|---|---|---|---|---|
| 1 | OP-13 代码落地 | 代码工厂 | Components/Events/Table/StateSystem | 源码变更报告 | ✅ 完成 |
| 2 | 定向构建验收 | 测试构建闭环 | Debug + target=ui | 构建报告 | ✅ 完成 |
| 3 | 文档状态同步（OP-13/C16） | 代码工厂 | 两份 docs | 文档变更报告 | ✅ 完成 |

## 调度时间线
- 16:30 创建协调记录
- 16:33 派发工作包 #1，完成 OP-13 代码落地
- 16:36 派发工作包 #2，ui 目标构建通过
- 16:38 派发工作包 #3，完成 OP-13/C16 文档状态同步
- 16:40 追加微修：清理 Table.hpp 无用 include

## 待用户决策
- [ ] 无

## 结论
- 状态：✅ 完成
- 关键产物：
  - src/common/Components.hpp（移除 TableInfo.onCellClicked）
  - src/common/Events.hpp（新增 events::TableCellClicked）
  - src/api/Table.hpp（移除 SetOnCellClicked / TableOnCellClicked，清理无用 include）
  - src/api/Table.cpp（移除 SetOnCellClicked 定义）
  - src/systems/StateSystem.hpp（点击路径触发 TableCellClicked）
  - docs/test-reports/run-20260520-1434.md（构建验收报告）
  - docs/OPTIMIZATION_SUGGESTIONS.md（OP-13 标记已完成）
  - docs/ARCHITECTURE_CRITIQUE.md（C16 标记已完成）
- 下一工作包：OP-14（Image 句柄 RAII）
