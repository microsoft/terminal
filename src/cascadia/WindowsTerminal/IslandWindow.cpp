// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "IslandWindow.h"
#include "../types/inc/Viewport.hpp"
#include "resource.h"
#include "icon.h"
#include "NotificationIcon.h"
#include <dwmapi.h>
#include <TerminalThemeHelpers.h>
#include <CoreWindow.h>

extern "C" IMAGE_DOS_HEADER __ImageBase;

using namespace winrt::Windows::UI;
using namespace winrt::Windows::UI::Composition;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Hosting;
using namespace winrt::Windows::Foundation::Numerics;
using namespace winrt::Microsoft::Terminal::Settings::Model;
using namespace winrt::Microsoft::Terminal::Control;
using namespace winrt::Microsoft::Terminal;
using namespace ::Microsoft::Console::Types;
using VirtualKeyModifiers = winrt::Windows::System::VirtualKeyModifiers;

#define XAML_HOSTING_WINDOW_CLASS_NAME L"CASCADIA_HOSTING_WINDOW_CLASS"
#define IDM_SYSTEM_MENU_BEGIN 0x1000

IslandWindow::IslandWindow() noexcept :
    _interopWindowHandle{ nullptr },
    _rootGrid{ nullptr },
    _source{ nullptr },
    _pfnCreateCallback{ nullptr }
{
}

IslandWindow::~IslandWindow()
{
    Close();
}

void IslandWindow::Close()
{
    // GH#15454: Unset the user data for the window. This will prevent future
    // callbacks that come onto our window message loop from being sent to the
    // IslandWindow (or other derived class's) implementation.
    //
    // Specifically, this prevents a pending coroutine from being able to call
    // something like ShowWindow, and have that come back on the IslandWindow
    // message loop, where it'll end up asking XAML something that XAML is no
    // longer able to answer.
    SetWindowLongPtr(_window.get(), GWLP_USERDATA, 0);

    if (_source)
    {
        _source.Close();
        _source = nullptr;
    }
}

// Clear out any state that might be associated with this app instance, so that
// we can later re-use this HWND for another instance.
//
// This doesn't actually close out our HWND or DesktopWindowXamlSource, but it
// will remove all our content, and SW_HIDE the window, so it isn't accessible.
void IslandWindow::Refrigerate() noexcept
{
    // Similar to in Close - unset our HWND's user data. We'll re-set this when
    // we get re-heated, so that while we're refrigerated, we won't have
    // unexpected callbacks into us while we don't have content.
    //
    // This pointer will get re-set in _warmInitialize
    SetWindowLongPtr(_window.get(), GWLP_USERDATA, 0);

    _pfnCreateCallback = nullptr;
    _pfnSnapDimensionCallback = nullptr;

    _rootGrid.Children().Clear();
    ShowWindow(_window.get(), SW_HIDE);
}

HWND IslandWindow::GetInteropHandle() const
{
    return _interopWindowHandle;
}

