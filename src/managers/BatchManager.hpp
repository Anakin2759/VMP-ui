/**
 * ************************************************************************
 *
 * @file BatchManager.hpp
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2026-01-30
 * @version 0.1
 * @brief 批次管理器 - 负责渲染批次的组装、合并和优化
 *
 * ************************************************************************
 * @copyright Copyright (c) 2026 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */

#pragma once
#include <vector>
#include <optional>
#include <array>
#include <memory_resource>
#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_rect.h>
#include <Eigen/Dense>
#include "../common/RenderTypes.hpp"

namespace ui::managers
{

static constexpr size_t MAX_BATCH_COUNT = 256ULL * 1024ULL;
/**
 * @brief 批次管理器
 *
 * 负责：
 * 1. 收集渲染命令并组装成批次
 * 2. 批次合并优化（相同纹理、相同裁剪区域）
 * 3. 状态排序减少状态切换
 */
class BatchManager
{
public:
    BatchManager()
        : m_bufferResource(MAX_BATCH_COUNT), // 预分配 256KB
          m_batches(std::in_place, &m_bufferResource)
    {
    }

    /**
     * @brief 清空所有批次
     */
    void clear()
    {
        m_currentBatch.reset();
        m_batches.reset();
        m_bufferResource.release();
        m_batches.emplace(&m_bufferResource);
    }

    /**
     * @brief 开始新的批次
     * @param texture 纹理指针
     * @param scissor 裁剪区域
     * @param pushConstants 推送常量（逐实体字段由内部提取至顶点属性，保留全量用于兼容 FallbackBackendRenderer）
     */
    void beginBatch(SDL_GPUTexture* texture,
                    const std::optional<SDL_Rect>& scissor,
                    const render::UiPushConstants& pushConstants)
    {
        // 从 UiPushConstants 提取逐实体字段到 m_pendingQuadParams，addRect 时写入顶点属性
        m_pendingQuadParams.rect_size[0]    = pushConstants.rect_size[0];
        m_pendingQuadParams.rect_size[1]    = pushConstants.rect_size[1];
        m_pendingQuadParams.radius[0]       = pushConstants.radius[0];
        m_pendingQuadParams.radius[1]       = pushConstants.radius[1];
        m_pendingQuadParams.radius[2]       = pushConstants.radius[2];
        m_pendingQuadParams.radius[3]       = pushConstants.radius[3];
        m_pendingQuadParams.shadow_soft     = pushConstants.shadow_soft;
        m_pendingQuadParams.shadow_offset_x = pushConstants.shadow_offset_x;
        m_pendingQuadParams.shadow_offset_y = pushConstants.shadow_offset_y;
        m_pendingQuadParams.opacity         = pushConstants.opacity;
        m_pendingQuadParams.padding_flag    = pushConstants.padding;
        m_pendingQuadParams.stroke_width    = pushConstants.stroke_width;
        m_pendingQuadParams.draw_mode       = pushConstants.draw_mode;

        // 检查是否可以与当前批次合并（只比较纹理 + 裁剪区域）
        if (m_currentBatch.has_value())
        {
            bool canMerge = (m_currentBatch->texture == texture);

            // 检查裁剪区域是否相同
            if (canMerge)
            {
                if (scissor.has_value() && m_currentBatch->scissorRect.has_value())
                {
                    const SDL_Rect& nextScissor = scissor.value();
                    const SDL_Rect& currentScissor = m_currentBatch->scissorRect.value();
                    canMerge = (nextScissor.x == currentScissor.x && nextScissor.y == currentScissor.y &&
                                nextScissor.w == currentScissor.w && nextScissor.h == currentScissor.h);
                }
                else if (scissor.has_value() != m_currentBatch->scissorRect.has_value())
                {
                    canMerge = false;
                }
            }

            if (!canMerge)
            {
                flushBatch();
            }
        }

        if (!m_currentBatch.has_value())
        {
            m_currentBatch.emplace(&m_bufferResource);
            m_currentBatch->texture = texture;
            m_currentBatch->scissorRect = scissor;
            m_currentBatch->pushConstants = pushConstants; // 保留全量供 FallbackBackendRenderer 使用
        }
    }

    /**
     * @brief 添加顶点到当前批次
     */
    void addVertex(const render::Vertex& vertex)
    {
        if (!m_currentBatch.has_value())
        {
            return;
        }
        m_currentBatch->vertices.push_back(vertex);
    }

    /**
     * @brief 添加索引到当前批次
     */
    void addIndex(uint16_t index)
    {
        if (!m_currentBatch.has_value())
        {
            return;
        }
        m_currentBatch->indices.push_back(index);
    }

    /**
     * @brief 获取当前批次已写入的顶点数（用于自定义图元计算 baseIndex）
     */
    [[nodiscard]] uint32_t currentVertexCount() const
    {
        return m_currentBatch.has_value() ? static_cast<uint32_t>(m_currentBatch->vertices.size()) : 0U;
    }

