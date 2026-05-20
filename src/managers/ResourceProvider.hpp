/**
 * ************************************************************************
 *
 * @file ResourceProvider.hpp
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2026-03-14
 * @version 0.2
 * @brief UI 资源抽象层
 *
 * 目标：将上层资源使用方与具体嵌入方案解耦。
 * 当前默认后端为 cmrc；未来切换到 std::embed 时，调用方无需改动。
 *
 * ************************************************************************
 * @copyright Copyright (c) 2026 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */

#pragma once

#include <cstddef>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include "../common/Result.hpp"

namespace ui::managers
{

struct BinaryResource
{
    std::span<const std::byte> bytes;
    std::shared_ptr<const void> owner;

    [[nodiscard]] const std::byte* data() const noexcept { return bytes.data(); }
    [[nodiscard]] size_t size() const noexcept { return bytes.size(); }
    [[nodiscard]] bool empty() const noexcept { return bytes.empty(); }
    [[nodiscard]] explicit operator bool() const noexcept { return !bytes.empty(); }
};

class IResourceProvider
{
public:
    virtual ~IResourceProvider() = default;
    IResourceProvider() = default;
    IResourceProvider(const IResourceProvider&) = delete;
    IResourceProvider& operator=(const IResourceProvider&) = delete;
    IResourceProvider(IResourceProvider&&) = delete;
    IResourceProvider& operator=(IResourceProvider&&) = delete;

    [[nodiscard]] virtual bool exists(std::string_view path) const = 0;
    [[nodiscard]] virtual ui::Result<BinaryResource> loadBinary(std::string_view path) const = 0;
};

[[nodiscard]] std::shared_ptr<const IResourceProvider> GetDefaultUiResourceProvider();

} // namespace ui::managers