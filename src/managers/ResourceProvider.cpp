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
#include "common/ErrorCodes.hpp"
#include "singleton/Logger.hpp"
#include <string_view>
#include <string>
#include "common/Result.hpp"
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
            ui::Logger::error("[ResourceProvider/cmrc] resource not found: {}", normalizedPath);
            return ui::MakeError(ui::UiErrc::ASSET_NOT_FOUND);
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
    [[nodiscard]] bool exists(std::string_view path) const override { return FindStdEmbedResource(path) != nullptr; }

    [[nodiscard]] ui::Result<BinaryResource> loadBinary(std::string_view path) const override
    {
        const StdEmbedResourceEntry* entry = FindStdEmbedResource(path);
        if (entry == nullptr)
        {
            ui::Logger::error("[ResourceProvider/std_embed] resource not found: {}", path);
            return ui::MakeError(ui::ui_errc::asset_not_found);
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
    [[nodiscard]] bool exists(std::string_view path) const override
    {
        static_cast<void>(path);
        return false;
    }

    [[nodiscard]] ui::Result<BinaryResource> loadBinary(std::string_view path) const override
    {
        ui::Logger::error("[ResourceProvider] no UI resource backend selected at compile time for: {}", path);
        static_cast<void>(path);
        return ui::MakeError(ui::ui_errc::backend_unavailable);
    }
};
#endif

} // namespace

std::shared_ptr<const IResourceProvider> GetDefaultUiResourceProvider()
{
#ifdef UI_RESOURCE_BACKEND_STD_EMBED
    static const std::shared_ptr<const IResourceProvider> defaultUiResourceProvider =
        std::make_shared<StdEmbedResourceProvider>();
    return defaultUiResourceProvider;
#elifdef UI_RESOURCE_BACKEND_CMRC
    static const std::shared_ptr<const IResourceProvider> defaultUiResourceProvider =
        std::make_shared<CmrcResourceProvider>();
    return defaultUiResourceProvider;
#else
    static const std::shared_ptr<const IResourceProvider> defaultUiResourceProvider =
        std::make_shared<UnavailableResourceProvider>();
    return defaultUiResourceProvider;
#endif
}

} // namespace ui::managers