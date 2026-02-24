/**
 * ************************************************************************
 *
 * @file RenderSystem.hpp
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2026-01-30
 * @version 0.2
 * @brief SDL GPU 渲染系统 - 重构版
 *
 * 采用模块化设计：
 * - 渲染器层：专门处理特定类型的渲染（ShapeRenderer, TextRenderer等）
 * - 批处理层：批次组装和优化（BatchManager）
 * - 命令层：GPU命令封装（CommandBuffer）
 * - 协调层：渲染管线调度（RenderSystem）
 *
 * ************************************************************************
 * @copyright Copyright (c) 2026 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */

#pragma once
#include <memory>
#include <vector>
#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>
#include <entt/entt.hpp>
#include <Eigen/Dense>
#include <cmrc/cmrc.hpp>

#include "../singleton/Logger.hpp"
#include "../singleton/Dispatcher.hpp"
#include "../common/Events.hpp"
#include "../interface/Isystem.hpp"
#include "../managers/FontManager.hpp"
#include "../managers/IconManager.hpp"
#include "../managers/DeviceManager.hpp"
#include "../common/GPUWrappers.hpp"
#include "../managers/PipelineCache.hpp"
#include "../managers/TextTextureCache.hpp"
#include "../managers/BatchManager.hpp"
#include "../managers/CommandBuffer.hpp"
#include "../interface/IRenderer.hpp"
#include "../interface/IBackendRenderer.hpp"
#include "../core/RenderContext.hpp"

CMRC_DECLARE(ui_fonts);
CMRC_DECLARE(ui_icons);

namespace ui::systems
{

/**
 * @brief SDL GPU 渲染系统
 *
 * 使用模块化架构：
 * 1. 渲染器负责收集渲染数据
 * 2. BatchManager 负责批次优化
 * 3. CommandBuffer 负责GPU命令执行
 */
class RenderSystem final : public interface::EnableRegister<RenderSystem>
{
public:
    RenderSystem();
    ~RenderSystem();

    RenderSystem(const RenderSystem&) = delete;
    RenderSystem& operator=(const RenderSystem&) = delete;
    RenderSystem(RenderSystem&&) noexcept;
    RenderSystem& operator=(RenderSystem&&) noexcept;

    struct RenderStats
    {
        uint64_t frameCount = 0;
        uint32_t batchCount = 0;
        uint32_t vertexCount = 0;
        uint32_t textureCount = 0;
        float lastFrameTime = 0.0F;
    };

    [[nodiscard]] const RenderStats& getStats() const { return m_stats; }

    void registerHandlersImpl()
    {
        Logger::info("[RenderSystem] Registering event handlers");
        Dispatcher::Sink<events::WindowGraphicsContextSetEvent>().connect<&RenderSystem::onWindowsGraphicsContextSet>(
            *this);
        Dispatcher::Sink<events::WindowGraphicsContextUnsetEvent>()
            .connect<&RenderSystem::onWindowsGraphicsContextUnset>(*this);
        Dispatcher::Sink<events::UpdateRendering>().connect<&RenderSystem::update>(*this);
        Logger::info("[RenderSystem] Event handlers registered successfully");
    }

    void unregisterHandlersImpl()
    {
        Dispatcher::Sink<events::WindowGraphicsContextSetEvent>()
            .disconnect<&RenderSystem::onWindowsGraphicsContextSet>(*this);
        Dispatcher::Sink<events::WindowGraphicsContextUnsetEvent>()
            .disconnect<&RenderSystem::onWindowsGraphicsContextUnset>(*this);
        Dispatcher::Sink<events::UpdateRendering>().disconnect<&RenderSystem::update>(*this);
    }

private:
    void onWindowsGraphicsContextSet(const events::WindowGraphicsContextSetEvent& event);
    void onWindowsGraphicsContextUnset(const events::WindowGraphicsContextUnsetEvent& event);
    void cleanup();
    void createWhiteTexture();

    struct RenderItem
    {
        uint64_t sortKey = 0; // Sort key for rendering order
        entt::entity entity = entt::null;
        core::IRenderer* renderer = nullptr;
        core::RenderContext context;

        // Custom comparator for sorting
        bool operator<(const RenderItem& other) const { return sortKey < other.sortKey; }
    };

public:
    void update() noexcept;

private:
    void ensureInitialized();
    bool tryInitializeFallback(SDL_Window* window);
    void initializeRenderers();

    /**
     * @brief 递归收集渲染数据
     * @param entity 当前实体
     * @param context 渲染上下文
     */
    void collectRenderData(entt::entity entity, core::RenderContext& context);

    /**
     * @brief 收集实体背景的渲染数据
     */
    void collectBackgroundData(entt::entity entity, core::RenderContext& context);

    std::unique_ptr<managers::DeviceManager> m_deviceManager;
    std::unique_ptr<managers::FontManager> m_fontManager;
    std::unique_ptr<managers::IconManager> m_iconManager;
    std::unique_ptr<managers::PipelineCache> m_pipelineCache;
    std::unique_ptr<managers::TextTextureCache> m_textTextureCache;
    std::unique_ptr<managers::BatchManager> m_batchManager;
    std::unique_ptr<managers::CommandBuffer> m_commandBuffer;
    std::unique_ptr<interface::IBackendRenderer> m_backendRenderer;

    // 渲染器列表
    std::vector<std::unique_ptr<core::IRenderer>> m_renderers;

    // 渲染队列
    std::vector<RenderItem> m_renderQueue;
    uint32_t m_submissionIndex = 0; // Ensures stability for same Z-order items

    RenderStats m_stats;
    wrappers::UniqueGPUTexture m_whiteTexture;
    SDL_GPUTexture* m_fallbackWhiteTextureTag = reinterpret_cast<SDL_GPUTexture*>(1);

    bool m_useFallback = false;
    bool m_forceFallback = false;

    float m_screenWidth = 0.0F;
    float m_screenHeight = 0.0F;
};

} // namespace ui::systems
