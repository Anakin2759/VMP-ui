# UI 模块下一步更新计划

## 1. 交互体验增强 (Priority: Medium)

- [ ] **滚动条完善**: 补全 `ScrollBarRenderer` 与 `StateSystem` 中的水平滚动条逻辑。
- [ ] **状态反馈**: 为滚动条滑块 (Thumb) 添加 Hover 和 Active 视觉状态反馈。
- [ ] **Tooltips 基础**: 基于 `ZOrder` 实现浮动提示层（Tooltip）的逻辑框架。

## 2. 动画系统雏形 (Priority: Low)

- [ ] **补间组件**: 引入 `Tween` 组件，支持对 `Alpha`、`Position` 等组件进行时间轴插值。
- [ ] **交互动效**: 实现按钮点击时的缩放或颜色渐变效果。

## 3. 主题系统 (Priority: Low)

- [ ] **样式抽离**: 将常用的颜色、圆角、间距等硬编码参数整合为 `Theme` 资源。
- [ ] **动态切换**: 支持在运行时一键切换全局主题方案（如 Dark/Light Mode）。

## 4.CPU渲染支持 (Priority: Low)

- [ ] 提供一个全局状态组件，管理渲染后端策略
- [ ] 默认策略：按照责任链遍历，谁先能够渲染就选谁
- [ ] 新增渲染器接口抽象层
- [ ] VULKAN策略 使用VULKAN
- [ ] D3D12策略 使用D3D12
- [ ] 后备策略，使用sdl传统接口

## 5.终极目标：实现DSL (Priority: Low)

- [ ] **引入一种简单的json-like DSL用来描述界面**。
- [ ] 写一个DSL解析工具 动态调用工厂函数/工具函数/回调函数 不是现在的|链式调用，而是jsonlike