// Method Description:
// - Create the actual window that we'll use for the application.
// Arguments:
// - <none>
// Return Value:
// - <none>
void IslandWindow::MakeWindow() noexcept
{
    if (_window)
    {
        // no-op if we already have a window.
        return;
    }

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
    //
    // We need WS_EX_NOREDIRECTIONBITMAP for vintage style opacity, GH#603
    //
    // WS_EX_LAYERED acts REAL WEIRD with TerminalTrySetTransparentBackground,
    // but it works just fine when the window is in the TOPMOST group. But if
    // you enable it always, activating the window will remove our DWM frame
    // entirely. Weird.
    WINRT_VERIFY(CreateWindowEx(WS_EX_NOREDIRECTIONBITMAP | (_alwaysOnTop ? WS_EX_TOPMOST : 0),
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
// - Set a callback to be called when we process a WM_CREATE message. This gives
//   the AppHost a chance to resize the window to the proper size.
// Arguments:
// - pfn: a function to be called during the handling of WM_CREATE. It takes two
//   parameters:
//      * HWND: the HWND of the window that's being created.
//      * til::rect: The position on the screen that the system has proposed for our
//        window.
// Return Value:
// - <none>
void IslandWindow::SetCreateCallback(std::function<void(const HWND, const til::rect&)> pfn) noexcept
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
    auto pcs = reinterpret_cast<CREATESTRUCTW*>(lParam);
    til::rect rc;
    rc.left = pcs->x;
    rc.top = pcs->y;
    rc.right = rc.left + pcs->cx;
    rc.bottom = rc.top + pcs->cy;

    if (_pfnCreateCallback)
    {
        _pfnCreateCallback(_window.get(), rc);
    }

    // GH#11561: DO NOT call ShowWindow here. The AppHost will call ShowWindow
    // once the app has completed its initialization.

    UpdateWindow(_window.get());

    UpdateWindowIconForActiveMetrics(_window.get());
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
        return false;
    }

    auto winRect = reinterpret_cast<LPRECT>(lParam);

    // Find nearest monitor.
    auto hmon = MonitorFromRect(winRect, MONITOR_DEFAULTTONEAREST);

    // This API guarantees that dpix and dpiy will be equal, but neither is an
    // optional parameter so give two UINTs.
    UINT dpix = USER_DEFAULT_SCREEN_DPI;
    UINT dpiy = USER_DEFAULT_SCREEN_DPI;
    // If this fails, we'll use the default of 96. I think it can only fail for
    // bad parameters, which we won't have, so no big deal.
    LOG_IF_FAILED(GetDpiForMonitor(hmon, MDT_EFFECTIVE_DPI, &dpix, &dpiy));

    const long minWidthScaled = minimumWidth * dpix / USER_DEFAULT_SCREEN_DPI;

    const auto nonClientSize = GetTotalNonClientExclusiveSize(dpix);

    auto clientWidth = winRect->right - winRect->left - nonClientSize.width;
    clientWidth = std::max(minWidthScaled, clientWidth);

    auto clientHeight = winRect->bottom - winRect->top - nonClientSize.height;

    // If we're the quake window, prevent resizing on all sides except the
    // bottom. This also applies to resizing with the Alt+Space menu
    if (IsQuakeWindow() && wParam != WMSZ_BOTTOM)
    {
        // Stuff our current window size into the lParam, and return true. This
        // will tell User32 to use our current dimensions to resize to.
        ::GetWindowRect(_window.get(), winRect);
        return true;
    }

    if (wParam != WMSZ_TOP && wParam != WMSZ_BOTTOM)
    {
        // If user has dragged anything but the top or bottom border (so e.g. left border,
        // top-right corner etc.), then this means that the width has changed. We thus ask to
        // adjust this new width so that terminal(s) is/are aligned to their character grid(s).
        clientWidth = gsl::narrow_cast<decltype(clientWidth)>(_pfnSnapDimensionCallback(true, gsl::narrow_cast<float>(clientWidth)));
    }
    if (wParam != WMSZ_LEFT && wParam != WMSZ_RIGHT)
    {
        // Analogous to above, but for height.
        clientHeight = gsl::narrow_cast<decltype(clientHeight)>(_pfnSnapDimensionCallback(false, gsl::narrow_cast<float>(clientHeight)));
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
        winRect->left = winRect->right - (clientWidth + nonClientSize.width);
        break;
    case WMSZ_RIGHT:
    case WMSZ_TOPRIGHT:
    case WMSZ_BOTTOMRIGHT:
        winRect->right = winRect->left + (clientWidth + nonClientSize.width);
        break;
    }

    // Set height
    switch (wParam)
    {
    case WMSZ_BOTTOM:
    case WMSZ_BOTTOMLEFT:
    case WMSZ_BOTTOMRIGHT:
        winRect->bottom = winRect->top + (clientHeight + nonClientSize.height);
        break;
    case WMSZ_TOP:
    case WMSZ_TOPLEFT:
    case WMSZ_TOPRIGHT:
        winRect->top = winRect->bottom - (clientHeight + nonClientSize.height);
        break;
    }

    return true;
}

// Method Description:
// - Handle the WM_MOVING message
// - If we're the quake window, then we don't want to be able to be moved.
//   Immediately return our current window position, which will prevent us from
//   being moved at all.
// Arguments:
// - lParam: a LPRECT with the proposed window position, that should be filled
//   with the resultant position.
// Return Value:
// - true iff we handled this message.
LRESULT IslandWindow::_OnMoving(const WPARAM /*wParam*/, const LPARAM lParam)
{
    auto winRect = reinterpret_cast<LPRECT>(lParam);

    // If we're the quake window, prevent moving the window. If we don't do
    // this, then Alt+Space...Move will still be able to move the window.
    if (IsQuakeWindow())
    {
        // Stuff our current window into the lParam, and return true. This
        // will tell User32 to use our current position to move to.
        ::GetWindowRect(_window.get(), winRect);
        return true;
    }
    return false;
}

// return true if this was a "cold" initialize, that didn't start XAML before.
bool IslandWindow::Initialize()
{
    if (!_source)
    {
        _coldInitialize();
        return true;
    }
    else
    {
        // This was a "warm" initialize - we've already got an HWND, but we need
        // to move it to the new correct place, new size, and reset any leftover
        // runtime state.
        _warmInitialize();
        return false;
    }
}

// Method Description:
// - Start this window for the first time. This will instantiate our XAML
//   island, set up our root grid, and initialize some other members that only
//   need to be initialized once.
// - This should only be called once.
void IslandWindow::_coldInitialize()
{
    _source = DesktopWindowXamlSource{};

    auto interop = _source.as<IDesktopWindowXamlSourceNative>();
    winrt::check_hresult(interop->AttachToWindow(_window.get()));

    // stash the child interop handle so we can resize it when the main hwnd is resized
    interop->get_WindowHandle(&_interopWindowHandle);

    // Immediately hide our XAML island hwnd. On earlier versions of Windows,
    // this HWND could sometimes appear as an actual window in the taskbar
    // without this!
    ShowWindow(_interopWindowHandle, SW_HIDE);

    _rootGrid = winrt::Windows::UI::Xaml::Controls::Grid();
    _source.Content(_rootGrid);

    // initialize the taskbar object
    if (auto taskbar = wil::CoCreateInstanceNoThrow<ITaskbarList3>(CLSID_TaskbarList))
    {
        if (SUCCEEDED(taskbar->HrInit()))
        {
            _taskbar = std::move(taskbar);
        }
    }

    _systemMenuNextItemId = IDM_SYSTEM_MENU_BEGIN;

    // Enable vintage opacity by removing the XAML emergency backstop, GH#603.
    // We don't really care if this failed or not.
    TerminalTrySetTransparentBackground(true);
}
void IslandWindow::_warmInitialize()
{
    // re-add the pointer to us to our HWND's user data, so that we can start
    // getting window proc callbacks again.
    _setupUserData();

    // Manually ask how we want to be created.
    if (_pfnCreateCallback)
    {
        til::rect rc{ GetWindowRect() };
        _pfnCreateCallback(_window.get(), rc);
    }

    // Don't call IslandWindow::OnSize - that will set the Width/Height members
    // of the _rootGrid. However, NonClientIslandWindow doesn't use those! If you set them, here,
    // the contents of the window will never resize.
    UpdateWindow(_window.get());
    ForceResize();
}

void IslandWindow::OnSize(const UINT width, const UINT height)
{
    // NOTE: This _isn't_ called by NonClientIslandWindow::OnSize. The
    // NonClientIslandWindow has very different logic for positioning the
    // DesktopWindowXamlSource inside its HWND.

    // update the interop window size
    SetWindowPos(_interopWindowHandle, nullptr, 0, 0, width, height, SWP_SHOWWINDOW | SWP_NOACTIVATE);

    if (_rootGrid)
    {
        const auto size = GetLogicalSize();
        _rootGrid.Width(size.Width);
        _rootGrid.Height(size.Height);
    }
}

// Method Description:
// - Handles a WM_GETMINMAXINFO message, issued before the window sizing starts.
//   This message allows to modify the minimal and maximal dimensions of the window.
//   We focus on minimal dimensions here
//   (the maximal dimension will be calculate upon maximizing)
//   Our goal is to protect against to downsizing to less than minimal allowed dimensions,
//   that might occur in the scenarios where _OnSizing is bypassed.
//   An example of such scenario is anchoring the window to the top/bottom screen border
//   in order to maximize window height (GH# 8026).
//   The computation is similar to what we do in _OnSizing:
//   we need to consider both the client area and non-client exclusive area sizes,
//   while taking DPI into account as well.
// Arguments:
// - lParam: Pointer to the requested MINMAXINFO struct,
//   a ptMinTrackSize field of which we want to update with the computed dimensions.
//   It also acts as the return value (it's a ref parameter).
// Return Value:
// - <none>

void IslandWindow::_OnGetMinMaxInfo(const WPARAM /*wParam*/, const LPARAM lParam)
{
    // Without a callback we don't know to snap the dimensions of the client area.
    // Should not be a problem, the callback is not set early in the startup
    // The initial dimensions will be set later on
    if (!_pfnSnapDimensionCallback)
    {
        return;
    }

    auto hmon = MonitorFromWindow(GetHandle(), MONITOR_DEFAULTTONEAREST);
    if (hmon == NULL)
    {
        return;
    }

    UINT dpix = USER_DEFAULT_SCREEN_DPI;
    UINT dpiy = USER_DEFAULT_SCREEN_DPI;
    GetDpiForMonitor(hmon, MDT_EFFECTIVE_DPI, &dpix, &dpiy);

    // From now we use dpix for all computations (same as in _OnSizing).
    const auto nonClientSizeScaled = GetTotalNonClientExclusiveSize(dpix);

    auto lpMinMaxInfo = reinterpret_cast<LPMINMAXINFO>(lParam);
    lpMinMaxInfo->ptMinTrackSize.x = _calculateTotalSize(true, minimumWidth * dpix / USER_DEFAULT_SCREEN_DPI, nonClientSizeScaled.width);
    lpMinMaxInfo->ptMinTrackSize.y = _calculateTotalSize(false, minimumHeight * dpiy / USER_DEFAULT_SCREEN_DPI, nonClientSizeScaled.height);
}

// Method Description:
// - Helper function that calculates a single dimension value, given initialWindow and nonClientSizes
// Arguments:
// - isWidth: parameter to pass to SnapDimensionCallback.
//   True if the method is invoked for width computation, false if for height.
// - clientSize: the size of the client area (already)
// - nonClientSizeScaled: the exclusive non-client size (already scaled)
// Return Value:
// - The total dimension
long IslandWindow::_calculateTotalSize(const bool isWidth, const long clientSize, const long nonClientSize)
{
    if (_pfnSnapDimensionCallback)
    {
        return gsl::narrow_cast<int>(_pfnSnapDimensionCallback(isWidth, gsl::narrow_cast<float>(clientSize)) + nonClientSize);
    }
    // We might have been called in WM_CREATE, before we've initialized XAML or
    // our page. That's okay.
    return clientSize + nonClientSize;
}

[[nodiscard]] LRESULT IslandWindow::MessageHandler(UINT const message, WPARAM const wparam, LPARAM const lparam) noexcept
{
    switch (message)
    {
    case WM_GETMINMAXINFO:
    {
        _OnGetMinMaxInfo(wparam, lparam);
        return 0;
    }
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
            return 0;
        }
        break;
    }
    case WM_ACTIVATE:
    {
        // wparam = 0 indicates the window was deactivated
        const bool activated = LOWORD(wparam) != 0;
        _WindowActivatedHandlers(activated);

        if (_autoHideWindow && !activated)
        {
            if (_isQuakeWindow || _minimizeToNotificationArea)
            {
                HideWindow();
            }
            else
            {
                ShowWindow(GetHandle(), SW_MINIMIZE);
            }
        }

        break;
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
    case WM_SIZE:
    {
        if (wparam == SIZE_RESTORED || wparam == SIZE_MAXIMIZED)
        {
            _WindowVisibilityChangedHandlers(true);
            _MaximizeChangedHandlers(wparam == SIZE_MAXIMIZED);
        }

        if (wparam == SIZE_MINIMIZED)
        {
            _WindowVisibilityChangedHandlers(false);
            if (_isQuakeWindow)
            {
                ShowWindow(GetHandle(), SW_HIDE);
                return 0;
            }
        }

        // BODGY This is a fix for the upstream:
        //
        // https://github.com/microsoft/microsoft-ui-xaml/issues/3577
        //
        // ContentDialogs don't resize themselves when the XAML island resizes.
        // However, if we manually resize our CoreWindow, that'll actually
        // trigger a resize of the ContentDialog.
        if (const auto& coreWindow{ winrt::Windows::UI::Core::CoreWindow::GetForCurrentThread() })
        {
            if (const auto& interop{ coreWindow.as<ICoreWindowInterop>() })
            {
                HWND coreWindowInterop;
                interop->get_WindowHandle(&coreWindowInterop);
                PostMessage(coreWindowInterop, message, wparam, lparam);
            }
        }

        break;
    }
    case WM_MOVING:
    {
        return _OnMoving(wparam, lparam);
    }
    case WM_MOVE:
    {
        _WindowMovedHandlers();
        break;
    }
    case WM_CLOSE:
    {
        // If the user wants to close the app by clicking 'X' button,
        // we hand off the close experience to the app layer. If all the tabs
        // are closed, the window will be closed as well.
        _WindowCloseButtonClickedHandlers();
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
            const til::rect windowRect{ GetWindowRect() };
            const auto origin = windowRect.origin();
            const auto relative = eventPoint - origin;
            // Convert to logical scaling before raising the event.
            const auto scale = GetCurrentDpiScale();
            const til::point real{ til::math::flooring, relative.x / scale, relative.y / scale };

            const auto wheelDelta = static_cast<short>(HIWORD(wparam));

            // Raise an event, so any listeners can handle the mouse wheel event manually.
            _MouseScrolledHandlers(real, wheelDelta);
            return 0;
        }
        CATCH_LOG();
        break;
    case WM_THEMECHANGED:
        UpdateWindowIconForActiveMetrics(_window.get());
        return 0;
    case WM_WINDOWPOSCHANGING:
    {
        // GH#10274 - if the quake window gets moved to another monitor via aero
        // snap (win+shift+arrows), then re-adjust the size for the new monitor.
        if (IsQuakeWindow())
        {
            // Retrieve the suggested dimensions and make a rect and size.
            auto lpwpos = (LPWINDOWPOS)lparam;

            // We only need to apply restrictions if the position is changing.
            // The SWP_ flags are confusing to read. This is
            // "if we're not moving the window, do nothing."
            if (WI_IsFlagSet(lpwpos->flags, SWP_NOMOVE))
            {
                break;
            }
            // Figure out the suggested dimensions and position.
            RECT rcSuggested;
            rcSuggested.left = lpwpos->x;
            rcSuggested.top = lpwpos->y;
            rcSuggested.right = rcSuggested.left + lpwpos->cx;
            rcSuggested.bottom = rcSuggested.top + lpwpos->cy;

            // Find the bounds of the current monitor, and the monitor that
            // we're suggested to be on.

            auto current = MonitorFromWindow(_window.get(), MONITOR_DEFAULTTONEAREST);
            MONITORINFO currentInfo;
            currentInfo.cbSize = sizeof(MONITORINFO);
            GetMonitorInfo(current, &currentInfo);

            auto proposed = MonitorFromRect(&rcSuggested, MONITOR_DEFAULTTONEAREST);
            MONITORINFO proposedInfo;
            proposedInfo.cbSize = sizeof(MONITORINFO);
            GetMonitorInfo(proposed, &proposedInfo);

            // If the monitor changed...
            if (til::rect{ proposedInfo.rcMonitor } !=
                til::rect{ currentInfo.rcMonitor })
            {
                const auto newWindowRect{ _getQuakeModeSize(proposed) };

                // Inform User32 that we want to be placed at the position
                // and dimensions that _getQuakeModeSize returned. When we
                // snap across monitor boundaries, this will re-evaluate our
                // size for the new monitor.
                lpwpos->x = newWindowRect.left;
                lpwpos->y = newWindowRect.top;
                lpwpos->cx = newWindowRect.width();
                lpwpos->cy = newWindowRect.height();

                return 0;
            }
        }
        break;
    }
    case WM_SYSCOMMAND:
    {
        // the low 4 bits contain additional information (that we don't care about)
        auto highBits = wparam & 0xFFF0;
        if (highBits == SC_RESTORE || highBits == SC_MAXIMIZE)
        {
            _MaximizeChangedHandlers(highBits == SC_MAXIMIZE);
        }

        if (wparam == SC_RESTORE && _fullscreen)
        {
            _ShouldExitFullscreenHandlers();
            return 0;
        }
        auto search = _systemMenuItems.find(LOWORD(wparam));
        if (search != _systemMenuItems.end())
        {
            search->second.callback();
        }
        break;
    }
    case WM_SETTINGCHANGE:
    {
        // Currently, we only support checking when the OS theme changes. In
        // that case, wParam is 0. Re-evaluate when we decide to reload env vars
        // (GH#1125)
        if (wparam == 0 && lparam != 0)
        {
            const std::wstring param{ (wchar_t*)lparam };
            // ImmersiveColorSet seems to be the notification that the OS theme
            // changed. If that happens, let the app know, so it can hot-reload
            // themes, color schemes that might depend on the OS theme
            if (param == L"ImmersiveColorSet")
            {
                _UpdateSettingsRequestedHandlers();
            }
        }
        break;
    }
    case WM_ENDSESSION:
    {
        // For WM_QUERYENDSESSION and WM_ENDSESSION, refer to:
        //
        // https://docs.microsoft.com/en-us/windows/win32/rstmgr/guidelines-for-applications
        //
        // The OS will send us a WM_QUERYENDSESSION when it's preparing an
        // update for our app. It will then send us a WM_ENDSESSION, which gives
        // us a small timeout (~30s) to actually shut down gracefully. After
        // that timeout, it will send us a WM_CLOSE. If we still don't close
        // after the WM_CLOSE, it'll force-kill us (causing a crash which will be
        // bucketed to MoAppHang).
        //
        // If we need to do anything to prepare for being told to shutdown,
        // start it in WM_QUERYENDSESSION. If (in the future) we need to prevent
        // logoff, we can return false there. (DefWindowProc returns true)
        //
        // The OS is going to shut us down here. We will manually start a quit,
        // so that we can persist the state. If we refuse to gracefully shut
        // down here, the OS will crash us to forcefully terminate us. We choose
        // to quit here, rather than just close, to skip over any warning
        // dialogs (e.g. "Are you sure you want to close all tabs?") which might
        // prevent a WM_CLOSE from cleanly closing the window.
        //
        // This will cause a appHost._RequestQuitAll, which will notify the
        // monarch to collect up all the window state and save it.

        TraceLoggingWrite(
            g_hWindowsTerminalProvider,
            "EndSession",
            TraceLoggingDescription("Emitted when the OS has sent a WM_ENDSESSION"),
            TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
            TraceLoggingKeyword(TIL_KEYWORD_TRACE));

        _AutomaticShutdownRequestedHandlers();
        return true;
    }
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
    if (_minimizeToNotificationArea)
    {
        HideWindow();
    }
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
// - Get the dimensions of our non-client area, as a rect where each component
//   represents that side.
// - The .left will be a negative number, to represent that the actual side of
//   the non-client area is outside the border of our window. It's roughly 8px (
//   * DPI scaling) to the left of the visible border.
// - The .right component will be positive, indicating that the nonclient border
//   is in the positive-x direction from the edge of our client area.
// - This will also include our titlebar! It's in the nonclient area for us.
// Arguments:
// - dpi: the scaling that we should use to calculate the border sizes.
// Return Value:
// - a til::rect whose components represent the margins of the nonclient area,
//   relative to the client area.
til::rect IslandWindow::GetNonClientFrame(const UINT dpi) const noexcept
{
    const auto windowStyle = static_cast<DWORD>(GetWindowLong(_window.get(), GWL_STYLE));
    RECT islandFrame{};

    // If we failed to get the correct window size for whatever reason, log
    // the error and go on. We'll use whatever the control proposed as the
    // size of our window, which will be at least close.
    LOG_IF_WIN32_BOOL_FALSE(AdjustWindowRectExForDpi(&islandFrame, windowStyle, false, 0, dpi));
    return til::rect{ islandFrame };
}

// Method Description:
// - Gets the difference between window and client area size.
// Arguments:
// - dpi: dpi of a monitor on which the window is placed
// Return Value
// - The size difference
til::size IslandWindow::GetTotalNonClientExclusiveSize(const UINT dpi) const noexcept
{
    const auto islandFrame{ GetNonClientFrame(dpi) };
    return {
        islandFrame.right - islandFrame.left,
        islandFrame.bottom - islandFrame.top
    };
}

void IslandWindow::OnAppInitialized()
{
    // Do a quick resize to force the island to paint
    const auto size = GetPhysicalSize();
    OnSize(size.width, size.height);
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
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }
}

// Method Description:
// - Posts a message to the window message queue that the window visibility has changed
//   and should then be minimized or restored.
// Arguments:
// - showOrHide: True for show; false for hide.
// Return Value:
// - <none>
void IslandWindow::ShowWindowChanged(const bool showOrHide)
{
    if (const auto hwnd = GetHandle())
    {
        // IMPORTANT!
        //
        // ONLY "restore" if already minimized. If the window is maximized or
        // snapped, a restore will restore-down the window instead.
        if (showOrHide == true && ::IsIconic(hwnd))
        {
            ::PostMessage(hwnd, WM_SYSCOMMAND, SC_RESTORE, 0);
        }
        else if (showOrHide == false)
        {
            ::PostMessage(hwnd, WM_SYSCOMMAND, SC_MINIMIZE, 0);
        }
    }
}

// Method Description
// - Flash the taskbar icon, indicating to the user that something needs their attention
void IslandWindow::FlashTaskbar()
{
    // Using 'false' as the boolean argument ensures that the taskbar is
    // only flashed if the app window is not focused
    FlashWindow(_window.get(), false);
}

// Method Description:
// - Sets the taskbar progress indicator
// - We follow the ConEmu style for the state and progress values,
//   more details at https://conemu.github.io/en/AnsiEscapeCodes.html#ConEmu_specific_OSC
// Arguments:
// - state: indicates the progress state
// - progress: indicates the progress value
void IslandWindow::SetTaskbarProgress(const size_t state, const size_t progress)
{
    if (_taskbar)
    {
        switch (state)
        {
        case 0:
            // removes the taskbar progress indicator
            _taskbar->SetProgressState(_window.get(), TBPF_NOPROGRESS);
            break;
        case 1:
            // sets the progress value to value given by the 'progress' parameter
            _taskbar->SetProgressState(_window.get(), TBPF_NORMAL);
            _taskbar->SetProgressValue(_window.get(), progress, 100);
            break;
        case 2:
            // sets the progress indicator to an error state
            _taskbar->SetProgressState(_window.get(), TBPF_ERROR);
            _taskbar->SetProgressValue(_window.get(), progress, 100);
            break;
        case 3:
            // sets the progress indicator to an indeterminate state.
            // FIRST, set the progress to "no progress". That'll clear out any
            // progress value from the previous state. Otherwise, a transition
            // from (error,x%) or (warning,x%) to indeterminate will leave the
            // progress value unchanged, and not show the spinner.
            _taskbar->SetProgressState(_window.get(), TBPF_NOPROGRESS);
            _taskbar->SetProgressState(_window.get(), TBPF_INDETERMINATE);
            break;
        case 4:
            // sets the progress indicator to a pause state
            _taskbar->SetProgressState(_window.get(), TBPF_PAUSED);
            _taskbar->SetProgressValue(_window.get(), progress, 100);
            break;
        default:
            break;
        }
    }
}

// From GdiEngine::s_SetWindowLongWHelper
void SetWindowLongWHelper(const HWND hWnd, const int nIndex, const LONG dwNewLong) noexcept
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
    const auto lResult = SetWindowLongW(hWnd, nIndex, dwNewLong);
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

    const auto hWnd = GetHandle();

    // First, modify regular window styles as appropriate
    auto windowStyle = _getDesiredWindowStyle();
    SetWindowLongWHelper(hWnd, GWL_STYLE, windowStyle);

    // Now modify extended window styles as appropriate
    // When moving to fullscreen, remove the window edge style to avoid an
    // ugly border when not focused.
    auto exWindowStyle = GetWindowLongW(hWnd, GWL_EXSTYLE);
    WI_UpdateFlag(exWindowStyle, WS_EX_WINDOWEDGE, !_fullscreen);
    SetWindowLongWHelper(hWnd, GWL_EXSTYLE, exWindowStyle);

    // Resize the window, with SWP_FRAMECHANGED, to trigger user32 to
    // recalculate the non/client areas
    const til::rect windowPos{ GetWindowRect() };

    SetWindowPos(GetHandle(),
                 HWND_TOP,
                 windowPos.left,
                 windowPos.top,
                 windowPos.width(),
                 windowPos.height(),
                 SWP_SHOWWINDOW | SWP_FRAMECHANGED | SWP_NOACTIVATE);
}

