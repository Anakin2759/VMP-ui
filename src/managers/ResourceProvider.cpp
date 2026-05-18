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

#if defined(UI_RESOURCE_BACKEND_CMRC)
#include <cmrc/cmrc.hpp>

CMRC_DECLARE(ui_fonts);
#elif defined(UI_RESOURCE_BACKEND_STD_EMBED)
#include "StdEmbedResourceTable.hpp"
#endif

namespace ui::managers
{
namespace
{

#if defined(UI_RESOURCE_BACKEND_CMRC)
class CmrcResourceProvider final : public IResourceProvider
{
public:
    [[nodiscard]] bool exists(std::string_view path) const override
    {
        const auto fileSystem = cmrc::ui_fonts::get_filesystem();
        return fileSystem.exists(std::string(path));
    }

    [[nodiscard]] std::expected<BinaryResource, std::string> loadBinary(std::string_view path) const override
    {
        const auto fileSystem = cmrc::ui_fonts::get_filesystem();
        const std::string normalizedPath(path);
        if (!fileSystem.exists(normalizedPath))
        {
            return std::unexpected("resource not found: " + normalizedPath);
        }

        auto file = std::make_shared<cmrc::file>(fileSystem.open(normalizedPath));

        BinaryResource resource{};
        resource.owner = std::shared_ptr<const void>(file, file.get());
        resource.bytes = std::as_bytes(std::span(file->begin(), file->size()));
        return resource;
    }
};
#endif

#if defined(UI_RESOURCE_BACKEND_STD_EMBED)
class StdEmbedResourceProvider final : public IResourceProvider
{
public:
    [[nodiscard]] bool exists(std::string_view path) const override { return FindStdEmbedResource(path) != nullptr; }

    [[nodiscard]] std::expected<BinaryResource, std::string> loadBinary(std::string_view path) const override
    {
        const StdEmbedResourceEntry* entry = FindStdEmbedResource(path);
        if (entry == nullptr)
        {
            return std::unexpected("std::embed resource not found in generated table: " + std::string(path));
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

    [[nodiscard]] std::expected<BinaryResource, std::string> loadBinary(std::string_view path) const override
    {
        return std::unexpected("no UI resource backend selected at compile time for provider request: " +
                               std::string(path));
    }
};
#endif

} // namespace

std::shared_ptr<const IResourceProvider> GetDefaultUiResourceProvider()
{
#if defined(UI_RESOURCE_BACKEND_STD_EMBED)
    static const std::shared_ptr<const IResourceProvider> DEFAULT_UI_RESOURCE_PROVIDER =
        std::make_shared<StdEmbedResourceProvider>();
    return DEFAULT_UI_RESOURCE_PROVIDER;
#elif defined(UI_RESOURCE_BACKEND_CMRC)
    static const std::shared_ptr<const IResourceProvider> DEFAULT_UI_RESOURCE_PROVIDER =
        std::make_shared<CmrcResourceProvider>();
    return DEFAULT_UI_RESOURCE_PROVIDER;
#else
    static const std::shared_ptr<const IResourceProvider> DEFAULT_UI_RESOURCE_PROVIDER =
        std::make_shared<UnavailableResourceProvider>();
    return DEFAULT_UI_RESOURCE_PROVIDER;
#endif
}

} // namespace ui::managers