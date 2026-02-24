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
    float4 color = tex * input.color;

    // 文本纹理路径：直接使用预乘 Alpha 颜色，避免再叠加矩形 SDF 边缘遮罩
    if (_padding > 0.5)
    {
        float out_alpha = color.a * opacity;
        float3 out_rgb = color.rgb * opacity;

        if (out_alpha < 0.001)
            discard;

        return float4(out_rgb, out_alpha);
    }

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

    // _padding > 0.5 表示纹理已经是预乘 Alpha（如文本位图）
    float3 body_rgb = (_padding > 0.5)
        ? color.rgb * body_mask   // 预乘纹理：避免二次预乘
        : color.rgb * body_alpha; // 直通纹理：手动预乘

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