// Method Description:
// - Called when entering fullscreen, with the window's current monitor rect and work area.
// - The current window position, dpi, work area, and maximized state are stored, and the
//   window is positioned to the monitor rect.
void IslandWindow::_SetFullscreenPosition(const RECT& rcMonitor, const RECT& rcWork)
{
    const auto hWnd = GetHandle();

    ::GetWindowRect(hWnd, &_rcWindowBeforeFullscreen);
    _dpiBeforeFullscreen = GetDpiForWindow(hWnd);
    _fWasMaximizedBeforeFullscreen = IsZoomed(hWnd);
    _rcWorkBeforeFullscreen = rcWork;

    SetWindowPos(hWnd,
                 HWND_TOP,
                 rcMonitor.left,
                 rcMonitor.top,
                 rcMonitor.right - rcMonitor.left,
                 rcMonitor.bottom - rcMonitor.top,
                 SWP_FRAMECHANGED);
}

// Method Description:
// - Called when exiting fullscreen, with the window's current monitor work area.
// - The window is restored to its previous position, migrating that previous position to the
//   window's current monitor (if the current work area or window DPI have changed).
// - A fullscreen window's monitor can be changed by win+shift+left/right hotkeys or monitor
//   topology changes (for example unplugging a monitor or disconnecting a remote session).
void IslandWindow::_RestoreFullscreenPosition(const RECT& rcWork)
{
    const auto hWnd = GetHandle();

    // If the window was previously maximized, re-maximize the window.
    if (_fWasMaximizedBeforeFullscreen)
    {
        ShowWindow(hWnd, SW_SHOWMAXIMIZED);
        SetWindowPos(hWnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

        return;
    }

    // Start with the stored window position.
    auto rcRestore = _rcWindowBeforeFullscreen;

    // If the window DPI has changed, re-size the stored position by the change in DPI. This
    // ensures the window restores to the same logical size (even if to a monitor with a different
    // DPI/ scale factor).
    auto dpiWindow = GetDpiForWindow(hWnd);
    rcRestore.right = rcRestore.left + MulDiv(rcRestore.right - rcRestore.left, dpiWindow, _dpiBeforeFullscreen);
    rcRestore.bottom = rcRestore.top + MulDiv(rcRestore.bottom - rcRestore.top, dpiWindow, _dpiBeforeFullscreen);

    // Offset the stored position by the difference in work area.
    OffsetRect(&rcRestore,
               rcWork.left - _rcWorkBeforeFullscreen.left,
               rcWork.top - _rcWorkBeforeFullscreen.top);

    const til::size ncSize{ GetTotalNonClientExclusiveSize(dpiWindow) };

    auto rcWorkAdjusted = rcWork;

    // GH#10199 - adjust the size of the "work" rect by the size of our borders.
    // We want to make sure the window is restored within the bounds of the
    // monitor we're on, but it's totally fine if the invisible borders are
    // outside the monitor.
    const auto halfWidth{ ncSize.width / 2 };
    const auto halfHeight{ ncSize.height / 2 };

    rcWorkAdjusted.left -= halfWidth;
    rcWorkAdjusted.right += halfWidth;
    rcWorkAdjusted.top -= halfHeight;
    rcWorkAdjusted.bottom += halfHeight;

    // Enforce that our position is entirely within the bounds of our work area.
    // Prefer the top-left be on-screen rather than bottom-right (right before left, bottom before top).
    if (rcRestore.right > rcWorkAdjusted.right)
    {
        OffsetRect(&rcRestore, rcWorkAdjusted.right - rcRestore.right, 0);
    }
    if (rcRestore.left < rcWorkAdjusted.left)
    {
        OffsetRect(&rcRestore, rcWorkAdjusted.left - rcRestore.left, 0);
    }
    if (rcRestore.bottom > rcWorkAdjusted.bottom)
    {
        OffsetRect(&rcRestore, 0, rcWorkAdjusted.bottom - rcRestore.bottom);
    }
    if (rcRestore.top < rcWorkAdjusted.top)
    {
        OffsetRect(&rcRestore, 0, rcWorkAdjusted.top - rcRestore.top);
    }

    // Show the window at the computed position.
    SetWindowPos(hWnd,
                 HWND_TOP,
                 rcRestore.left,
                 rcRestore.top,
                 rcRestore.right - rcRestore.left,
                 rcRestore.bottom - rcRestore.top,
                 SWP_SHOWWINDOW | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
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
    const auto fChangingFullscreen = (fullscreenEnabled != _fullscreen);
    _fullscreen = fullscreenEnabled;

    const auto hWnd = GetHandle();

    // First, modify regular window styles as appropriate
    auto windowStyle = _getDesiredWindowStyle();
    SetWindowLongWHelper(hWnd, GWL_STYLE, windowStyle);

    // Now modify extended window styles as appropriate
    // When moving to fullscreen, remove the window edge style to avoid an
    // ugly border when not focused.
    auto exWindowStyle = GetWindowLongW(hWnd, GWL_EXSTYLE);
    WI_UpdateFlag(exWindowStyle, WS_EX_WINDOWEDGE, !_fullscreen);
    SetWindowLongWHelper(hWnd, GWL_EXSTYLE, exWindowStyle);

    // Only change the window position if changing fullscreen state.
    if (fChangingFullscreen)
    {
        // Get the monitor info for the window's current monitor.
        MONITORINFO mi = {};
        mi.cbSize = sizeof(mi);
        GetMonitorInfo(MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST), &mi);

        if (_fullscreen)
        {
            // Store the window's current position and size the window to the monitor.
            _SetFullscreenPosition(mi.rcMonitor, mi.rcWork);
        }
        else
        {
            // Restore the stored window position.
            _RestoreFullscreenPosition(mi.rcWork);
        }
    }
}

// Method Description:
// - Summon the window, or possibly dismiss it. If toggleVisibility is true,
//   then we'll dismiss (minimize) the window if it's currently active.
//   Otherwise, we'll always just try to activate the window.
// Arguments:
// - toggleVisibility: controls how we should behave when already in the foreground.
// Return Value:
// - <none>
winrt::fire_and_forget IslandWindow::SummonWindow(Remoting::SummonWindowBehavior args)
{
    // On the foreground thread:
    co_await wil::resume_foreground(_rootGrid.Dispatcher());
    _summonWindowRoutineBody(args);
}

// Method Description:
// - As above.
//   BODGY: ARM64 BUILD FAILED WITH fatal error C1001: Internal compiler error
//   when this was part of the coroutine body.
void IslandWindow::_summonWindowRoutineBody(Remoting::SummonWindowBehavior args)
{
    auto actualDropdownDuration = args.DropdownDuration();
    // If the user requested an animation, let's check if animations are enabled in the OS.
    if (actualDropdownDuration > 0)
    {
        auto animationsEnabled = TRUE;
        SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &animationsEnabled, 0);
        if (!animationsEnabled)
        {
            // The OS has animations disabled - we should respect that and
            // disable the animation here.
            //
            // We're doing this here, rather than in _doSlideAnimation, to
            // preempt any other specific behavior that
            // _globalActivateWindow/_globalDismissWindow might do if they think
            // there should be an animation (like making the window appear with
            // SetWindowPlacement rather than ShowWindow)
            actualDropdownDuration = 0;
        }
    }

    // * If the user doesn't want to toggleVisibility, then just always try to
    //   activate.
    // * If the user does want to toggleVisibility,
    //   - If we're the foreground window, ToMonitor == ToMouse, and the mouse is on the monitor we are
    //      - activate the window
    //   - else
    //      - dismiss the window
    if (args.ToggleVisibility() && GetForegroundWindow() == _window.get())
    {
        auto handled = false;

        // They want to toggle the window when it is the FG window, and we are
        // the FG window. However, if we're on a different monitor than the
        // mouse, then we should move to that monitor instead of dismissing.
        if (args.ToMonitor() == Remoting::MonitorBehavior::ToMouse)
        {
            const til::rect cursorMonitorRect{ _getMonitorForCursor().rcMonitor };
            const til::rect currentMonitorRect{ _getMonitorForWindow(GetHandle()).rcMonitor };
            if (cursorMonitorRect != currentMonitorRect)
            {
                // We're not on the same monitor as the mouse. Go to that monitor.
                _globalActivateWindow(actualDropdownDuration, args.ToMonitor());
                handled = true;
            }
        }

        if (!handled)
        {
            _globalDismissWindow(actualDropdownDuration);
        }
    }
    else
    {
        _globalActivateWindow(actualDropdownDuration, args.ToMonitor());
    }
}

