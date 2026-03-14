/**
 * @file PlatformWindow.cpp
 * @brief 跨平台窗口定制实现（Windows / Linux X11 / 通用回退）
 *
 * Windows 实现要点：
 *   1. 修改窗口样式：移除 WS_CAPTION 但保留 WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX
 *      → 保留边缘缩放、Aero Snap、任务栏最小化等原生行为
 *   2. 子类化窗口过程（SetWindowSubclass）：
 *      - WM_NCCALCSIZE: 返回 0 让客户区填满整个窗口（移除标题栏区域）
 *      - WM_NCHITTEST: 在窗口边缘返回 HTLEFT/HTRIGHT/... 使缩放手柄生效
 *   3. DwmExtendFrameIntoClientArea: 允许 GPU alpha 通道控制透明度
 *
 * Linux/X11 实现要点：
 *   1. 通过 _MOTIF_WM_HINTS 原子属性移除窗口装饰
 *   2. X11 窗口管理器仍保留移动/缩放/图层管理
 *
 * Wayland / 其他平台：
 *   空操作——依赖 SDL_WINDOW_BORDERLESS 本身的行为
 */

#include "PlatformWindow.hpp"

// ============================================================================
// Windows 实现
// ============================================================================
#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <windowsx.h> // GET_X_LPARAM, GET_Y_LPARAM
#include <dwmapi.h>
#include <commctrl.h> // SetWindowSubclass

#pragma comment(lib, "dwmapi")
#pragma comment(lib, "comctl32")

