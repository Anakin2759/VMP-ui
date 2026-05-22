#include "IconManager.hpp"
#include "DeviceManager.hpp"
#include "freetype/config/ftheader.h"
#include "singleton/Logger.hpp"
#include "freetype/fttypes.h"
#include "common/Result.hpp"
#include "common/ErrorCodes.hpp"
#include "freetype/ftimage.h"
#include "common/GPUWrappers.hpp"
#include "SDL3/SDL_stdinc.h"
#include <SDL3/SDL_gpu.h>
#include <string>
#include <ios>
#include <vector>
#include <utility>
#include <sstream>
#include <istream>
#include <cstdio>
#include <cstdint>
#include <exception>
#include <string_view>
#include <chrono>
#include <iterator>
#include <algorithm>
#include FT_FREETYPE_H
#include <cstring>
#include <fstream>
#include <span>
namespace ui::managers
{
namespace
{
void WriteStderr(const char* text) noexcept
{
    if (text == nullptr)
    {
        return;
    }

    const auto textSize = std::strlen(text);
    if (std::fwrite(text, 1U, textSize, stderr) != textSize)
    {
        std::clearerr(stderr);
    }
}

std::vector<uint32_t> BuildPremultipliedRgba(const FT_Bitmap& bitmap, int width, int height)
{
    const std::span<const uint8_t> bitmapData(bitmap.buffer, static_cast<size_t>(width) * static_cast<size_t>(height));
    std::vector<uint32_t> rgbaPixels(bitmapData.size());
    size_t pixelIndex = 0;
    for (const uint8_t alpha : bitmapData)
    {
        auto premultRGB = static_cast<uint8_t>((255U * alpha) / 255U);
        rgbaPixels.at(pixelIndex) = (static_cast<uint32_t>(alpha) << 24U) |
                                    (static_cast<uint32_t>(premultRGB) << 16U) |
                                    (static_cast<uint32_t>(premultRGB) << 8U) | premultRGB;
        ++pixelIndex;
    }
    return rgbaPixels;
}
} // namespace

IconManager::~IconManager() noexcept
{
    try
    {
        shutdown();
    }
    catch (const std::exception& exception)
    {
        WriteStderr("[IconManager] destructor cleanup failed: ");
        WriteStderr(exception.what());
        WriteStderr("\n");
    }
    catch (...)
    {
        WriteStderr("[IconManager] destructor cleanup failed with unknown exception\n");
    }
}

/**
 * @brief 加载 IconFont 字体和 codepoints 文件
 * @param name 字体库名称（用于后续引用）
 * @param fontPath TTF 字体文件路径
 * @param codepointsPath codepoints 文件路径（支持 .txt 或 .json）
 * @param fontSize 字体大小
 * @return true 加载成功
 * @return false 加载失败
 */
bool IconManager::loadIconFont(const std::string& name,
                               const std::string& fontPath,
                               const std::string& codepointsPath,
                               int fontSize)
{
    if (m_ftLibrary == nullptr)
    {
        Logger::error("[IconManager] FreeType not initialized");
        return false;
    }

    Logger::info("Loading IconFont '{}' from '{}'", name, fontPath);

    // 读取字体文件
    // NOLINTNEXTLINE(hicpp-signed-bitwise) -- std::ios::openmode 底层为有符号类型，属标准库既有设计
    std::ifstream file(fontPath, std::ios::binary | std::ios::ate);
    if (!file.is_open())
    {
        Logger::error("Failed to open font file: {}", fontPath);
        return false;
    }

    std::streamsize const size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<unsigned char> buffer;
    buffer.reserve(static_cast<size_t>(size));
    for (std::istreambuf_iterator<char> input(file), end; input != end; ++input)
    {
        buffer.push_back(static_cast<unsigned char>(*input));
    }
    if (buffer.size() != static_cast<size_t>(size))
    {
        Logger::error("Failed to read font file: {}", fontPath);
        return false;
    }

    // 创建 FreeType Face
    FT_Face face = nullptr;
    FT_Error error = FT_New_Memory_Face(m_ftLibrary, buffer.data(), static_cast<FT_Long>(buffer.size()), 0, &face);

    if (error != 0)
    {
        Logger::error("Failed to load font face: {} (error {})", fontPath, error);
        return false;
    }

    // 设置像素大小
    error = FT_Set_Pixel_Sizes(face, 0, static_cast<FT_UInt>(fontSize));
    if (error != 0)
    {
        Logger::error("Failed to set pixel size: {} (error {})", fontSize, error);
        FT_Done_Face(face);
        return false;
    }

    // 解析 codepoints 文件
    auto codepoints = parseCodepoints(codepointsPath);
    if (codepoints.empty())
    {
        Logger::warn("No codepoints loaded from: {}", codepointsPath);
    }

    // 存储字体和映射
    m_fonts[name] = FontData{.buffer = std::move(buffer), .face = face, .fontSize = fontSize};
    m_codepoints[name] = std::move(codepoints);

    Logger::info("IconFont '{}' loaded: {} icons", name, m_codepoints[name].size());
    return true;
}

Result<void> IconManager::loadIconFontFromMemory(const std::string& name,
                                                 const void* fontData,
                                                 size_t fontLength,
                                                 const void* codepointsData,
                                                 size_t codepointsLength,
                                                 int fontSize)
{
    if (m_ftLibrary == nullptr)
    {
        Logger::error("[IconManager] FreeType not initialized");
        return MakeError(ui_errc::device_unavailable);
    }

    if (fontData == nullptr || fontLength == 0)
    {
        Logger::error("[IconManager] Invalid font data");
        return MakeError(ui_errc::invalid_argument);
    }
    if (codepointsData == nullptr || codepointsLength == 0)
    {
        Logger::error("[IconManager] Invalid codepoints data");
        return MakeError(ui_errc::invalid_argument);
    }

    // 复制字体数据（FreeType 需要持久内存）
    std::vector<unsigned char> buffer(fontLength);
    std::memcpy(buffer.data(), fontData, fontLength);

    // 创建 FreeType Face
    FT_Face face = nullptr;
    FT_Error error = FT_New_Memory_Face(m_ftLibrary, buffer.data(), static_cast<FT_Long>(buffer.size()), 0, &face);

    if (error != 0)
    {
        Logger::error("[IconManager] Failed to load font face from memory '{}' (error {})", name, error);
        return MakeError(ui_errc::asset_load_failed);
    }

    // 设置像素大小
    error = FT_Set_Pixel_Sizes(face, 0, static_cast<FT_UInt>(fontSize));
    if (error != 0)
    {
        FT_Done_Face(face);
        Logger::error("[IconManager] Failed to set pixel size {} (error {})", fontSize, error);
        return MakeError(ui_errc::asset_load_failed);
    }

    // 解析 codepoints
    std::string const codepointsStr(static_cast<const char*>(codepointsData), codepointsLength);
    std::istringstream stream(codepointsStr);

    CodepointMap codepoints;

    // 启发式检测JSON vs TXT
    char firstChar = 0;
    while (stream >> std::ws && stream.peek() != EOF)
    {
        firstChar = static_cast<char>(stream.peek());
        break;
    }
    stream.seekg(0);

    if (firstChar == '{')
    {
        codepoints = parseCodepointsJSON(stream);
    }
    else
    {
        codepoints = parseCodepointsTXT(stream);
    }

    if (codepoints.empty())
    {
        Logger::warn("No codepoints loaded from memory for: {}", name);
        return MakeError(ui_errc::asset_decode_failed);
    }

    m_fonts[name] = FontData{.buffer = std::move(buffer), .face = face, .fontSize = fontSize};
    m_codepoints[name] = std::move(codepoints);

    Logger::info("IconFont '{}' loaded from memory: {} icons", name, m_codepoints[name].size());
    return Ok();
}

uint32_t IconManager::getCodepoint(std::string_view fontName, std::string_view iconName) const
{
    if (auto fontIt = m_codepoints.find(fontName); fontIt != m_codepoints.end())
    {
        if (auto iconIt = fontIt->second.find(iconName); iconIt != fontIt->second.end())
        {
            return iconIt->second;
        }
        Logger::warn("Icon '{}' not found in font '{}'", iconName, fontName);
        return 0;
    }

    Logger::warn("IconFont '{}' not found", fontName);
    return 0;
}

FT_Face IconManager::getFont(std::string_view fontName)
{
    if (auto iterator = m_fonts.find(fontName); iterator != m_fonts.end())
    {
        return iterator->second.face;
    }
    return nullptr;
}

bool IconManager::hasIcon(std::string_view fontName, std::string_view iconName) const
{
    if (auto iterator = m_codepoints.find(fontName); iterator != m_codepoints.end())
    {
        return iterator->second.contains(iconName);
    }
    return false;
}

std::vector<std::string> IconManager::getIconNames(std::string_view fontName) const
{
    std::vector<std::string> names;
    if (auto iterator = m_codepoints.find(fontName); iterator != m_codepoints.end())
    {
        names.reserve(iterator->second.size());
        for (const auto& [name, codepoint] : iterator->second)
        {
            names.push_back(name);
        }
    }
    return names;
}

void IconManager::unloadIconFont(std::string_view fontName)
{
    // 释放 FreeType Face
    if (auto iterator = m_fonts.find(fontName); iterator != m_fonts.end())
    {
        if (iterator->second.face != nullptr)
        {
            FT_Done_Face(iterator->second.face);
        }
        m_fonts.erase(iterator);
    }
    m_codepoints.erase(fontName);
    Logger::info("IconFont '{}' unloaded", fontName);
}

void IconManager::shutdown()
{
    // 释放所有 FreeType Faces
    for (auto& [name, fontData] : m_fonts)
    {
        if (fontData.face != nullptr)
        {
            FT_Done_Face(fontData.face);
            fontData.face = nullptr;
        }
    }

    m_fontTextureCache.clear();
    m_imageTextureCache.clear();
    m_fonts.clear();
    m_codepoints.clear();

    if (m_ftLibrary != nullptr)
    {
        FT_Done_FreeType(m_ftLibrary);
        m_ftLibrary = nullptr;
    }

    Logger::info("[IconManager] Shutdown complete. Total evictions: {}", m_evictionCount);
}

const TextureInfo* IconManager::getTextureInfo(std::string_view fontName, uint32_t codepoint, float size)
{
    // 量化大小以减少缓存条目
    float quantizedSize = quantizeSize(size);

    // 构造 cache key
    std::string const cacheKey =
        std::string(fontName) + "_" + std::to_string(codepoint) + "_" + std::to_string(static_cast<int>(quantizedSize));

    // 检查缓存
    auto iter = m_fontTextureCache.find(cacheKey);
    if (iter != m_fontTextureCache.end())
    {
        // 更新访问时间和计数
        iter->second.lastAccessTime = std::chrono::steady_clock::now();
        iter->second.accessCount++;
        return &iter->second.textureInfo;
    }

    // 如果缓存已满，执行 LRU 驱逐
    if (m_fontTextureCache.size() >= MAX_FONT_CACHE_SIZE)
    {
        evictLRUFromFontCache();
    }

    auto fontDataIt = m_fonts.find(fontName);
    if (fontDataIt == m_fonts.end())
    {
        return nullptr;
    }

    FT_Face face = fontDataIt->second.face;
    if (face == nullptr)
    {
        Logger::error("[IconManager] Invalid FT_Face for font '{}'", fontName);
        return nullptr;
    }

    // 设置字符大小（如果与当前不同）
    FT_Error error = FT_Set_Pixel_Sizes(face, 0, static_cast<FT_UInt>(quantizedSize));
    if (error != 0)
    {
        Logger::warn("[IconManager] Failed to set pixel size {} for codepoint {}", quantizedSize, codepoint);
        return nullptr;
    }

    // 加载字形
    FT_UInt const glyphIndex = FT_Get_Char_Index(face, codepoint);
    error = FT_Load_Glyph(face, glyphIndex, FT_LOAD_DEFAULT);
    if (error != 0)
    {
        Logger::warn("[IconManager] Failed to load glyph for codepoint {}: error {}", codepoint, error);
        return nullptr;
    }

    // 渲染为 alpha 位图
    error = FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL);
    if (error != 0)
    {
        Logger::warn("[IconManager] Failed to render glyph for codepoint {}: error {}", codepoint, error);
        return nullptr;
    }

