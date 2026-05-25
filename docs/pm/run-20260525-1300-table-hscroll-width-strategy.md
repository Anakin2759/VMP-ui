# 项目经理协调记录 - Table 横向滚动条 + 宽度策略 + 最小宽高

- 时间：2026-05-25 13:00
- 输入来源：用户需求 — 修复table滚动条问题、支持横向滚动条、固定宽度策略、比例/自适应列宽分配、最小行列宽度
- 本轮范围：全流程
- 验收标准：cmake --build build --config Debug 通过；横向滚动条可见、列宽策略可配置

## 工作包

| # | 工作包 | Agent | 输入 | 产物 | 状态 |
|---|---|---|---|---|---|
| 1 | 实现全部 Table 功能扩展 | 代码工厂 | 本文档规划 | 6 个文件变更 | 完成 |
| 2 | 构建验证 | 测试构建闭环 | 工作包 1 产物 | `docs/test-reports/run-20260525-1718.md` | 完成 |

## 规划详情

### 涉及文件
1. `src/common/Policies.hpp` — 新增 `TableColumnSizing` 枚举
2. `src/common/components/Data.hpp` — `TableInfo` 新增 `columnSizing`、`minColumnWidths`、`minRowHeight`
3. `src/api/Table.hpp` — 新增 API 声明和 Chain DSL
4. `src/api/Table.cpp` — 新增函数实现
5. `src/renderers/TableRenderer.hpp` — `TableRenderState` 新增 `scrollOffsetX`、`contentWidth`、`effectiveRowHeight`
6. `src/renderers/TableRenderer.cpp` — 更新 computeColWidths / updateScrollArea / makeRenderState / collect / 所有渲染方法
7. `src/renderers/ScrollBarRenderer.hpp` — 新增水平滚动条渲染
8. `src/systems/StateSystem.cpp` — 修复 `tryEmitTableCellClicked` 横向滚动列检测

### 详细设计

#### TableColumnSizing 枚举（Policies.hpp）
```cpp
enum class TableColumnSizing : uint8_t {
    EQUAL,        ///< 均等：所有列等宽，总和 == 可见宽度（默认）
    FIXED,        ///< 固定：columnWidths 直接使用，超出时横向滚动
    PROPORTIONAL, ///< 比例：columnWidths 作为权重按可见宽度分配（应用 minColumnWidths 下界）
    ADAPTIVE,     ///< 自适应：columnWidths>0 的列固定，其余列均分剩余空间
};
```

#### TableInfo 新增字段（Data.hpp）
在 `std::vector<float> columnWidths;` 之后：
```cpp
policies::TableColumnSizing columnSizing = policies::TableColumnSizing::EQUAL; ///< 列宽策略
std::vector<float> minColumnWidths;  ///< 各列最小宽度
float minRowHeight = 0.0F;           ///< 行最小高度（0 表示不限制）
```

#### 新增 API（Table.hpp + Table.cpp）
```cpp
void SetColumnSizing(entt::entity entity, policies::TableColumnSizing sizing);
void SetMinColumnWidths(entt::entity entity, std::vector<float> minWidths);
void SetMinRowHeight(entt::entity entity, float height);
void SetRowHeight(entt::entity entity, float height);
```
Chain DSL:
```cpp
inline auto TableColumnSizingMode(policies::TableColumnSizing sizing);
inline auto TableMinColumnWidths(std::vector<float> minWidths);
inline auto TableMinRowHeight(float height);
inline auto TableRowHeight(float height);
```

#### TableRenderState 更新（TableRenderer.hpp）
新增字段：
```cpp
float scrollOffsetX = 0.0F;      // 水平滚动偏移
float contentWidth = 0.0F;       // 所有列宽之和
float effectiveRowHeight = 0.0F; // max(rowHeight, minRowHeight)
```

#### computeColWidths 更新（TableRenderer.cpp）
根据 `info.columnSizing` 分支：

**EQUAL（默认）**：
```
w = totalWidth / columnCount; each = max(w, minColumnWidths[i])
```
注意：如果 minWidth 导致总宽超出 totalWidth，contentWidth > totalWidth，触发横向滚动

**FIXED**：
```
直接使用 columnWidths[i]，每列 clamp 到 minColumnWidths[i]
若 columnWidths 为空则等分
```

**PROPORTIONAL**：
```
1. 将 columnWidths 作为权重（为空则各列权重=1）
2. totalWeight = sum(weights)
3. proportionalW[i] = (weight[i] / totalWeight) * totalWidth
4. width[i] = max(proportionalW[i], minColumnWidths[i])
5. 若 minWidth 夹紧导致超出，contentWidth > totalWidth（触发横向滚动）
```