// Method Description:
// - Helper for performing a sliding animation. This will animate our _Xaml
//   Island_, either growing down or shrinking up, using SetWindowRgn.
// - This function does the entire animation on the main thread (the UI thread),
//   and **DOES NOT YIELD IT**. The window will be animating for the entire
//   duration of dropdownDuration.
// - At the end of the animation, we'll reset the window region, so that it's as
//   if nothing occurred.
// Arguments:
// - dropdownDuration: The duration to play the animation, in milliseconds. If
//   0, we won't perform a dropdown animation.
// - down: if true, increase the height from top to bottom. otherwise, decrease
//   the height, from bottom to top.
// Return Value:
// - <none>
void IslandWindow::_doSlideAnimation(const uint32_t dropdownDuration, const bool down)
{
    til::rect fullWindowSize{ GetWindowRect() };
    const auto fullHeight = fullWindowSize.height();

    const double animationDuration = dropdownDuration; // use floating-point math throughout
    const auto start = std::chrono::system_clock::now();

    // Do at most dropdownDuration frames. After that, just bail straight to the
    // final state.
    for (uint32_t i = 0; i < dropdownDuration; i++)
    {
        const auto end = std::chrono::system_clock::now();
        const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        const auto dt = ::base::saturated_cast<double>(millis.count());

        if (dt > animationDuration)
        {
            break;
        }

        // If going down, increase the height over time. If going up, decrease the height.
        const auto currentHeight = ::base::saturated_cast<int>(
            down ? ((dt / animationDuration) * fullHeight) :
                   ((1.0 - (dt / animationDuration)) * fullHeight));

        wil::unique_hrgn rgn{ CreateRectRgn(0, 0, fullWindowSize.width(), currentHeight) };
        SetWindowRgn(_interopWindowHandle, rgn.get(), true);

        // Go immediately into another frame. This prevents the window from
        // doing anything else (tearing our state). A Sleep() here will cause a
        // weird stutter, and causes the animation to not be as smooth.
    }

    // Reset the window.
    SetWindowRgn(_interopWindowHandle, nullptr, true);
}

