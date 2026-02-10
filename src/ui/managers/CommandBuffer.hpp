/**
 * ************************************************************************
 *
 * @file CommandBuffer.hpp
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2026-01-30
 * @version 0.1
 * @brief 命令缓冲区包装器 - 封装SDL GPU命令和资源管理
    池化
 *
 * ************************************************************************
 * @copyright Copyright (c) 2026 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */

#pragma once
#include <array>
#include <cstdint>
#include <vector>
#include <SDL3/SDL_gpu.h>
#include "../managers/DeviceManager.hpp"
#include "../managers/PipelineCache.hpp"
#include "../common/RenderTypes.hpp"
#include "../common/GPUWrappers.hpp"
#include "../singleton/Logger.hpp"

namespace ui::managers
{

/**
 * @brief 命令缓冲区包装器
 *
 * 负责：
 * 1. 封装SDL GPU命令的提交、渲染通道等操作
 * 2. 管理顶点/索引缓冲区的生命周期和池化
 */
class CommandBuffer
{
public:
    CommandBuffer(DeviceManager& deviceManager, PipelineCache& pipelineCache)
        : m_deviceManager(deviceManager), m_pipelineCache(pipelineCache)
    {
    }

    ~CommandBuffer() { cleanup(); }
    CommandBuffer(const CommandBuffer&) = delete;
    CommandBuffer& operator=(const CommandBuffer&) = delete;
    CommandBuffer(CommandBuffer&&) = delete;
    CommandBuffer& operator=(CommandBuffer&&) = delete;

    /**
     * @brief 执行渲染批次
     * @param batches 渲染批次列表
     */
    void execute(SDL_Window* window, int width, int height, const std::pmr::vector<render::RenderBatch>& batches)
    {
        SDL_GPUDevice* device = m_deviceManager.getDevice();
        if (device == nullptr) return;

        auto [totalVertexCount, totalIndexCount] = calculateBatchTotals(batches);
        if (totalVertexCount == 0 || totalIndexCount == 0) return;

        uint32_t totalVertexSize = totalVertexCount * sizeof(render::Vertex);
        uint32_t totalIndexSize = totalIndexCount * sizeof(uint16_t);

        // 获取当前帧资源
        FrameResource& currentFrame = m_frameResources[m_frameIndex % MAX_FRAMES_IN_FLIGHT];

        // 确保缓冲区足够大
        if (!resizeBuffers(device, currentFrame, totalVertexSize, totalIndexSize))
        {
            Logger::error("Failed to resize buffers.");
            return;
        }

        if (!uploadToTransferBuffer(device, batches, totalVertexSize))
        {
            Logger::error("Failed to map transfer buffer.");
            return;
        }

        SDL_GPUCommandBuffer* cmdBuf = SDL_AcquireGPUCommandBuffer(device);
        if (cmdBuf == nullptr) return;

        SDL_GPUTexture* swapchainTexture = nullptr;
        if (!SDL_WaitAndAcquireGPUSwapchainTexture(cmdBuf, window, &swapchainTexture, nullptr, nullptr))
        {
            Logger::warn("Swapchain texture not ready yet.");
            SDL_CancelGPUCommandBuffer(cmdBuf);
            return;
        }

        if (swapchainTexture == nullptr)
        {
            SDL_SubmitGPUCommandBuffer(cmdBuf);
            return;
        }

        recordCopyPass(cmdBuf, currentFrame, totalVertexSize, totalIndexSize);
        recordRenderPass(cmdBuf, swapchainTexture, width, height, currentFrame, batches);

        // 提交命令缓冲区
        SDL_SubmitGPUCommandBuffer(cmdBuf);

        // 切换到下一帧
        m_frameIndex++;
    }

    /**
     * @brief 清理资源
     */
    void cleanup()
    {
        // RAII handles destruction
        for (auto& frame : m_frameResources)
        {
            frame.vertexBuffer.reset();
            frame.indexBuffer.reset();
            frame.vertexBufferSize = 0;
            frame.indexBufferSize = 0;
        }
        m_transferBuffer.reset();
        m_transferBufferSize = 0;
    }

private:
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

    struct FrameResource
    {
        wrappers::UniqueGPUBuffer vertexBuffer;
        wrappers::UniqueGPUBuffer indexBuffer;
        uint32_t vertexBufferSize = 0;
        uint32_t indexBufferSize = 0;
    };

