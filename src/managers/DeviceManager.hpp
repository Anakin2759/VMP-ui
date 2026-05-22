/**
 * ************************************************************************
 *
 * @file DeviceManager.hpp
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2026-01-30
 * @version 0.1
 * @brief 管理 GPU 设备和窗口声明

    GPU 后端回退方案实现：
    - 直接在 DeviceManager 内部维护一个后端列表（如 D3D12、Vulkan），并在初始化时逐个尝试创建 GPU 设备。
    - 如果当前后端无法声明窗口（例如在 VM 中 D3D12 无法渲染），则自动切换到下一个后端并重试，直到成功或所有后端均失败。
    - 尝试后备方案使用 cmrc 内嵌资源库的 swiftshader 库提供的 vulkan 实现，确保在没有物理 GPU 的环境中也能提供基本的渲染支持。
    - CPU/software fallback 由 RenderSystem 的 FallbackBackendRenderer 处理，不属于 SDL_GPU 后端列表。
    - 虚拟机环境可能遇到初始化成功，但无法声明窗口的情况（例如 D3D12 在某些 VM 中无法渲染）。
    - 在这种情况下，DeviceManager 将自动切换到下一个后端（如 Vulkan）并重试声明窗口，确保应用能够继续运行而不是崩溃。
 *
 * ************************************************************************
 * @copyright Copyright (c) 2026 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */
#pragma once
#include <algorithm>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>
#include <functional>
#include <filesystem>
#include <fstream>
#include <SDL3/SDL.h>
#include "../common/AppConfig.hpp"
#include "../common/Result.hpp"
#include "../common/ErrorCodes.hpp"

// CMRC_DECLARE(ui_swiftshader);

#include <SDL3/SDL_gpu.h>
#include "../singleton/Logger.hpp"
#include "../common/GPUWrappers.hpp"

namespace ui::managers
{

/**
 * @brief 管理 GPU 设备和窗口声明
 */
class DeviceManager
{
public:
    struct BackendConfig
    {
        std::string name;
        std::function<void(SDL_PropertiesID)> configure;
    };

    DeviceManager()
    {
        m_backends = {{.name = "direct3d12",
                       .configure =
                       [](SDL_PropertiesID props)
                       {
                           SDL_SetStringProperty(props, SDL_PROP_GPU_DEVICE_CREATE_NAME_STRING, "direct3d12");
                           SDL_SetBooleanProperty(props, SDL_PROP_GPU_DEVICE_CREATE_DEBUGMODE_BOOLEAN, true);
                           SDL_SetBooleanProperty(props, SDL_PROP_GPU_DEVICE_CREATE_SHADERS_DXIL_BOOLEAN, true);
                       }},
                      {.name = "vulkan",
                       .configure =
                           [](SDL_PropertiesID props)
                       {
                           SDL_SetStringProperty(props, SDL_PROP_GPU_DEVICE_CREATE_NAME_STRING, "vulkan");
                           SDL_SetBooleanProperty(props, SDL_PROP_GPU_DEVICE_CREATE_DEBUGMODE_BOOLEAN, false);
                           SDL_SetBooleanProperty(props, SDL_PROP_GPU_DEVICE_CREATE_SHADERS_SPIRV_BOOLEAN, true);
                       }}

        };
    }

    ~DeviceManager() { cleanup(); }
    DeviceManager(const DeviceManager&) = delete;
    DeviceManager& operator=(const DeviceManager&) = delete;
    DeviceManager(DeviceManager&&) noexcept = default;
    DeviceManager& operator=(DeviceManager&&) noexcept = default;

    Result<void> initialize()
    {
        if (m_gpuDevice != nullptr) return Ok();

        Logger::info("DeviceManager: 开始初始化 GPU 后端 (Strategy: Iterative Configuration)");

        applyPreferredBackend();

        for (size_t i = 0; i < m_backends.size(); ++i)
        {
            if (createDevice(i))
            {
                return Ok();
            }
        }

        Logger::error("所有 GPU 后端方案均初始化失败！请检查显卡驱动或虚拟机 3D 加速设置。");
        return MakeError(ui_errc::backend_unavailable);
    }