void IslandWindow::_dropdownWindow(const uint32_t dropdownDuration,
                                   const Remoting::MonitorBehavior toMonitor)
{
    // First, get the window that's currently in the foreground. We'll need
    // _this_ window to be able to appear on top of. If we just use
    // GetForegroundWindow after the SetWindowPlacement call, _we_ will be the
    // foreground window.
    const auto oldForegroundWindow = GetForegroundWindow();

    // First, restore the window. SetWindowPlacement has a fun undocumented
    // piece of functionality where it will restore the window position
    // _without_ the animation, so use that instead of ShowWindow(SW_RESTORE).
    WINDOWPLACEMENT wpc{};
    wpc.length = sizeof(WINDOWPLACEMENT);
    GetWindowPlacement(_window.get(), &wpc);

    // If the window is hidden, SW_SHOW it first.
    if (!IsWindowVisible(GetHandle()))
    {
        wpc.showCmd = SW_SHOW;
        SetWindowPlacement(_window.get(), &wpc);
    }
    wpc.showCmd = SW_RESTORE;
    SetWindowPlacement(_window.get(), &wpc);

    // Possibly go to the monitor of the mouse / old foreground window.
    _moveToMonitor(oldForegroundWindow, toMonitor);

    // Now that we're visible, animate the dropdown.
    _doSlideAnimation(dropdownDuration, true);
}

