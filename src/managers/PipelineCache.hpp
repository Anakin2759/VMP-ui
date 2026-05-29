/**
 * ************************************************************************
 *
 * @file PipelineCache.hpp
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2026-01-30
 * @version 0.1
 * @brief 渲染管线缓存管理器
 *
 * ************************************************************************
 * @copyright Copyright (c) 2026 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */
#pragma once
#include <bit>
#include <array>
#include <memory>
#include <SDL3/SDL_gpu.h>
#include "singleton/Logger.hpp"
#include "common/CustomizationPoints.hpp"
#include "common/RenderTypes.hpp"
#include "common/GPUWrappers.hpp"
#include "DeviceManager.hpp"
#include "ResourceProvider.hpp"
#include "common/Result.hpp"
#include "common/ErrorCodes.hpp"

namespace ui::managers
{

class PipelineCache
{
public:
    explicit PipelineCache(DeviceManager& deviceManager,
                           std::shared_ptr<const IResourceProvider> resourceProvider = GetDefaultUiResourceProvider())
        : m_deviceManager(&deviceManager), m_resourceProvider(std::move(resourceProvider))
    {
    }
    ~PipelineCache() { cleanup(); }
    PipelineCache(const PipelineCache&) = delete;
    PipelineCache& operator=(const PipelineCache&) = delete;
    PipelineCache(PipelineCache&&) = default;
    PipelineCache& operator=(PipelineCache&&) = default;

    void loadShaders()
    {
        SDL_GPUDevice* device = m_deviceManager->getDevice();
        if (device == nullptr) return;

        // 根据驱动类型选择着色器格式
        const std::string& driver = m_deviceManager->getDriverName();
        bool isVulkan = (driver == "vulkan");

        if (isVulkan)
        {
            m_vertexShader = loadShaderFromResource(
                "assets/shader/vert.spv", SDL_GPU_SHADERSTAGE_VERTEX, SDL_GPU_SHADERFORMAT_SPIRV);
            m_fragmentShader = loadShaderFromResource(
                "assets/shader/frag.spv", SDL_GPU_SHADERSTAGE_FRAGMENT, SDL_GPU_SHADERFORMAT_SPIRV);
        }
        else
        {
            m_vertexShader = loadShaderFromResource(
                "assets/shader/vert.dxil", SDL_GPU_SHADERSTAGE_VERTEX, SDL_GPU_SHADERFORMAT_DXIL);
            m_fragmentShader = loadShaderFromResource(
                "assets/shader/frag.dxil", SDL_GPU_SHADERSTAGE_FRAGMENT, SDL_GPU_SHADERFORMAT_DXIL);
        }

        if (m_vertexShader == nullptr || m_fragmentShader == nullptr)
        {
            Logger::error("着色器加载失败 (驱动: {})", driver);
        }
        else
        {
            Logger::info("着色器加载成功 (驱动: {})", driver);
        }
    }

