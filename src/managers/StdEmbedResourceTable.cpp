/**
 * ************************************************************************
 *
 * @file StdEmbedResourceTable.cpp
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2026-03-14
 * @version 0.1
 * @brief std::embed 资源表骨架实现
 *
 * 当前实现为空表，只负责建立稳定的接入点。
 * 真正启用 std::embed 时，在此文件中加入逻辑路径到嵌入对象的静态映射即可。
 *
 * ************************************************************************
 * @copyright Copyright (c) 2026 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */

#include "StdEmbedResourceTable.hpp"

#include <array>
#include <span>
#include <string_view>

namespace ui::managers
{
namespace
{

constexpr std::array<StdEmbedResourceEntry, 0> STD_EMBED_RESOURCE_TABLE{};

} // namespace

std::span<const StdEmbedResourceEntry> GetStdEmbedResourceTable()
{
    return STD_EMBED_RESOURCE_TABLE;
}

const StdEmbedResourceEntry* FindStdEmbedResource(std::string_view logicalPath)
{
    for (const auto& entry : STD_EMBED_RESOURCE_TABLE)
    {
        if (entry.logicalPath == logicalPath)
        {
            return &entry;
        }
    }

    return nullptr;
}

} // namespace ui::managers