    FT_Bitmap const& bitmap = face->glyph->bitmap;
    int const width = static_cast<int>(bitmap.width);
    int const height = static_cast<int>(bitmap.rows);

    if (width == 0 || height == 0)
    {
        Logger::warn("[IconManager] Empty bitmap for codepoint {}", codepoint);
        return nullptr;
    }

    SDL_GPUDevice* device = m_deviceManager->getDevice();
    if (device == nullptr)
    {
        Logger::error("[IconManager] GPU device is null");
        return nullptr;
    }

    // 转换 alpha 位图为 RGBA（预乘 Alpha 格式）
    auto rgbaPixels = BuildPremultipliedRgba(bitmap, width, height);

    // 创建并上传纹理
    auto texture =
        createAndUploadIconTexture(device, rgbaPixels, static_cast<uint32_t>(width), static_cast<uint32_t>(height));
    if (texture == nullptr)
    {
        return nullptr;
    }

    return cacheIconTexture(cacheKey, std::move(texture), width, height);
}

const TextureInfo* IconManager::cacheIconTexture(const std::string& cacheKey,
                                                 wrappers::UniqueGPUTexture texture,
                                                 int width,
                                                 int height)
{
    CachedTextureEntry entry{};
    entry.textureInfo.texture = std::move(texture);
    entry.textureInfo.uvMin = {0.0F, 0.0F};
    entry.textureInfo.uvMax = {1.0F, 1.0F};
    entry.textureInfo.width = static_cast<float>(width);
    entry.textureInfo.height = static_cast<float>(height);
    entry.lastAccessTime = std::chrono::steady_clock::now();
    entry.accessCount = 1;

    auto [iterator, inserted] = m_fontTextureCache.insert_or_assign(cacheKey, std::move(entry));
    (void)inserted;
    return &iterator->second.textureInfo;
}

