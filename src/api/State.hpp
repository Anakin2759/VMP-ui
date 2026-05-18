/**
 * ************************************************************************
 *
 * @file State.hpp
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2026-02-09
 * @version 0.1
 * @brief 通过访问ui::globalcontext::StateContext组件获取全局UI状态
  - 提供对最新鼠标位置、焦点实体等状态的访问接口
  - 方便系统和组件查询当前UI交互状态
  - 解耦状态存储和访问逻辑
  - 提供只读访问，防止意外修改全局状态
  - 支持查询实体类型tag 如 Button、Slider 等
 *
 * ************************************************************************
 * @copyright Copyright (c) 2026 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */
#pragma once