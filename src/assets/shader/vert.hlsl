#define UI_STAGE_VERTEX 1
#include "common.hlsl"

// =========================================================================
// 顶点着色器 (Vertex Shader)
// =========================================================================
PSInput main_vs(VSInput input)
{
    PSInput output;

    // 将像素坐标转为 NDC [-1, 1], 原点设在左上角, Y轴向下
    float2 ndc = float2((input.position.x / screen_size.x) * 2.0f - 1.0f,
                        1.0f - (input.position.y / screen_size.y) * 2.0f);

    output.sv_position  = float4(ndc, 0.0f, 1.0f);
    output.texcoord     = input.texcoord;
    output.color        = input.color;
    output.rect_size    = input.rect_size;
    output.radius       = input.radius;
    output.shadow_params = input.shadow_params;
    output.mode_params  = input.mode_params;
    return output;
}