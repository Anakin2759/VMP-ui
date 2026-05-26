/**
 * ************************************************************************
 *
 * @file CustomizationPoints.hpp
 * @date 2026-03-24
 * @version 0.1
 * @brief UI 模块开放定制点的 tag_invoke 型 CPO
 *
 * ************************************************************************
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace ui::interface
{
/**
 * @brief 后端渲染器接口，定义了渲染器的基本功能和行为规范。
 */
enum class BackendCapability : std::uint8_t;
} // namespace ui::interface

namespace ui::cpo
{

// NOLINTBEGIN(readability-identifier-naming)
void tag_invoke(); // 声明一个无定义的 tag_invoke，所有 CPO 实现都通过重载此函数实现

template <typename Tag, typename... Args>
concept TagInvocable = requires(Tag tag, Args&&... args) { tag_invoke(tag, std::forward<Args>(args)...); };

struct ResourceExists
{
    template <typename Provider>
    [[nodiscard]] bool operator()(const Provider& provider, std::string_view path) const
    {
        if constexpr (TagInvocable<ResourceExists, const Provider&, std::string_view>)
        {
            return tag_invoke(*this, provider, path);
        }

        return provider.exists(path);
    }
};

struct LoadBinaryResource
{
    template <typename Provider>
    [[nodiscard]] auto operator()(const Provider& provider, std::string_view path) const
    {
        if constexpr (TagInvocable<LoadBinaryResource, const Provider&, std::string_view>)
        {
            return tag_invoke(*this, provider, path);
        }

        return provider.loadBinary(path);
    }
};

struct BackendSupports
{
    template <typename Backend>
    [[nodiscard]] bool operator()(const Backend& backend, interface::BackendCapability capability) const
    {
        if constexpr (TagInvocable<BackendSupports, const Backend&, interface::BackendCapability>)
        {
            return tag_invoke(*this, backend, capability);
        }
        else if constexpr (requires { backend.supports(capability); })
        {
            return backend.supports(capability);
        }

        return false;
    }
};
/**
 * @brief 测量文本宽度的 CPO，接受一个文本测量器对象、文本内容和可选的字体大小，返回文本的像素宽度。
 */
struct MeasureTextWidth
{
    template <typename TextMeasurer>
    [[nodiscard]] int operator()(TextMeasurer& textMeasurer, std::string_view text, float fontSize = 0.0F) const
    {
        if constexpr (TagInvocable<MeasureTextWidth, TextMeasurer&, std::string_view, float>)
        {
            return tag_invoke(*this, textMeasurer, text, fontSize);
        }
        else if constexpr (requires { textMeasurer.measureTextWidth(std::string(text), fontSize); })
        {
            return textMeasurer.measureTextWidth(std::string(text), fontSize);
        }

        return textMeasurer.measureTextWidth(std::string(text));
    }
};

struct LoadIconFontFromMemory
{
    template <typename IconLoader>
    [[nodiscard]] auto operator()(IconLoader& iconLoader,
                                  std::string_view fontName,
                                  std::span<const std::byte> fontBytes,
                                  std::span<const std::byte> codepointBytes,
                                  int fontSize = 16) const
    {
        if constexpr (TagInvocable<LoadIconFontFromMemory,
                                   IconLoader&,
                                   std::string_view,
                                   std::span<const std::byte>,
                                   std::span<const std::byte>,
                                   int>)
        {
            return tag_invoke(*this, iconLoader, fontName, fontBytes, codepointBytes, fontSize);
        }

        return iconLoader.loadIconFontFromMemory(std::string(fontName),
                                                 fontBytes.data(),
                                                 fontBytes.size(),
                                                 codepointBytes.data(),
                                                 codepointBytes.size(),
                                                 fontSize);
    }
};

inline constexpr ResourceExists resource_exists{};
inline constexpr LoadBinaryResource load_binary_resource{};
inline constexpr BackendSupports backend_supports{};
inline constexpr MeasureTextWidth measure_text_width{};
inline constexpr LoadIconFontFromMemory load_icon_font_from_memory{};
// NOLINTEND(readability-identifier-naming)

} // namespace ui::cpo