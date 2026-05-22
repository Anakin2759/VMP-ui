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
#include "SDL3/SDL_video.h"
#include "SDL3/SDL_properties.h"

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

#include <Windows.h> // IWYU pragma: keep
#include <windowsx.h> // GET_X_LPARAM, GET_Y_LPARAM
#include <basetsd.h>
#include <minwindef.h>
#include <windef.h>
#include <winuser.h>
#include <wingdi.h>
#include <dwmapi.h>
#include <commctrl.h> // SetWindowSubclass
#include <uxtheme.h>

#pragma comment(lib, "dwmapi")
#pragma comment(lib, "comctl32")

namespace
{

/// 缩放边框命中区域宽度（像素），由 SetupCustomTitleBar 设置
constexpr UINT_PTR SUBCLASS_ID = 1;

struct SubclassData
{
    int borderWidth = 6;
    int cornerRadius = 0; // 0 = 不裁剪圆角；> 0 时在 WM_SIZE 中同步更新 SetWindowRgn
};

LRESULT HandleNcCalcSize(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
    if (wParam != TRUE)
    {
        return DefSubclassProc(hWnd, WM_NCCALCSIZE, wParam, lParam);
    }

    auto* params = reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam); // NOLINT

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

    return 0;
}

LRESULT HandleNcHitTest(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, const SubclassData* data)
{
    LRESULT const result = DefWindowProcW(hWnd, uMsg, wParam, lParam);
    if (result != HTCLIENT)
    {
        return result;
    }

    RECT windowRect{};
    GetWindowRect(hWnd, &windowRect);

    int const cursorX = GET_X_LPARAM(lParam);
    int const cursorY = GET_Y_LPARAM(lParam);
    int const border = (data != nullptr) ? data->borderWidth : 6;

    bool const onLeft = (cursorX >= windowRect.left && cursorX < windowRect.left + border);
    bool const onRight = (cursorX < windowRect.right && cursorX >= windowRect.right - border);
    bool const onTop = (cursorY >= windowRect.top && cursorY < windowRect.top + border);
    bool const onBottom = (cursorY < windowRect.bottom && cursorY >= windowRect.bottom - border);

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

void SyncRoundedRegion(HWND hWnd, LPARAM lParam, const SubclassData* data)
{
    if (data == nullptr || data->cornerRadius <= 0)
    {
        return;
    }

    int const winW = static_cast<int>(LOWORD(lParam));
    int const winH = static_cast<int>(HIWORD(lParam));
    if (winW <= 0 || winH <= 0)
    {
        return;
    }

    HRGN hRgn = CreateRoundRectRgn(0, 0, winW + 1, winH + 1, data->cornerRadius * 2, data->cornerRadius * 2);
    SetWindowRgn(hWnd, hRgn, TRUE);
}

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
            return HandleNcCalcSize(hWnd, wParam, lParam);
        }

        case WM_NCHITTEST:
        {
            return HandleNcHitTest(hWnd, uMsg, wParam, lParam, data);
        }

        case WM_SIZE:
        {
            SyncRoundedRegion(hWnd, lParam, data);
            break; // 继续默认处理
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

    // 1. 修改窗口样式：保留 WS_CAPTION + WS_THICKFRAME（Win11 伪无边框方案）
    //    WS_CAPTION 必须保留，否则 DWM 不会应用 Win11 圆角和透明合成，
    //    导致圆角外区域显示黑色。标题栏区域通过 WM_NCCALCSIZE 返回 0 来隐藏。
    auto style = GetWindowLongW(hwnd, GWL_STYLE);
    style &= ~WS_SYSMENU;                                        // 仅移除系统菜单，保留 WS_CAPTION 给 DWM
    style |= (WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX);
    SetWindowLongW(hwnd, GWL_STYLE, style);

    // 2. 子类化窗口过程以处理 WM_NCCALCSIZE / WM_NCHITTEST
    auto* subclassData = new SubclassData{borderWidth};                                               // NOLINT
    SetWindowSubclass(hwnd, CustomFrameProc, SUBCLASS_ID, reinterpret_cast<DWORD_PTR>(subclassData)); // NOLINT

    // 3. 通知系统帧已更改，触发重绘
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void EnableTransparency(SDL_Window* sdlWindow, int cornerRadius)
{
    HWND hwnd = GetHwndFromSDL(sdlWindow);
    if (hwnd == nullptr) return;

    // 1. WS_EX_NOREDIRECTIONBITMAP：告知 DWM 直接从 GPU 交换链读取像素（含 alpha 通道），
    //    而非使用 GDI 重定向位图（后者忽略 alpha）。在支持的驱动/OS 上可使 GPU alpha 生效。
    auto exStyle = GetWindowLongW(hwnd, GWL_EXSTYLE);
    exStyle |= WS_EX_NOREDIRECTIONBITMAP;
    SetWindowLongW(hwnd, GWL_EXSTYLE, exStyle);

    // 2. DWM 帧扩展到整个客户区 → GPU alpha = 窗口透明度
    const MARGINS margins = {.cxLeftWidth = -1, .cxRightWidth = -1, .cyTopHeight = -1, .cyBottomHeight = -1};
    DwmExtendFrameIntoClientArea(hwnd, &margins);

    // 3. SetWindowRgn 兜底：在 OS 层裁剪圆角，确保 DWM alpha 合成不可用时也无黑角。
    //    WM_SIZE 子类化处理器会在窗口缩放时自动同步此区域。
    if (cornerRadius > 0)
    {
        // 将 cornerRadius 写入 SubclassData，供 WM_SIZE 使用
        DWORD_PTR dwRefData = 0;
        if (GetWindowSubclass(hwnd, CustomFrameProc, SUBCLASS_ID, &dwRefData) != FALSE && dwRefData != 0)
        {
            reinterpret_cast<SubclassData*>(dwRefData)->cornerRadius = cornerRadius; // NOLINT
        }

        RECT rect;
        GetWindowRect(hwnd, &rect);
        int const winW = rect.right - rect.left;
        int const winH = rect.bottom - rect.top;
        if (winW > 0 && winH > 0)
        {
            HRGN hRgn = CreateRoundRectRgn(0, 0, winW + 1, winH + 1, cornerRadius * 2, cornerRadius * 2);
            SetWindowRgn(hwnd, hRgn, TRUE);
        }
    }
}

} // namespace ui::platform

// ============================================================================
// Linux / X11 实现
// ============================================================================
#elifdef __linux__

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

void EnableTransparency([[maybe_unused]] SDL_Window* sdlWindow, [[maybe_unused]] int cornerRadius)
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

void EnableTransparency([[maybe_unused]] SDL_Window* sdlWindow, [[maybe_unused]] int cornerRadius)
{
    // 空操作
}

} // namespace ui::platform

#endif