**ADAPTIVE**：
```
1. 有 columnWidths[i]>0 的列：固定宽度 = max(columnWidths[i], minColumnWidths[i])
2. 剩余空间 = totalWidth - fixedTotal
3. 无固定宽度的列：flexW = max(remaining / flexCount, minColumnWidths[i])
```

#### updateScrollArea 更新（TableRenderer.cpp）
```cpp
// 现有：
const float contentHeight = rowCount * effectiveRowHeight;
const float maxScrollY = max(0, contentHeight - viewportHeight);
scrollOffset.y() = clamp(y, 0, maxScrollY);
state.scrollOffsetY = scrollOffset.y();

// 新增：
const float maxScrollX = max(0, contentWidth - totalWidth);
scrollOffset.x() = clamp(x, 0, maxScrollX);
state.scrollOffsetX = scrollOffset.x();
```

#### collect 更新（TableRenderer.cpp）
为表头部分加 scissor，以支持横向裁剪：
```cpp
// 表头渲染
renderHeaderBackground(*info, context, state);
const SDL_Rect headerScissor{tableX, tableY, totalWidth, headerHeight};
context.pushScissor(headerScissor);
renderHeaderText(*info, context, state);
context.popScissor();

// 表体渲染（保持现有 bodyRect scissor）
```

#### 渲染方法中应用 scrollOffsetX
- `renderHeaderText`：`float headerX = state.tableX - state.scrollOffsetX;`
- `renderBodyGrid`：
  - 竖向分隔线：`float columnX = state.tableX - state.scrollOffsetX;`（只绘制在可见范围内）
  - 横向分隔线：`{tableX, rowY, totalWidth, 1.0}` — 不横向滚动，始终覆盖可见宽度
- `renderCellText`：`float cellX = state.tableX - state.scrollOffsetX;`
- `renderHeaderSeparators`：
  - 表头-表体分隔线：`{tableX, bodyY, totalWidth, 1.0}` — 不横向滚动
  - 列头分隔线：`float columnX = state.tableX - state.scrollOffsetX;`

rowBackgrounds 不需要横向滚动（行背景始终覆盖可见宽度）。

#### ScrollBarRenderer 更新（ScrollBarRenderer.hpp）
在现有竖向滚动条绘制后，添加水平滚动条绘制：
```cpp
bool hasHorizontalScroll = (scrollArea.scroll == policies::Scroll::HORIZONTAL ||
                             scrollArea.scroll == policies::Scroll::BOTH);
if (hasHorizontalScroll && scrollArea.contentSize.x() > size.x()) {
    // 渲染水平滚动条（底部）
    // trackHeight = 12.0, 位置 = (pos.x, pos.y + size.y - 12 - 2)
    // 逻辑同竖向滚动条，但水平方向
}
```

#### StateSystem.cpp — tryEmitTableCellClicked 修复
横向滚动时，列命中检测需要加上水平滚动偏移：
```cpp
float bodyY = localY - info->headerHeight;
float contentX = localX;  // 将 localX 转换为 contentX
if (const auto* scroll = Registry::TryGet<components::ScrollArea>(hitEntity)) {
    bodyY += scroll->scrollOffset.y();
    contentX += scroll->scrollOffset.x();  // NEW
}

const float effectiveRowH = std::max(info->rowHeight, info->minRowHeight);
const int row = static_cast<int>(bodyY / effectiveRowH);  // 使用 effectiveRowH

// 列命中使用 contentX 而不是 localX
// 若使用 PROPORTIONAL/FIXED/ADAPTIVE，用 computeColWidths 等价逻辑
// 简化：总是重新计算实际列宽后用 contentX 定位列
```
注意：StateSystem.cpp 中无法直接调用 `TableRenderer::computeColWidths`（不同模块），需要内联实现相同的列宽计算逻辑，或将该逻辑提取到 `ui::table` 命名空间的工具函数中。
推荐方案：在 `src/api/Table.hpp` 中声明 `ComputeColumnWidths(const components::TableInfo&, float tableWidth) -> std::vector<float>`，在 `Table.cpp` 中实现，StateSystem.cpp 和 TableRenderer.cpp 共同使用。

## 调度时间线
- 17:18 代码工厂完成 Table 横向滚动、列宽策略、最小行列宽与命中检测修复。
- 17:18 测试构建闭环完成 Debug target=all 构建与 ui_tests 验证。

## 待用户决策
无

## 结论
- 状态：完成
- 关键产物：Table 修复源码变更；`docs/test-reports/run-20260525-1718.md`
- 验证结果：`cmake --build d:/test/PMK/build --config Debug` 通过；CTest 90 个用例全部通过；问题日志新增 0 条。