void IslandWindow::_slideUpWindow(const uint32_t dropdownDuration)
{
    // First, animate the window sliding up.
    _doSlideAnimation(dropdownDuration, false);

    // Then, use SetWindowPlacement to minimize without the animation.
    WINDOWPLACEMENT wpc{};
    wpc.length = sizeof(WINDOWPLACEMENT);
    GetWindowPlacement(_window.get(), &wpc);
    wpc.showCmd = SW_MINIMIZE;
    SetWindowPlacement(_window.get(), &wpc);
}

// Method Description:
// - Force activate this window. This method will bring us to the foreground and
//   activate us. If the window is minimized, it will restore the window. If the
//   window is on another desktop, the OS will switch to that desktop.
// - If the window is minimized, and dropdownDuration is greater than 0, we'll
//   perform a "slide in" animation. We won't do this if the window is already
//   on the screen (since that seems silly).
// Arguments:
// - dropdownDuration: The duration to play the dropdown animation, in
//   milliseconds. If 0, we won't perform a dropdown animation.
// Return Value:
// - <none>
void IslandWindow::_globalActivateWindow(const uint32_t dropdownDuration,
                                         const Remoting::MonitorBehavior toMonitor)
{
    // First, get the window that's currently in the foreground. We'll need
    // _this_ window to be able to appear on top of. If we just use
    // GetForegroundWindow after the SetWindowPlacement/ShowWindow call, _we_
    // will be the foreground window.
    const auto oldForegroundWindow = GetForegroundWindow();

    // From: https://stackoverflow.com/a/59659421
    // > The trick is to make windows ‘think’ that our process and the target
    // > window (hwnd) are related by attaching the threads (using
    // > AttachThreadInput API) and using an alternative API: BringWindowToTop.
    // If the window is minimized, then restore it. We don't want to do this
    // always though, because if you SW_RESTORE a maximized window, it will
    // restore-down the window.
    if (IsIconic(_window.get()))
    {
        if (dropdownDuration > 0)
        {
            _dropdownWindow(dropdownDuration, toMonitor);
        }
        else
        {
            // If the window is hidden, SW_SHOW it first. Note that hidden !=
            // minimized. A hidden window doesn't appear in the taskbar, while a
            // minimized window will. If you don't do this, then we won't be
            // able to properly set this as the foreground window.
            if (!IsWindowVisible(GetHandle()))
            {
                ShowWindow(_window.get(), SW_SHOW);
            }
            ShowWindow(_window.get(), SW_RESTORE);

            // Once we've been restored, throw us on the active monitor.
            _moveToMonitor(oldForegroundWindow, toMonitor);
        }
    }
    else
    {
        // Try first to send a message to the current foreground window. If it's not responding, it may
        // be waiting on us to finish launching. Passing SMTO_NOTIMEOUTIFNOTHUNG means that we get the same
        // behavior as before--that is, waiting for the message loop--but we've done an early return if
        // it turns out that it was hung.
        // SendMessageTimeoutW returns nonzero if it succeeds.
        if (0 != SendMessageTimeoutW(oldForegroundWindow, WM_NULL, 0, 0, SMTO_NOTIMEOUTIFNOTHUNG | SMTO_BLOCK | SMTO_ABORTIFHUNG, 1000, nullptr))
        {
            const auto windowThreadProcessId = GetWindowThreadProcessId(oldForegroundWindow, nullptr);
            const auto currentThreadId = GetCurrentThreadId();

            LOG_IF_WIN32_BOOL_FALSE(AttachThreadInput(windowThreadProcessId, currentThreadId, true));
            // Just in case, add the thread detach as a scope_exit, to make _sure_ we do it.
            auto detachThread = wil::scope_exit([windowThreadProcessId, currentThreadId]() {
                LOG_IF_WIN32_BOOL_FALSE(AttachThreadInput(windowThreadProcessId, currentThreadId, false));
            });
            LOG_IF_WIN32_BOOL_FALSE(BringWindowToTop(_window.get()));
            ShowWindow(_window.get(), SW_SHOW);

            // Activate the window too. This will force us to the virtual desktop this
            // window is on, if it's on another virtual desktop.
            LOG_LAST_ERROR_IF_NULL(SetActiveWindow(_window.get()));

            // Throw us on the active monitor.
            _moveToMonitor(oldForegroundWindow, toMonitor);
        }
    }
}

