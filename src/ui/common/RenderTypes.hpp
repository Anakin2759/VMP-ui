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
    float rect_size[2];    // 矩形尺寸 (float2)
    float radius[4];       // 四角圆角 (float4: 左上, 右上, 右下, 左下)
    float shadow_soft;     // 阴影柔和度
    float shadow_offset_x; // 阴影 X 偏移
    float shadow_offset_y; // 阴影 Y 偏移
    float opacity;         // 整体透明度
    float padding;         // 标志位：>0.5 表示纹理为预乘 Alpha（用于文本/图标）
    float stroke_width;    // 描边宽度（像素）
    float draw_mode;       // 绘制模式：0=填充，1=描边
    float reserved0;       // 保留字段（16字节对齐）
};
/**
 * @brief 顶点结构
 */
struct Vertex
{
    float position[2]; // POSITION
    float texCoord[2]; // TEXCOORD0
    float color[4];    // COLOR0
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
