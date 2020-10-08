// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "IslandWindow.h"
#include "../types/inc/Viewport.hpp"
#include "resource.h"

extern "C" IMAGE_DOS_HEADER __ImageBase;

using namespace winrt::Windows::UI;
using namespace winrt::Windows::UI::Composition;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Hosting;
using namespace winrt::Windows::Foundation::Numerics;
using namespace winrt::Microsoft::Terminal::Settings::Model;
using namespace ::Microsoft::Console::Types;

#define XAML_HOSTING_WINDOW_CLASS_NAME L"CASCADIA_HOSTING_WINDOW_CLASS"

IslandWindow::IslandWindow() noexcept :
    _interopWindowHandle{ nullptr },
    _rootGrid{ nullptr },
    _source{ nullptr },
    _pfnCreateCallback{ nullptr }
{
}

IslandWindow::~IslandWindow()
{
    _source.Close();
}

// Method Description:
// - Create the actual window that we'll use for the application.
// Arguments:
// - <none>
// Return Value:
// - <none>
void IslandWindow::MakeWindow() noexcept
{
    WNDCLASS wc{};
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hInstance = reinterpret_cast<HINSTANCE>(&__ImageBase);
    wc.lpszClassName = XAML_HOSTING_WINDOW_CLASS_NAME;
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hIcon = LoadIconW(wc.hInstance, MAKEINTRESOURCEW(IDI_APPICON));
    RegisterClass(&wc);
    WINRT_ASSERT(!_window);

    // Create the window with the default size here - During the creation of the
    // window, the system will give us a chance to set its size in WM_CREATE.
    // WM_CREATE will be handled synchronously, before CreateWindow returns.
    WINRT_VERIFY(CreateWindowEx(_alwaysOnTop ? WS_EX_TOPMOST : 0,
                                wc.lpszClassName,
                                L"Windows Terminal",
                                WS_OVERLAPPEDWINDOW,
                                CW_USEDEFAULT,
                                CW_USEDEFAULT,
                                CW_USEDEFAULT,
                                CW_USEDEFAULT,
                                nullptr,
                                nullptr,
                                wc.hInstance,
                                this));

    WINRT_ASSERT(_window);
}

// Method Description:
// - Called when no tab is remaining to close the window.
// Arguments:
// - <none>
// Return Value:
// - <none>
void IslandWindow::Close()
{
    PostQuitMessage(0);
}

// Method Description:
// - Set a callback to be called when we process a WM_CREATE message. This gives
//   the AppHost a chance to resize the window to the proper size.
// Arguments:
// - pfn: a function to be called during the handling of WM_CREATE. It takes two
//   parameters:
//      * HWND: the HWND of the window that's being created.
//      * RECT: The position on the screen that the system has proposed for our
//        window.
// Return Value:
// - <none>
void IslandWindow::SetCreateCallback(std::function<void(const HWND, const RECT, LaunchMode& launchMode)> pfn) noexcept
{
    _pfnCreateCallback = pfn;
}

// Method Description:
// - Set a callback to be called when the window is being resized by user. For given
//   requested window dimension (width or height, whichever border is dragged) it should
//   return a resulting window dimension that is actually set. It is used to make the
//   window 'snap' to the underling terminal's character grid.
// Arguments:
// - pfn: a function that transforms requested to actual window dimension.
//   pfn's parameters:
//      * widthOrHeight: whether the dimension is width (true) or height (false)
//      * dimension: The requested dimension that comes from user dragging a border
//        of the window. It is in pixels and represents only the client area.
//   pfn's return value:
//      * A dimension of client area that the window should resize to.
// Return Value:
// - <none>
void IslandWindow::SetSnapDimensionCallback(std::function<float(bool, float)> pfn) noexcept
{
    _pfnSnapDimensionCallback = pfn;
}

