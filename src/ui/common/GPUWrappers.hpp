/**
 * ************************************************************************
 *
 * @file GPUWrappers.hpp
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2026-02-07
 * @version 0.1
 * @brief SDL GPU 资源管理 RAII 包装器
 *
 * ************************************************************************
 * @copyright Copyright (c) 2026 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */

#pragma once
#include <memory>
#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>

namespace ui::wrappers
{

/**
 * @brief RAII wrapper for SDL_PropertiesID (Uint32 handle)
 */
class UniquePropertiesID
{
public:
    UniquePropertiesID() : m_id(0) {}
    explicit UniquePropertiesID(SDL_PropertiesID propertiesId) : m_id(propertiesId) {}
    ~UniquePropertiesID() { reset(); }

    UniquePropertiesID(const UniquePropertiesID&) = delete;
    UniquePropertiesID& operator=(const UniquePropertiesID&) = delete;

    UniquePropertiesID(UniquePropertiesID&& other) noexcept : m_id(other.m_id) { other.m_id = 0; }

    UniquePropertiesID& operator=(UniquePropertiesID&& other) noexcept
    {
        if (this != &other)
        {
            reset();
            m_id = other.m_id;
            other.m_id = 0;
        }
        return *this;
    }

    void reset(SDL_PropertiesID newId = 0)
    {
        if (m_id != 0)
        {
            SDL_DestroyProperties(m_id);
        }
        m_id = newId;
    }
    /**
     * @brief  隐式转换为 SDL_PropertiesID 以便直接传递给 SDL 函数
     * @return SDL_PropertiesID
     */
    operator SDL_PropertiesID() const { return m_id; } // NOLINT
    [[nodiscard]] SDL_PropertiesID get() const { return m_id; }

private:
    SDL_PropertiesID m_id;
};

/**
 * @brief Deleter for SDL_GPUDevice
 */
struct GPUDeviceDeleter
{
    void operator()(SDL_GPUDevice* device) const
    {
        if (device != nullptr)
        {
            SDL_DestroyGPUDevice(device);
        }
    }
};

using UniqueGPUDevice = std::unique_ptr<SDL_GPUDevice, GPUDeviceDeleter>;

/**
 * @brief Generic Deleter for SDL GPU resources that require a device pointer to release
 * @tparam RELEASE_FUNC The SDL function to release the resource
 */
template <auto RELEASE_FUNC>
struct GPUResourceDeleter
{
    SDL_GPUDevice* device = nullptr;

    // Default constructor
    GPUResourceDeleter() = default;

    // Constructor with device
    explicit GPUResourceDeleter(SDL_GPUDevice* dev) : device(dev) {}

    template <typename T>
    void operator()(T* resource) const
    {
        if (device && resource)
        {
            RELEASE_FUNC(device, resource);
        }
    }
};

// Define unique_ptr types for various SDL GPU resources
using UniqueGPUBuffer = std::unique_ptr<SDL_GPUBuffer, GPUResourceDeleter<SDL_ReleaseGPUBuffer>>;
using UniqueGPUTransferBuffer =
    std::unique_ptr<SDL_GPUTransferBuffer, GPUResourceDeleter<SDL_ReleaseGPUTransferBuffer>>;
using UniqueGPUTexture = std::unique_ptr<SDL_GPUTexture, GPUResourceDeleter<SDL_ReleaseGPUTexture>>;
using UniqueGPUShader = std::unique_ptr<SDL_GPUShader, GPUResourceDeleter<SDL_ReleaseGPUShader>>;
using UniqueGPUSampler = std::unique_ptr<SDL_GPUSampler, GPUResourceDeleter<SDL_ReleaseGPUSampler>>;
using UniqueGPUGraphicsPipeline =
    std::unique_ptr<SDL_GPUGraphicsPipeline, GPUResourceDeleter<SDL_ReleaseGPUGraphicsPipeline>>;

/**
 * @brief 创建gpu资源
 */
template <typename UniqueType, typename CreatorFunc, typename... Args>
UniqueType MakeGpuResource(SDL_GPUDevice* device, CreatorFunc creator, Args&&... args)
{
    using DeleterType = typename UniqueType::deleter_type;
    auto* resource = creator(device, std::forward<Args>(args)...);
    return UniqueType(resource, DeleterType(device));
}

} // namespace ui::wrappers
