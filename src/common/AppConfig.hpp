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

#include <span>
#include <string>
#include <string_view>

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
public:
    [[nodiscard]] static AppConfig& instance() noexcept
    {
        static AppConfig s_instance;
        return s_instance;
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
     *
     * 未识别的参数会被忽略，避免影响调用方自身的解析逻辑。
     * 后端名称按小写约定传入。
     */
    void parseCommandLine(std::span<char* const> argv)
    {
        constexpr std::string_view kLong = "--backend";
        constexpr std::string_view kShort = "-b";
        for (std::size_t i = 0; i < argv.size(); ++i)
        {
            const char* raw = argv[i];
            if (raw == nullptr) continue;
            std::string_view token(raw);

            if (token.starts_with(kLong))
            {
                if (token.size() > kLong.size() && token[kLong.size()] == '=')
                {
                    setPreferredBackend(std::string(token.substr(kLong.size() + 1)));
                }
                else if (token == kLong && i + 1 < argv.size() && argv[i + 1] != nullptr)
                {
                    setPreferredBackend(std::string(argv[i + 1]));
                    ++i;
                }
            }
            else if (token == kShort && i + 1 < argv.size() && argv[i + 1] != nullptr)
            {
                setPreferredBackend(std::string(argv[i + 1]));
                ++i;
            }
        }
    }

private:
    AppConfig() = default;
    std::string m_preferredBackend;
    std::string m_appIconPath;
    std::string m_logFilePath;
};

} // namespace ui::config
