/**
 * ************************************************************************
 *
 * @file Types.h
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2026-01-13
 * @version 0.1
 * @brief UI模块核心类型定义
 *
 * 使用Eigen向量类型替代ImVec，提供统一的数学类型支持。
 * 包含颜色、向量、矩阵等基础类型定义和转换工具。
 *
 * ************************************************************************
 * @copyright Copyright (c) 2026 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */

#pragma once

#include <Eigen/Core>
#include <Eigen/Geometry>
#include <cstdint>
#include <algorithm>

namespace ui
{

// ===================== 基础向量类型 =====================

/**
 * @brief 2D向量类型（替代ImVec2）
 */
using Vec2 = Eigen::Vector2f;

/**
 * @brief 3D向量类型
 */
using Vec3 = Eigen::Vector3f;

/**
 * @brief 4D向量类型（替代ImVec4，用于边距/内边距等）
 */
using Vec4 = Eigen::Vector4f;

/**
 * @brief 2x2矩阵类型
 */
using Mat2 = Eigen::Matrix2f;

/**
 * @brief 3x3矩阵类型（用于2D变换）
 */
using Mat3 = Eigen::Matrix3f;

/**
 * @brief 4x4矩阵类型（用于3D变换）
 */
using Mat4 = Eigen::Matrix4f;

/**
 * @brief 仿射变换类型（2D）
 */
using Transform2D = Eigen::Affine2f;

/**
 * @brief 仿射变换类型（3D）
 */
using Transform3D = Eigen::Affine3f;

// ===================== 颜色类型 =====================

/**
 * @brief RGBA颜色结构体（浮点数表示，范围0.0-1.0）
 */
struct Color
{
    float red = 1.0F;
    float green = 1.0F;
    float blue = 1.0F;
    float alpha = 1.0F;

    constexpr Color() = default;
    constexpr Color(float red, float green, float blue, float alpha = 1.0F)
        : red(red), green(green), blue(blue), alpha(alpha)
    {
    }

    /**
     * @brief 从Vec4构造颜色
     */
    explicit Color(const Vec4& vec) : red(vec.x()), green(vec.y()), blue(vec.z()), alpha(vec.w()) {}

    /**
     * @brief 转换为Vec4
     */
    [[nodiscard]] Vec4 toVec4() const { return {red, green, blue, alpha}; }

    /**
     * @brief 转换为SDL颜色格式（RGBA8888）
     */
    [[nodiscard]] uint32_t toSDLColor() const
    {
        auto clamp = [](float vertical) -> uint8_t
        { return static_cast<uint8_t>(std::clamp(vertical, 0.0F, 1.0F) * 255.0F); };
        return (static_cast<uint32_t>(clamp(red)) << 24U) | (static_cast<uint32_t>(clamp(green)) << 16U) |
               (static_cast<uint32_t>(clamp(blue)) << 8U) | static_cast<uint32_t>(clamp(alpha));
    }

    /**
     * @brief 从SDL颜色格式创建
     */
    static Color fromSDLColor(uint32_t sdlColor)
    {
        return {static_cast<float>((sdlColor >> 24) & 0xFF) / 255.0F,
                static_cast<float>((sdlColor >> 16) & 0xFF) / 255.0F,
                static_cast<float>((sdlColor >> 8) & 0xFF) / 255.0F,
                static_cast<float>(sdlColor & 0xFF) / 255.0F};
    }

    /**
     * @brief 从32位RGBA值创建（0-255范围）
     */
    static Color fromRGBA(uint8_t r_, uint8_t g_, uint8_t b_, uint8_t a_ = 255)
    {
        return Color(static_cast<float>(r_) / 255.0f,
                     static_cast<float>(g_) / 255.0f,
                     static_cast<float>(b_) / 255.0f,
                     static_cast<float>(a_) / 255.0f);
    }

    /**
     * @brief 带透明度调整的颜色
     */
    [[nodiscard]] Color withAlpha(float newAlpha) const { return Color(red, green, blue, newAlpha); }

    /**
     * @brief 乘以透明度因子
     */
    [[nodiscard]] Color multiplyAlpha(float factor) const { return Color(red, green, blue, alpha * factor); }

    // 预定义颜色
    static constexpr Color White() { return {1.0F, 1.0F, 1.0F, 1.0F}; }
    static constexpr Color Black() { return {0.0F, 0.0F, 0.0F, 1.0F}; }
    static constexpr Color Red() { return {1.0F, 0.0F, 0.0F, 1.0F}; }
    static constexpr Color Green() { return {0.0F, 1.0F, 0.0F, 1.0F}; }
    static constexpr Color Blue() { return {0.0F, 0.0F, 1.0F, 1.0F}; }
    static constexpr Color Yellow() { return {1.0F, 1.0F, 0.0F, 1.0F}; }
    static constexpr Color Cyan() { return {0.0F, 1.0F, 1.0F, 1.0F}; }
    static constexpr Color Magenta() { return {1.0F, 0.0F, 1.0F, 1.0F}; }
    static constexpr Color Transparent() { return {0.0F, 0.0F, 0.0F, 0.0F}; }
    static constexpr Color Gray() { return {0.5F, 0.5F, 0.5F, 1.0F}; }
};

// ===================== 矩形类型 =====================

/**
 * @brief 轴对齐矩形（AABB）
 */
struct Rect
{
    Vec2 position{0.0f, 0.0f}; // 左上角位置
    Vec2 size{0.0f, 0.0f};     // 尺寸