    Result<void> createPipeline(SDL_Window* sdlWindow)
    {
        SDL_GPUDevice* device = m_deviceManager->getDevice();
        if (device == nullptr || m_vertexShader == nullptr || m_fragmentShader == nullptr)
        {
            return MakeError(UiErrc::DEVICE_UNAVAILABLE);
        }

        if (m_pipeline != nullptr)
        {
            return Ok();
        }
        if (m_creationFailed)
        {
            return MakeError(UiErrc::PIPELINE_UNAVAILABLE);
        }

        SDL_GPUVertexBufferDescription vertexBufferDesc = {};
        vertexBufferDesc.slot = 0;
        vertexBufferDesc.pitch = sizeof(ui::render::Vertex);
        vertexBufferDesc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
        vertexBufferDesc.instance_step_rate = 0;

        const auto vertexAttributes = buildVertexAttributes();
        const SDL_GPUVertexInputState vertexInputState = buildVertexInputState(vertexBufferDesc, vertexAttributes);

        // 颜色附件描述
        SDL_GPUColorTargetDescription colorTargetDesc = {};
        colorTargetDesc.format = SDL_GetGPUSwapchainTextureFormat(device, sdlWindow);
        if (colorTargetDesc.format == SDL_GPU_TEXTUREFORMAT_INVALID)
        {
            Logger::warn("Swapchain format invalid, falling back to B8G8R8A8_UNORM");
            colorTargetDesc.format = SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;
        }
        colorTargetDesc.blend_state = buildBlendState();

        // 光栅化状态
        const SDL_GPURasterizerState rasterizerState = buildRasterizerState();

        SDL_GPUMultisampleState multisampleState = {};
        multisampleState.sample_count = SDL_GPU_SAMPLECOUNT_1;

        // 深度/模板状态
        const SDL_GPUDepthStencilState depthStencilState = buildDepthStencilState();

        // 创建图形管线
        SDL_GPUGraphicsPipelineCreateInfo pipelineInfo = {};
        pipelineInfo.vertex_shader = m_vertexShader.get();
        pipelineInfo.fragment_shader = m_fragmentShader.get();
        pipelineInfo.vertex_input_state = vertexInputState;
        pipelineInfo.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        pipelineInfo.rasterizer_state = rasterizerState;
        pipelineInfo.multisample_state = multisampleState;
        pipelineInfo.depth_stencil_state = depthStencilState;
        pipelineInfo.target_info.num_color_targets = 1;
        pipelineInfo.target_info.color_target_descriptions = &colorTargetDesc;

        m_pipeline = wrappers::MakeGpuResource<wrappers::UniqueGPUGraphicsPipeline>(
            device, SDL_CreateGPUGraphicsPipeline, &pipelineInfo);

        if (m_pipeline == nullptr)
        {
            Logger::error("图形管线创建失败: {}", SDL_GetError());
            m_creationFailed = true;                        // 标记失败，阻止后续重试
            return MakeError(UiErrc::PIPELINE_UNAVAILABLE); // 管线失败则不创建采样器，避免下次 guard 失效导致重复重试
        }

        // 创建采样器
        SDL_GPUSamplerCreateInfo samplerInfo = {};
        samplerInfo.min_filter = SDL_GPU_FILTER_LINEAR;
        samplerInfo.mag_filter = SDL_GPU_FILTER_LINEAR;
        samplerInfo.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
        samplerInfo.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        samplerInfo.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;

        m_sampler = wrappers::MakeGpuResource<wrappers::UniqueGPUSampler>(device, SDL_CreateGPUSampler, &samplerInfo);
        return Ok();
    }

    void cleanup()
    {
        m_sampler.reset();
        m_pipeline.reset();
        m_vertexShader.reset();
        m_fragmentShader.reset();
        m_creationFailed = false;
    }

    [[nodiscard]] SDL_GPUGraphicsPipeline* getPipeline() const { return m_pipeline.get(); }
    [[nodiscard]] SDL_GPUSampler* getSampler() const { return m_sampler.get(); }
    [[nodiscard]] bool hasCreationFailed() const { return m_creationFailed; }

private:
    [[nodiscard]] static std::array<SDL_GPUVertexAttribute, 7> buildVertexAttributes()
    {
        std::array<SDL_GPUVertexAttribute, 7> vertexAttributes{};

        vertexAttributes[0] = {.location = 0,
                               .buffer_slot = 0,
                               .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
                               .offset = static_cast<uint32_t>(offsetof(ui::render::Vertex, position))};
        vertexAttributes[1] = {.location = 1,
                               .buffer_slot = 0,
                               .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
                               .offset = static_cast<uint32_t>(offsetof(ui::render::Vertex, texCoord))};
        vertexAttributes[2] = {.location = 2,
                               .buffer_slot = 0,
                               .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
                               .offset = static_cast<uint32_t>(offsetof(ui::render::Vertex, color))};
        vertexAttributes[3] = {.location = 3,
                               .buffer_slot = 0,
                               .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
                               .offset = static_cast<uint32_t>(offsetof(ui::render::Vertex, rect_size))};
        vertexAttributes[4] = {.location = 4,
                               .buffer_slot = 0,
                               .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
                               .offset = static_cast<uint32_t>(offsetof(ui::render::Vertex, radius))};
        vertexAttributes[5] = {.location = 5,
                               .buffer_slot = 0,
                               .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
                               .offset = static_cast<uint32_t>(offsetof(ui::render::Vertex, shadow_params))};
        vertexAttributes[6] = {.location = 6,
                               .buffer_slot = 0,
                               .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
                               .offset = static_cast<uint32_t>(offsetof(ui::render::Vertex, mode_params))};
        return vertexAttributes;
    }

    [[nodiscard]] static SDL_GPUVertexInputState
        buildVertexInputState(SDL_GPUVertexBufferDescription& vertexBufferDesc,
                              const std::array<SDL_GPUVertexAttribute, 7>& vertexAttributes)
    {
        SDL_GPUVertexInputState vertexInputState = {};
        vertexInputState.vertex_buffer_descriptions = &vertexBufferDesc;
        vertexInputState.num_vertex_buffers = 1;
        vertexInputState.vertex_attributes = vertexAttributes.data();
        vertexInputState.num_vertex_attributes = static_cast<uint32_t>(vertexAttributes.size());
        return vertexInputState;
    }

