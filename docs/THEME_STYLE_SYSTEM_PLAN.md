# ThemeSystem 最小实现规划

> 日期：2026-05-28
> 目标：为 VMP-ui 落地一个最小可用的 ThemeSystem，不追求完整 CSS/选择器能力，只解决“控件默认样式分散、缺少统一主题入口”的问题，并补最小交互态视觉反馈。

## 1. 本次实现范围

本次实现以下能力：

1. 提供运行时级别的主题上下文，存放最小 theme token。
2. 新增 ThemeSystem，并接入 SystemManager。
3. ThemeSystem 在帧更新时为“尚未主题化”的实体补默认样式。
4. 显式写过的样式不被 ThemeSystem 覆盖。
5. 提供最小 public API，可切换到默认深色主题并触发重新应用。
6. 为 Button、Label、TextEdit、CheckBox、DropDown、Slider、ProgressBar、Window、Dialog 提供最小默认主题值。
7. 增加一个很小的伪状态增量：
	- Button 支持 HoveredTag / ActiveTag 背景切换
	- Button / TextEdit / DropDown 支持 FocusedTag 边框切换
8. 增加单元测试，验证主题应用、重新应用和交互态切换行为。

## 2. 明确不做

本次不做以下能力：

1. CSS 选择器、类名、层叠优先级。
2. 完整的伪状态样式系统和状态优先级配置。
3. 主题继承、多主题文件加载、序列化。
4. 动画化切换主题。
5. 细粒度 token 系统，如 spacing scale、font scale、icon scale。
6. Table、Canvas、富文本、SVG 等复杂控件的主题化。

## 3. 最小设计

### 3.1 主题上下文

在 runtime context 中新增 ThemeContext，至少包含：

- 当前主题 palette
- 上一版 palette
- theme version
- 是否请求重新应用

### 3.2 主题标记

新增 ThemedTag，表示该实体已经被当前主题系统处理过。

用途：

- 避免每帧重复补默认样式
- 为切主题后的重新应用留下钩子

### 3.3 交互态跟踪

新增 ThemeStyleState 组件，记录当前仍由主题系统托管的背景色和边框色。

用途：

- 在 Hover / Active / Focused 状态变化时，只更新仍由主题接管的字段
- 如果用户在首次主题应用后手动改色，后续状态切换不再覆盖该字段

### 3.4 ThemeSystem 触发策略

ThemeSystem 订阅 events::UpdateEvent。

处理逻辑：

1. 若当前 runtime 没有 ThemeContext，创建默认深色主题上下文。
2. 若 reapplyRequested 为 true，移除所有 ThemedTag。
3. 扫描所有没有 ThemedTag 的实体，补齐默认样式。
4. 初始化 ThemeStyleState 中的受主题托管字段。
5. 扫描已主题化实体，根据 HoveredTag / ActiveTag / FocusedTag 应用最小交互态样式。
6. 处理完成后清除 reapplyRequested，并同步 previousPalette。

选择 UpdateEvent 而不是 factory 阶段直接写入，原因：

- 不需要改动每个 factory 的默认样式逻辑
- 能覆盖运行时新增实体
- 保持 ThemeSystem 是独立系统，而不是散落在各 API 里的 if/else

## 4. 默认主题 token

本次保留的最小 token：

- windowBackground
- surfaceBackground
- surfaceBorder
- primaryButtonBackground
- primaryButtonBackgroundHover
- primaryButtonBackgroundActive
- primaryButtonText
- textPrimary
- textMuted
- inputBackground
- inputBorder
- focusBorderColor
- inputText
- accent
- accentMuted

默认采用深色主题，风格与当前示例接近，减少视觉回归风险。

## 5. 控件映射规则

### 5.1 Button

- 若没有 Background，补按钮背景
- 若没有 Border，补按钮边框
- 若 Text.color 仍为默认值，补按钮文字色
- 若 Background.borderRadius 仍为 0，补小圆角
- 若存在 HoveredTag，背景切换到 primaryButtonBackgroundHover
- 若存在 ActiveTag，背景切换到 primaryButtonBackgroundActive
- 若存在 FocusedTag，边框切换到 focusBorderColor

### 5.2 Label

- 若 Text.color 仍为默认值，补 textPrimary

### 5.3 TextEdit

- 若没有 Background，补输入框背景
- 若没有 Border，补输入框边框
- 若 TextEdit.textColor 仍为默认值，补输入文字色
- 若存在 FocusedTag，边框切换到 focusBorderColor

### 5.4 CheckBox

- 若 Text.color 仍为默认值，补 textPrimary
- 若 CheckBox.checkColor / boxColor 仍为组件默认值，替换为 theme token

### 5.5 DropDown

- 若没有 Background，补输入式背景
- 若没有 Border，补边框
- 若 Text.color 仍为默认值，补输入文字色
- 若存在 FocusedTag，边框切换到 focusBorderColor

### 5.6 Slider / ProgressBar

- 若颜色仍为组件默认值，替换为 theme token

### 5.7 Window / Dialog

- 若没有 Background，补容器背景
- 若没有 Border，补容器边框

## 6. 显式样式不覆盖规则

ThemeSystem 只在以下情况下写入默认值：

1. 目标组件不存在。
2. 目标字段仍为组件默认初始值。
3. 目标字段当前仍等于上一版主题写入的值。

对于交互态字段，只有当前值仍等于 ThemeStyleState 中记录的“主题托管值”时，ThemeSystem 才会继续更新。

因此用户通过 DSL 或 API 显式设置的 BackgroundColor、TextColor、BorderColor 等不会被后续主题更新和状态切换覆盖。

## 7. 最小 public API

新增 src/api/Theme.hpp/.cpp：

- SetTheme(const ThemePalette& palette)
- UseDefaultDarkTheme()
- RequestThemeReapply()

SetTheme() 与 UseDefaultDarkTheme() 的行为：

1. 更新 ThemeContext.palette
2. version 自增
3. 设置 reapplyRequested = true
4. 标记全局需要重新渲染

## 8. 验收标准

### 8.1 功能验收

1. Button 在不显式设置背景/边框时，经过 UpdateEvent 后自动获得主题默认样式。
2. Button 在显式设置 BackgroundColor 后，主题应用不会覆盖该颜色。
3. TextEdit 在默认状态下获得输入框背景与边框。
4. SetTheme() 后可以重新应用新主题，而不是只影响新建实体。
5. Button 在 Hover / Active 状态下，背景会切换到对应主题色。
6. TextEdit 在 Focused 状态下，边框会切换到主题焦点色。

### 8.2 非功能约束

1. 不修改现有控件对外 API 语义。
2. 尽量少动 factory 逻辑。
3. 允许最小 renderer 配合改动：焦点态只保留边框粗细增强，颜色由 ThemeSystem 写入 Border 组件。

## 9. 实施步骤

1. 新增规划文档。
2. 增加 ThemeContext 和 ThemedTag。
3. 实现 ThemeSystem。
4. 在 SystemManager 注册 ThemeSystem。
5. 增加 src/api/Theme.hpp/.cpp 并导出到 include/ui.hpp。
6. 增加 ThemeSystem 单测。
7. 扩展最小伪状态支持。
8. 运行聚焦测试验证。
