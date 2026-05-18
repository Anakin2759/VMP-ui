#pragma once
#include <cstdint>
#include <vector>
#include <optional>
#include <memory_resource>
#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_rect.h>

namespace ui::render
{
/**
 * @brief UI 着色器推送常量结构
 */
struct alignas(16) UiPushConstants
{
    float screen_size[2];  // 屏幕尺寸 (float2)
    float rect_size[2];    // 矩形尺寸 (float2)（兼容字段，实际由顶点属性传递）
    float radius[4];       // 四角圆角 (float4)（兼容字段，实际由顶点属性传递）
    float shadow_soft;     // 阴影柔和度（兼容字段，实际由顶点属性传递）
    float shadow_offset_x; // 阴影 X 偏移（兼容字段，实际由顶点属性传递）
    float shadow_offset_y; // 阴影 Y 偏移（兼容字段，实际由顶点属性传递）
    float opacity;         // 整体透明度（兼容字段，实际由顶点属性传递）
    float padding;         // 纹理标志位（兼容字段，实际由顶点属性传递）
    float stroke_width;    // 描边宽度（兼容字段，实际由顶点属性传递）
    float draw_mode;       // 绘制模式（兼容字段，实际由顶点属性传递）
    float reserved0;       // 保留字段（16字节对齐）
};
/**
 * @brief 顶点结构（含逐实体 SDF 参数，迁移自 UiPushConstants）
 */
struct Vertex
{
    float position[2];      // location 0 — 屏幕像素坐标
    float texCoord[2];      // location 1 — UV 坐标
    float color[4];         // location 2 — RGBA 颜色
    float rect_size[2];     // location 3 — 矩形像素尺寸（SDF 用）
    float radius[4];        // location 4 — 四角圆角（左上,右上,右下,左下）
    float shadow_params[4]; // location 5 — [shadow_soft, shadow_offset_x, shadow_offset_y, opacity]
    float mode_params[4];   // location 6 — [padding_flag, stroke_width, draw_mode, _reserved]
};

/**
 * @brief 渲染批次结构
 */
struct RenderBatch
{
    using allocator_type = std::pmr::polymorphic_allocator<std::byte>;

    std::pmr::vector<Vertex> vertices;
    std::pmr::vector<uint16_t> indices;
    UiPushConstants pushConstants{};
    SDL_GPUTexture* texture = nullptr;
    std::optional<SDL_Rect> scissorRect;

    // PMR 兼容构造函数
    RenderBatch(allocator_type alloc = {}) : vertices(alloc), indices(alloc) {}

    RenderBatch(const RenderBatch& other, allocator_type alloc = {})
        : vertices(other.vertices, alloc), indices(other.indices, alloc), pushConstants(other.pushConstants),
          texture(other.texture), scissorRect(other.scissorRect)
    {
    }

    RenderBatch(RenderBatch&& other, allocator_type alloc = {})
        : vertices(std::move(other.vertices), alloc), indices(std::move(other.indices), alloc),
          pushConstants(other.pushConstants), texture(other.texture), scissorRect(other.scissorRect)
    {
    }

    RenderBatch& operator=(const RenderBatch&) = default;
    RenderBatch& operator=(RenderBatch&&) = default;
};

// 纹理缓存条目（用于文本）
struct CachedTexture
{
    SDL_GPUTexture* texture = nullptr;
    uint32_t width = 0;
    uint32_t height = 0;
};

} // namespace ui::render