IconManager::CodepointMap IconManager::parseCodepoints(const std::string& filePath)
{
    CodepointMap result;
    std::ifstream file(filePath);

    if (!file.is_open())
    {
        Logger::error("Failed to open codepoints file: {}", filePath);
        return result;
    }

    if (filePath.contains(".json"))
    {
        result = parseCodepointsJSON(file);
    }
    else
    {
        result = parseCodepointsTXT(file);
    }

    file.close();
    return result;
}

IconManager::CodepointMap IconManager::parseCodepointsTXT(std::istream& file)
{
    CodepointMap result;
    std::string line;

    while (std::getline(file, line))
    {
        if (line.empty() || line.front() == '#')
        {
            continue;
        }

        std::istringstream iss(line);
        std::string iconName;
        std::string hexCode;

        if (iss >> iconName >> hexCode)
        {
            try
            {
                uint32_t const codepoint = std::stoul(hexCode, nullptr, 16);
                result[iconName] = codepoint;
            }
            catch (...)
            {
                Logger::warn("Invalid codepoint format: {} - {}", iconName, hexCode);
            }
        }
    }

    return result;
}

IconManager::CodepointMap IconManager::parseCodepointsJSON(std::istream& file)
{
    CodepointMap result;
    std::string const content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    size_t pos = 0;
    while (true)
    {
        size_t const keyStart = content.find('"', pos);
        if (keyStart == std::string::npos) break;

        size_t const keyEnd = content.find('"', keyStart + 1);
        if (keyEnd == std::string::npos) break;

        std::string key = content.substr(keyStart + 1, keyEnd - keyStart - 1);

        size_t const valueStart = content.find('"', keyEnd + 1);
        if (valueStart == std::string::npos) break;

        size_t const valueEnd = content.find('"', valueStart + 1);
        if (valueEnd == std::string::npos) break;

        std::string value = content.substr(valueStart + 1, valueEnd - valueStart - 1);

        try
        {
            uint32_t const codepoint = std::stoul(value, nullptr, 16);
            result[key] = codepoint;
        }
        catch (...)
        {
            Logger::warn("Invalid codepoint in JSON: {} - {}", key, value);
        }

        pos = valueEnd + 1;
    }

    return result;
}

