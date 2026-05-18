/**
 * ************************************************************************
 *
 * @file IconManager.hpp
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2026-01-29
 * @version 0.2
 * @brief 加载图标字体文件和管理图标资源（基于 FreeType）
 *
 * 支持：
 * - 加载 TTF/OTF/TTC 格式图标文件作为默认图标
 * - 解析 codepoints 映射文件（JSON 或 TXT 格式）
 * - 通过图标名称获取 Unicode 码点
 * - 管理多个 IconFont 图标库
 * - 默认加载 ui/assets/icons/xxx.ttf 和 codepoints 文件
 * - 默认从 ui/assets/icons/ 读取 MaterialSymbols 图标字体
 *
 * 2026-02-10 更新说明：
 *  统一使用 FreeType 进行字体渲染（与 FontManager 保持一致）
 *
 * ************************************************************************
 * @copyright Copyright (c) 2026 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */
#pragma once

#include <unordered_map>
#include <string>
#include <string_view>
#include <vector>
#include <array>
#include <chrono>
#include <algorithm>
#include <expected>
#include <system_error>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <SDL3/SDL.h>
#include <Eigen/Core>
#include "../singleton/Logger.hpp"
#include "../common/GPUWrappers.hpp"
#include "../common/UiErrors.hpp"

namespace ui::managers
{

class DeviceManager;

struct FontData
{
    std::vector<unsigned char> buffer; // 字体数据（FreeType 需要持久内存）
    FT_Face face;                      // FreeType Face
    int fontSize;
};

struct TextureInfo
{
    wrappers::UniqueGPUTexture texture;
    Eigen::Vector2f uvMin;
    Eigen::Vector2f uvMax;
    float width;
    float height;
};

struct CachedTextureEntry
{
    TextureInfo textureInfo;
    std::chrono::steady_clock::time_point lastAccessTime;
    uint32_t accessCount = 0;
};

/**
 * @brief IconFont 管理器（基于 FreeType）
 *
 * 负责加载和管理 IconFont 字体及其 codepoints 映射
 * 与 FontManager 共享相同的 FreeType 渲染管线
 *
 * 使用示例：
 * @code
 * IconManager::LoadIconFont("default", "assets/fonts/iconfont.ttf", "assets/fonts/codepoints.txt", 16);
 * uint32_t homeIcon = IconManager::GetCodepoint("default", "home");
 * auto* face = IconManager::GetFont("default");
 * @endcode
 */
class IconManager
{
public:
    explicit IconManager(DeviceManager* deviceManager) : m_deviceManager(deviceManager)
    {
        FT_Error error = FT_Init_FreeType(&m_ftLibrary);
        if (error)
        {
            Logger::error("[IconManager] Failed to initialize FreeType: error {}", error);
            m_ftLibrary = nullptr;
        }
        else
        {
            Logger::info("[IconManager] FreeType initialized");
        }
    }
    ~IconManager() { shutdown(); }
    IconManager(const IconManager&) = delete;
    IconManager& operator=(const IconManager&) = delete;
    IconManager(IconManager&&) noexcept = default;
    IconManager& operator=(IconManager&&) = default;

    /**
     * @brief 加载 IconFont 字体和 codepoints 文件
     * @param name 字体库名称（用于后续引用）
     * @param fontPath TTF 字体文件路径
     * @param codepointsPath codepoints 文件路径（支持 .txt 或 .json）
     * @param fontSize 字体大小
     * @return 是否加载成功
     */
    bool loadIconFont(const std::string& name,
                      const std::string& fontPath,
                      const std::string& codepointsPath,
                      int fontSize = 16);

    /**
     * @brief 从内存加载 IconFont 字体和 codepoints 数据
     * @param name 字体库名称
     * @param fontData 字体数据指针
     * @param fontLength 字体数据长度
     * @param codepointsData codepoints 数据指针
     * @param codepointsLength codepoints 数据长度
     * @param fontSize 字体大小
     * @return 是否加载成功
     */
    std::expected<void, std::error_code> loadIconFontFromMemory(const std::string& name,
                                                                const void* fontData,
                                                                size_t fontLength,
                                                                const void* codepointsData,
                                                                size_t codepointsLength,
                                                                int fontSize = 16);