// Method Description:
// - Handles a WM_CREATE message. Calls our create callback, if one's been set.
// Arguments:
// - wParam: unused
// - lParam: the lParam of a WM_CREATE, which is a pointer to a CREATESTRUCTW
// Return Value:
// - <none>
void IslandWindow::_HandleCreateWindow(const WPARAM, const LPARAM lParam) noexcept
{
    // Get proposed window rect from create structure
    CREATESTRUCTW* pcs = reinterpret_cast<CREATESTRUCTW*>(lParam);
    RECT rc;
    rc.left = pcs->x;
    rc.top = pcs->y;
    rc.right = rc.left + pcs->cx;
    rc.bottom = rc.top + pcs->cy;

    LaunchMode launchMode = LaunchMode::DefaultMode;
    if (_pfnCreateCallback)
    {
        _pfnCreateCallback(_window.get(), rc, launchMode);
    }

    int nCmdShow = SW_SHOW;
    if (launchMode == LaunchMode::MaximizedMode)
    {
        nCmdShow = SW_MAXIMIZE;
    }

    ShowWindow(_window.get(), nCmdShow);

    UpdateWindow(_window.get());
}

// Method Description:
// - Handles a WM_SIZING message, which occurs when user drags a window border
//   or corner. It intercepts this resize action and applies 'snapping' i.e.
//   aligns the terminal's size to its cell grid. We're given the window size,
//   which we then adjust based on the terminal's properties (like font size).
// Arguments:
// - wParam: Specifies which edge of the window is being dragged.
// - lParam: Pointer to the requested window rectangle (this is, the one that
//   originates from current drag action). It also acts as the return value
//   (it's a ref parameter).
// Return Value:
// - <none>
LRESULT IslandWindow::_OnSizing(const WPARAM wParam, const LPARAM lParam)
{
    if (!_pfnSnapDimensionCallback)
    {
        // If we haven't been given the callback that would adjust the dimension,
        // then we can't do anything, so just bail out.
        return FALSE;
    }

    LPRECT winRect = reinterpret_cast<LPRECT>(lParam);

    // Find nearest monitor.
    HMONITOR hmon = MonitorFromRect(winRect, MONITOR_DEFAULTTONEAREST);

    // This API guarantees that dpix and dpiy will be equal, but neither is an
    // optional parameter so give two UINTs.
    UINT dpix = USER_DEFAULT_SCREEN_DPI;
    UINT dpiy = USER_DEFAULT_SCREEN_DPI;
    // If this fails, we'll use the default of 96.
    GetDpiForMonitor(hmon, MDT_EFFECTIVE_DPI, &dpix, &dpiy);

    const auto widthScale = base::ClampedNumeric<float>(dpix) / USER_DEFAULT_SCREEN_DPI;
    const long minWidthScaled = minimumWidth * widthScale;

    const auto nonClientSize = GetTotalNonClientExclusiveSize(dpix);

    auto clientWidth = winRect->right - winRect->left - nonClientSize.cx;
    clientWidth = std::max(minWidthScaled, clientWidth);

    auto clientHeight = winRect->bottom - winRect->top - nonClientSize.cy;

    if (wParam != WMSZ_TOP && wParam != WMSZ_BOTTOM)
    {
        // If user has dragged anything but the top or bottom border (so e.g. left border,
        // top-right corner etc.), then this means that the width has changed. We thus ask to
        // adjust this new width so that terminal(s) is/are aligned to their character grid(s).
        clientWidth = gsl::narrow_cast<int>(_pfnSnapDimensionCallback(true, gsl::narrow_cast<float>(clientWidth)));
    }
    if (wParam != WMSZ_LEFT && wParam != WMSZ_RIGHT)
    {
        // Analogous to above, but for height.
        clientHeight = gsl::narrow_cast<int>(_pfnSnapDimensionCallback(false, gsl::narrow_cast<float>(clientHeight)));
    }

    // Now make the window rectangle match the calculated client width and height,
    // regarding which border the user is dragging. E.g. if user drags left border, then
    // we make sure to adjust the 'left' component of rectangle and not the 'right'. Note
    // that top-left and bottom-left corners also 'include' left border, hence we match
    // this in multi-case switch.

    // Set width
    switch (wParam)
    {
    case WMSZ_LEFT:
    case WMSZ_TOPLEFT:
    case WMSZ_BOTTOMLEFT:
        winRect->left = winRect->right - (clientWidth + nonClientSize.cx);
        break;
    case WMSZ_RIGHT:
    case WMSZ_TOPRIGHT:
    case WMSZ_BOTTOMRIGHT:
        winRect->right = winRect->left + (clientWidth + nonClientSize.cx);
        break;
    }

    // Set height
    switch (wParam)
    {
    case WMSZ_BOTTOM:
    case WMSZ_BOTTOMLEFT:
    case WMSZ_BOTTOMRIGHT:
        winRect->bottom = winRect->top + (clientHeight + nonClientSize.cy);
        break;
    case WMSZ_TOP:
    case WMSZ_TOPLEFT:
    case WMSZ_TOPRIGHT:
        winRect->top = winRect->bottom - (clientHeight + nonClientSize.cy);
        break;
    }

    return TRUE;
}

