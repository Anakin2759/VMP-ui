/**
 * ************************************************************************
 *
 * @file Query.hpp
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2026-05-30
 * @version 0.1
 * @brief 实体查询 API
 *  - 支持通过 entity 句柄或别名（alias）查找实体
 *  - 返回 Result<entity>，失败时携带错误码说明原因
 *
 * ************************************************************************
 * @copyright Copyright (c) 2026 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */
#pragma once

#include <string>
#include <string_view>

#include "Entity.hpp"
#include "common/Result.hpp"

namespace ui::query
{

/**
 * @brief 检查实体句柄是否有效（存在于注册表中）。
 * @param e 要检查的实体
 * @return true 表示实体存在且未被销毁
 */
[[nodiscard]] bool IsValid(entity e) noexcept;

/**
 * @brief 通过别名查找实体。
 *
 * 遍历所有拥有 BaseInfo 组件的实体，返回第一个 alias 匹配的实体。
 *
 * @param alias 实体创建时设置的别名（非空）
 * @return 成功时返回匹配的 entity；
 *         alias 为空时返回 UiErrc::INVALID_ARGUMENT；
 *         未找到时返回 UiErrc::INVALID_ENTITY。
 */
[[nodiscard]] Result<entity> FindByAlias(std::string_view alias);

/**
 * @brief 获取实体的别名字符串。
 * @param e 目标实体
 * @return 别名字符串；实体无效或无 BaseInfo 时返回空字符串。
 */
[[nodiscard]] std::string GetAlias(entity e);

} // namespace ui::query
