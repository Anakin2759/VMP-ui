/**
 * ************************************************************************
 *
 * @file Batcher.hpp
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2026-01-30
 * @version 0.1
 * @brief UI 渲染批处理器
 *
 * ************************************************************************
 * @copyright Copyright (c) 2026 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */
#pragma once
#include <vector>
#include <span>
#include <optional>
#include <SDL3/SDL_gpu.h>
#include <Eigen/Dense>
#include "common/RenderTypes.hpp"
#include "common/GPUWrappers.hpp"
#include "managers/DeviceManager.hpp"
#include "managers/PipelineCache.hpp"

namespace ui::core
{

class Batcher
{
public:
    Batcher(ui::managers::DeviceManager& deviceManager, ui::managers::PipelineCache& pipelineCache)
        : m_deviceManager(deviceManager), m_pipelineCache(pipelineCache)
    {
    }

    ~Batcher() { cleanup(); }
    Batcher(const Batcher&) = delete;
    Batcher& operator=(const Batcher&) = delete;
    Batcher(Batcher&&) = delete;
    Batcher& operator=(Batcher&&) = delete;

    void begin()
    {
        m_batches.clear();
        m_scissorStack.clear();
        if (!m_whiteTexture)
        {
            createWhiteTexture();
        }
    }

    void cleanup()
    {
        m_whiteTexture.reset();
        m_batches.clear();
    }

    // 设置屏幕尺寸 (用于 PushConstants)
    void setScreenSize(float width, float height)
    {
        m_screenWidth = width;
        m_screenHeight = height;
    }

    void pushScissor(const SDL_Rect& rect)
    {
        if (m_scissorStack.empty())
        {
            m_scissorStack.push_back(rect);
        }
        else
        {
            const SDL_Rect& top = m_scissorStack.back();
            SDL_Rect intersect;
            if (SDL_GetRectIntersection(&top, &rect, &intersect))
            {
                m_scissorStack.push_back(intersect);
            }
            else
            {
                // 无交集
                m_scissorStack.push_back({0, 0, 0, 0});
            }
        }
    }

    void popScissor()
    {
        if (!m_scissorStack.empty())
        {
            m_scissorStack.pop_back();
        }
    }

    void addRectFilledWithRounding(const Eigen::Vector2f& pos,
                                   const Eigen::Vector2f& size,
                                   const Eigen::Vector4f& color,
                                   const Eigen::Vector4f& radius,
                                   float opacity,
                                   float shadowSoft = 0.0F,
                                   float shadowOffsetX = 0.0F,
                                   float shadowOffsetY = 0.0F)
    {
        render::RenderBatch batch;
        if (!m_scissorStack.empty())
        {
            batch.scissorRect = m_scissorStack.back();
        }
        batch.texture = m_whiteTexture.get();

        batch.pushConstants.screen_size[0] = m_screenWidth;
        batch.pushConstants.screen_size[1] = m_screenHeight;
        batch.pushConstants.rect_size[0] = size.x();
        batch.pushConstants.rect_size[1] = size.y();
        batch.pushConstants.radius[0] = radius.x();
        batch.pushConstants.radius[1] = radius.y();
        batch.pushConstants.radius[2] = radius.z();
        batch.pushConstants.radius[3] = radius.w();
        batch.pushConstants.shadow_soft = shadowSoft;
        batch.pushConstants.shadow_offset_x = shadowOffsetX;
        batch.pushConstants.shadow_offset_y = shadowOffsetY;
        batch.pushConstants.opacity = opacity;
        batch.pushConstants.padding = 0.0F;

        Eigen::Vector2f max = pos + size;

        batch.vertices.push_back({{pos.x(), pos.y()}, {0.0F, 0.0F}, {color.x(), color.y(), color.z(), color.w()}});
        batch.vertices.push_back({{max.x(), pos.y()}, {1.0F, 0.0F}, {color.x(), color.y(), color.z(), color.w()}});
        batch.vertices.push_back({{max.x(), max.y()}, {1.0F, 1.0F}, {color.x(), color.y(), color.z(), color.w()}});
        batch.vertices.push_back({{pos.x(), max.y()}, {0.0F, 1.0F}, {color.x(), color.y(), color.z(), color.w()}});

        batch.indices = {0, 1, 2, 0, 2, 3};

        m_batches.push_back(std::move(batch));
    }