    Result<void> claimWindow(SDL_Window* sdlWindow)
    {
        if (m_gpuDevice == nullptr || sdlWindow == nullptr)
        {
            Logger::error("claimWindow: 无效的设备或窗口句柄");
            return MakeError(ui_errc::invalid_argument);
        }

        SDL_WindowID windowID = SDL_GetWindowID(sdlWindow);
        if (m_claimedWindows.contains(windowID))
        {
            return Ok();
        }

        // 尝试声明窗口
        if (SDL_ClaimWindowForGPUDevice(m_gpuDevice.get(), sdlWindow))
        {
            m_claimedWindows.insert(windowID);
            return Ok();
        }

        // 核心修改：如果声明失败（例如 D3D12 在 VM 中无法渲染），尝试回退到其他后端
        Logger::warn("当前后端 {} 无法声明窗口 ({}). 尝试切换其他后端...", m_gpuDriver, SDL_GetError());

        // 尝试后续的后端
        size_t nextIndex = m_currentBackendIndex + 1;
        while (nextIndex < m_backends.size())
        {
            // 清理当前失败的设备
            cleanup();

            // 尝试创建下一个设备
            if (createDevice(nextIndex))
            {
                Logger::info("已切换至后端: {}，重试声明窗口...", m_gpuDriver);
                if (SDL_ClaimWindowForGPUDevice(m_gpuDevice.get(), sdlWindow))
                {
                    m_claimedWindows.insert(windowID);
                    return Ok();
                }
                Logger::warn("后端 {} 也无法声明窗口，继续寻找...", m_gpuDriver);
            }
            nextIndex++;
        }

        Logger::error("致命错误: 所有可用后端均无法声明/渲染窗口！");
        return MakeError(ui_errc::window_claim_failed);
    }

    void unclaimWindow(SDL_Window* sdlWindow)
    {
        if (m_gpuDevice == nullptr || sdlWindow == nullptr) return;

        SDL_WindowID windowID = SDL_GetWindowID(sdlWindow);
        if (m_claimedWindows.contains(windowID))
        {
            SDL_ReleaseWindowFromGPUDevice(m_gpuDevice.get(), sdlWindow);
            m_claimedWindows.erase(windowID);
        }
    }

    void cleanup()
    {
        if (m_gpuDevice == nullptr) return;

        SDL_WaitForGPUIdle(m_gpuDevice.get());

        // 释放所有窗口声明
        for (auto windowID : m_claimedWindows)
        {
            SDL_Window* window = SDL_GetWindowFromID(windowID);
            if (window != nullptr)
            {
                SDL_ReleaseWindowFromGPUDevice(m_gpuDevice.get(), window);
            }
        }
        m_claimedWindows.clear();

        m_gpuDevice.reset();
    }

    [[nodiscard]] SDL_GPUDevice* getDevice() const { return m_gpuDevice.get(); }
    [[nodiscard]] const std::string& getDriverName() const { return m_gpuDriver; }

    [[nodiscard]] SDL_GPUTexture* getWhiteTexture() const { return nullptr; } // TODO: Implement white texture creation

private:
    void applyPreferredBackend()
    {
        auto preferred = config::AppConfig::instance().preferredBackend();
        if (preferred.empty()) return;

        if (config::AppConfig::instance().forceFallbackRenderer())
        {
            return;
        }

        auto it = std::find_if(m_backends.begin(),
                               m_backends.end(),
                               [&](const BackendConfig& cfg) { return cfg.name == preferred; });
        if (it == m_backends.end())
        {
            Logger::warn("未知 GPU 后端 \"{}\"，使用默认顺序。可选: direct3d12 / vulkan", preferred);
            return;
        }
        if (it != m_backends.begin())
        {
            std::rotate(m_backends.begin(), it, it + 1);
        }
        Logger::info("应用命令行 GPU 后端偏好：优先尝试 {}", m_backends.front().name);
    }

    bool createDevice(size_t index)
    {
        if (index >= m_backends.size()) return false;

        const auto& config = m_backends[index];
        Logger::info("尝试初始化后端: {}...", config.name);

        wrappers::UniquePropertiesID props(SDL_CreateProperties());
        if (config.configure)
        {
            config.configure(props);
        }

        SDL_GPUDevice* device = SDL_CreateGPUDeviceWithProperties(props);
        if (device != nullptr)
        {
            m_gpuDevice.reset(device);
            m_gpuDriver = config.name;
            m_currentBackendIndex = index;
            Logger::info("GPU 初始化成功，锁定后端: {}", m_gpuDriver);
            return true;
        }

        Logger::warn("后端 {} 初始化失败 ({})", config.name, SDL_GetError());
        return false;
    }

    wrappers::UniqueGPUDevice m_gpuDevice;
    std::string m_gpuDriver;
    std::unordered_set<SDL_WindowID> m_claimedWindows;

    std::vector<BackendConfig> m_backends;
    size_t m_currentBackendIndex = 0;
};

} // namespace ui::managers
