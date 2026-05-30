/**
 * @file PlatformWindow.hpp
 * @brief 跨平台窗口定制层（Windows / Linux）
 *
 * SDL3 的 SDL_WINDOW_BORDERLESS 会彻底切断系统对窗口装饰的管理，
 * 导致丢失原生的边缘缩放、窗口吸附（Aero Snap）、系统阴影等特性。
 *
 * 本模块通过平台原生 API 进行底层"手术"：
 * - Windows: 保留 WS_THICKFRAME 等窗口样式，通过 WM_NCCALCSIZE 移除标题栏，
 *            DWM 实现透明合成，WM_NCHITTEST 提供边缘缩放命中测试。
 * - Linux/X11: 通过 _MOTIF_WM_HINTS 移除窗口装饰，保留窗口管理器功能。
 * - Linux/Wayland: 直接使用 SDL_WINDOW_BORDERLESS（Wayland 协议不暴露底层句柄）。
 *
 * @note 所有函数在不支持的平台上退化为空操作。
 */
#pragma once

#include <SDL3/SDL_video.h>
#include <SDL3/SDL_properties.h>

namespace ui::platform
{

/**
 * @brief 配置窗口为自绘标题栏模式
 *
 * 移除系统标题栏，但保留：
 * - 边缘缩放（可拖拽调整大小）
 * - 窗口吸附（Windows Aero Snap / Linux WM 管理）
 * - DWM 阴影（Windows）
 * - 可见的窗口轮廓
 *
 * @param sdlWindow SDL窗口指针
 * @param borderWidth 边缘缩放命中区域宽度（像素），仅 Windows 有效
 */
void SetupCustomTitleBar(SDL_Window* sdlWindow, int borderWidth = 6);

/**
 * @brief 启用窗口透明合成
 *
 * 让 GPU 渲染的 alpha 通道控制窗口透明度。
 * 配合清屏色 {0,0,0,0}，可实现圆角外区域透明。
 *
 * - Windows: DwmExtendFrameIntoClientArea 全客户区扩展 + WS_EX_NOREDIRECTIONBITMAP + SetWindowRgn
 * - Linux: 不需要额外操作（由 compositor 处理）
 *
 * @param sdlWindow SDL窗口指针
 * @param cornerRadius 圆角半径（像素），用于 SetWindowRgn 层裁剪；0 则不裁剪
 */
void EnableTransparency(SDL_Window* sdlWindow, int cornerRadius = 8);

/**
 * @brief 查询窗口当前的 framebuffer 缩放比例
 *
 * 返回窗口坐标到物理 backbuffer 像素的比例，用于 viewport/scissor/纹理清晰度。
 *
 * @param sdlWindow SDL窗口指针
 * @return 显示缩放比例；无效窗口返回 1.0F
 */
[[nodiscard]] float GetWindowDisplayScale(SDL_Window* sdlWindow) noexcept;

[[nodiscard]] float GetWindowFramebufferScale(SDL_Window* sdlWindow) noexcept;

[[nodiscard]] float GetWindowUiScale(SDL_Window* sdlWindow) noexcept;

[[nodiscard]] float GetPrimaryDisplayUiScale() noexcept;

/**
 * @brief 一次性完成自绘窗口所需的全部平台设置
 *
 * 等价于依次调用 SetupCustomTitleBar() + EnableTransparency()
 *
 * @param sdlWindow SDL窗口指针
 * @param borderWidth 边缘缩放命中区域宽度（像素）
 * @param cornerRadius 圆角半径（像素），传入 EnableTransparency 用于 SetWindowRgn
 */
inline void InitCustomWindow(SDL_Window* sdlWindow, int borderWidth = 6, int cornerRadius = 8)
{
    // 先设置标题栏以确保正确的窗口样式，再启用透明合成
    SetupCustomTitleBar(sdlWindow, borderWidth);
    // Windows 需要 DWM 扩展帧 + WS_EX_NOREDIRECTIONBITMAP + SetWindowRgn，以支持 GPU alpha，并充分圆角全透明
    EnableTransparency(sdlWindow, cornerRadius);
}

} // namespace ui::platform