    /**
     * @brief 添加有向四边形（4顶点任意位置，用于旋转线段/胶囊）
     *
     * @param v0~v3  四个角的屏幕坐标（顺序：左上、右上、右下、左下，以线段方向为 X 轴）
     * @param uv0~uv3 对应的 UV 坐标（编码局部坐标，由调用方计算）
     * @param color  顶点颜色
     */
    void addOrientedQuad(const Eigen::Vector2f& vert0, const Eigen::Vector2f& vert1,
                         const Eigen::Vector2f& vert2, const Eigen::Vector2f& vert3,
                         const Eigen::Vector2f& uvVert0, const Eigen::Vector2f& uvVert1,
                         const Eigen::Vector2f& uvVert2, const Eigen::Vector2f& uvVert3,
                         const Eigen::Vector4f& color)
    {
        if (!m_currentBatch.has_value())
        {
            return;
        }

        auto baseIndex = static_cast<uint16_t>(m_currentBatch->vertices.size());

        // 按顺序嵌入四个顶点
        const std::array<Eigen::Vector2f, 4> positions{vert0, vert1, vert2, vert3};
        const std::array<Eigen::Vector2f, 4> uvCoords{uvVert0, uvVert1, uvVert2, uvVert3};

        for (std::size_t vIdx = 0; vIdx < 4; ++vIdx)
        {
            render::Vertex vtx{};
            vtx.position[0] = positions.at(vIdx).x(); // NOLINT
            vtx.position[1] = positions.at(vIdx).y(); // NOLINT
            vtx.texCoord[0] = uvCoords.at(vIdx).x();  // NOLINT
            vtx.texCoord[1] = uvCoords.at(vIdx).y();  // NOLINT
            vtx.color[0] = color.x();
            vtx.color[1] = color.y();
            vtx.color[2] = color.z();
            vtx.color[3] = color.w();
            vtx.rect_size[0]     = m_pendingQuadParams.rect_size[0];
            vtx.rect_size[1]     = m_pendingQuadParams.rect_size[1];
            vtx.radius[0]        = m_pendingQuadParams.radius[0];
            vtx.radius[1]        = m_pendingQuadParams.radius[1];
            vtx.radius[2]        = m_pendingQuadParams.radius[2];
            vtx.radius[3]        = m_pendingQuadParams.radius[3];
            vtx.shadow_params[0] = m_pendingQuadParams.shadow_soft;
            vtx.shadow_params[1] = m_pendingQuadParams.shadow_offset_x;
            vtx.shadow_params[2] = m_pendingQuadParams.shadow_offset_y;
            vtx.shadow_params[3] = m_pendingQuadParams.opacity;
            vtx.mode_params[0]   = m_pendingQuadParams.padding_flag;
            vtx.mode_params[1]   = m_pendingQuadParams.stroke_width;
            vtx.mode_params[2]   = m_pendingQuadParams.draw_mode;
            vtx.mode_params[3]   = 0.0F;
            m_currentBatch->vertices.push_back(vtx);
        }

        std::array<uint16_t, 6> indices = {baseIndex,
                                           static_cast<uint16_t>(baseIndex + 1),
                                           static_cast<uint16_t>(baseIndex + 2),
                                           baseIndex,
                                           static_cast<uint16_t>(baseIndex + 2),
                                           static_cast<uint16_t>(baseIndex + 3)};
        for (const auto& idx : indices)
        {
            m_currentBatch->indices.push_back(idx);
        }
    }