    void addImageBatch(SDL_GPUTexture* texture,
                       const Eigen::Vector2f& pos,
                       const Eigen::Vector2f& size,
                       const Eigen::Vector2f& uvMin,
                       const Eigen::Vector2f& uvMax,
                       const Eigen::Vector4f& tint,
                       float opacity)
    {
        render::RenderBatch batch;
        if (!m_scissorStack.empty())
        {
            batch.scissorRect = m_scissorStack.back();
        }
        batch.texture = texture;

        batch.pushConstants.screen_size[0] = m_screenWidth;
        batch.pushConstants.screen_size[1] = m_screenHeight;
        batch.pushConstants.rect_size[0] = size.x();
        batch.pushConstants.rect_size[1] = size.y();
        batch.pushConstants.radius[0] = 0.0F;
        batch.pushConstants.radius[1] = 0.0F;
        batch.pushConstants.radius[2] = 0.0F;
        batch.pushConstants.radius[3] = 0.0F;
        batch.pushConstants.shadow_soft = 0.0F;
        batch.pushConstants.shadow_offset_x = 0.0F;
        batch.pushConstants.shadow_offset_y = 0.0F;
        batch.pushConstants.opacity = opacity;
        batch.pushConstants.padding = 0.0F;

        Eigen::Vector2f max = pos + size;

        batch.vertices.push_back(
            {{pos.x(), pos.y()}, {uvMin.x(), uvMin.y()}, {tint.x(), tint.y(), tint.z(), tint.w()}});
        batch.vertices.push_back(
            {{max.x(), pos.y()}, {uvMax.x(), uvMin.y()}, {tint.x(), tint.y(), tint.z(), tint.w()}});
        batch.vertices.push_back(
            {{max.x(), max.y()}, {uvMax.x(), uvMax.y()}, {tint.x(), tint.y(), tint.z(), tint.w()}});
        batch.vertices.push_back(
            {{pos.x(), max.y()}, {uvMin.x(), uvMax.y()}, {tint.x(), tint.y(), tint.z(), tint.w()}});

        batch.indices = {0, 1, 2, 0, 2, 3};

        m_batches.push_back(std::move(batch));
    }