    Rect() = default;
    Rect(float x, float y, float w, float h) : position(x, y), size(w, h) {}
    Rect(const Vec2& pos, const Vec2& sz) : position(pos), size(sz) {}

    [[nodiscard]] float x() const { return position.x(); }
    [[nodiscard]] float y() const { return position.y(); }
    [[nodiscard]] float width() const { return size.x(); }
    [[nodiscard]] float height() const { return size.y(); }

    [[nodiscard]] float left() const { return position.x(); }
    [[nodiscard]] float top() const { return position.y(); }
    [[nodiscard]] float right() const { return position.x() + size.x(); }
    [[nodiscard]] float bottom() const { return position.y() + size.y(); }

    [[nodiscard]] Vec2 topLeft() const { return position; }
    [[nodiscard]] Vec2 topRight() const { return Vec2(right(), top()); }
    [[nodiscard]] Vec2 bottomLeft() const { return Vec2(left(), bottom()); }
    [[nodiscard]] Vec2 bottomRight() const { return position + size; }
    [[nodiscard]] Vec2 center() const { return position + size * 0.5f; }

    /**
     * @brief 点是否在矩形内
     */
    [[nodiscard]] bool contains(const Vec2& point) const
    {
        return point.x() >= left() && point.x() <= right() && point.y() >= top() && point.y() <= bottom();
    }

    /**
     * @brief 矩形是否相交
     */
    [[nodiscard]] bool intersects(const Rect& other) const
    {
        return !(other.left() > right() || other.right() < left() || other.top() > bottom() || other.bottom() < top());
    }

    /**
     * @brief 扩展矩形
     */
    [[nodiscard]] Rect expanded(float amount) const
    {
        return Rect(position.x() - amount, position.y() - amount, size.x() + amount * 2, size.y() + amount * 2);
    }

    /**
     * @brief 按边距缩小矩形
     */
    [[nodiscard]] Rect shrunk(const Vec4& margins) const
    {
        // margins: (top, right, bottom, left)
        return Rect(position.x() + margins.w(),
                    position.y() + margins.x(),
                    size.x() - margins.y() - margins.w(),
                    size.y() - margins.x() - margins.z());
    }
};

// ===================== 边距/内边距类型 =====================

/**
 * @brief 边距结构体（Top, Right, Bottom, Left顺序）
 */
struct EdgeInsets
{
    float top = 0.0f;
    float right = 0.0f;
    float bottom = 0.0f;
    float left = 0.0f;

    constexpr EdgeInsets() = default;
    constexpr EdgeInsets(float all) : top(all), right(all), bottom(all), left(all) {}
    constexpr EdgeInsets(float vertical, float horizontal)
        : top(vertical), right(horizontal), bottom(vertical), left(horizontal)
    {
    }
    constexpr EdgeInsets(float t, float r, float b, float l) : top(t), right(r), bottom(b), left(l) {}

    /**
     * @brief 从Vec4构造（Top, Right, Bottom, Left）
     */
    explicit EdgeInsets(const Vec4& vec) : top(vec.x()), right(vec.y()), bottom(vec.z()), left(vec.w()) {}

    /**
     * @brief 转换为Vec4
     */
    [[nodiscard]] Vec4 toVec4() const { return Vec4(top, right, bottom, left); }

    [[nodiscard]] float horizontal() const { return left + right; }
    [[nodiscard]] float vertical() const { return top + bottom; }
};

// ===================== 工具函数 =====================

/**
 * @brief 创建Vec2
 */
inline Vec2 MakeVec2(float x, float y)
{
    return {x, y};
}

/**
 * @brief 创建Vec4
 */
inline Vec4 makeVec4(float x, float y, float z, float w)
{
    return Vec4(x, y, z, w);
}

/**
 * @brief 线性插值
 */
inline Vec2 Lerp(const Vec2& a, const Vec2& b, float t)
{
    return a + (b - a) * t;
}

inline float Lerp(float a, float b, float t)
{
    return a + (b - a) * t;
}

inline Color Lerp(const Color& a, const Color& b, float t)
{
    return {Lerp(a.red, b.red, t), Lerp(a.green, b.green, t), Lerp(a.blue, b.blue, t), Lerp(a.alpha, b.alpha, t)};
}

/**
 * @brief 2D旋转矩阵
 */
inline Mat2 Rotation2D(float angleRadians)
{
    float c = std::cos(angleRadians);
    float s = std::sin(angleRadians);
    Mat2 mat;
    mat << c, -s, s, c;
    return mat;
}

/**
 * @brief 2D缩放矩阵
 */
inline Mat2 Scale2D(float sx, float sy)
{
    Mat2 mat;
    mat << sx, 0, 0, sy;
    return mat;
}

/**
 * @brief 创建2D仿射变换
 */
inline Transform2D MakeTransform2D(const Vec2& translation, float rotation = 0.0f, const Vec2& scale = Vec2(1, 1))
{
    Transform2D transform = Transform2D::Identity();
    transform.translate(translation);
    transform.rotate(rotation);
    transform.scale(scale);
    return transform;
}

} // namespace ui
