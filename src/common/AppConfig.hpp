/**
 * ************************************************************************
 *
 * @file AppConfig.hpp
 * @brief 应用级别的全局可调配置（命令行/环境变量驱动）
 *
 * 目前用于在命令行测试多个渲染后端（如 direct3d12 / vulkan / cpu）。
 *
 * ************************************************************************
 * @copyright Copyright (c) 2026 AnakinLiu
 * ************************************************************************
 */
#pragma once

#include <cstdlib>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#ifndef UI_ENABLE_PLATFORM_SCALING
#define UI_ENABLE_PLATFORM_SCALING 1
#endif

namespace ui::config
{
/**
 * @brief 应用级别的全局配置（进程内单例）
 *
 * - `preferredBackend`：用户期望优先尝试的渲染后端名称。
 *   - 空字符串：使用 DeviceManager 默认顺序。
 *   - 非空字符串：DeviceManager 会将匹配的后端排到首位，其余作为回退。
 *   - cpu/software/fallback：强制使用 SDL_Renderer fallback software 后端。
 */
class AppConfig
{
private:
    static std::optional<std::string_view> readLongOptionValue(std::string_view token,
                                                               std::string_view option,
                                                               std::span<char* const> argv,
                                                               std::size_t& index)
    {
        if (!token.starts_with(option))
        {
            return std::nullopt;
        }

        if (token.size() > option.size() && token[option.size()] == '=')
        {
            return token.substr(option.size() + 1);
        }

        if (token == option && index + 1 < argv.size() && argv[index + 1] != nullptr)
        {
            ++index;
            return std::string_view(argv[index]);
        }

        return std::nullopt;
    }

    static float parsePositiveFloat(std::string_view value)
    {
        std::string buffer(value);
        char* end = nullptr;
        const float parsed = std::strtof(buffer.c_str(), &end);
        return end != buffer.c_str() && parsed > 0.0F ? parsed : 0.0F;
    }

    void applyPlatformScalingMode(std::string_view value) noexcept
    {
        if (value == "off" || value == "false" || value == "0")
        {
            setPlatformScalingEnabled(false);
        }
        else if (value == "auto" || value == "on" || value == "true" || value == "1")
        {
            setPlatformScalingEnabled(true);
            setForcedPlatformScale(0.0F);
        }
    }

public:
    [[nodiscard]] static AppConfig& instance() noexcept
    {
        static AppConfig instance;
        return instance;
    }

    /// 设置优先后端（小写名称，例如 "direct3d12" / "vulkan" / "cpu"）
    void setPreferredBackend(std::string name) { m_preferredBackend = std::move(name); }

    [[nodiscard]] std::string_view preferredBackend() const noexcept { return m_preferredBackend; }

    /// 设置应用程序窗口图标路径（支持 BMP / PNG / JPG，空字符串表示不设置）
    void setAppIconPath(std::string path) { m_appIconPath = std::move(path); }

    [[nodiscard]] std::string_view appIconPath() const noexcept { return m_appIconPath; }

    /// 设置日志文件路径（空字符串表示使用默认路径 logs/pestmankill.log）
    void setLogFilePath(std::string path) { m_logFilePath = std::move(path); }

    [[nodiscard]] std::string_view logFilePath() const noexcept { return m_logFilePath; }

    void setPlatformScalingEnabled(bool enabled) noexcept
    {
        m_platformScalingEnabled = enabled;
        if (!enabled)
        {
            m_forcedPlatformScale = 0.0F;
        }
    }

    [[nodiscard]] bool platformScalingEnabled() const noexcept { return m_platformScalingEnabled; }

    void setForcedPlatformScale(float scale) noexcept
    {
        m_forcedPlatformScale = scale > 0.0F ? scale : 0.0F;
        if (m_forcedPlatformScale > 0.0F)
        {
            m_platformScalingEnabled = true;
        }
    }

    [[nodiscard]] float forcedPlatformScale() const noexcept { return m_forcedPlatformScale; }

    void setPlatformUiScale(float scale) noexcept { m_platformUiScale = scale > 0.0F ? scale : 1.0F; }

    [[nodiscard]] float platformUiScale() const noexcept
    {
        if (!m_platformScalingEnabled) return 1.0F;
        return m_forcedPlatformScale > 0.0F ? m_forcedPlatformScale : m_platformUiScale;
    }

    [[nodiscard]] bool forceFallbackRenderer() const noexcept
    {
        return m_preferredBackend == "cpu" || m_preferredBackend == "software" || m_preferredBackend == "fallback";
    }

    /**
     * @brief 解析命令行参数，提取受支持的开关
     *
     * 支持的形式：
     *   --backend=<name>
     *   --backend <name>
     *   -b <name>
     *   --ui-platform-scaling=auto|on|off
     *   --ui-platform-scale=<scale>
     *
     * 未识别的参数会被忽略，避免影响调用方自身的解析逻辑。
     * 后端名称按小写约定传入。
     */
    void parseCommandLine(std::span<char* const> argv)
    {
        constexpr std::string_view BACKEND_LONG = "--backend";
        constexpr std::string_view BACKEND_SHORT = "-b";
        constexpr std::string_view PLATFORM_SCALING = "--ui-platform-scaling";
        constexpr std::string_view PLATFORM_SCALE = "--ui-platform-scale";

        for (std::size_t index = 0; index < argv.size(); ++index)
        {
            const char* raw = argv[index];
            if (raw == nullptr) continue;
            std::string_view token(raw);

            if (auto value = readLongOptionValue(token, BACKEND_LONG, argv, index))
            {
                setPreferredBackend(std::string(*value));
            }
            else if (token == BACKEND_SHORT && index + 1 < argv.size() && argv[index + 1] != nullptr)
            {
                ++index;
                setPreferredBackend(std::string(argv[index]));
            }
            else if (auto value = readLongOptionValue(token, PLATFORM_SCALING, argv, index))
            {
                applyPlatformScalingMode(*value);
            }
            else if (auto value = readLongOptionValue(token, PLATFORM_SCALE, argv, index))
            {
                setForcedPlatformScale(parsePositiveFloat(*value));
            }
        }
    }

private:
    AppConfig() = default;
    std::string m_preferredBackend;
    std::string m_appIconPath;
    std::string m_logFilePath;
    bool m_platformScalingEnabled = UI_ENABLE_PLATFORM_SCALING != 0;
    float m_forcedPlatformScale = 0.0F;
    float m_platformUiScale = 1.0F;
};

} // namespace ui::config