    /**
     * @brief 通过图标名称获取 Unicode 码点
     * @param fontName 字体库名称
     * @param iconName 图标名称（如 "home", "user"）
     * @return Unicode 码点，未找到返回 0
     */
    uint32_t getCodepoint(std::string_view fontName, std::string_view iconName) const;

    /**
     * @brief 获取字体信息
     * @param fontName 字体库名称
     * @return FT_Face 指针，未找到返回 nullptr
     */
    FT_Face getFont(std::string_view fontName);

    /**
     * @brief 检查图标是否存在
     */
    bool hasIcon(std::string_view fontName, std::string_view iconName) const;

    /**
     * @brief 获取字体库中所有图标名称
     */
    std::vector<std::string> getIconNames(std::string_view fontName) const;

    /**
     * @brief 卸载指定字体库
     */
    void unloadIconFont(std::string_view fontName);

    /**
     * @brief 清理所有资源
     */
    void shutdown();

    /**
     * @brief 获取图标纹理信息（字体图标）
     */
    const TextureInfo* getTextureInfo(std::string_view fontName, uint32_t codepoint, float size);

    /**
     * @brief 获取图标纹理信息（普通纹理图标 - 暂未实现完整逻辑，目前仅作为接口）
     */
    [[nodiscard]] const TextureInfo* getTextureInfo(std::string_view textureId) const
    {
        if (auto iterator = m_imageTextureCache.find(textureId); iterator != m_imageTextureCache.end())
        {
            return &iterator->second.textureInfo;
        }
        return nullptr;
    }

    /**
     * @brief 获取缓存统计信息
     */
    struct CacheStats
    {
        size_t fontCacheSize;
        size_t imageCacheSize;
        size_t maxCacheSize;
        size_t evictionCount;
    };

    CacheStats getCacheStats() const
    {
        return {.fontCacheSize = m_fontTextureCache.size(),
                .imageCacheSize = m_imageTextureCache.size(),
                .maxCacheSize = MAX_FONT_CACHE_SIZE,
                .evictionCount = m_evictionCount};
    }

private:
    // 使用透明哈希 (C++20/23) 以支持 string_view 查找而无需分配临时 string
    struct StringHash
    {
        using is_transparent = void;
        size_t operator()(std::string_view stringView) const { return std::hash<std::string_view>{}(stringView); }
    };

    template <typename T>
    using StringMap = std::unordered_map<std::string, T, StringHash, std::equal_to<>>;

    using CodepointMap = StringMap<uint32_t>;

    /**
     * @brief 解析 codepoints 文件
     */
    CodepointMap parseCodepoints(const std::string& filePath);

    CodepointMap parseCodepointsTXT(std::istream& file);

    CodepointMap parseCodepointsJSON(std::istream& file);

    /**
     * @brief 量化图标大小，减少缓存条目数量
     */
    static float quantizeSize(float size)
    {
        // 量化到标准尺寸：16, 24, 32, 48, 64, 96, 128
        constexpr std::array<float, 7> STANDARD_SIZES = {16.0F, 24.0F, 32.0F, 48.0F, 64.0F, 96.0F, 128.0F};
        for (float standardSize : STANDARD_SIZES)
        {
            if (size <= standardSize)
            {
                return standardSize;
            }
        }
        return 128.0F; // 最大尺寸
    }

    /**
     * @brief 驱逐最少使用的缓存条目（LRU策略）
     */
    void evictLRUFromFontCache();

    /**
     * @brief 创建并上传图标纹理
     */
    wrappers::UniqueGPUTexture createAndUploadIconTexture(SDL_GPUDevice* device,
                                                          const std::vector<uint32_t>& rgbaPixels,
                                                          uint32_t width,
                                                          uint32_t height);

    DeviceManager* m_deviceManager;
    FT_Library m_ftLibrary = nullptr;

    StringMap<FontData> m_fonts;
    StringMap<CodepointMap> m_codepoints;

    // 缓存容量限制
    static constexpr size_t MAX_FONT_CACHE_SIZE = 128;
    static constexpr size_t MAX_IMAGE_CACHE_SIZE = 64;
    static constexpr size_t EVICTION_BATCH = 16;

    // 缓存：键为 "fontName_codepoint_size"
    StringMap<CachedTextureEntry> m_fontTextureCache;
    // 缓存：键为 textureId
    StringMap<CachedTextureEntry> m_imageTextureCache;

    // 统计信息
    mutable size_t m_evictionCount = 0;
};

} // namespace ui::managers