/**
 * ************************************************************************
 *
 * @file StdEmbedResourceTable.hpp
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2026-03-14
 * @version 0.1
 * @brief std::embed 资源表骨架
 *
 * 说明：
 * - 当前文件只定义逻辑路径到静态字节视图的统一表结构。
 * - 未来接入 std::embed 时，应仅补齐表项和生成逻辑，避免改动 provider / 调用方。
 *
 * ************************************************************************
 * @copyright Copyright (c) 2026 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */

#pragma once

#include <cstddef>
#include <span>
#include <string_view>

namespace ui::managers
{

struct StdEmbedResourceEntry
{
    std::string_view logicalPath;
    std::span<const std::byte> bytes;
};

[[nodiscard]] std::span<const StdEmbedResourceEntry> GetStdEmbedResourceTable();
[[nodiscard]] const StdEmbedResourceEntry* FindStdEmbedResource(std::string_view logicalPath);

} // namespace ui::managers