    void render(SDL_Window* window, int width, int height)
    {
        SDL_GPUDevice* device = m_deviceManager.getDevice();
        SDL_GPUGraphicsPipeline* pipeline = m_pipelineCache.getPipeline();

        if (device == nullptr || pipeline == nullptr) return;

        SDL_GPUCommandBuffer* cmdBuf = SDL_AcquireGPUCommandBuffer(device);
        if (cmdBuf == nullptr) return;

        SDL_GPUTexture* swapchainTexture = nullptr;
        if (!SDL_WaitAndAcquireGPUSwapchainTexture(cmdBuf, window, &swapchainTexture, nullptr, nullptr))
        {
            // Failed to acquire swapchain
            SDL_SubmitGPUCommandBuffer(cmdBuf);
            return;
        }

        if (swapchainTexture == nullptr)
        {
            SDL_SubmitGPUCommandBuffer(cmdBuf);
            return;
        }

        SDL_GPUColorTargetInfo colorTarget = {};
        colorTarget.texture = swapchainTexture;
        colorTarget.clear_color = {0.0F, 0.0F, 0.0F, 0.0F};
        colorTarget.load_op = SDL_GPU_LOADOP_CLEAR;
        colorTarget.store_op = SDL_GPU_STOREOP_STORE;

        SDL_GPURenderPass* renderPass = SDL_BeginGPURenderPass(cmdBuf, &colorTarget, 1, nullptr);
        SDL_BindGPUGraphicsPipeline(renderPass, pipeline);

        SDL_GPUViewport viewport = {};
        viewport.x = 0;
        viewport.y = 0;
        viewport.w = static_cast<float>(width);
        viewport.h = static_cast<float>(height);
        viewport.min_depth = 0.0F;
        viewport.max_depth = 1.0F;
        SDL_SetGPUViewport(renderPass, &viewport);

        for (const auto& batch : m_batches)
        {
            if (batch.scissorRect.has_value())
            {
                SDL_Rect scissor = batch.scissorRect.value();
                SDL_SetGPUScissor(renderPass, &scissor);
            }
            else
            {
                SDL_Rect scissor = {0, 0, width, height};
                SDL_SetGPUScissor(renderPass, &scissor);
            }

            SDL_GPUTextureSamplerBinding textureSamplerBinding = {};
            textureSamplerBinding.texture = batch.texture;
            textureSamplerBinding.sampler = m_pipelineCache.getSampler();

            SDL_BindGPUFragmentSamplers(renderPass, 0, &textureSamplerBinding, 1);

            SDL_PushGPUFragmentUniformData(cmdBuf, 0, &batch.pushConstants, sizeof(render::UiPushConstants));

            SDL_GPUBuffer* vertBuf = uploadBatchVertices(batch.vertices);
            SDL_GPUBuffer* idxBuf = uploadBatchIndices(batch.indices);

            if (vertBuf != nullptr && idxBuf != nullptr)
            {
                SDL_GPUBufferBinding vertexBinding = {};
                vertexBinding.buffer = vertBuf;
                vertexBinding.offset = 0;
                SDL_BindGPUVertexBuffers(renderPass, 0, &vertexBinding, 1);

                SDL_GPUBufferBinding indexBinding = {};
                indexBinding.buffer = idxBuf;
                indexBinding.offset = 0;
                SDL_BindGPUIndexBuffer(renderPass, &indexBinding, SDL_GPU_INDEXELEMENTSIZE_16BIT);

                SDL_DrawGPUIndexedPrimitives(renderPass, static_cast<uint32_t>(batch.indices.size()), 1, 0, 0, 0);

                SDL_ReleaseGPUBuffer(device, vertBuf);
                SDL_ReleaseGPUBuffer(device, idxBuf);
            }
        }

        SDL_EndGPURenderPass(renderPass);
        SDL_SubmitGPUCommandBuffer(cmdBuf);
    }

private:
    void createWhiteTexture()
    {
        SDL_GPUDevice* device = m_deviceManager.getDevice();
        if (device == nullptr) return;

        SDL_GPUTextureCreateInfo texInfo = {};
        texInfo.type = SDL_GPU_TEXTURETYPE_2D;
        texInfo.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
        texInfo.width = 1;
        texInfo.height = 1;
        texInfo.layer_count_or_depth = 1;
        texInfo.num_levels = 1;
        texInfo.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;

        m_whiteTexture = wrappers::MakeGpuResource<wrappers::UniqueGPUTexture>(device, SDL_CreateGPUTexture, &texInfo);
        if (!m_whiteTexture) return;

        uint32_t whitePixel = 0xFFFFFFFF;
        SDL_GPUTransferBufferCreateInfo transferInfo = {};
        transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        transferInfo.size = sizeof(whitePixel);

        SDL_GPUTransferBuffer* transfer = SDL_CreateGPUTransferBuffer(device, &transferInfo);
        void* data = SDL_MapGPUTransferBuffer(device, transfer, false);
        SDL_memcpy(data, &whitePixel, sizeof(whitePixel));
        SDL_UnmapGPUTransferBuffer(device, transfer);

        SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(device);
        SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmd);

        SDL_GPUTextureTransferInfo srcInfo = {};
        srcInfo.transfer_buffer = transfer;
        srcInfo.pixels_per_row = 1;
        srcInfo.rows_per_layer = 1;

        SDL_GPUTextureRegion dstRegion = {};
        dstRegion.texture = m_whiteTexture.get();
        dstRegion.w = 1;
        dstRegion.h = 1;
        dstRegion.d = 1;

        SDL_UploadToGPUTexture(copyPass, &srcInfo, &dstRegion, false);
        SDL_EndGPUCopyPass(copyPass);
        SDL_SubmitGPUCommandBuffer(cmd);
        SDL_ReleaseGPUTransferBuffer(device, transfer);
    }

