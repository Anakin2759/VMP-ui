// =========================================================================
// SDL3 GPU 着色器公共定义
// 支持：圆角矩形 UI、外发光阴影、抗锯齿裁切、整体透明度
// 兼容 Vulkan 和 DX12 后端
// =========================================================================

// --- 0. Uniform Buffer (必须与 C++ 结构体 16 字节对齐) ---
// D3D12 后端的 Root Signature 使用不同的 register space：
// VS: space1, PS: space3。Vulkan 下也保持一致即可。
#if defined(UI_STAGE_VERTEX)
cbuffer UiConstants : register(b0, space1)
#elif defined(UI_STAGE_PIXEL)
cbuffer UiConstants : register(b0, space3)
#else
cbuffer UiConstants : register(b0)
#endif
{
    float2 screen_size;    // 屏幕尺寸 (用于坐标转换)
    float2 rect_size;      // 矩形实际像素尺寸 (用于 SDF 计算)
    float4 radius;         // 四个角的半径 (x:左上, y:右上, z:右下, w:左下)
    float shadow_soft;     // 阴影柔和度 (像素单位，0则无阴影)
    float shadow_offset_x; // 阴影偏移 X
    float shadow_offset_y; // 阴影偏移 Y
    float opacity;         // 整体透明度 (0.0 - 1.0)
    float _padding;        // 标志位：>0.5 表示纹理为预乘 Alpha（用于文本/图标）
    float stroke_width;    // 描边宽度（像素）
    float draw_mode;       // 绘制模式：0=填充，1=描边
    float _reserved0;      // 保留字段（16字节对齐）
};

// --- 1. 输入输出结构 ---
struct VSInput
{
    // D3D12 后端使用统一的语义名（默认 TEXCOORD），此处用 TEXCOORD0/1/2 对齐
    float2 position : TEXCOORD0;
    float2 texcoord : TEXCOORD1;
    float4 color : TEXCOORD2;
};

struct PSInput
{
    float4 sv_position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float4 color : TEXCOORD1;
};

// --- 2. 纹理定义 ---
// SDL GPU 绑定从 slot 0 开始，D3D12 片段阶段使用 space2
#if defined(UI_STAGE_PIXEL)
Texture2D u_texture : register(t0, space2);
SamplerState u_sampler : register(s0, space2);
#else
Texture2D u_texture : register(t0);
SamplerState u_sampler : register(s0);
#endif