    [[nodiscard]] std::pair<uint32_t, uint32_t>
        calculateBatchTotals(const std::pmr::vector<render::RenderBatch>& batches) const
    {
        uint32_t vertices = 0;
        uint32_t indices = 0;
        for (const auto& batch : batches)
        {
            vertices += static_cast<uint32_t>(batch.vertices.size());
            indices += static_cast<uint32_t>(batch.indices.size());
        }
        return {vertices, indices};
    }

    bool uploadToTransferBuffer(SDL_GPUDevice* device,
                                const std::pmr::vector<render::RenderBatch>& batches,
                                uint32_t totalVertexSize)
    {
        void* mapData = SDL_MapGPUTransferBuffer(device, m_transferBuffer.get(), true);
        if (mapData == nullptr) return false;

        auto* ptr = static_cast<uint8_t*>(mapData);
        uint32_t vertexOffset = 0;
        uint32_t indexOffset = totalVertexSize;

        for (const auto& batch : batches)
        {
            if (batch.vertices.empty()) continue;
            auto vSize = static_cast<uint32_t>(batch.vertices.size() * sizeof(render::Vertex));
            SDL_memcpy(ptr + vertexOffset, batch.vertices.data(), vSize); // NOLINT
            vertexOffset += vSize;
        }

        for (const auto& batch : batches)
        {
            if (batch.indices.empty()) continue;
            auto iSize = static_cast<uint32_t>(batch.indices.size() * sizeof(uint16_t));
            SDL_memcpy(ptr + indexOffset, batch.indices.data(), iSize);
            indexOffset += iSize;
        }

        SDL_UnmapGPUTransferBuffer(device, m_transferBuffer.get());
        return true;
    }

    void recordCopyPass(SDL_GPUCommandBuffer* cmdBuf,
                        const FrameResource& currentFrame,
                        uint32_t totalVertexSize,
                        uint32_t totalIndexSize)
    {
        SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmdBuf);

        SDL_GPUTransferBufferLocation srcLoc = {};
        srcLoc.transfer_buffer = m_transferBuffer.get();
        srcLoc.offset = 0;

        SDL_GPUBufferRegion dstReg = {};
        dstReg.buffer = currentFrame.vertexBuffer.get();
        dstReg.offset = 0;
        dstReg.size = totalVertexSize;

        SDL_UploadToGPUBuffer(copyPass, &srcLoc, &dstReg, false);

        srcLoc.offset = totalVertexSize;
        dstReg.buffer = currentFrame.indexBuffer.get();
        dstReg.size = totalIndexSize;
        SDL_UploadToGPUBuffer(copyPass, &srcLoc, &dstReg, false);

        SDL_EndGPUCopyPass(copyPass);
    }

