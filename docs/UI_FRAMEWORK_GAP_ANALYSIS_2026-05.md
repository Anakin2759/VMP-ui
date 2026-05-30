# VMP-ui UI 框架能力缺口盘点

> 日期：2026-05-28  
> 范围：对照当前 `src/api`、`src/common/components`、`src/common/Tags.hpp`、`src/systems`、`src/renderers`、`tests/unittest`、`example/ui_demo` 与现有 `docs/*PLAN.md` 文档，记录 UI 框架常见组件和基础能力中尚未规划或尚未实现的部分。

## 1. 当前已有能力基线

当前框架已经具备一条可运行的 ECS UI 主链路：

| 领域 | 当前状态 |
|---|---|
| 应用/窗口 | `CreateApplication`、`CreateWindow`、`CreateDialog`、自绘标题栏、无边框窗口平台桥接已有实现。 |
| 布局 | Yoga Flexbox 接入，已有 `VBoxLayout`、`HBoxLayout`、`Spacer`、`ScrollArea`、层级树、脏标记。 |
| 基础控件 | Button、Label、TextEdit/LineEdit/TextBrowser、Image、Icon、CheckBox、DropDown、Slider、ProgressBar、Canvas、Table 已有工厂或 API。 |
| 渲染 | Shape/Text/Icon/Image/Canvas/Table/CheckBox/DropDown/Slider/ProgressBar/ScrollBar renderer 已存在。 |
| 输入交互 | SDL3 输入泵、HitTest、Hover/Active/Focus、点击、滚轮、滚动条拖动、Slider 拖动、文本输入系统已有主路径。 |
| 动画 | Tween 组件和 `TweenSystem` 已支持 position/alpha/scale/render offset/color，交互动效已有入口。 |
| 拖放 | `Draggable`、`Droppable`、Drag/Drop 事件、默认 reparent 行为、单元测试 `test_DragDrop.cpp` 已存在。 |
| 资源 | 字体、图标、图片、内嵌资源、文本纹理缓存和 GPU/CPU fallback 管线已有基础。 |
| 文档规划 | Rich Text、SVG、HiDPI、CPU render issue、架构评审已有独立文档。 |

因此本文中的“缺口”不是否定当前框架，而是记录一个通用 UI 框架继续成熟通常需要补齐的组件族、系统能力和规划文档。

## 2. 已规划但尚未完整实现

| 能力 | 当前证据 | 缺口说明 | 建议优先级 |
|---|---|---|---|
| Theme / Style System | `src/systems/ThemeSystem.hpp` 只有说明性头部；架构文档标记为“存根，待实现”。 | 缺少主题 token、状态样式、选择器、继承/覆盖、动态切换、默认控件样式表。当前样式主要靠逐实体组件设置。 | P0 |
| Rich Text | `docs/RICH_TEXT_PLAN.md` 已有详细规划。 | 尚未看到对应 `RichText` 组件、解析器、布局引擎和 renderer 接入。 | P1 |
| SVG 支持 | `docs/SVG_SUPPORT_PLAN.md` 已有规划。 | 尚未形成 `CreateSvg`/SVG source 组件/解码缓存/render 接入的稳定 API。 | P1 |
| HiDPI 完整支持 | `docs/HIDPI_SUPPORT_PLAN.md` 已从纯规划进入实现态，P0-P3 已落地。 | 核心链路已修复；剩余缺口集中在字体过采样 DPI 感知、图标按物理尺寸选型，以及跨显示器 DPI 变化后的清晰度回归测试。 | P2 |
| JS/外部 DSL 解析 | 旧的 `src/core/DslPaser.hpp` TODO 存根已删除。 | 当前仍缺少 DSL schema、解析器、错误报告、实体创建映射和安全边界；后续应以正式模块重新引入，而不是保留无实现假入口。 | P3 |
| 动画系统完整化 | `需求文档.md` 提到“实现完整可用的动画系统”；`TweenSystem` 已有核心插值。 | 缺少 timeline/sequence/parallel、暂停恢复、完成回调、取消策略、动画事件、布局动画边界、统一测试矩阵。 | P1 |
| 拖放体验完整化 | 需求文档仍提到拖曳和可放置 tag；代码已有基础实现。 | 缺少拖拽预览/ghost、放置高亮、可放置规则策略、跨容器排序、拖放取消、吸附/约束、可访问键盘拖放。 | P1 |

## 3. 有数据结构或标记，但还不是可用控件