// Method Description:
// - Minimize the window. This is called when the window is summoned, but is
//   already active
// - If dropdownDuration is greater than 0, we'll perform a "slide in"
//   animation, before minimizing the window.
// Arguments:
// - dropdownDuration: The duration to play the slide-up animation, in
//   milliseconds. If 0, we won't perform a slide-up animation.
// Return Value:
// - <none>
void IslandWindow::_globalDismissWindow(const uint32_t dropdownDuration)
{
    if (dropdownDuration > 0)
    {
        _slideUpWindow(dropdownDuration);
    }
    else
    {
        ShowWindow(_window.get(), SW_MINIMIZE);
    }
}

// Method Description:
// - Get the monitor the mouse cursor is currently on
// Arguments:
// - dropdownDuration: The duration to play the slide-up animation, in
//   milliseconds. If 0, we won't perform a slide-up animation.
// Return Value:
// - The MONITORINFO for the monitor the mouse cursor is on
MONITORINFO IslandWindow::_getMonitorForCursor()
{
    POINT p{};
    GetCursorPos(&p);

    // Get the monitor info for the window's current monitor.
    MONITORINFO activeMonitor{};
    activeMonitor.cbSize = sizeof(activeMonitor);
    GetMonitorInfo(MonitorFromPoint(p, MONITOR_DEFAULTTONEAREST), &activeMonitor);
    return activeMonitor;
}

// Method Description:
// - Get the monitor for a given HWND
// Arguments:
// - <none>
// Return Value:
// - The MONITORINFO for the given HWND
MONITORINFO IslandWindow::_getMonitorForWindow(HWND foregroundWindow)
{
    // Get the monitor info for the window's current monitor.
    MONITORINFO activeMonitor{};
    activeMonitor.cbSize = sizeof(activeMonitor);
    GetMonitorInfo(MonitorFromWindow(foregroundWindow, MONITOR_DEFAULTTONEAREST), &activeMonitor);
    return activeMonitor;
}

// Method Description:
// - Based on the value in toMonitor, move the window to the monitor of the
//   given HWND, the monitor of the mouse pointer, or just leave it where it is.
// Arguments:
// - oldForegroundWindow: when toMonitor is ToCurrent, we'll move to the monitor
//   of this HWND. Otherwise, this param is ignored.
// - toMonitor: Controls which monitor we should move to.
// Return Value:
// - <none>
void IslandWindow::_moveToMonitor(HWND oldForegroundWindow, Remoting::MonitorBehavior toMonitor)
{
    if (toMonitor == Remoting::MonitorBehavior::ToCurrent)
    {
        _moveToMonitorOf(oldForegroundWindow);
    }
    else if (toMonitor == Remoting::MonitorBehavior::ToMouse)
    {
        _moveToMonitorOfMouse();
    }
}

// Method Description:
// - Move our window to the monitor the mouse is on.
// Arguments:
// - <none>
// Return Value:
// - <none>
void IslandWindow::_moveToMonitorOfMouse()
{
    _moveToMonitor(_getMonitorForCursor());
}

// Method Description:
// - Move our window to the monitor that the given HWND is on.
// Arguments:
// - <none>
// Return Value:
// - <none>
void IslandWindow::_moveToMonitorOf(HWND foregroundWindow)
{
    _moveToMonitor(_getMonitorForWindow(foregroundWindow));
}

// Method Description:
// - Move our window to the given monitor. This will do nothing if we're already
//   on that monitor.
// - We'll retain the same relative position on the new monitor as we had on the
//   old monitor.
// Arguments:
// - activeMonitor: the monitor to move to.
// Return Value:
// - <none>
void IslandWindow::_moveToMonitor(const MONITORINFO activeMonitor)
{
    // Get the monitor info for the window's current monitor.
    const auto currentMonitor = _getMonitorForWindow(GetHandle());

    const til::rect currentRect{ currentMonitor.rcMonitor };
    const til::rect activeRect{ activeMonitor.rcMonitor };
    if (currentRect != activeRect)
    {
        const til::rect currentWindowRect{ GetWindowRect() };
        const auto offset{ currentWindowRect.origin() - currentRect.origin() };
        const auto newOrigin{ activeRect.origin() + offset };

        SetWindowPos(GetHandle(),
                     0,
                     newOrigin.x,
                     newOrigin.y,
                     currentWindowRect.width(),
                     currentWindowRect.height(),
                     SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);

        // GH#10274, GH#10182: Re-evaluate the size of the quake window when we
        // move to another monitor.
        if (IsQuakeWindow())
        {
            _enterQuakeMode();
        }
    }
}

bool IslandWindow::IsQuakeWindow() const noexcept
{
    return _isQuakeWindow;
}

void IslandWindow::IsQuakeWindow(bool isQuakeWindow) noexcept
{
    if (_isQuakeWindow != isQuakeWindow)
    {
        _isQuakeWindow = isQuakeWindow;
        // Don't enter quake mode if we don't have an HWND yet
        if (IsQuakeWindow() && _window)
        {
            _enterQuakeMode();
        }
    }
}

void IslandWindow::SetAutoHideWindow(bool autoHideWindow) noexcept
{
    _autoHideWindow = autoHideWindow;
}

// Method Description:
// - Enter quake mode for the monitor this window is currently on. This involves
//   resizing it to the top half of the monitor.
// Arguments:
// - <none>
// Return Value:
// - <none>
void IslandWindow::_enterQuakeMode()
{
    if (!_window)
    {
        return;
    }

    auto windowRect = GetWindowRect();
    auto hmon = MonitorFromRect(&windowRect, MONITOR_DEFAULTTONEAREST);

    // Get the size and position of the window that we should occupy
    const auto newRect{ _getQuakeModeSize(hmon) };

    SetWindowPos(GetHandle(),
                 HWND_TOP,
                 newRect.left,
                 newRect.top,
                 newRect.width(),
                 newRect.height(),
                 SWP_SHOWWINDOW | SWP_FRAMECHANGED | SWP_NOACTIVATE);
}