    /**
     * @brief 添加矩形（4个顶点 + 6个索引）
     */
    void addRect(const Eigen::Vector2f& pos,
                 const Eigen::Vector2f& size,
                 const Eigen::Vector4f& color,
                 const Eigen::Vector2f& uvMin = {0.0F, 0.0F},
                 const Eigen::Vector2f& uvMax = {1.0F, 1.0F})
    {
        if (!m_currentBatch.has_value())
        {
            return;
        }

        auto baseIndex = static_cast<uint16_t>(m_currentBatch->vertices.size());

        // 添加4个顶点
        std::array<render::Vertex, 4> vertices{};

        // 左上
        vertices[0].position[0] = pos.x();
        vertices[0].position[1] = pos.y();
        vertices[0].texCoord[0] = uvMin.x();
        vertices[0].texCoord[1] = uvMin.y();
        vertices[0].color[0] = color.x();
        vertices[0].color[1] = color.y();
        vertices[0].color[2] = color.z();
        vertices[0].color[3] = color.w();

        // 右上
        vertices[1].position[0] = pos.x() + size.x();
        vertices[1].position[1] = pos.y();
        vertices[1].texCoord[0] = uvMax.x();
        vertices[1].texCoord[1] = uvMin.y();
        vertices[1].color[0] = color.x();
        vertices[1].color[1] = color.y();
        vertices[1].color[2] = color.z();
        vertices[1].color[3] = color.w();

        // 右下
        vertices[2].position[0] = pos.x() + size.x();
        vertices[2].position[1] = pos.y() + size.y();
        vertices[2].texCoord[0] = uvMax.x();
        vertices[2].texCoord[1] = uvMax.y();
        vertices[2].color[0] = color.x();
        vertices[2].color[1] = color.y();
        vertices[2].color[2] = color.z();
        vertices[2].color[3] = color.w();

        // 左下
        vertices[3].position[0] = pos.x();
        vertices[3].position[1] = pos.y() + size.y();
        vertices[3].texCoord[0] = uvMin.x();
        vertices[3].texCoord[1] = uvMax.y();
        vertices[3].color[0] = color.x();
        vertices[3].color[1] = color.y();
        vertices[3].color[2] = color.z();
        vertices[3].color[3] = color.w();

        // 写入逐实体 SDF 参数到所有 4 个顶点
        for (auto& vtx : vertices)
        {
            vtx.rect_size[0]     = m_pendingQuadParams.rect_size[0];
            vtx.rect_size[1]     = m_pendingQuadParams.rect_size[1];
            vtx.radius[0]        = m_pendingQuadParams.radius[0];
            vtx.radius[1]        = m_pendingQuadParams.radius[1];
            vtx.radius[2]        = m_pendingQuadParams.radius[2];
            vtx.radius[3]        = m_pendingQuadParams.radius[3];
            vtx.shadow_params[0] = m_pendingQuadParams.shadow_soft;
            vtx.shadow_params[1] = m_pendingQuadParams.shadow_offset_x;
            vtx.shadow_params[2] = m_pendingQuadParams.shadow_offset_y;
            vtx.shadow_params[3] = m_pendingQuadParams.opacity;
            vtx.mode_params[0]   = m_pendingQuadParams.padding_flag;
            vtx.mode_params[1]   = m_pendingQuadParams.stroke_width;
            vtx.mode_params[2]   = m_pendingQuadParams.draw_mode;
            vtx.mode_params[3]   = 0.0F;
        }

        for (const auto& val : vertices)
        {
            m_currentBatch->vertices.push_back(val);
        }

        // 添加6个索引（2个三角形）
        std::array<uint16_t, 6> indices = {baseIndex,
                                           static_cast<uint16_t>(baseIndex + 1),
                                           static_cast<uint16_t>(baseIndex + 2),
                                           baseIndex,
                                           static_cast<uint16_t>(baseIndex + 2),
                                           static_cast<uint16_t>(baseIndex + 3)};

        for (const auto& idx : indices)
        {
            m_currentBatch->indices.push_back(idx);
        }
    }

    /**
     * @brief 刷新当前批次
     */
    void flushBatch()
    {
        if (m_currentBatch.has_value() && !m_currentBatch->vertices.empty() && !m_currentBatch->indices.empty())
        {
            m_batches->push_back(std::move(m_currentBatch.value()));
        }
        m_currentBatch.reset();
    }

    /**
     * @brief 优化批次（当前简单实现，未来可以添加更多优化）
     */
    void optimize()
    {
        flushBatch(); // 确保当前批次已刷新

        // TODO:
        // 1. 按纹理排序减少纹理切换
        // 2. 合并相邻的相同纹理批次
        // 3. Z-order排序处理透明度
    }

    /**
     * @brief 获取所有批次
     */
    [[nodiscard]] const std::pmr::vector<render::RenderBatch>& getBatches() const { return *m_batches; }

    /**
     * @brief 获取批次数量
     */
    [[nodiscard]] size_t getBatchCount() const { return m_batches->size(); }

    /**
     * @brief 获取总顶点数
     */
    [[nodiscard]] size_t getTotalVertexCount() const
    {
        size_t count = 0;
        for (const auto& batch : *m_batches)
        {
            count += batch.vertices.size();
        }
        return count;
    }

private:
    struct PendingQuadParams
    {
        float rect_size[2]     = {0.0F, 0.0F};
        float radius[4]        = {0.0F, 0.0F, 0.0F, 0.0F};
        float shadow_soft      = 0.0F;
        float shadow_offset_x  = 0.0F;
        float shadow_offset_y  = 0.0F;
        float opacity          = 1.0F;
        float padding_flag     = 0.0F;
        float stroke_width     = 0.0F;
        float draw_mode        = 0.0F;
    };

    std::pmr::monotonic_buffer_resource m_bufferResource;                       // 帧内内存池资源
    std::optional<std::pmr::vector<render::RenderBatch>> m_batches;             // 存储所有渲染批次
    std::optional<render::RenderBatch> m_currentBatch;                          // 当前正在构建的批次
    PendingQuadParams m_pendingQuadParams{};                                     // 当前逐实体 SDF 参数
};

} // namespace ui::managers