这些能力在组件或 tag 层已经露头，但缺少工厂、API、renderer、状态系统、示例和测试中的至少一环。

| 能力 | 当前残留 | 未完成内容 |
|---|---|---|
| List / ListView | `ListArea` 组件、`ListAreaTag` 存在。 | 缺少 `CreateListView`/`CreateListItem`、选择模型、滚动虚拟化、键盘导航、renderer 或基于现有 ScrollArea 的组合 API。 |
| Menu | `Menu` 组件存在；示例里 `View/menu.h` 是 demo 菜单界面，不是通用菜单控件。 | 缺少 MenuBar、ContextMenu、MenuItem、子菜单、快捷键提示、弹出层定位、关闭策略。 |
| Calendar / DatePicker | `Calendar` 组件存在。 | 缺少日期选择控件工厂、月份导航、日期网格 renderer、键盘操作、本地化和回调事件。 |
| Dialog / Modal | `CreateDialog` 已有基础容器。 | 缺少模态遮罩、焦点陷阱、z-order 弹窗栈、默认按钮、Esc/Enter 行为、结果回传、关闭策略。 |
| Draggable / Droppable tag | `DraggableTag`、`DroppableTag` 与对应组件存在。 | tag 与组件的语义边界还需要文档化：何时只用 tag、何时用组件保存策略；默认行为和用户回调的顺序也需要固定契约。 |

## 4. 尚未规划且未实现的常见控件

以下控件在当前 `Factory`、API、renderer 和 docs 规划中未形成明确条目。若项目目标是“工具/游戏内嵌 UI 框架”，建议按使用频率逐步补齐。

| 控件族 | 具体缺口 | 价值 |
|---|---|---|
| 单选与二态输入 | RadioButton / RadioGroup、Switch/Toggle。 | 表单和设置页常用；可复用 CheckBox 的状态与 renderer 思路。 |
| 分组与导航 | TabBar/TabView、Accordion、Collapsible Panel、Navigation Rail/Sidebar。 | 工具型 UI 需要在同一窗口内组织多页面/多面板。 |
| 树与层级数据 | TreeView、TreeItem、可展开节点、复选树。 | ECS/资源管理器/调试器类场景高频。 |
| 弹出与提示 | Tooltip、Popover、Toast/Notification。 | 需要统一 overlay layer、延迟显示、自动避让和 z-order 管理。 |
| 菜单体系 | MenuBar、ContextMenu、Command Palette。 | 桌面工具和编辑器类 UI 的基本交互入口。 |
| 数据展示增强 | ListView、VirtualList、GridView、分页器、排序/筛选表格表头。 | 当前 Table 偏静态展示，尚未覆盖大数据量和交互式数据视图。 |
| 输入增强 | ComboBox、SpinBox/NumericInput、SearchBox、ColorPicker、FilePicker。 | 表单、编辑器和配置面板需要更完整输入族。 |
| 容器布局增强 | Splitter/SplitPane、DockPanel、OverlayLayer、Portal。 | 支撑复杂工具布局、浮层和可调整面板。 |
| 媒体与图形 | Video/AnimatedImage、NinePatch/9-slice、矢量图形高级 path。 | 游戏 UI 和富视觉控件常见；SVG 规划可覆盖一部分。 |

## 5. 尚未规划且未实现的框架级能力

这些不是单个控件，但会决定框架能否从 demo 级走向可维护的应用 UI。