    void recordRenderPass(SDL_GPUCommandBuffer* cmdBuf,
                          SDL_GPUTexture* swapchainTexture,
                          int width,
                          int height,
                          const FrameResource& currentFrame,
                          const std::pmr::vector<render::RenderBatch>& batches)
    {
        SDL_GPUColorTargetInfo colorTarget = {};
        colorTarget.texture = swapchainTexture;
        // 纯黑清屏色，确保圆角裁剪区域与元素背景形成对比
        colorTarget.clear_color = {.r = 0.0F, .g = 0.0F, .b = 0.0F, .a = 1.0F};
        colorTarget.load_op = SDL_GPU_LOADOP_CLEAR;
        colorTarget.store_op = SDL_GPU_STOREOP_STORE;

        SDL_GPURenderPass* renderPass = SDL_BeginGPURenderPass(cmdBuf, &colorTarget, 1, nullptr);

        SDL_BindGPUGraphicsPipeline(renderPass, m_pipelineCache.getPipeline());

        SDL_GPUViewport viewport = {};
        viewport.x = 0;
        viewport.y = 0;
        viewport.w = static_cast<float>(width);
        viewport.h = static_cast<float>(height);
        viewport.min_depth = 0.0F;
        viewport.max_depth = 1.0F;
        SDL_SetGPUViewport(renderPass, &viewport);

        SDL_GPUBufferBinding vertexBinding = {};
        vertexBinding.buffer = currentFrame.vertexBuffer.get();
        vertexBinding.offset = 0;
        SDL_BindGPUVertexBuffers(renderPass, 0, &vertexBinding, 1);

        SDL_GPUBufferBinding indexBinding = {};
        indexBinding.buffer = currentFrame.indexBuffer.get();
        indexBinding.offset = 0;
        SDL_BindGPUIndexBuffer(renderPass, &indexBinding, SDL_GPU_INDEXELEMENTSIZE_16BIT);

        uint32_t currentVertexOffset = 0;
        uint32_t currentIndexOffset = 0;

        for (const auto& batch : batches)
        {
            if (batch.vertices.empty() || batch.indices.empty()) continue;

            if (batch.scissorRect.has_value())
            {
                SDL_Rect r = batch.scissorRect.value();
                SDL_SetGPUScissor(renderPass, &r);
            }
            else
            {
                SDL_Rect fullViewport = {0, 0, width, height};
                SDL_SetGPUScissor(renderPass, &fullViewport);
            }

            if (batch.texture != nullptr)
            {
                SDL_GPUTextureSamplerBinding texSamplerBinding = {};
                texSamplerBinding.texture = static_cast<SDL_GPUTexture*>(batch.texture);
                texSamplerBinding.sampler = m_pipelineCache.getSampler();
                SDL_BindGPUFragmentSamplers(renderPass, 0, &texSamplerBinding, 1);
            }

            SDL_PushGPUVertexUniformData(cmdBuf, 0, &batch.pushConstants, sizeof(render::UiPushConstants));
            SDL_PushGPUFragmentUniformData(cmdBuf, 0, &batch.pushConstants, sizeof(render::UiPushConstants));

            SDL_DrawGPUIndexedPrimitives(renderPass,
                                         static_cast<uint32_t>(batch.indices.size()),
                                         1,
                                         currentIndexOffset,
                                         static_cast<int32_t>(currentVertexOffset),
                                         0);

            currentVertexOffset += static_cast<uint32_t>(batch.vertices.size());
            currentIndexOffset += static_cast<uint32_t>(batch.indices.size());
        }

        SDL_EndGPURenderPass(renderPass);
    }

    bool resizeBuffers(SDL_GPUDevice* device, FrameResource& frame, uint32_t vSize, uint32_t iSize)
    {
        // 传输缓冲区 (Shared across frames, handled by cycle=true)
        uint32_t neededTransfer = vSize + iSize;

        if (m_transferBufferSize < neededTransfer)
        {
            m_transferBufferSize =
                neededTransfer > m_transferBufferSize * 2 ? neededTransfer : m_transferBufferSize * 2;
            if (m_transferBufferSize < neededTransfer) m_transferBufferSize = neededTransfer;

            SDL_GPUTransferBufferCreateInfo tInfo = {};
            tInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
            tInfo.size = m_transferBufferSize;

            m_transferBuffer = wrappers::MakeGpuResource<wrappers::UniqueGPUTransferBuffer>(
                device, SDL_CreateGPUTransferBuffer, &tInfo);
            if (!m_transferBuffer) return false;
        }

        // 顶点缓冲区 (Per Frame)
        if (frame.vertexBufferSize < vSize)
        {
            frame.vertexBufferSize = vSize > frame.vertexBufferSize * 2 ? vSize : frame.vertexBufferSize * 2;

            SDL_GPUBufferCreateInfo bInfo = {};
            bInfo.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
            bInfo.size = frame.vertexBufferSize;
            frame.vertexBuffer =
                wrappers::MakeGpuResource<wrappers::UniqueGPUBuffer>(device, SDL_CreateGPUBuffer, &bInfo);
            if (!frame.vertexBuffer) return false;
        }

        // 索引缓冲区 (Per Frame)
        if (frame.indexBufferSize < iSize)
        {
            frame.indexBufferSize = iSize > frame.indexBufferSize * 2 ? iSize : frame.indexBufferSize * 2;

            SDL_GPUBufferCreateInfo bInfo = {};
            bInfo.usage = SDL_GPU_BUFFERUSAGE_INDEX;
            bInfo.size = frame.indexBufferSize;
            frame.indexBuffer =
                wrappers::MakeGpuResource<wrappers::UniqueGPUBuffer>(device, SDL_CreateGPUBuffer, &bInfo);
            if (!frame.indexBuffer) return false;
        }
        return true;
    }

    DeviceManager& m_deviceManager;
    PipelineCache& m_pipelineCache;

    // 帧资源池
    std::array<FrameResource, MAX_FRAMES_IN_FLIGHT> m_frameResources;
    uint32_t m_frameIndex = 0;

    wrappers::UniqueGPUTransferBuffer m_transferBuffer;
    uint32_t m_transferBufferSize = 0;
};

} // namespace ui::managers
