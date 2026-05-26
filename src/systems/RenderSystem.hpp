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
#include <cstdint>
#include <memory>

#include "../common/Events.hpp"
#include "../common/Result.hpp"
#include "../interface/Isystem.hpp"

struct SDL_Window;

namespace ui::core
{
struct RenderContext;
}

namespace ui::systems
{

struct RenderSystemImpl;

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
    /**
     * @brief 获取当前渲染统计数据
     * @return const RenderStats& 当前渲染统计数据的常量引用
     */
    [[nodiscard]] const RenderStats& getStats() const;

    /**
     * @brief 注册事件处理程序
     */
    void registerHandlersImpl();

    /**
     * @brief 注销事件处理程序
     */
    void unregisterHandlersImpl();

    interface::SystemPhase getPhase();

    void update();

private:
    /**
     * @brief 处理窗口图形上下文设置事件
     * @param event 窗口图形上下文设置事件
     */
    void onWindowsGraphicsContextSet(const events::WindowGraphicsContextSetEvent& event);
    /**
     * @brief 处理窗口图形上下文取消事件
     * @param event 窗口图形上下文取消事件
     */
    void onWindowsGraphicsContextUnset(const events::WindowGraphicsContextUnsetEvent& event);
    /**
     * @brief 渲染系统清理资源
     */
    void cleanup();
    /**
     * @brief 创建一个纯白色纹理，用于默认渲染
     */
    void createWhiteTexture();
    void ensureInitialized();
    ui::Result<void> tryInitializeFallback(SDL_Window* window);
    void initializeRenderers();

    /**
     * @brief 递归收集渲染数据
     * @param entity 当前实体
     * @param context 渲染上下文
     */
    void collectRenderData(entt::entity entity, core::RenderContext& context);

    std::unique_ptr<RenderSystemImpl> m_impl;
};

} // namespace ui::systems