void IconManager::evictLRUFromFontCache()
{
    if (m_fontTextureCache.empty())
    {
        return;
    }

    SDL_GPUDevice const* device = m_deviceManager->getDevice();
    if (device == nullptr)
    {
        return;
    }

    // 找到最少使用的条目
    auto lruEntry = m_fontTextureCache.begin();
    for (auto iter = m_fontTextureCache.begin(); iter != m_fontTextureCache.end(); ++iter)
    {
        if (iter->second.lastAccessTime < lruEntry->second.lastAccessTime)
        {
            lruEntry = iter;
        }
    }

    // 释放纹理资源
    /*
    if (lruEntry->second.textureInfo.texture != nullptr)
    {
        SDL_ReleaseGPUTexture(device, lruEntry->second.textureInfo.texture);
    }
    */

    Logger::debug("[IconManager] Evicted LRU entry: {} (access count: {})",
                  lruEntry->first.substr(0, 50),
                  lruEntry->second.accessCount);

    m_fontTextureCache.erase(lruEntry);
    m_evictionCount++;

    // 如果仍然过大，批量驱逐
    if (m_fontTextureCache.size() >= MAX_FONT_CACHE_SIZE)
    {
        std::vector<std::pair<std::string, std::chrono::steady_clock::time_point>> entries;
        entries.reserve(m_fontTextureCache.size());

        for (const auto& [key, entry] : m_fontTextureCache)
        {
            entries.emplace_back(key, entry.lastAccessTime);
        }

        // 按访问时间排序
        std::ranges::sort(entries,

                          [](const auto& first, const auto& second) { return first.second < second.second; });

        // 驱逐最旧的条目
        size_t evicted = 0;
        for (size_t idx = 0; idx < std::min(EVICTION_BATCH, entries.size()); ++idx)
        {
            auto iter = m_fontTextureCache.find(entries.at(idx).first);
            if (iter != m_fontTextureCache.end())
            {
                m_fontTextureCache.erase(iter);
                evicted++;
            }
        }

        m_evictionCount += evicted;
        Logger::info("[IconManager] Batch evicted {} entries, cache size: {}", evicted, m_fontTextureCache.size());
    }
}
/**
 * @brief
 * @param devices 设备
 * @param rgbaPixels {comment}
 * @param width {comment}
 * @param height {comment}
 * @return wrappers::UniqueGPUTexture {comment}
 */
