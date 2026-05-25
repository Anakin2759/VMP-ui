# 项目经理协调记录 - C30 大型 .hpp-only 拆分

- 时间：2026-05-25 12:00
- 输入来源：需求文档 C30 — 大量 `.hpp`-only 的 system / renderer 仍未拆分
- 本轮范围：实现 / 全流程
- 验收标准：三个目标文件完成 `.hpp`/`.cpp` 拆分；`CMakeLists.txt` 已更新；`cmake --build build --config Debug` 成功通过

## 工作包

| # | 工作包 | Agent | 输入 | 产物 | 状态 |
|---|---|---|---|---|---|
| 1 | 拆分三个大型头文件为 hpp+cpp | 代码工厂 | 本文档规划 | 六个文件变更 + CMakeLists.txt | 待办 |
| 2 | 构建验证 | 测试构建闭环 | 工作包 1 产物 | 构建报告 | 待办 |

## 调度时间线

- 12:00 创建协调文档
- 12:01 派发工作包 1 → 代码工厂

## 规划详情

### 目标文件

| 文件 | 当前行数 | 可移至 .cpp 的内容 | 必须留在 .hpp 的内容 |
|------|---------|-------------------|---------------------|
| `src/systems/HitTestSystem.hpp` | 371 | 所有非模板成员方法实现体 | 类声明、模板方法 `connectInvalidate*` / `disconnectInvalidate*` |
| `src/systems/StateSystem.hpp` | 1233 | 所有方法实现体（无模板） | 类声明、嵌套结构体声明、成员变量 |
| `src/managers/TextTextureCache.hpp` | 430 | 所有私有/公有方法实现体（无模板） | 类声明、嵌套类型声明、常量 |

### HitTestSystem 拆分要点

**留在 .hpp 的模板方法**（必须保留在头文件，因为是模板）：
```cpp
template <typename Component> void connectInvalidateConstructDestroy()
template <typename Component> void disconnectInvalidateConstructDestroy()
template <typename Component> void connectInvalidateConstructUpdateDestroy()
template <typename Component> void disconnectInvalidateConstructUpdateDestroy()
```

**移入 .cpp 的方法**：
- `registerHandlersImpl()` / `unregisterHandlersImpl()`
- `static bool isPointInRect(...)`
- `static Vec2 getAbsolutePosition(...)`
- `static entt::entity findRootWindow(...)`
- `std::vector<entt::entity> getZOrderedInteractables(...)`
- `entt::entity findHitEntity(...)`
- `void invalidateAllCaches()`
- `void markGlobalHitCacheDirty()`
- `void markHitCacheDirty(...)`
- `void processPendingCacheInvalidationTags()`
- `static bool isEntityInteractable(...)`
- `static int calculateZOrder(...)`
- `entt::entity resolveHitEntity(...)`
- `void onRawPointerMove(...)` / `onRawPointerButton(...)` / `onRawPointerWheel(...)`

### StateSystem 拆分要点

StateSystem 无模板成员，所有方法实现体均可移入 .cpp。
嵌套 struct（`SliderStateHelpers`、`ScrollbarStateHelpers`、`PointerStateHelpers`、`WindowStateHelpers`、`EndFrameStateHelpers`）的声明留在 .hpp，方法实现体全部移入 .cpp。

**头文件保留**：类声明、枚举 `ScrollbarHitType`、嵌套 struct 声明（方法声明）、成员变量声明。

**注意**：`ScrollbarHitType` 是 `StateSystem` 的公共枚举类型，需要在头文件中声明。

### TextTextureCache 拆分要点

TextTextureCache 无模板成员，所有方法实现体均可移入 .cpp。

**头文件保留**：类声明、`CacheStats` 嵌套结构体声明、`CacheEntry` 嵌套结构体声明、常量 `MAX_CACHE_SIZE`/`EVICTION_BATCH`、枚举 `R8UnormSampledSupportState`、成员变量声明。

**移入 .cpp 的方法**：构造函数、析构函数、`clear()`、`getStats()`、`size()`、`getOrUpload()`、所有私有方法。

### CMakeLists.txt 变更

在 `src/CMakeLists.txt` 的 `UI_SOURCES` 列表中追加：
```cmake
systems/HitTestSystem.cpp
systems/StateSystem.cpp
managers/TextTextureCache.cpp
```

## 待用户决策

无

## 结论

- 状态：完成
- 关键产物：
  - `src/systems/HitTestSystem.cpp`（新建）
  - `src/systems/StateSystem.cpp`（新建）
  - `src/managers/TextTextureCache.cpp`（新建）
  - `src/CMakeLists.txt`（追加三个新 .cpp 到 UI_SOURCES）
  - `docs/test-reports/run-20260525-1200.md`（构建报告）
- 修复说明：StateSystem.hpp 中 16 个方法内联体未完全清除，构建闭环自修后构建通过（exit code 0）