void IslandWindow::Initialize()
{
    const bool initialized = (_interopWindowHandle != nullptr);

    _source = DesktopWindowXamlSource{};

    auto interop = _source.as<IDesktopWindowXamlSourceNative>();
    winrt::check_hresult(interop->AttachToWindow(_window.get()));

    // stash the child interop handle so we can resize it when the main hwnd is resized
    interop->get_WindowHandle(&_interopWindowHandle);

    _rootGrid = winrt::Windows::UI::Xaml::Controls::Grid();
    _source.Content(_rootGrid);
}

void IslandWindow::OnSize(const UINT width, const UINT height)
{
    // update the interop window size
    SetWindowPos(_interopWindowHandle, nullptr, 0, 0, width, height, SWP_SHOWWINDOW);

    if (_rootGrid)
    {
        const auto size = GetLogicalSize();
        _rootGrid.Width(size.Width);
        _rootGrid.Height(size.Height);
    }
}

[[nodiscard]] LRESULT IslandWindow::MessageHandler(UINT const message, WPARAM const wparam, LPARAM const lparam) noexcept
{
    switch (message)
    {
    case WM_CREATE:
    {
        _HandleCreateWindow(wparam, lparam);
        return 0;
    }
    case WM_SETFOCUS:
    {
        if (_interopWindowHandle != nullptr)
        {
            // send focus to the child window
            SetFocus(_interopWindowHandle);
            return 0; // eat the message
        }
    }

    case WM_NCLBUTTONDOWN:
    case WM_NCLBUTTONUP:
    case WM_NCMBUTTONDOWN:
    case WM_NCMBUTTONUP:
    case WM_NCRBUTTONDOWN:
    case WM_NCRBUTTONUP:
    case WM_NCXBUTTONDOWN:
    case WM_NCXBUTTONUP:
    {
        // If we clicked in the titlebar, raise an event so the app host can
        // dispatch an appropriate event.
        _DragRegionClickedHandlers();
        break;
    }
    case WM_MENUCHAR:
    {
        // GH#891: return this LRESULT here to prevent the app from making a
        // bell when alt+key is pressed. A menu is active and the user presses a
        // key that does not correspond to any mnemonic or accelerator key,
        return MAKELRESULT(0, MNC_CLOSE);
    }
    case WM_SIZING:
    {
        return _OnSizing(wparam, lparam);
    }
    case WM_CLOSE:
    {
        // If the user wants to close the app by clicking 'X' button,
        // we hand off the close experience to the app layer. If all the tabs
        // are closed, the window will be closed as well.
        _windowCloseButtonClickedHandler();
        return 0;
    }
    case WM_MOUSEWHEEL:
        try
        {
            // This whole handler is a hack for GH#979.
            //
            // On some laptops, their trackpads won't scroll inactive windows
            // _ever_. With our entire window just being one giant XAML Island, the
            // touchpad driver thinks our entire window is inactive, and won't
            // scroll the XAML island. On those types of laptops, we'll get a
            // WM_MOUSEWHEEL here, in our root window, when the trackpad scrolls.
            // We're going to take that message and manually plumb it through to our
            // TermControl's, or anything else that implements IMouseWheelListener.

            // https://msdn.microsoft.com/en-us/library/windows/desktop/ms645617(v=vs.85).aspx
            // Important! Do not use the LOWORD or HIWORD macros to extract the x-
            // and y- coordinates of the cursor position because these macros return
            // incorrect results on systems with multiple monitors. Systems with
            // multiple monitors can have negative x- and y- coordinates, and LOWORD
            // and HIWORD treat the coordinates as unsigned quantities.
            const til::point eventPoint{ GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };
            // This mouse event is relative to the display origin, not the window. Convert here.
            const til::rectangle windowRect{ GetWindowRect() };
            const auto origin = windowRect.origin();
            const auto relative = eventPoint - origin;
            // Convert to logical scaling before raising the event.
            const auto real = relative / GetCurrentDpiScale();

            const short wheelDelta = static_cast<short>(HIWORD(wparam));

            // Raise an event, so any listeners can handle the mouse wheel event manually.
            _MouseScrolledHandlers(real, wheelDelta);
            return 0;
        }
        CATCH_LOG();
    }

    // TODO: handle messages here...
    return base_type::MessageHandler(message, wparam, lparam);
}