| 能力 | 缺口说明 | 建议优先级 |
|---|---|---|
| 统一样式系统 | 缺少主题 token、样式类、伪状态、继承、局部覆盖、默认控件皮肤。ThemeSystem 应成为下一阶段根能力。 | P0 |
| 焦点与键盘导航规范 | 当前有 FocusedTag 和文本/快捷键系统，但缺少 Tab 顺序、方向键导航、焦点环、焦点陷阱、禁用/隐藏元素跳过规则。 | P0 |
| Clipboard / 文本编辑命令 | 当前文本输入已有基础，但未看到剪切/复制/粘贴、全选、撤销/重做、选择拖拽、组合输入策略的完整规划。 | P1 |
| Accessibility | 缺少语义 role/name/value/state、键盘等价操作、屏幕阅读器桥接、高对比度主题。即使短期不做 OS 级桥接，也应先规划语义组件层。 | P2 |
| Overlay / Popup 管理器 | DropDown 自己创建 popup，但缺少统一浮层栈、定位、裁剪逃逸、焦点恢复、外部点击关闭、z-order 规则。Tooltip/Menu/Popover/Modal 都依赖它。 | P0 |
| 数据绑定和状态模型 | 目前主要靠回调和组件直接写入。缺少属性绑定、observable model、列表数据源、表单校验、批量更新事务。 | P2 |
| 虚拟化 | ScrollArea/Table/List 尚未形成大数据量虚拟滚动策略。 | P1 |
| 布局能力扩展 | Yoga Flexbox 足够覆盖大量场景，但缺少 Grid、Anchor、Dock、Absolute overlay、约束布局等高阶布局方案规划。 | P2 |
| 命令系统 | 快捷键已有 `ShortcutSystem`，但缺少 command registry、菜单/按钮/快捷键复用同一命令、启用状态联动。 | P2 |
| 输入设备扩展 | 当前以鼠标键盘为主；触摸、多指、手柄/gamepad、笔输入、鼠标捕获策略未规划。 | P3 |
| 国际化与本地化 | 已支持 Unicode/CJK 字体，但缺少字符串资源、语言切换、RTL/bidi、数字/日期格式、本地化布局验证。 | P2 |
| 测试与可视化验证 | 已有单元测试，但架构文档也列出 LayoutSystem/RenderSystem/HitTestSystem/StateSystem 等缺失覆盖。缺少截图回归、交互录制、性能基准。 | P1 |
| 开发调试工具 | 缺少 UI inspector、实体树查看、布局边框/命中区域可视化、样式来源追踪、frame stats overlay。 | P2 |

## 6. 建议落地顺序

1. **先补根能力**：Theme/Style、Overlay/Popup、焦点键盘导航。这三项会影响后续 Tooltip、Menu、Tab、Modal、ComboBox 等大量控件，不宜每个控件各自实现一套。
2. **再补高频控件**：RadioGroup、Switch、ListView、TabView、Tooltip、ContextMenu、ModalDialog。它们覆盖设置页、工具面板和常规应用交互。
3. **完善已有半成品**：ListArea、Menu、Calendar、拖放、动画系统。已有结构的地方优先补齐契约和测试，减少长期悬空组件。
4. **扩展复杂数据能力**：VirtualList、TreeView、Table 排序/筛选/列调整、数据源绑定。
5. **最后做生态能力**：Accessibility、I18N、开发者 inspector、视觉回归测试、外部 DSL。

## 7. 建议新增规划文档

建议后续拆分这些文档，避免所有缺口堆在一份总表里：

| 文档 | 目的 |
|---|---|
| `docs/THEME_STYLE_SYSTEM_PLAN.md` | 定义 ThemeSystem、样式 token、控件默认样式和状态样式。 |
| `docs/OVERLAY_POPUP_PLAN.md` | 统一 DropDown、Tooltip、Menu、Popover、Modal 的浮层机制。 |
| `docs/FOCUS_KEYBOARD_NAVIGATION_PLAN.md` | 固定焦点模型、Tab 顺序、键盘导航和焦点陷阱。 |
| `docs/CORE_WIDGETS_ROADMAP.md` | 跟踪 Radio/Switch/List/Tab/Menu/Tooltip/Modal/Tree 等控件路线图。 |
| `docs/DRAG_DROP_COMPLETION_PLAN.md` | 明确拖放预览、放置反馈、排序、取消和策略约束。 |
| `docs/UI_TESTING_STRATEGY.md` | 单元测试、集成交互测试、截图回归和性能基准。 |

## 8. 当前最值得立即补的工作包

| 工作包 | 目标 | 验收点 |
|---|---|---|
| WP-STYLE-1 | 实现最小 ThemeSystem。 | Button/Label/TextEdit/CheckBox/DropDown/Slider/ProgressBar 能从统一 theme token 取默认颜色和尺寸；支持运行时切主题并触发 RenderDirty。 |
| WP-OVERLAY-1 | 抽出 OverlayLayer/PopupManager。 | DropDown 改用统一 popup 路径；外部点击关闭、z-order、焦点恢复有测试。 |
| WP-FOCUS-1 | 建立焦点导航模型。 | Tab/Shift+Tab 遍历、Disabled/Hidden 跳过、TextEdit 与 Button 焦点行为有测试。 |
| WP-WIDGET-1 | 补 RadioGroup + Switch。 | 有 Factory、组件、renderer、DSL、ActionSystem 回调、示例和单元测试。 |
| WP-WIDGET-2 | 补 Tooltip + ContextMenu。 | 复用 OverlayLayer；支持延迟、定位、外部关闭和快捷键触发。 |
