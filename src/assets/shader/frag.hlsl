#define UI_STAGE_PIXEL 1
#include "common.hlsl"

// =========================================================================
// Pixel Shader (Premultiplied Alpha, Correct Shadow Compositing)
// =========================================================================

// 圆角矩形 SDF
// radius 布局: (x:左上, y:右上, z:右下, w:左下)
// 屏幕坐标系 Y 轴朝下: p.y<0 = 上半部分, p.y>0 = 下半部分
float sdRoundedBox(float2 p, float2 b, float4 r)
{
    // p.x>0 → 右侧 → (右上, 右下); p.x<0 → 左侧 → (左上, 左下)
    float2 q_radius = (p.x > 0.0) ? r.yz : r.xw;
    // p.y>0 → 下半 → 取 .y (右下/左下); p.y<0 → 上半 → 取 .x (右上/左上)
    float actual_r = (p.y > 0.0) ? q_radius.y : q_radius.x;

    float2 q = abs(p) - b + actual_r;
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - actual_r;
}

float4 main_ps(PSInput input) : SV_Target
{
    float4 tex = u_texture.Sample(u_sampler, input.texcoord);

    // 从顶点属性读取逐实体参数
    float2 rect_size     = input.rect_size;
    float4 radius        = input.radius;
    float  shadow_soft   = input.shadow_params.x;
    float  shadow_offset_x = input.shadow_params.y;
    float  shadow_offset_y = input.shadow_params.z;
    float  opacity       = input.shadow_params.w;
    float  _padding      = input.mode_params.x;
    float  stroke_width  = input.mode_params.y;
    float  draw_mode     = input.mode_params.z;

    // 文本 alpha mask 路径：R8 单通道纹理，采样 .r 作为 coverage，由 shader 统一着色
    if (_padding > 1.5)
    {
        float mask_alpha = tex.r;
        float out_alpha = mask_alpha * input.color.a * opacity;
        float3 out_rgb = input.color.rgb * out_alpha;

        if (out_alpha < 0.001)
            discard;

        return float4(out_rgb, out_alpha);
    }

    // 预乘纹理路径：例如图标纹理
    if (_padding > 0.5)
    {
        float out_alpha = tex.a * input.color.a * opacity;
        float3 out_rgb = tex.rgb * input.color.rgb * input.color.a * opacity;

        if (out_alpha < 0.001)
            discard;

        return float4(out_rgb, out_alpha);
    }

    float4 color = tex * input.color;

    // ------------------------------------------------------------
    // 1. 像素坐标（以矩形中心为原点）
    // ------------------------------------------------------------
    float2 p = (input.texcoord - 0.5) * rect_size;
    float2 half_size = rect_size * 0.5;

    // ------------------------------------------------------------
    // 2. 主体 SDF
    // ------------------------------------------------------------
    float dist = sdRoundedBox(p, half_size, radius);

    float edge = fwidth(dist);
    edge = max(edge, 0.0001); // 避免除零或无穷小
    float outer_mask = 1.0 - smoothstep(-edge, edge, dist);

    // 胶囊线段路径（draw_mode == 2）：rect_size.x = 2*half_length + 2*radius, rect_size.y = 2*radius
    // texcoord 为归一化 UV（[0,1]），通过 rect_size 还原局部坐标
    if (draw_mode > 1.5)
    {
        // 胶囊 SDF：p 已在矩形中心为原点的局部坐标中
        // half_length = half_size.x - half_size.y（去掉两端半圆后的半长）
        float half_r = half_size.y;
        float half_l = half_size.x - half_r;
        half_l = max(half_l, 0.0);

        // capsule SDF = length(max(|p| - (half_l, 0), 0)) - half_r
        float2 clamped = max(abs(p) - float2(half_l, 0.0), 0.0);
        float dist_cap = length(clamped) - half_r;

        float edge_cap = fwidth(dist_cap);
        edge_cap = max(edge_cap, 0.0001);
        float mask_cap = 1.0 - smoothstep(-edge_cap, edge_cap, dist_cap);

        float out_alpha = color.a * mask_cap * opacity;
        float3 out_rgb = color.rgb * color.a * mask_cap * opacity;

        if (out_alpha < 0.001)
            discard;

        return float4(out_rgb, out_alpha);
    }

    // 描边路径：绘制圆角环形区域（内缩描边）
    if (draw_mode > 0.5)
    {
        float stroke = max(stroke_width, 0.0);
        float inner_mask = 1.0 - smoothstep(-edge, edge, dist + stroke);
        float border_mask = saturate(outer_mask - inner_mask);

        float out_alpha = color.a * border_mask * opacity;
        float3 out_rgb = color.rgb * color.a * border_mask * opacity;

        if (out_alpha < 0.001)
            discard;

        return float4(out_rgb, out_alpha);
    }

    float body_mask = outer_mask;

    // ------------------------------------------------------------
    // 3. 阴影计算 (预乘 Alpha 逻辑)
    // ------------------------------------------------------------
    float shadow_alpha = 0.0;

    if (shadow_soft > 0.0)
    {
        float2 shadow_p = p - float2(shadow_offset_x, shadow_offset_y);
        float dist_shadow = sdRoundedBox(shadow_p, half_size, radius);

        // 阴影从边缘(0)开始向外平滑衰减到shadow_soft
        // 内部保持 1.0，以便后续正确进行 Over 合成
        shadow_alpha = 1.0 - smoothstep(0.0, shadow_soft, dist_shadow);

        // 强度控制（UI 阴影不宜过重）
        shadow_alpha *= 0.5;
    }

    // ------------------------------------------------------------
    // 4. 主体颜色（预乘 Alpha）
    // ------------------------------------------------------------
    // 主体 alpha
    float body_alpha = color.a * body_mask;

    // 此分支仅在 _padding <= 0.5 时到达（普通纹理），直接手动预乘
    float3 body_rgb = color.rgb * body_alpha;

    // ------------------------------------------------------------
    // 5. 阴影颜色（预乘 Alpha，纯黑）
    // ------------------------------------------------------------
    // 阴影层位于主体下方，采用标准的 Over 混合公式进行合成
    // C_out = C_body + C_shadow * (1.0 - A_body)
    float shadow_a = shadow_alpha * (1.0 - body_alpha);
    float3 shadow_rgb = float3(0.0, 0.0, 0.0); // 黑色已预乘

    // ------------------------------------------------------------
    // 6. 合成（预乘 Alpha 空间）
    // ------------------------------------------------------------
    float out_alpha = body_alpha + shadow_a;
    float3 out_rgb = body_rgb + shadow_rgb; // shadow_rgb 是 0，所以这里其实是 body_rgb

    // 全局透明度（UI 树 Alpha）
    out_alpha *= opacity;
    out_rgb *= opacity;

    // 剔除无效像素
    if (out_alpha < 0.001)
        discard;

    return float4(out_rgb, out_alpha);
}