// Method Description:
// - Called when the window has been resized (or maximized)
// Arguments:
// - width: the new width of the window _in pixels_
// - height: the new height of the window _in pixels_
void IslandWindow::OnResize(const UINT width, const UINT height)
{
    if (_interopWindowHandle)
    {
        OnSize(width, height);
    }
}

// Method Description:
// - Called when the window is minimized to the taskbar.
void IslandWindow::OnMinimize()
{
    // TODO GH#1989 Stop rendering island content when the app is minimized.
}

// Method Description:
// - Called when the window is restored from having been minimized.
void IslandWindow::OnRestore()
{
    // TODO GH#1989 Stop rendering island content when the app is minimized.
}

void IslandWindow::SetContent(winrt::Windows::UI::Xaml::UIElement content)
{
    _rootGrid.Children().Clear();
    _rootGrid.Children().Append(content);
}

// Method Description:
// - Gets the difference between window and client area size.
// Arguments:
// - dpi: dpi of a monitor on which the window is placed
// Return Value
// - The size difference
SIZE IslandWindow::GetTotalNonClientExclusiveSize(const UINT dpi) const noexcept
{
    const auto windowStyle = static_cast<DWORD>(GetWindowLong(_window.get(), GWL_STYLE));
    RECT islandFrame{};

    // If we failed to get the correct window size for whatever reason, log
    // the error and go on. We'll use whatever the control proposed as the
    // size of our window, which will be at least close.
    LOG_IF_WIN32_BOOL_FALSE(AdjustWindowRectExForDpi(&islandFrame, windowStyle, false, 0, dpi));

    return {
        islandFrame.right - islandFrame.left,
        islandFrame.bottom - islandFrame.top
    };
}

void IslandWindow::OnAppInitialized()
{
    // Do a quick resize to force the island to paint
    const auto size = GetPhysicalSize();
    OnSize(size.cx, size.cy);
}

// Method Description:
// - Called when the app wants to change its theme. We'll update the root UI
//   element of the entire XAML tree, so that all UI elements get the theme
//   applied.
// Arguments:
// - arg: the ElementTheme to use as the new theme for the UI
// Return Value:
// - <none>
void IslandWindow::OnApplicationThemeChanged(const winrt::Windows::UI::Xaml::ElementTheme& requestedTheme)
{
    _rootGrid.RequestedTheme(requestedTheme);
    // Invalidate the window rect, so that we'll repaint any elements we're
    // drawing ourselves to match the new theme
    ::InvalidateRect(_window.get(), nullptr, false);
}

// Method Description:
// - Updates our focus mode state. See _SetIsBorderless for more details.
// Arguments:
// - <none>
// Return Value:
// - <none>
void IslandWindow::FocusModeChanged(const bool focusMode)
{
    // Do nothing if the value was unchanged.
    if (focusMode == _borderless)
    {
        return;
    }

    _SetIsBorderless(focusMode);
}

// Method Description:
// - Updates our fullscreen state. See _SetIsFullscreen for more details.
// Arguments:
// - <none>
// Return Value:
// - <none>
void IslandWindow::FullscreenChanged(const bool fullscreen)
{
    // Do nothing if the value was unchanged.
    if (fullscreen == _fullscreen)
    {
        return;
    }

    _SetIsFullscreen(fullscreen);
}

// Method Description:
// - Enter or exit the "always on top" state. Before the window is created, this
//   value will later be used when we create the window to create the window on
//   top of all others. After the window is created, it will either enter the
//   group of topmost windows, or exit the group of topmost windows.
// Arguments:
// - alwaysOnTop: whether we should be entering or exiting always on top mode.
// Return Value:
// - <none>
void IslandWindow::SetAlwaysOnTop(const bool alwaysOnTop)
{
    _alwaysOnTop = alwaysOnTop;

    const auto hwnd = GetHandle();
    if (hwnd)
    {
        SetWindowPos(hwnd,
                     _alwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST,
                     0, // the window dimensions are unused, because we're passing SWP_NOSIZE
                     0,
                     0,
                     0,
                     SWP_NOMOVE | SWP_NOSIZE);
    }
}