// Method Description:
// - Get the size and position of the window that a "quake mode" should occupy
//   on the given monitor.
// - The window will occupy the top half of the monitor.
// Arguments:
// - <none>
// Return Value:
// - <none>
til::rect IslandWindow::_getQuakeModeSize(HMONITOR hmon)
{
    MONITORINFO nearestMonitorInfo;

    UINT dpix = USER_DEFAULT_SCREEN_DPI;
    UINT dpiy = USER_DEFAULT_SCREEN_DPI;
    // If this fails, we'll use the default of 96. I think it can only fail for
    // bad parameters, which we won't have, so no big deal.
    LOG_IF_FAILED(GetDpiForMonitor(hmon, MDT_EFFECTIVE_DPI, &dpix, &dpiy));

    nearestMonitorInfo.cbSize = sizeof(MONITORINFO);
    // Get monitor dimensions:
    GetMonitorInfo(hmon, &nearestMonitorInfo);
    const til::size desktopDimensions{ (nearestMonitorInfo.rcWork.right - nearestMonitorInfo.rcWork.left),
                                       (nearestMonitorInfo.rcWork.bottom - nearestMonitorInfo.rcWork.top) };

    // If we just use rcWork by itself, we'll fail to account for the invisible
    // space reserved for the resize handles. So retrieve that size here.
    const til::size ncSize{ GetTotalNonClientExclusiveSize(dpix) };
    const auto availableSpace = desktopDimensions + ncSize;

    // GH#10201 - The borders are still visible in quake mode, so make us 1px
    // smaller on either side to account for that, so they don't hang onto
    // adjacent monitors.
    const til::point origin{
        ::base::ClampSub(nearestMonitorInfo.rcWork.left, (ncSize.width / 2)) + 1,
        (nearestMonitorInfo.rcWork.top)
    };
    const til::size dimensions{
        availableSpace.width - 2,
        availableSpace.height / 2
    };

    return { origin, dimensions };
}

void IslandWindow::HideWindow()
{
    ShowWindow(GetHandle(), SW_HIDE);
}

void IslandWindow::SetMinimizeToNotificationAreaBehavior(bool MinimizeToNotificationArea) noexcept
{
    _minimizeToNotificationArea = MinimizeToNotificationArea;
}

// Method Description:
// - Opens the window's system menu.
// - The system menu is the menu that opens when the user presses Alt+Space or
//   right clicks on the title bar.
// - Before updating the menu, we update the buttons like "Maximize" and
//   "Restore" so that they are grayed out depending on the window's state.
// Arguments:
// - cursorX: the cursor's X position in screen coordinates
// - cursorY: the cursor's Y position in screen coordinates
void IslandWindow::OpenSystemMenu(const std::optional<int> mouseX, const std::optional<int> mouseY) const noexcept
{
    const auto systemMenu = GetSystemMenu(_window.get(), FALSE);

    WINDOWPLACEMENT placement;
    if (!GetWindowPlacement(_window.get(), &placement))
    {
        return;
    }
    const auto isMaximized = placement.showCmd == SW_SHOWMAXIMIZED;

    // Update the options based on window state.
    MENUITEMINFO mii;
    mii.cbSize = sizeof(MENUITEMINFO);
    mii.fMask = MIIM_STATE;
    mii.fType = MFT_STRING;
    auto setState = [&](UINT item, bool enabled) {
        mii.fState = enabled ? MF_ENABLED : MF_DISABLED;
        SetMenuItemInfo(systemMenu, item, FALSE, &mii);
    };
    setState(SC_RESTORE, isMaximized);
    setState(SC_MOVE, !isMaximized);
    setState(SC_SIZE, !isMaximized);
    setState(SC_MINIMIZE, true);
    setState(SC_MAXIMIZE, !isMaximized);
    setState(SC_CLOSE, true);
    SetMenuDefaultItem(systemMenu, UINT_MAX, FALSE);

    int xPos;
    int yPos;
    if (mouseX && mouseY)
    {
        xPos = mouseX.value();
        yPos = mouseY.value();
    }
    else
    {
        RECT windowPos;
        ::GetWindowRect(GetHandle(), &windowPos);
        xPos = windowPos.left;
        yPos = windowPos.top;
    }
    const auto ret = TrackPopupMenu(systemMenu, TPM_RETURNCMD, xPos, yPos, 0, _window.get(), nullptr);
    if (ret != 0)
    {
        PostMessage(_window.get(), WM_SYSCOMMAND, ret, 0);
    }
}

void IslandWindow::AddToSystemMenu(const winrt::hstring& itemLabel, winrt::delegate<void()> callback)
{
    const auto systemMenu = GetSystemMenu(_window.get(), FALSE);
    auto wID = _systemMenuNextItemId;

    MENUITEMINFOW item;
    item.cbSize = sizeof(MENUITEMINFOW);
    item.fMask = MIIM_STATE | MIIM_ID | MIIM_STRING;
    item.fState = MF_ENABLED;
    item.wID = wID;
    item.dwTypeData = const_cast<LPWSTR>(itemLabel.c_str());
    item.cch = static_cast<UINT>(itemLabel.size());

    if (LOG_LAST_ERROR_IF(!InsertMenuItemW(systemMenu, wID, FALSE, &item)))
    {
        return;
    }
    _systemMenuItems.insert({ wID, { itemLabel, callback } });
    _systemMenuNextItemId++;
}

void IslandWindow::RemoveFromSystemMenu(const winrt::hstring& itemLabel)
{
    const auto systemMenu = GetSystemMenu(_window.get(), FALSE);
    auto itemCount = GetMenuItemCount(systemMenu);
    if (LOG_LAST_ERROR_IF(itemCount == -1))
    {
        return;
    }

    auto it = std::find_if(_systemMenuItems.begin(), _systemMenuItems.end(), [&itemLabel](const std::pair<UINT, SystemMenuItemInfo>& elem) {
        return elem.second.label == itemLabel;
    });
    if (it == _systemMenuItems.end())
    {
        return;
    }

    if (LOG_LAST_ERROR_IF(!DeleteMenu(systemMenu, it->first, MF_BYCOMMAND)))
    {
        return;
    }
    _systemMenuItems.erase(it->first);
}

void IslandWindow::UseDarkTheme(const bool v)
{
    const BOOL attribute = v ? TRUE : FALSE;
    std::ignore = DwmSetWindowAttribute(GetHandle(), DWMWA_USE_IMMERSIVE_DARK_MODE, &attribute, sizeof(attribute));
}

void IslandWindow::UseMica(const bool newValue, const double /*titlebarOpacity*/)
{
    // This block of code enables Mica for our window. By all accounts, this
    // version of the code will only work on Windows 11, SV2. There's a slightly
    // different API surface for enabling Mica on Windows 11 22000.0.
    //
    // This API was only publicly supported as of Windows 11 SV2, 22621. Before
    // that version, this API will just return an error and do nothing silently.

    const int attribute = newValue ? DWMSBT_MAINWINDOW : DWMSBT_NONE;
    std::ignore = DwmSetWindowAttribute(GetHandle(), DWMWA_SYSTEMBACKDROP_TYPE, &attribute, sizeof(attribute));
}

// Method Description:
// - This method is called when the window receives the WM_NCCREATE message.
// Return Value:
// - The value returned from the window proc.
[[nodiscard]] LRESULT IslandWindow::OnNcCreate(WPARAM wParam, LPARAM lParam) noexcept
{
    const auto ret = BaseWindow::OnNcCreate(wParam, lParam);
    if (!ret)
    {
        return FALSE;
    }

    // This is a hack to make the window borders dark instead of light.
    // It must be done before WM_NCPAINT so that the borders are rendered with
    // the correct theme.
    // For more information, see GH#6620.
    //
    // Theoretically, we don't need this anymore, since _updateTheme will update
    // the darkness of our window. However, we're keeping this call to prevent
    // the window from appearing as a white rectangle for a frame before we load
    // the rest of the settings.
    UseDarkTheme(true);

    return TRUE;
}