    SDL_GPUBuffer* uploadBatchVertices(std::span<const render::Vertex> vertices)
    {
        SDL_GPUDevice* device = m_deviceManager.getDevice();
        const auto bufferSize = static_cast<uint32_t>(vertices.size() * sizeof(render::Vertex));

        SDL_GPUBufferCreateInfo bufferInfo = {};
        bufferInfo.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
        bufferInfo.size = bufferSize;
        SDL_GPUBuffer* buffer = SDL_CreateGPUBuffer(device, &bufferInfo);

        if (buffer == nullptr) return nullptr;

        SDL_GPUTransferBufferCreateInfo transferInfo = {};
        transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        transferInfo.size = bufferSize;

        SDL_GPUTransferBuffer* transferBuffer = SDL_CreateGPUTransferBuffer(device, &transferInfo);
        if (transferBuffer == nullptr)
        {
            SDL_ReleaseGPUBuffer(device, buffer);
            return nullptr;
        }

        void* data = SDL_MapGPUTransferBuffer(device, transferBuffer, false);
        SDL_memcpy(data, vertices.data(), bufferSize);
        SDL_UnmapGPUTransferBuffer(device, transferBuffer);

        SDL_GPUCommandBuffer* uploadCmd = SDL_AcquireGPUCommandBuffer(device);
        SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(uploadCmd);

        SDL_GPUTransferBufferLocation srcLocation = {};
        srcLocation.transfer_buffer = transferBuffer;
        srcLocation.offset = 0;

        SDL_GPUBufferRegion dstRegion = {};
        dstRegion.buffer = buffer;
        dstRegion.offset = 0;
        dstRegion.size = bufferSize;

        SDL_UploadToGPUBuffer(copyPass, &srcLocation, &dstRegion, false);
        SDL_EndGPUCopyPass(copyPass);
        SDL_SubmitGPUCommandBuffer(uploadCmd);

        SDL_ReleaseGPUTransferBuffer(device, transferBuffer);
        return buffer;
    }

    SDL_GPUBuffer* uploadBatchIndices(std::span<const uint16_t> indices)
    {
        SDL_GPUDevice* device = m_deviceManager.getDevice();
        const auto bufferSize = static_cast<uint32_t>(indices.size() * sizeof(uint16_t));

        SDL_GPUBufferCreateInfo bufferInfo = {};
        bufferInfo.usage = SDL_GPU_BUFFERUSAGE_INDEX;
        bufferInfo.size = bufferSize;
        SDL_GPUBuffer* buffer = SDL_CreateGPUBuffer(device, &bufferInfo);

        if (buffer == nullptr) return nullptr;

        SDL_GPUTransferBufferCreateInfo transferInfo = {};
        transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        transferInfo.size = bufferSize;

        SDL_GPUTransferBuffer* transferBuffer = SDL_CreateGPUTransferBuffer(device, &transferInfo);
        if (transferBuffer == nullptr)
        {
            SDL_ReleaseGPUBuffer(device, buffer);
            return nullptr;
        }

        void* data = SDL_MapGPUTransferBuffer(device, transferBuffer, false);
        SDL_memcpy(data, indices.data(), bufferSize);
        SDL_UnmapGPUTransferBuffer(device, transferBuffer);

        SDL_GPUCommandBuffer* uploadCmd = SDL_AcquireGPUCommandBuffer(device);
        SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(uploadCmd);

        SDL_GPUTransferBufferLocation srcLocation = {};
        srcLocation.transfer_buffer = transferBuffer;
        srcLocation.offset = 0;

        SDL_GPUBufferRegion dstRegion = {};
        dstRegion.buffer = buffer;
        dstRegion.offset = 0;
        dstRegion.size = bufferSize;

        SDL_UploadToGPUBuffer(copyPass, &srcLocation, &dstRegion, false);
        SDL_EndGPUCopyPass(copyPass);
        SDL_SubmitGPUCommandBuffer(uploadCmd);

        SDL_ReleaseGPUTransferBuffer(device, transferBuffer);
        return buffer;
    }

    ui::managers::DeviceManager& m_deviceManager;
    ui::managers::PipelineCache& m_pipelineCache;

    std::vector<render::RenderBatch> m_batches;
    std::vector<SDL_Rect> m_scissorStack;
    wrappers::UniqueGPUTexture m_whiteTexture;

    float m_screenWidth = 0.0F;
    float m_screenHeight = 0.0F;
};

} // namespace ui::core