// From GdiEngine::s_SetWindowLongWHelper
void _SetWindowLongWHelper(const HWND hWnd, const int nIndex, const LONG dwNewLong) noexcept
{
    // SetWindowLong has strange error handling. On success, it returns the
    // previous Window Long value and doesn't modify the Last Error state. To
    // deal with this, we set the last error to 0/S_OK first, call it, and if
    // the previous long was 0, we check if the error was non-zero before
    // reporting. Otherwise, we'll get an "Error: The operation has completed
    // successfully." and there will be another screenshot on the internet
    // making fun of Windows. See:
    // https://msdn.microsoft.com/en-us/library/windows/desktop/ms633591(v=vs.85).aspx
    SetLastError(0);
    LONG const lResult = SetWindowLongW(hWnd, nIndex, dwNewLong);
    if (0 == lResult)
    {
        LOG_LAST_ERROR_IF(0 != GetLastError());
    }
}

// Method Description:
// - This is a helper to figure out what the window styles should be, given the
//   current state of flags like borderless mode and fullscreen mode.
// Arguments:
// - <none>
// Return Value:
// - a LONG with the appropriate flags set for our current window mode, to be used with GWL_STYLE
LONG IslandWindow::_getDesiredWindowStyle() const
{
    auto windowStyle = GetWindowLongW(GetHandle(), GWL_STYLE);

    // If we're both fullscreen and borderless, fullscreen mode takes precedence.

    if (_fullscreen)
    {
        // When moving to fullscreen, remove WS_OVERLAPPEDWINDOW, which specifies
        // styles for non-fullscreen windows (e.g. caption bar), and add the
        // WS_POPUP style to allow us to size ourselves to the monitor size.
        // Do the reverse when restoring from fullscreen.
        // Doing these modifications to that window will cause a vista-style
        // window frame to briefly appear when entering and exiting fullscreen.
        WI_ClearFlag(windowStyle, WS_BORDER);
        WI_ClearFlag(windowStyle, WS_SIZEBOX);
        WI_ClearAllFlags(windowStyle, WS_OVERLAPPEDWINDOW);

        WI_SetFlag(windowStyle, WS_POPUP);
        return windowStyle;
    }
    else if (_borderless)
    {
        // When moving to borderless, remove WS_OVERLAPPEDWINDOW, which
        // specifies styles for non-fullscreen windows (e.g. caption bar), and
        // add the WS_BORDER and WS_SIZEBOX styles. This allows us to still have
        // a small resizing frame, but without a full titlebar, nor caption
        // buttons.

        WI_ClearAllFlags(windowStyle, WS_OVERLAPPEDWINDOW);
        WI_ClearFlag(windowStyle, WS_POPUP);

        WI_SetFlag(windowStyle, WS_BORDER);
        WI_SetFlag(windowStyle, WS_SIZEBOX);
        return windowStyle;
    }

    // Here, we're not in either fullscreen or borderless mode. Return to
    // WS_OVERLAPPEDWINDOW.
    WI_ClearFlag(windowStyle, WS_POPUP);
    WI_ClearFlag(windowStyle, WS_BORDER);
    WI_ClearFlag(windowStyle, WS_SIZEBOX);

    WI_SetAllFlags(windowStyle, WS_OVERLAPPEDWINDOW);

    return windowStyle;
}

// Method Description:
// - Enable or disable focus mode. When entering focus mode, we'll
//   need to manually hide the entire titlebar.
// - When we're entering focus we need to do some additional modification
//   of our window styles. However, the NonClientIslandWindow very explicitly
//   _doesn't_ need to do these steps.
// Arguments:
// - borderlessEnabled: If true, we're entering focus mode. If false, we're leaving.
// Return Value:
// - <none>
void IslandWindow::_SetIsBorderless(const bool borderlessEnabled)
{
    _borderless = borderlessEnabled;

    HWND const hWnd = GetHandle();

    // First, modify regular window styles as appropriate
    auto windowStyle = _getDesiredWindowStyle();
    _SetWindowLongWHelper(hWnd, GWL_STYLE, windowStyle);

    // Now modify extended window styles as appropriate
    // When moving to fullscreen, remove the window edge style to avoid an
    // ugly border when not focused.
    auto exWindowStyle = GetWindowLongW(hWnd, GWL_EXSTYLE);
    WI_UpdateFlag(exWindowStyle, WS_EX_WINDOWEDGE, !_fullscreen);
    _SetWindowLongWHelper(hWnd, GWL_EXSTYLE, exWindowStyle);

    // Resize the window, with SWP_FRAMECHANGED, to trigger user32 to
    // recalculate the non/client areas
    const til::rectangle windowPos{ GetWindowRect() };
    SetWindowPos(GetHandle(),
                 HWND_TOP,
                 windowPos.left<int>(),
                 windowPos.top<int>(),
                 windowPos.width<int>(),
                 windowPos.height<int>(),
                 SWP_SHOWWINDOW | SWP_FRAMECHANGED);
}

