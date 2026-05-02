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
     * @param pushConstants 推送常量
     */
    void beginBatch(SDL_GPUTexture* texture,
                    const std::optional<SDL_Rect>& scissor,
                    const render::UiPushConstants& pushConstants)
    {
        // 检查是否可以与当前批次合并
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

            // 检查推送常量是否相同
            // 注意：PushConstants 控制着 Shader 中的 SDF 计算参数，必须完全一致才能合并
            if (canMerge)
            {
                const auto& curr = m_currentBatch->pushConstants;
                const auto& next = pushConstants;
                constexpr float EPSILON = 0.001F;

                bool paramsMatch = true;

                // 屏幕尺寸
                paramsMatch &= (std::abs(curr.screen_size[0] - next.screen_size[0]) < EPSILON);
                paramsMatch &= (std::abs(curr.screen_size[1] - next.screen_size[1]) < EPSILON);

                // 矩形尺寸 (关键：不一样会导致 SDF 计算错误)
                paramsMatch &= (std::abs(curr.rect_size[0] - next.rect_size[0]) < EPSILON);
                paramsMatch &= (std::abs(curr.rect_size[1] - next.rect_size[1]) < EPSILON);

                // 圆角半径
                paramsMatch &= (std::abs(curr.radius[0] - next.radius[0]) < EPSILON);
                paramsMatch &= (std::abs(curr.radius[1] - next.radius[1]) < EPSILON);
                paramsMatch &= (std::abs(curr.radius[2] - next.radius[2]) < EPSILON);
                paramsMatch &= (std::abs(curr.radius[3] - next.radius[3]) < EPSILON);

                // 阴影参数
                paramsMatch &= (std::abs(curr.shadow_offset_x - next.shadow_offset_x) < EPSILON);
                paramsMatch &= (std::abs(curr.shadow_offset_y - next.shadow_offset_y) < EPSILON);
                paramsMatch &= (std::abs(curr.shadow_soft - next.shadow_soft) < EPSILON);

                // 透明度
                paramsMatch &= (std::abs(curr.opacity - next.opacity) < EPSILON);

                // 纹理预乘标志、描边模式与描边宽度
                paramsMatch &= (std::abs(curr.padding - next.padding) < EPSILON);
                paramsMatch &= (std::abs(curr.stroke_width - next.stroke_width) < EPSILON);
                paramsMatch &= (std::abs(curr.draw_mode - next.draw_mode) < EPSILON);

                canMerge = paramsMatch;
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
            m_currentBatch->pushConstants = pushConstants;
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
    std::pmr::monotonic_buffer_resource m_bufferResource;                       // 帧内内存池资源
    std::optional<std::pmr::vector<render::RenderBatch>> m_batches;             // 存储所有渲染批次
    std::optional<render::RenderBatch> m_currentBatch;                          // 当前正在构建的批次
};

} // namespace ui::managers