wrappers::UniqueGPUTexture IconManager::createAndUploadIconTexture(SDL_GPUDevice* device,
                                                                   const std::vector<uint32_t>& rgbaPixels,
                                                                   uint32_t width,
                                                                   uint32_t height)
{
    // 创建纹理
    SDL_GPUTextureCreateInfo texInfo = {};
    texInfo.type = SDL_GPU_TEXTURETYPE_2D;
    texInfo.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    texInfo.width = width;
    texInfo.height = height;
    texInfo.layer_count_or_depth = 1;
    texInfo.num_levels = 1;
    texInfo.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;

    auto texture = wrappers::MakeGpuResource<wrappers::UniqueGPUTexture>(device, SDL_CreateGPUTexture, &texInfo);
    if (!texture)
    {
        Logger::error("[IconManager] Failed to create GPU texture");
        return nullptr;
    }

    // 创建传输缓冲区
    SDL_GPUTransferBufferCreateInfo transferInfo = {};
    transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    transferInfo.size = static_cast<uint32_t>(rgbaPixels.size() * sizeof(uint32_t));

    auto transferBuffer = wrappers::MakeGpuResource<wrappers::UniqueGPUTransferBuffer>(
        device, SDL_CreateGPUTransferBuffer, &transferInfo);
    if (!transferBuffer)
    {
        Logger::error("[IconManager] Failed to create transfer buffer");
        return nullptr;
    }

    // 映射传输缓冲区
    void* mappedData = SDL_MapGPUTransferBuffer(device, transferBuffer.get(), false);
    if (mappedData == nullptr)
    {
        Logger::error("[IconManager] Failed to map transfer buffer");
        return nullptr;
    }

    SDL_memcpy(mappedData, rgbaPixels.data(), transferInfo.size);
    SDL_UnmapGPUTransferBuffer(device, transferBuffer.get());

    // 上传到GPU
    SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(device);
    if (cmd == nullptr)
    {
        Logger::error("[IconManager] Failed to acquire command buffer");
        return nullptr;
    }

    SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmd);

    SDL_GPUTextureTransferInfo srcInfo = {};
    srcInfo.transfer_buffer = transferBuffer.get();
    srcInfo.pixels_per_row = width;
    srcInfo.rows_per_layer = height;

    SDL_GPUTextureRegion dstRegion = {};
    dstRegion.texture = texture.get();
    dstRegion.w = width;
    dstRegion.h = height;
    dstRegion.d = 1;

    SDL_UploadToGPUTexture(copyPass, &srcInfo, &dstRegion, false);
    SDL_EndGPUCopyPass(copyPass);
    SDL_SubmitGPUCommandBuffer(cmd);

    return texture;
}

} // namespace ui::managers