// Method Description:
// - Controls setting us into or out of fullscreen mode. Largely taken from
//   Window::SetIsFullscreen in conhost.
// - When entering fullscreen mode, we'll save the current window size and
//   location, and expand to take the entire monitor size. When leaving, we'll
//   use that saved size to restore back to.
// Arguments:
// - fullscreenEnabled true if we should enable fullscreen mode, false to disable.
// Return Value:
// - <none>
void IslandWindow::_SetIsFullscreen(const bool fullscreenEnabled)
{
    // It is possible to enter _SetIsFullscreen even if we're already in full
    // screen. Use the old is in fullscreen flag to gate checks that rely on the
    // current state.
    const auto oldIsInFullscreen = _fullscreen;
    _fullscreen = fullscreenEnabled;

    HWND const hWnd = GetHandle();

    // First, modify regular window styles as appropriate
    auto windowStyle = _getDesiredWindowStyle();
    _SetWindowLongWHelper(hWnd, GWL_STYLE, windowStyle);

    // Now modify extended window styles as appropriate
    // When moving to fullscreen, remove the window edge style to avoid an
    // ugly border when not focused.
    auto exWindowStyle = GetWindowLongW(hWnd, GWL_EXSTYLE);
    WI_UpdateFlag(exWindowStyle, WS_EX_WINDOWEDGE, !_fullscreen);
    _SetWindowLongWHelper(hWnd, GWL_EXSTYLE, exWindowStyle);

    // When entering/exiting fullscreen mode, we also need to backup/restore the
    // current window size, and resize the window to match the new state.
    _BackupWindowSizes(oldIsInFullscreen);
    _ApplyWindowSize();
}

// Method Description:
// - Used in entering/exiting fullscreen mode. Saves the current window size,
//   and the full size of the monitor, for use in _ApplyWindowSize.
// - Taken from conhost's Window::_BackupWindowSizes
// Arguments:
// - fCurrentIsInFullscreen: true if we're currently in fullscreen mode.
// Return Value:
// - <none>
void IslandWindow::_BackupWindowSizes(const bool fCurrentIsInFullscreen)
{
    if (_fullscreen)
    {
        // Note: the current window size depends on the current state of the
        // window. So don't back it up if we're already in full screen.
        if (!fCurrentIsInFullscreen)
        {
            _nonFullscreenWindowSize = GetWindowRect();
        }

        // get and back up the current monitor's size
        HMONITOR const hCurrentMonitor = MonitorFromWindow(GetHandle(), MONITOR_DEFAULTTONEAREST);
        MONITORINFO currMonitorInfo;
        currMonitorInfo.cbSize = sizeof(currMonitorInfo);
        if (GetMonitorInfo(hCurrentMonitor, &currMonitorInfo))
        {
            _fullscreenWindowSize = currMonitorInfo.rcMonitor;
        }
    }
}

// Method Description:
// - Applys the appropriate window size for transitioning to/from fullscreen mode.
// - Taken from conhost's Window::_ApplyWindowSize
// Arguments:
// - <none>
// Return Value:
// - <none>
void IslandWindow::_ApplyWindowSize()
{
    const auto newSize = _fullscreen ? _fullscreenWindowSize : _nonFullscreenWindowSize;
    LOG_IF_WIN32_BOOL_FALSE(SetWindowPos(GetHandle(),
                                         HWND_TOP,
                                         newSize.left,
                                         newSize.top,
                                         newSize.right - newSize.left,
                                         newSize.bottom - newSize.top,
                                         SWP_FRAMECHANGED));
}

DEFINE_EVENT(IslandWindow, DragRegionClicked, _DragRegionClickedHandlers, winrt::delegate<>);
DEFINE_EVENT(IslandWindow, WindowCloseButtonClicked, _windowCloseButtonClickedHandler, winrt::delegate<>);
