/**
 * ************************************************************************
 *
 * @file ResourceProvider.cpp
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2026-03-14
 * @version 0.1
 * @brief UI 资源抽象层默认实现（cmrc 后端）
 *
 * ************************************************************************
 * @copyright Copyright (c) 2026 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */

#include "ResourceProvider.hpp"
#include "ui/ErrorCodes.hpp"
#include <string_view>
#include <string>
#include "ui/Result.hpp"
#include <memory>
#include <span>

#ifdef UI_RESOURCE_BACKEND_CMRC
#include <cmrc/cmrc.hpp>

// NOLINTNEXTLINE(modernize-type-traits)
CMRC_DECLARE(ui_fonts);
#elifdef UI_RESOURCE_BACKEND_STD_EMBED
#include "StdEmbedResourceTable.hpp"
#endif

namespace ui::managers
{
namespace
{

#ifdef UI_RESOURCE_BACKEND_CMRC
class CmrcResourceProvider final : public IResourceProvider
{
   public:
    CmrcResourceProvider() = default;
    [[nodiscard]] bool exists(std::string_view path) const override
    {
        const auto fileSystem = cmrc::ui_fonts::get_filesystem();
        return fileSystem.exists(std::string(path));
    }

    [[nodiscard]] ui::Result<BinaryResource> loadBinary(std::string_view path) const override
    {
        const auto fileSystem = cmrc::ui_fonts::get_filesystem();
        const std::string normalizedPath(path);
        if (!fileSystem.exists(normalizedPath))
        {
            return ui::Err(ui::UiErrc::ASSET_NOT_FOUND, normalizedPath);
        }

        auto file = std::make_shared<cmrc::file>(fileSystem.open(normalizedPath));

        BinaryResource resource{};
        resource.owner = std::shared_ptr<const void>(file, file.get());
        resource.bytes = std::as_bytes(std::span(file->begin(), file->size()));
        return resource;
    }
};
#endif

#ifdef UI_RESOURCE_BACKEND_STD_EMBED
class StdEmbedResourceProvider final : public IResourceProvider
{
   public:
    StdEmbedResourceProvider() = default;
    [[nodiscard]] bool exists(std::string_view path) const override
    {
        return FindStdEmbedResource(path) != nullptr;
    }

    [[nodiscard]] ui::Result<BinaryResource> loadBinary(std::string_view path) const override
    {
        const StdEmbedResourceEntry* entry = FindStdEmbedResource(path);
        if (entry == nullptr)
        {
            return ui::Err(ui::UiErrc::ASSET_NOT_FOUND, std::string(path));
        }

        BinaryResource resource{};
        resource.bytes = entry->bytes;
        return resource;
    }
};
#endif

#if !defined(UI_RESOURCE_BACKEND_STD_EMBED) && !defined(UI_RESOURCE_BACKEND_CMRC)
class UnavailableResourceProvider final : public IResourceProvider
{
   public:
    UnavailableResourceProvider() = default;
    [[nodiscard]] bool exists(std::string_view path) const override
    {
        static_cast<void>(path);
        return false;
    }

    [[nodiscard]] ui::Result<BinaryResource> loadBinary(std::string_view path) const override
    {
        static_cast<void>(path);
        return ui::Err(ui::UiErrc::BACKEND_UNAVAILABLE, std::string(path));
    }
};
#endif

}  // namespace

std::shared_ptr<const IResourceProvider> GetDefaultUiResourceProvider()
{
#ifdef UI_RESOURCE_BACKEND_STD_EMBED
    static const auto defaultUiResourceProvider = std::make_shared<StdEmbedResourceProvider>();
    return defaultUiResourceProvider;
#elifdef UI_RESOURCE_BACKEND_CMRC
    static const auto defaultUiResourceProvider = std::make_shared<CmrcResourceProvider>();
    return defaultUiResourceProvider;
#else
    static const auto defaultUiResourceProvider = std::make_shared<UnavailableResourceProvider>();
    return defaultUiResourceProvider;
#endif
}

}  // namespace ui::managers