    [[nodiscard]] static SDL_GPUColorTargetBlendState buildBlendState()
    {
        SDL_GPUColorTargetBlendState blendState = {};
        blendState.enable_blend = true;
        blendState.src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        blendState.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        blendState.color_blend_op = SDL_GPU_BLENDOP_ADD;
        blendState.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        blendState.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        blendState.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
        blendState.color_write_mask =
            SDL_GPU_COLORCOMPONENT_R | SDL_GPU_COLORCOMPONENT_G | SDL_GPU_COLORCOMPONENT_B | SDL_GPU_COLORCOMPONENT_A;
        blendState.enable_color_write_mask = true;
        return blendState;
    }

    [[nodiscard]] static SDL_GPURasterizerState buildRasterizerState()
    {
        SDL_GPURasterizerState rasterizerState = {};
        rasterizerState.fill_mode = SDL_GPU_FILLMODE_FILL;
        rasterizerState.cull_mode = SDL_GPU_CULLMODE_NONE;
        rasterizerState.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
        rasterizerState.enable_depth_clip = true;
        return rasterizerState;
    }

    [[nodiscard]] static SDL_GPUDepthStencilState buildDepthStencilState()
    {
        SDL_GPUDepthStencilState depthStencilState = {};
        depthStencilState.compare_op = SDL_GPU_COMPAREOP_ALWAYS;
        depthStencilState.back_stencil_state.compare_op = SDL_GPU_COMPAREOP_ALWAYS;
        depthStencilState.back_stencil_state.fail_op = SDL_GPU_STENCILOP_KEEP;
        depthStencilState.back_stencil_state.pass_op = SDL_GPU_STENCILOP_KEEP;
        depthStencilState.back_stencil_state.depth_fail_op = SDL_GPU_STENCILOP_KEEP;
        depthStencilState.front_stencil_state.compare_op = SDL_GPU_COMPAREOP_ALWAYS;
        depthStencilState.front_stencil_state.fail_op = SDL_GPU_STENCILOP_KEEP;
        depthStencilState.front_stencil_state.pass_op = SDL_GPU_STENCILOP_KEEP;
        depthStencilState.front_stencil_state.depth_fail_op = SDL_GPU_STENCILOP_KEEP;
        depthStencilState.enable_depth_test = false;
        depthStencilState.enable_stencil_test = false;
        return depthStencilState;
    }

    wrappers::UniqueGPUShader
        loadShaderFromResource(const char* resourcePath, SDL_GPUShaderStage stage, SDL_GPUShaderFormat format)
    {
        if (m_resourceProvider == nullptr)
        {
            Logger::error("着色器资源提供器未初始化: {}", resourcePath);
            return nullptr;
        }

        auto resourceResult = ui::cpo::load_binary_resource(*m_resourceProvider, resourcePath);
        if (!resourceResult.has_value())
        {
            Logger::error("着色器资源加载失败: {} ({})", resourcePath, resourceResult.error().message());
            return nullptr;
        }

        const BinaryResource& resource = resourceResult.value();
        SDL_GPUShaderCreateInfo shaderInfo = {};
        shaderInfo.code = std::bit_cast<const uint8_t*>(resource.data());
        shaderInfo.code_size = resource.size();
        shaderInfo.entrypoint = (stage == SDL_GPU_SHADERSTAGE_VERTEX) ? "main_vs" : "main_ps";
        shaderInfo.format = format;
        shaderInfo.stage = stage;
        shaderInfo.num_samplers = (stage == SDL_GPU_SHADERSTAGE_FRAGMENT) ? 1U : 0U;
        shaderInfo.num_uniform_buffers = 1U;

        return wrappers::MakeGpuResource<wrappers::UniqueGPUShader>(
            m_deviceManager->getDevice(), SDL_CreateGPUShader, &shaderInfo);
    }

    DeviceManager* m_deviceManager;
    std::shared_ptr<const IResourceProvider> m_resourceProvider;
    wrappers::UniqueGPUGraphicsPipeline m_pipeline;
    wrappers::UniqueGPUShader m_vertexShader;
    wrappers::UniqueGPUShader m_fragmentShader;
    wrappers::UniqueGPUSampler m_sampler;
    bool m_creationFailed = false;
};

} // namespace ui::managers