namespace
{

/// 缩放边框命中区域宽度（像素），由 SetupCustomTitleBar 设置
constexpr UINT_PTR SUBCLASS_ID = 1;

struct SubclassData
{
    int borderWidth = 6;
};

/**
 * @brief 窗口子类化过程
 *
 * 处理 WM_NCCALCSIZE 和 WM_NCHITTEST 以实现无标题栏 + 原生缩放行为。
 */
LRESULT CALLBACK CustomFrameProc(
    HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, [[maybe_unused]] UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
    auto* data = reinterpret_cast<SubclassData*>(dwRefData); // NOLINT

    switch (uMsg)
    {
        case WM_NCCALCSIZE:
        {
            // wParam == TRUE 时, lParam 指向 NCCALCSIZE_PARAMS
            // 返回 0 让客户区占据整个窗口区域（移除标题栏 + 边框的非客户区）
            if (wParam == TRUE)
            {
                auto* params = reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam); // NOLINT

                // 若窗口最大化，需要扣除屏幕边缘的不可见边框，
                // 否则最大化时窗口会溢出到相邻显示器
                WINDOWPLACEMENT placement{};
                placement.length = sizeof(WINDOWPLACEMENT);
                GetWindowPlacement(hWnd, &placement);

                if (placement.showCmd == SW_MAXIMIZE)
                {
                    HMONITOR monitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
                    MONITORINFO monitorInfo{};
                    monitorInfo.cbSize = sizeof(MONITORINFO);
                    GetMonitorInfoW(monitor, &monitorInfo);

                    params->rgrc[0] = monitorInfo.rcWork;
                }
                // 非最大化时不做任何修改——客户区 = 整个窗口
                return 0;
            }
            break;
        }

        case WM_NCHITTEST:
        {
            // 先让默认处理判断
            LRESULT result = DefWindowProcW(hWnd, uMsg, wParam, lParam);

            // 若默认判断已识别为客户区以外（如系统菜单），直接返回
            if (result != HTCLIENT)
            {
                return result;
            }

            // 在客户区边缘模拟缩放手柄
            RECT windowRect{};
            GetWindowRect(hWnd, &windowRect);

            int cursorX = GET_X_LPARAM(lParam);
            int cursorY = GET_Y_LPARAM(lParam);
            int border = (data != nullptr) ? data->borderWidth : 6;

            bool onLeft = (cursorX >= windowRect.left && cursorX < windowRect.left + border);
            bool onRight = (cursorX < windowRect.right && cursorX >= windowRect.right - border);
            bool onTop = (cursorY >= windowRect.top && cursorY < windowRect.top + border);
            bool onBottom = (cursorY < windowRect.bottom && cursorY >= windowRect.bottom - border);

            if (onTop && onLeft) return HTTOPLEFT;
            if (onTop && onRight) return HTTOPRIGHT;
            if (onBottom && onLeft) return HTBOTTOMLEFT;
            if (onBottom && onRight) return HTBOTTOMRIGHT;
            if (onLeft) return HTLEFT;
            if (onRight) return HTRIGHT;
            if (onTop) return HTTOP;
            if (onBottom) return HTBOTTOM;

            return HTCLIENT;
        }

        case WM_DESTROY:
        {
            // 清理子类化数据
            RemoveWindowSubclass(hWnd, CustomFrameProc, SUBCLASS_ID);
            delete data; // NOLINT
            return 0;
        }

        default:
            break;
    }

    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

/**
 * @brief 获取 SDL 窗口对应的 Win32 HWND
 */
HWND GetHwndFromSDL(SDL_Window* sdlWindow)
{
    return static_cast<HWND>(
        SDL_GetPointerProperty(SDL_GetWindowProperties(sdlWindow), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
}

} // anonymous namespace

namespace ui::platform
{

void SetupCustomTitleBar(SDL_Window* sdlWindow, int borderWidth)
{
    HWND hwnd = GetHwndFromSDL(sdlWindow);
    if (hwnd == nullptr) return;

    // 1. 修改窗口样式：移除标题栏但保留厚边框（缩放手柄）和 Min/Max 按钮功能
    LONG style = GetWindowLongW(hwnd, GWL_STYLE);
    style &= ~(WS_CAPTION | WS_SYSMENU);                        // 移除标题栏 + 系统菜单
    style |= (WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX); // 保留边框缩放 + 最小化/最大化
    SetWindowLongW(hwnd, GWL_STYLE, style);

    // 2. 子类化窗口过程以处理 WM_NCCALCSIZE / WM_NCHITTEST
    auto* subclassData = new SubclassData{borderWidth};                                               // NOLINT
    SetWindowSubclass(hwnd, CustomFrameProc, SUBCLASS_ID, reinterpret_cast<DWORD_PTR>(subclassData)); // NOLINT

    // 3. 通知系统帧已更改，触发重绘
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void EnableTransparency(SDL_Window* sdlWindow)
{
    HWND hwnd = GetHwndFromSDL(sdlWindow);
    if (hwnd == nullptr) return;

    // DWM 帧扩展到整个客户区 → GPU alpha = 窗口透明度
    const MARGINS margins = {-1, -1, -1, -1};
    DwmExtendFrameIntoClientArea(hwnd, &margins);
}

} // namespace ui::platform

// ============================================================================
// Linux / X11 实现
// ============================================================================
#elif defined(__linux__)

#include <X11/Xlib.h>
#include <X11/Xatom.h>

namespace
{

/**
 * @brief Motif WM Hints 结构体
 *
 * 通过 _MOTIF_WM_HINTS 原子属性通知窗口管理器移除装饰。
 * 大多数 Linux WM（GNOME/KDE/i3/Sway-XWayland）都支持此属性。
 */
struct MotifWmHints
{
    unsigned long flags;
    unsigned long functions;
    unsigned long decorations;
    long inputMode;
    unsigned long status;
};

constexpr unsigned long MWM_HINTS_DECORATIONS = (1L << 1);

} // anonymous namespace

namespace ui::platform
{

void SetupCustomTitleBar(SDL_Window* sdlWindow, [[maybe_unused]] int borderWidth)
{
    // 尝试 X11 路径
    auto* display = static_cast<Display*>(
        SDL_GetPointerProperty(SDL_GetWindowProperties(sdlWindow), SDL_PROP_WINDOW_X11_DISPLAY_POINTER, nullptr));
    auto window = static_cast<::Window>(
        SDL_GetNumberProperty(SDL_GetWindowProperties(sdlWindow), SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0));

    if (display != nullptr && window != 0)
    {
        // 通过 _MOTIF_WM_HINTS 移除窗口装饰
        Atom motifHintsAtom = XInternAtom(display, "_MOTIF_WM_HINTS", False);
        MotifWmHints hints{};
        hints.flags = MWM_HINTS_DECORATIONS;
        hints.decorations = 0; // 移除所有装饰

        XChangeProperty(display,
                        window,
                        motifHintsAtom,
                        motifHintsAtom,
                        32,
                        PropModeReplace,
                        reinterpret_cast<unsigned char*>(&hints), // NOLINT
                        5);
        XFlush(display);
        return;
    }

    // Wayland: 无法通过底层 API 移除装饰，依赖 SDL_WINDOW_BORDERLESS
    // （SDL3 创建窗口时已设置该标志）
}

void EnableTransparency([[maybe_unused]] SDL_Window* sdlWindow)
{
    // Linux 下的 compositing 由 Compositor 自动处理 alpha 通道
    // 若使用 X11 + 支持 ARGB visual 的合成器（picom/compton），透明自动生效
    // Wayland compositor 原生支持 alpha 合成
}

} // namespace ui::platform

// ============================================================================
// 其他平台（macOS / 移动端等）— 空操作
// ============================================================================
#else

namespace ui::platform
{

void SetupCustomTitleBar([[maybe_unused]] SDL_Window* sdlWindow, [[maybe_unused]] int borderWidth)
{
    // 空操作：未实现的平台依赖 SDL_WINDOW_BORDERLESS
}

void EnableTransparency([[maybe_unused]] SDL_Window* sdlWindow)
{
    // 空操作
}

} // namespace ui::platform

#endif
