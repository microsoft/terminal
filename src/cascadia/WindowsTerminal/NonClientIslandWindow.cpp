/********************************************************
*                                                       *
*   Copyright (C) Microsoft. All rights reserved.       *
*                                                       *
********************************************************/
#include "pch.h"
#include "NonClientIslandWindow.h"

#include <dwmapi.h>
#include <uxtheme.h>

#include "../types/inc/utils.hpp"

using namespace winrt::Windows::UI;
using namespace winrt::Windows::UI::Composition;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Hosting;
using namespace winrt::Windows::Foundation::Numerics;
using namespace ::Microsoft::Console;

static constexpr int AutohideTaskbarSize = 2;

NonClientIslandWindow::NonClientIslandWindow(const ElementTheme& requestedTheme) noexcept :
    IslandWindow{},
    _backgroundBrushColor{ 0, 0, 0 },
    _theme{ requestedTheme },
    _isMaximized{ false }
{
}

NonClientIslandWindow::~NonClientIslandWindow()
{
    Close();
}

void NonClientIslandWindow::Close()
{
    // Avoid further callbacks into XAML/WinUI-land after we've Close()d the DesktopWindowXamlSource
    // inside `IslandWindow::Close()`. XAML thanks us for doing that by not crashing. Thank you XAML.
    SetWindowLongPtr(_dragBarWindow.get(), GWLP_USERDATA, 0);
    IslandWindow::Close();
}

static constexpr const wchar_t* dragBarClassName{ L"DRAG_BAR_WINDOW_CLASS" };

[[nodiscard]] LRESULT __stdcall NonClientIslandWindow::_StaticInputSinkWndProc(HWND const window, UINT const message, WPARAM const wparam, LPARAM const lparam) noexcept
{
    WINRT_ASSERT(window);

    if (WM_NCCREATE == message)
    {
        auto cs = reinterpret_cast<CREATESTRUCT*>(lparam);
        auto nonClientIslandWindow{ reinterpret_cast<NonClientIslandWindow*>(cs->lpCreateParams) };
        SetWindowLongPtr(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(nonClientIslandWindow));
        // fall through to default window procedure
    }
    else if (auto nonClientIslandWindow{ reinterpret_cast<NonClientIslandWindow*>(GetWindowLongPtr(window, GWLP_USERDATA)) })
    {
        return nonClientIslandWindow->_InputSinkMessageHandler(message, wparam, lparam);
    }

    return DefWindowProc(window, message, wparam, lparam);
}

void NonClientIslandWindow::MakeWindow() noexcept
{
    if (_window)
    {
        // no-op if we already have a window.
        return;
    }

    IslandWindow::MakeWindow();

    static auto dragBarWindowClass{ []() {
        WNDCLASSEX wcEx{};
        wcEx.cbSize = sizeof(wcEx);
        wcEx.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
        wcEx.lpszClassName = dragBarClassName;
        wcEx.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
        wcEx.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wcEx.lpfnWndProc = &NonClientIslandWindow::_StaticInputSinkWndProc;
        wcEx.hInstance = wil::GetModuleInstanceHandle();
        wcEx.cbWndExtra = sizeof(NonClientIslandWindow*);
        return RegisterClassEx(&wcEx);
    }() };

    // The drag bar window is a child window of the top level window that is put
    // right on top of the drag bar. The XAML island window "steals" our mouse
    // messages which makes it hard to implement a custom drag area. By putting
    // a window on top of it, we prevent it from "stealing" the mouse messages.
    _dragBarWindow.reset(CreateWindowExW(WS_EX_LAYERED | WS_EX_NOREDIRECTIONBITMAP,
                                         dragBarClassName,
                                         L"",
                                         WS_CHILD,
                                         0,
                                         0,
                                         0,
                                         0,
                                         GetHandle(),
                                         nullptr,
                                         wil::GetModuleInstanceHandle(),
                                         this));
    THROW_HR_IF_NULL(E_UNEXPECTED, _dragBarWindow);
}

LRESULT NonClientIslandWindow::_dragBarNcHitTest(const til::point pointer)
{
    auto rcParent = GetWindowRect();
    // The size of the buttons doesn't change over the life of the application.
    const auto buttonWidthInDips{ _titlebar.CaptionButtonWidth() };

    // However, the DPI scaling might, so get the updated size of the buttons in pixels
    const auto buttonWidthInPixels{ buttonWidthInDips * GetCurrentDpiScale() };

    // make sure to account for the width of the window frame!
    const til::rect nonClientFrame{ GetNonClientFrame(_currentDpi) };
    const auto rightBorder{ rcParent.right - nonClientFrame.right };
    // From the right to the left,
    // * are we in the close button?
    // * the maximize button?
    // * the minimize button?
    // If we're not, then we're in either the top resize border, or just
    // generally in the titlebar.
    if ((rightBorder - pointer.x) < (buttonWidthInPixels))
    {
        return HTCLOSE;
    }
    else if ((rightBorder - pointer.x) < (buttonWidthInPixels * 2))
    {
        return HTMAXBUTTON;
    }
    else if ((rightBorder - pointer.x) < (buttonWidthInPixels * 3))
    {
        return HTMINBUTTON;
    }
    else
    {
        // If we're not on a caption button, then check if we're on the top
        // border. If we're not on the top border, then we're just generally in
        // the caption area.
        const auto resizeBorderHeight = _GetResizeHandleHeight();
        const auto isOnResizeBorder = pointer.y < rcParent.top + resizeBorderHeight;

        return isOnResizeBorder ? HTTOP : HTCAPTION;
    }
}

// Function Description:
// - The window procedure for the drag bar forwards clicks on its client area to
//   its parent as non-client clicks.
// - BODGY: It also _manually_ handles the caption buttons. They exist in the
//   titlebar, and work reasonably well with just XAML, if the drag bar isn't
//   covering them.
// - However, to get snap layout support (GH#9443), we need to actually return
//   HTMAXBUTTON where the maximize button is. If the drag bar doesn't cover the
//   caption buttons, then the core input site (which takes up the entirety of
//   the XAML island) will steal the WM_NCHITTEST before we get a chance to
//   handle it.
// - So, the drag bar covers the caption buttons, and manually handles hovering
//   and pressing them when needed. This gives the impression that they're
//   getting input as they normally would, even if they're not _really_ getting
//   input via XAML.
LRESULT NonClientIslandWindow::_InputSinkMessageHandler(UINT const message,
                                                        WPARAM const wparam,
                                                        LPARAM const lparam) noexcept
{
    switch (message)
    {
    case WM_NCHITTEST:
    {
        // Try to determine what part of the window is being hovered here. This
        // is absolutely critical to making sure Snap Layouts (GH#9443) works!
        return _dragBarNcHitTest({ GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) });
    }
    break;

    case WM_NCMOUSEMOVE:
        // When we get this message, it's because the mouse moved when it was
        // over somewhere we said was the non-client area.
        //
        // We'll use this to communicate state to the title bar control, so that
        // it can update its visuals.
        // - If we're over a button, hover it.
        // - If we're over _anything else_, stop hovering the buttons.
        switch (wparam)
        {
        case HTTOP:
        case HTCAPTION:
        {
            _titlebar.ReleaseButtons();

            // Pass caption-related nonclient messages to the parent window.
            // Make sure to do this for the HTTOP, which is the top resize
            // border, so we can resize the window on the top.
            auto parentWindow{ GetHandle() };
            return SendMessage(parentWindow, message, wparam, lparam);
        }
        case HTMINBUTTON:
        case HTMAXBUTTON:
        case HTCLOSE:
            _titlebar.HoverButton(static_cast<winrt::TerminalApp::CaptionButton>(wparam));
            break;
        default:
            _titlebar.ReleaseButtons();
        }

        // If we haven't previously asked for mouse tracking, request mouse
        // tracking. We need to do this so we can get the WM_NCMOUSELEAVE
        // message when the mouse leave the titlebar. Otherwise, we won't always
        // get that message (especially if the user moves the mouse _real
        // fast_).
        if (!_trackingMouse &&
            (wparam == HTMINBUTTON || wparam == HTMAXBUTTON || wparam == HTCLOSE))
        {
            TRACKMOUSEEVENT ev{};
            ev.cbSize = sizeof(TRACKMOUSEEVENT);
            // TME_NONCLIENT is absolutely critical here. In my experimentation,
            // we'd get WM_MOUSELEAVE messages after just a HOVER_DEFAULT
            // timeout even though we're not requesting TME_HOVER, which kinda
            // ruined the whole point of this.
            ev.dwFlags = TME_LEAVE | TME_NONCLIENT;
            ev.hwndTrack = _dragBarWindow.get();
            ev.dwHoverTime = HOVER_DEFAULT; // we don't _really_ care about this.
            LOG_IF_WIN32_BOOL_FALSE(TrackMouseEvent(&ev));
            _trackingMouse = true;
        }
        break;

    case WM_NCMOUSELEAVE:
    case WM_MOUSELEAVE:
        // When the mouse leaves the drag rect, make sure to dismiss any hover.
        _titlebar.ReleaseButtons();
        _trackingMouse = false;
        break;

    // NB: *Shouldn't be forwarding these* when they're not over the caption
    // because they can inadvertently take action using the system's default
    // metrics instead of our own.
    case WM_NCLBUTTONDOWN:
    case WM_NCLBUTTONDBLCLK:
        // Manual handling for mouse clicks in the drag bar. If it's in a
        // caption button, then tell the titlebar to "press" the button, which
        // should change its visual state.
        //
        // If it's not in a caption button, then just forward the message along
        // to the root HWND. Make sure to do this for the HTTOP, which is the
        // top resize border.
        switch (wparam)
        {
        case HTTOP:
        case HTCAPTION:
        {
            // Pass caption-related nonclient messages to the parent window.
            auto parentWindow{ GetHandle() };
            return SendMessage(parentWindow, message, wparam, lparam);
        }
        // The buttons won't work as you'd expect; we need to handle those
        // ourselves.
        case HTMINBUTTON:
        case HTMAXBUTTON:
        case HTCLOSE:
            _titlebar.PressButton(static_cast<winrt::TerminalApp::CaptionButton>(wparam));
            break;
        }
        return 0;

    case WM_NCLBUTTONUP:
        // Manual handling for mouse RELEASES in the drag bar. If it's in a
        // caption button, then manually handle what we'd expect for that button.
        //
        // If it's not in a caption button, then just forward the message along
        // to the root HWND.
        switch (wparam)
        {
        case HTTOP:
        case HTCAPTION:
        {
            // Pass caption-related nonclient messages to the parent window.
            // The buttons won't work as you'd expect; we need to handle those ourselves.
            auto parentWindow{ GetHandle() };
            return SendMessage(parentWindow, message, wparam, lparam);
        }
        break;

        // If we do find a button, then tell the titlebar to raise the same
        // event that would be raised if it were "tapped"
        case HTMINBUTTON:
        case HTMAXBUTTON:
        case HTCLOSE:
            _titlebar.ReleaseButtons();
            _titlebar.ClickButton(static_cast<winrt::TerminalApp::CaptionButton>(wparam));
            break;
        }
        return 0;

    // Make sure to pass along right-clicks in this region to our parent window
    // - we don't need to handle these.
    case WM_NCRBUTTONDOWN:
    case WM_NCRBUTTONDBLCLK:
    case WM_NCRBUTTONUP:
        auto parentWindow{ GetHandle() };
        return SendMessage(parentWindow, message, wparam, lparam);
    }

    return DefWindowProc(_dragBarWindow.get(), message, wparam, lparam);
}

// Method Description:
// - Resizes and shows/hides the drag bar input sink window.
//   This window is used to capture clicks on the non-client area.
void NonClientIslandWindow::_ResizeDragBarWindow() noexcept
{
    const til::rect rect{ _GetDragAreaRect() };
    if (_IsTitlebarVisible() && rect.size().area() > 0)
    {
        SetWindowPos(_dragBarWindow.get(),
                     HWND_TOP,
                     rect.left,
                     rect.top + _GetTopBorderHeight(),
                     rect.width(),
                     rect.height(),
                     SWP_NOACTIVATE | SWP_SHOWWINDOW);
        SetLayeredWindowAttributes(_dragBarWindow.get(), 0, 255, LWA_ALPHA);
    }
    else
    {
        SetWindowPos(_dragBarWindow.get(), HWND_BOTTOM, 0, 0, 0, 0, SWP_HIDEWINDOW | SWP_NOMOVE | SWP_NOSIZE);
    }
}

// Method Description:
// - Called when the app's size changes. When that happens, the size of the drag
//   bar may have changed. If it has, we'll need to update the WindowRgn of the
//   interop window.
// Arguments:
// - <unused>
// Return Value:
// - <none>
void NonClientIslandWindow::_OnDragBarSizeChanged(winrt::Windows::Foundation::IInspectable /*sender*/,
                                                  winrt::Windows::UI::Xaml::SizeChangedEventArgs /*eventArgs*/)
{
    _ResizeDragBarWindow();
}

void NonClientIslandWindow::OnAppInitialized()
{
    IslandWindow::OnAppInitialized();
}

void NonClientIslandWindow::Initialize()
{
    IslandWindow::Initialize();

    _UpdateFrameMargins();

    // Set up our grid of content. We'll use _rootGrid as our root element.
    // There will be two children of this grid - the TitlebarControl, and the
    // "client content"
    _rootGrid.Children().Clear();
    Controls::RowDefinition titlebarRow{};
    Controls::RowDefinition contentRow{};
    titlebarRow.Height(GridLengthHelper::Auto());

    _rootGrid.RowDefinitions().Clear();
    _rootGrid.RowDefinitions().Append(titlebarRow);
    _rootGrid.RowDefinitions().Append(contentRow);

    // Create our titlebar control
    _titlebar = winrt::TerminalApp::TitlebarControl{ reinterpret_cast<uint64_t>(GetHandle()) };
    _dragBar = _titlebar.DragBar();

    _callbacks.dragBarSizeChanged = _dragBar.SizeChanged(winrt::auto_revoke, { this, &NonClientIslandWindow::_OnDragBarSizeChanged });
    _callbacks.rootGridSizeChanged = _rootGrid.SizeChanged(winrt::auto_revoke, { this, &NonClientIslandWindow::_OnDragBarSizeChanged });

    _rootGrid.Children().Append(_titlebar);

    Controls::Grid::SetRow(_titlebar, 0);

    // GH#3440 - When the titlebar is loaded (officially added to our UI tree),
    // then make sure to update its visual state to reflect if we're in the
    // maximized state on launch.
    _callbacks.titlebarLoaded = _titlebar.Loaded(winrt::auto_revoke, [this](auto&&, auto&&) { _OnMaximizeChange(); });

    // LOAD BEARING: call _ResizeDragBarWindow to update the position of our
    // XAML island to reflect our current bounds. In the case of a "warm init"
    // (i.e. re-using an existing window), we need to manually update the
    // island's position to fill the new window bounds.
    _ResizeDragBarWindow();
}

// Method Description:
// - Set the content of the "client area" of our window to the given content.
// Arguments:
// - content: the new UI element to use as the client content
// Return Value:
// - <none>
void NonClientIslandWindow::SetContent(winrt::Windows::UI::Xaml::UIElement content)
{
    _rootGrid.Children().Append(content);

    // SetRow only works on FrameworkElement's, so cast it to a FWE before
    // calling. We know that our content is a Grid, so we don't need to worry
    // about this.
    const auto fwe = content.try_as<winrt::Windows::UI::Xaml::FrameworkElement>();
    if (fwe)
    {
        Controls::Grid::SetRow(fwe, 1);
    }
}

// Method Description:
// - Set the content of the "titlebar area" of our window to the given content.
// Arguments:
// - content: the new UI element to use as the titlebar content
// Return Value:
// - <none>
void NonClientIslandWindow::SetTitlebarContent(winrt::Windows::UI::Xaml::UIElement content)
{
    _titlebar.Content(content);

    // GH#4288 - add a SizeChanged handler to this content. It's possible that
    // this element's size will change after the dragbar's. When that happens,
    // the drag bar won't send another SizeChanged event, because the dragbar's
    // _size_ didn't change, only its position.
    const auto fwe = content.try_as<winrt::Windows::UI::Xaml::FrameworkElement>();
    if (fwe)
    {
        fwe.SizeChanged({ this, &NonClientIslandWindow::_OnDragBarSizeChanged });
    }
}

// Method Description:
// - This method computes the height of the little border above the title bar
//   and returns it. If the border is disabled, then this method will return 0.
// Return Value:
// - the height of the border above the title bar or 0 if it's disabled
int NonClientIslandWindow::_GetTopBorderHeight() const noexcept
{
    // No border when maximized or fullscreen.
    // Yet we still need it in the focus mode to allow dragging (GH#7012)
    if (_isMaximized || _fullscreen)
    {
        return 0;
    }

    return topBorderVisibleHeight;
}

til::rect NonClientIslandWindow::_GetDragAreaRect() const noexcept
{
    if (_dragBar && _dragBar.Visibility() == Visibility::Visible)
    {
        const auto scale = GetCurrentDpiScale();
        const auto transform = _dragBar.TransformToVisual(_rootGrid);

        // GH#9443: Previously, we'd only extend the drag bar from the left of
        // the tabs to the right of the caption buttons. Now, we're extending it
        // all the way to the right side of the window, covering the caption
        // buttons. We'll manually handle input to those buttons, to make it
        // seem like they're still getting XAML input. We do this so we can get
        // snap layout support for the maximize button.
        const auto logicalDragBarRect = winrt::Windows::Foundation::Rect{
            0.0f,
            0.0f,
            static_cast<float>(_rootGrid.ActualWidth()),
            static_cast<float>(_dragBar.ActualHeight())
        };

        const auto clientDragBarRect = transform.TransformBounds(logicalDragBarRect);

        // Make sure to trim the right side of the rectangle, so that it doesn't
        // hang off the right side of the root window. This normally wouldn't
        // matter, but UIA will still think its bounds can extend past the right
        // of the parent HWND.
        //
        // x here is the width of the tabs.
        const auto x = gsl::narrow_cast<til::CoordType>(clientDragBarRect.X * scale);

        return {
            x,
            gsl::narrow_cast<til::CoordType>(clientDragBarRect.Y * scale),
            gsl::narrow_cast<til::CoordType>((clientDragBarRect.Width + clientDragBarRect.X) * scale) - x,
            gsl::narrow_cast<til::CoordType>((clientDragBarRect.Height + clientDragBarRect.Y) * scale),
        };
    }

    return {};
}

// Method Description:
// - Called when the size of the window changes for any reason. Updates the
//   XAML island to match our new sizing and also updates the maximize icon
//   if the window went from maximized to restored or the opposite.
void NonClientIslandWindow::OnSize(const UINT width, const UINT height)
{
    _UpdateMaximizedState();

    if (_interopWindowHandle)
    {
        _UpdateIslandPosition(width, height);
    }

    // GH#11367: We need to do this,
    // otherwise the titlebar may still be partially visible
    // when we move between different DPI monitors.
    RefreshCurrentDPI();
    _UpdateFrameMargins();
}

// Method Description:
// - Checks if the window has been maximized or restored since the last time.
//   If it has been maximized or restored, then it updates the _isMaximized
//   flags and notifies of the change by calling
//   NonClientIslandWindow::_OnMaximizeChange.
void NonClientIslandWindow::_UpdateMaximizedState()
{
    const auto windowStyle = GetWindowStyle(_window.get());
    const auto newIsMaximized = WI_IsFlagSet(windowStyle, WS_MAXIMIZE);

    if (_isMaximized != newIsMaximized)
    {
        _isMaximized = newIsMaximized;
        _OnMaximizeChange();
    }
}

// Method Description:
// - Called when the windows goes from restored to maximized or from
//   maximized to restored. Updates the maximize button's icon and the frame
//   margins.
void NonClientIslandWindow::_OnMaximizeChange() noexcept
{
    if (_titlebar)
    {
        const auto windowStyle = GetWindowStyle(_window.get());
        const auto isIconified = WI_IsFlagSet(windowStyle, WS_ICONIC);

        const auto state = _isMaximized ? winrt::TerminalApp::WindowVisualState::WindowVisualStateMaximized :
                           isIconified  ? winrt::TerminalApp::WindowVisualState::WindowVisualStateIconified :
                                          winrt::TerminalApp::WindowVisualState::WindowVisualStateNormal;

        try
        {
            _titlebar.SetWindowVisualState(state);
        }
        CATCH_LOG();
    }

    // no frame margin when maximized
    _UpdateFrameMargins();
}

// Method Description:
// - Called when the size of the window changes for any reason. Updates the
//   sizes of our child XAML Islands to match our new sizing.
void NonClientIslandWindow::_UpdateIslandPosition(const UINT windowWidth, const UINT windowHeight)
{
    const auto originalTopHeight = _GetTopBorderHeight();
    // GH#7422
    // !! BODGY !!
    //
    // For inexplicable reasons, the top row of pixels on our tabs, new tab
    // button, and caption buttons is totally un-clickable. The mouse simply
    // refuses to interact with them. So when we're maximized, on certain
    // monitor configurations, this results in the top row of pixels not
    // reacting to clicks at all. To obey Fitt's Law, we're gonna shift
    // the entire island up one pixel. That will result in the top row of pixels
    // in the window actually being the _second_ row of pixels for those
    // buttons, which will make them clickable. It's perhaps not the right fix,
    // but it works.
    // _GetTopBorderHeight() returns 0 when we're maximized.
    const auto topBorderHeight = (originalTopHeight == 0) ? -1 : originalTopHeight;

    const til::point newIslandPos = { 0, topBorderHeight };

    winrt::check_bool(SetWindowPos(_interopWindowHandle,
                                   HWND_BOTTOM,
                                   newIslandPos.x,
                                   newIslandPos.y,
                                   windowWidth,
                                   windowHeight - topBorderHeight,
                                   SWP_SHOWWINDOW | SWP_NOACTIVATE));

    // This happens when we go from maximized to restored or the opposite
    // because topBorderHeight changes.
    if (!_oldIslandPos.has_value() || _oldIslandPos.value() != newIslandPos)
    {
        // The drag bar's position changed compared to the client area because
        // the island moved but we will not be notified about this in the
        // NonClientIslandWindow::OnDragBarSizeChanged method because this
        // method is only called when the position of the drag bar changes
        // **inside** the island which is not the case here.
        _ResizeDragBarWindow();

        _oldIslandPos = { newIslandPos };
    }
}

// Method Description:
// - Returns the height of the little space at the top of the window used to
//   resize the window.
// Return Value:
// - the height of the window's top resize handle
int NonClientIslandWindow::_GetResizeHandleHeight() const noexcept
{
    // there isn't a SM_CYPADDEDBORDER for the Y axis
    return ::GetSystemMetricsForDpi(SM_CXPADDEDBORDER, _currentDpi) +
           ::GetSystemMetricsForDpi(SM_CYSIZEFRAME, _currentDpi);
}

// Method Description:
// - Responds to the WM_NCCALCSIZE message by calculating and creating the new
//   window frame.
[[nodiscard]] LRESULT NonClientIslandWindow::_OnNcCalcSize(const WPARAM wParam, const LPARAM lParam) noexcept
{
    if (!wParam)
    {
        return 0;
    }

    auto params = reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam);

    // Store the original top before the default window proc applies the
    // default frame.
    const auto originalTop = params->rgrc[0].top;

    const auto originalSize = params->rgrc[0];

    // apply the default frame
    const auto ret = DefWindowProc(_window.get(), WM_NCCALCSIZE, wParam, lParam);
    if (ret != 0)
    {
        return ret;
    }

    auto newSize = params->rgrc[0];
    // Re-apply the original top from before the size of the default frame was applied.
    newSize.top = originalTop;

    // WM_NCCALCSIZE is called before WM_SIZE
    _UpdateMaximizedState();

    // We don't need this correction when we're fullscreen. We will have the
    // WS_POPUP size, so we don't have to worry about borders, and the default
    // frame will be fine.
    if (_isMaximized && !_fullscreen)
    {
        // When a window is maximized, its size is actually a little bit more
        // than the monitor's work area. The window is positioned and sized in
        // such a way that the resize handles are outside of the monitor and
        // then the window is clipped to the monitor so that the resize handle
        // do not appear because you don't need them (because you can't resize
        // a window when it's maximized unless you restore it).
        newSize.top += _GetResizeHandleHeight();
    }

    // GH#1438 - Attempt to detect if there's an autohide taskbar, and if there
    // is, reduce our size a bit on the side with the taskbar, so the user can
    // still mouse-over the taskbar to reveal it.
    // GH#5209 - make sure to use MONITOR_DEFAULTTONEAREST, so that this will
    // still find the right monitor even when we're restoring from minimized.
    auto hMon = MonitorFromWindow(_window.get(), MONITOR_DEFAULTTONEAREST);
    if (hMon && (_isMaximized || _fullscreen))
    {
        MONITORINFO monInfo{ 0 };
        monInfo.cbSize = sizeof(MONITORINFO);
        GetMonitorInfo(hMon, &monInfo);

        // First, check if we have an auto-hide taskbar at all:
        APPBARDATA autohide{ 0 };
        autohide.cbSize = sizeof(autohide);
        auto state = (UINT)SHAppBarMessage(ABM_GETSTATE, &autohide);
        if (WI_IsFlagSet(state, ABS_AUTOHIDE))
        {
            // This helper can be used to determine if there's a auto-hide
            // taskbar on the given edge of the monitor we're currently on.
            auto hasAutohideTaskbar = [&monInfo](const UINT edge) -> bool {
                APPBARDATA data{ 0 };
                data.cbSize = sizeof(data);
                data.uEdge = edge;
                data.rc = monInfo.rcMonitor;
                auto hTaskbar = (HWND)SHAppBarMessage(ABM_GETAUTOHIDEBAREX, &data);
                return hTaskbar != nullptr;
            };

            const auto onTop = hasAutohideTaskbar(ABE_TOP);
            const auto onBottom = hasAutohideTaskbar(ABE_BOTTOM);
            const auto onLeft = hasAutohideTaskbar(ABE_LEFT);
            const auto onRight = hasAutohideTaskbar(ABE_RIGHT);

            // If there's a taskbar on any side of the monitor, reduce our size
            // a little bit on that edge.
            //
            // Note to future code archeologists:
            // This doesn't seem to work for fullscreen on the primary display.
            // However, testing a bunch of other apps with fullscreen modes
            // and an auto-hiding taskbar has shown that _none_ of them
            // reveal the taskbar from fullscreen mode. This includes Edge,
            // Firefox, Chrome, Sublime Text, PowerPoint - none seemed to
            // support this.
            //
            // This does however work fine for maximized.
            if (onTop)
            {
                // Peculiarly, when we're fullscreen,
                newSize.top += AutohideTaskbarSize;
            }
            if (onBottom)
            {
                newSize.bottom -= AutohideTaskbarSize;
            }
            if (onLeft)
            {
                newSize.left += AutohideTaskbarSize;
            }
            if (onRight)
            {
                newSize.right -= AutohideTaskbarSize;
            }
        }
    }

    params->rgrc[0] = newSize;

    return 0;
}

// Method Description:
// - Hit test the frame for resizing and moving.
// Arguments:
// - ptMouse: the mouse point being tested, in absolute (NOT WINDOW) coordinates.
// Return Value:
// - one of the values from
//   https://docs.microsoft.com/en-us/windows/desktop/inputdev/wm-nchittest#return-value
//   corresponding to the area of the window that was hit
[[nodiscard]] LRESULT NonClientIslandWindow::_OnNcHitTest(POINT ptMouse) const noexcept
{
    // This will handle the left, right and bottom parts of the frame because
    // we didn't change them.
    LPARAM lParam = MAKELONG(ptMouse.x, ptMouse.y);
    const auto originalRet = DefWindowProc(_window.get(), WM_NCHITTEST, 0, lParam);

    if (originalRet != HTCLIENT)
    {
        // If we're the quake window, suppress resizing on any side except the
        // bottom. I don't believe that this actually works on the top. That's
        // handled below.
        if (IsQuakeWindow())
        {
            switch (originalRet)
            {
            case HTBOTTOMRIGHT:
            case HTRIGHT:
            case HTTOPRIGHT:
            case HTTOP:
            case HTTOPLEFT:
            case HTLEFT:
            case HTBOTTOMLEFT:
                return HTCLIENT;
            }
        }
        return originalRet;
    }

    // At this point, we know that the cursor is inside the client area so it
    // has to be either the little border at the top of our custom title bar,
    // the drag bar or something else in the XAML island. But the XAML Island
    // handles WM_NCHITTEST on its own so actually it cannot be the XAML
    // Island. Then it must be the drag bar or the little border at the top
    // which the user can use to move or resize the window.

    RECT rcWindow;
    winrt::check_bool(::GetWindowRect(_window.get(), &rcWindow));

    const auto resizeBorderHeight = _GetResizeHandleHeight();
    const auto isOnResizeBorder = ptMouse.y < rcWindow.top + resizeBorderHeight;

    // the top of the drag bar is used to resize the window
    if (!_isMaximized && isOnResizeBorder)
    {
        // However, if we're the quake window, then just return HTCAPTION so we
        // don't get a resize handle on the top.
        return IsQuakeWindow() ? HTCAPTION : HTTOP;
    }

    return HTCAPTION;
}

// Method Description:
// - Sets the cursor to the sizing cursor when we hit-test the top sizing border.
//   We need to do this because we've covered it up with a child window.
[[nodiscard]] LRESULT NonClientIslandWindow::_OnSetCursor(WPARAM wParam, LPARAM lParam) const noexcept
{
    if (LOWORD(lParam) == HTCLIENT)
    {
        // Get the cursor position from the _last message_ and not from
        // `GetCursorPos` (which returns the cursor position _at the
        // moment_) because if we're lagging behind the cursor's position,
        // we still want to get the cursor position that was associated
        // with that message at the time it was sent to handle the message
        // correctly.
        const auto screenPtLparam{ GetMessagePos() };
        const auto hitTest{ SendMessage(GetHandle(), WM_NCHITTEST, 0, screenPtLparam) };
        if (hitTest == HTTOP)
        {
            // We have to set the vertical resize cursor manually on
            // the top resize handle because Windows thinks that the
            // cursor is on the client area because it asked the asked
            // the drag window with `WM_NCHITTEST` and it returned
            // `HTCLIENT`.
            // We don't want to modify the drag window's `WM_NCHITTEST`
            // handling to return `HTTOP` because otherwise, the system
            // would resize the drag window instead of the top level
            // window!
            SetCursor(LoadCursor(nullptr, IDC_SIZENS));
            return TRUE;
        }
        else
        {
            // reset cursor
            SetCursor(LoadCursor(nullptr, IDC_ARROW));
            return TRUE;
        }
    }

    return DefWindowProc(GetHandle(), WM_SETCURSOR, wParam, lParam);
}
// Method Description:
// - Get the dimensions of our non-client area, as a rect where each component
//   represents that side.
// - The .left will be a negative number, to represent that the actual side of
//   the non-client area is outside the border of our window. It's roughly 8px (
//   * DPI scaling) to the left of the visible border.
// - The .right component will be positive, indicating that the nonclient border
//   is in the positive-x direction from the edge of our client area.
// - This DOES NOT include our titlebar! It's in the client area for us.
// Arguments:
// - dpi: the scaling that we should use to calculate the border sizes.
// Return Value:
// - a til::rect whose components represent the margins of the nonclient area,
//   relative to the client area.
til::rect NonClientIslandWindow::GetNonClientFrame(UINT dpi) const noexcept
{
    const auto windowStyle = static_cast<DWORD>(GetWindowLong(_window.get(), GWL_STYLE));
    til::rect islandFrame;

    // If we failed to get the correct window size for whatever reason, log
    // the error and go on. We'll use whatever the control proposed as the
    // size of our window, which will be at least close.
    LOG_IF_WIN32_BOOL_FALSE(AdjustWindowRectExForDpi(islandFrame.as_win32_rect(), windowStyle, false, 0, dpi));

    islandFrame.top = -topBorderVisibleHeight;
    return islandFrame;
}

// Method Description:
// - Gets the difference between window and client area size.
// Arguments:
// - dpi: dpi of a monitor on which the window is placed
// Return Value
// - The size difference
til::size NonClientIslandWindow::GetTotalNonClientExclusiveSize(UINT dpi) const noexcept
{
    const auto islandFrame{ GetNonClientFrame(dpi) };
    const auto scale = GetCurrentDpiScale();

    // If we have a titlebar, this is being called after we've initialized, and
    // we can just ask that titlebar how big it wants to be.
    const auto titleBarHeight = _titlebar ? static_cast<LONG>(_titlebar.ActualHeight()) * scale : 0;

    return {
        islandFrame.right - islandFrame.left,
        islandFrame.bottom - islandFrame.top + static_cast<til::CoordType>(titleBarHeight)
    };
}

// Method Description:
// - Updates the borders of our window frame, using DwmExtendFrameIntoClientArea.
// Arguments:
// - <none>
// Return Value:
// - the HRESULT returned by DwmExtendFrameIntoClientArea.
void NonClientIslandWindow::_UpdateFrameMargins() const noexcept
{
    MARGINS margins = { 0, 0, 0, 0 };

    // GH#603: When we're in Focus Mode, hide the titlebar, by setting it to a single
    // pixel tall. Otherwise, the titlebar will be visible underneath controls with
    // vintage opacity set.
    //
    // We can't set it to all 0's unfortunately.
    if (_borderless)
    {
        margins.cyTopHeight = 1;
    }
    else if (_GetTopBorderHeight() != 0)
    {
        RECT frame = {};
        winrt::check_bool(::AdjustWindowRectExForDpi(&frame, GetWindowStyle(_window.get()), FALSE, 0, _currentDpi));

        // We removed the whole top part of the frame (see handling of
        // WM_NCCALCSIZE) so the top border is missing now. We add it back here.
        // Note #1: You might wonder why we don't remove just the title bar instead
        //  of removing the whole top part of the frame and then adding the little
        //  top border back. I tried to do this but it didn't work: DWM drew the
        //  whole title bar anyways on top of the window. It seems that DWM only
        //  wants to draw either nothing or the whole top part of the frame.
        // Note #2: For some reason if you try to set the top margin to just the
        //  top border height (what we want to do), then there is a transparency
        //  bug when the window is inactive, so I've decided to add the whole top
        //  part of the frame instead and then we will hide everything that we
        //  don't need (that is, the whole thing but the little 1 pixel wide border
        //  at the top) in the WM_PAINT handler. This eliminates the transparency
        //  bug and it's what a lot of Win32 apps that customize the title bar do
        //  so it should work fine.
        //
        // Notes #3 (circa late 2022): We want to make some changes here to
        // support Mica. This introduces some complications.
        // - If we leave the titlebar visible AT ALL, then a transparent
        //   titlebar (theme.tabRow.background:#ff00ff00 for example) will allow
        //   the DWM titlebar to be visible, underneath our content. EVEN MORE
        //   SO: Mica + "show accent color on title bars" will _always_ show the
        //   accent-colored strip of the titlebar, even on top of the Mica.
        // - It _seems_ like we can just set this to 0, and have it work. You'd
        //   be wrong. On Windows 10, setting this to 0 will cause the topmost
        //   pixel of our window to be just a little darker than the rest of the
        //   frame. So ONLY set this to 0 when the user has explicitly asked for
        //   Mica. Though it won't do anything on Windows 10, they should be
        //   able to opt back out of having that weird dark pixel.
        // - This is LOAD-BEARING. By having the titlebar a totally empty rect,
        //   DWM will know that we don't have the traditional titlebar, and will
        //   use NCHITTEST to determine where to place the Snap Flyout. The drag
        //   rect will handle that.

        margins.cyTopHeight = (_micaStyle != winrt::Microsoft::Terminal::Settings::Model::MicaStyle::Default || _titlebarOpacity < 1.0) ? 0 : -frame.top;
    }

    // Extend the frame into the client area. microsoft/terminal#2735 - Just log
    // the failure here, don't crash. If DWM crashes for any reason, calling
    // THROW_IF_FAILED() will cause us to take a trip upstate. Just log, and
    // we'll fix ourselves when DWM comes back.
    LOG_IF_FAILED(DwmExtendFrameIntoClientArea(_window.get(), &margins));
}

// Method Description:
// - Handle window messages from the message loop.
// Arguments:
// - message: A window message ID identifying the message.
// - wParam: The contents of this parameter depend on the value of the message parameter.
// - lParam: The contents of this parameter depend on the value of the message parameter.
// Return Value:
// - The return value is the result of the message processing and depends on the
//   message sent.
[[nodiscard]] LRESULT NonClientIslandWindow::MessageHandler(UINT const message,
                                                            WPARAM const wParam,
                                                            LPARAM const lParam) noexcept
{
    switch (message)
    {
    case WM_SETCURSOR:
        return _OnSetCursor(wParam, lParam);
    case WM_DISPLAYCHANGE:
        // GH#4166: When the DPI of the monitor changes out from underneath us,
        // resize our drag bar, to reflect its newly scaled size.
        _ResizeDragBarWindow();
        return 0;
    case WM_NCCALCSIZE:
        return _OnNcCalcSize(wParam, lParam);
    case WM_NCHITTEST:
        return _OnNcHitTest({ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) });
    case WM_PAINT:
        return _OnPaint();
    case WM_NCRBUTTONUP:
        // The `DefWindowProc` function doesn't open the system menu for some
        // reason so we have to do it ourselves.
        if (wParam == HTCAPTION)
        {
            OpenSystemMenu(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        }
        break;
    }

    return IslandWindow::MessageHandler(message, wParam, lParam);
}

// Method Description:
// - This method is called when the window receives the WM_PAINT message.
// - It paints the client area with the color of the title bar to hide the
//   system's title bar behind the XAML Islands window during a resize.
//   Indeed, the XAML Islands window doesn't resize at the same time than
//   the top level window
//   (see https://github.com/microsoft/microsoft-ui-xaml/issues/759).
// Return Value:
// - The value returned from the window proc.
[[nodiscard]] LRESULT NonClientIslandWindow::_OnPaint() noexcept
{
    if (!_titlebar)
    {
        return 0;
    }

    PAINTSTRUCT ps{ 0 };
    const auto hdc = wil::BeginPaint(_window.get(), &ps);
    if (!hdc)
    {
        return 0;
    }

    const auto topBorderHeight = _GetTopBorderHeight();

    if (ps.rcPaint.top < topBorderHeight)
    {
        auto rcTopBorder = ps.rcPaint;
        rcTopBorder.bottom = topBorderHeight;

        // To show the original top border, we have to paint on top of it with
        // the alpha component set to 0. This page recommends to paint the area
        // in black using the stock BLACK_BRUSH to do this:
        // https://docs.microsoft.com/en-us/windows/win32/dwm/customframe#extending-the-client-frame
        ::FillRect(hdc.get(), &rcTopBorder, GetStockBrush(BLACK_BRUSH));
    }

    if (ps.rcPaint.bottom > topBorderHeight)
    {
        auto rcRest = ps.rcPaint;
        rcRest.top = topBorderHeight;

        const auto backgroundBrush = _titlebar.Background();
        const auto backgroundSolidBrush = backgroundBrush.try_as<Media::SolidColorBrush>();
        const auto backgroundAcrylicBrush = backgroundBrush.try_as<Media::AcrylicBrush>();

        til::color backgroundColor = Colors::Black();
        if (backgroundSolidBrush)
        {
            backgroundColor = backgroundSolidBrush.Color();
        }
        else if (backgroundAcrylicBrush)
        {
            backgroundColor = backgroundAcrylicBrush.FallbackColor();
        }

        if (!_backgroundBrush || backgroundColor != _backgroundBrushColor)
        {
            // Create brush for titlebar color.
            _backgroundBrush = wil::unique_hbrush(CreateSolidBrush(backgroundColor));
        }

        // To hide the original title bar, we have to paint on top of it with
        // the alpha component set to 255. This is a hack to do it with GDI.
        // See NonClientIslandWindow::_UpdateFrameMargins for more information.
        HDC opaqueDc;
        BP_PAINTPARAMS params = { sizeof(params), BPPF_NOCLIP | BPPF_ERASE };
        auto buf = BeginBufferedPaint(hdc.get(), &rcRest, BPBF_TOPDOWNDIB, &params, &opaqueDc);
        if (!buf || !opaqueDc)
        {
            // MSFT:34673647 - BeginBufferedPaint can fail, but it probably
            // shouldn't bring the whole Terminal down with it. So don't
            // throw_last_error here.
            LOG_LAST_ERROR();
            return 0;
        }

        ::FillRect(opaqueDc, &rcRest, _backgroundBrush.get());
        ::BufferedPaintSetAlpha(buf, nullptr, 255);
        ::EndBufferedPaint(buf, TRUE);
    }

    return 0;
}

// Method Description:
// - Called when the app wants to change its theme. We'll update the frame
//   theme to match the new theme.
// Arguments:
// - requestedTheme: the ElementTheme to use as the new theme for the UI
// Return Value:
// - <none>
void NonClientIslandWindow::OnApplicationThemeChanged(const ElementTheme& requestedTheme)
{
    IslandWindow::OnApplicationThemeChanged(requestedTheme);

    _theme = requestedTheme;
}

// Method Description:
// - Enable or disable borderless mode. When entering borderless mode, we'll
//   need to manually hide the entire titlebar.
// - See also IslandWindow::_SetIsBorderless, which does similar, but different work.
// Arguments:
// - borderlessEnabled: If true, we're entering borderless mode. If false, we're leaving.
// Return Value:
// - <none>
void NonClientIslandWindow::_SetIsBorderless(const bool borderlessEnabled)
{
    _borderless = borderlessEnabled;

    // Explicitly _don't_ call IslandWindow::_SetIsBorderless. That version will
    // change the window styles appropriately for the window with the default
    // titlebar, but for the tabs-in-titlebar mode, we can just get rid of the
    // title bar entirely.

    if (_titlebar)
    {
        _titlebar.Visibility(_IsTitlebarVisible() ? Visibility::Visible : Visibility::Collapsed);
    }

    // Update the margins when entering/leaving focus mode, so we can prevent
    // the titlebar from showing through transparent terminal controls
    _UpdateFrameMargins();

    // GH#4224 - When the auto-hide taskbar setting is enabled, then we don't
    // always get another window message to trigger us to remove the drag bar.
    // So, make sure to update the size of the drag region here, so that it
    // _definitely_ goes away.
    _ResizeDragBarWindow();

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
// - Enable or disable fullscreen mode. When entering fullscreen mode, we'll
//   need to check whether to hide the titlebar.
// - See also IslandWindow::_SetIsFullscreen, which does additional work.
// Arguments:
// - fullscreenEnabled: If true, we're entering fullscreen mode. If false, we're leaving.
// Return Value:
// - <none>
void NonClientIslandWindow::_SetIsFullscreen(const bool fullscreenEnabled)
{
    IslandWindow::_SetIsFullscreen(fullscreenEnabled);
    _UpdateTitlebarVisibility();
    // GH#4224 - When the auto-hide taskbar setting is enabled, then we don't
    // always get another window message to trigger us to remove the drag bar.
    // So, make sure to update the size of the drag region here, so that it
    // _definitely_ goes away.
    _ResizeDragBarWindow();
}

void NonClientIslandWindow::SetShowTabsFullscreen(const bool newShowTabsFullscreen)
{
    IslandWindow::SetShowTabsFullscreen(newShowTabsFullscreen);

    // don't waste time recalculating UI elements if we're not
    // in fullscreen state - this setting doesn't affect other
    // window states
    if (_fullscreen)
    {
        _UpdateTitlebarVisibility();
    }
}

void NonClientIslandWindow::_UpdateTitlebarVisibility()
{
    if (!_titlebar)
    {
        return;
    }

    const auto showTitlebar = _IsTitlebarVisible();
    _titlebar.Visibility(showTitlebar ? Visibility::Visible : Visibility::Collapsed);
    _titlebar.FullscreenChanged(_fullscreen);
}

// Method Description:
// - Returns true if the titlebar is visible. For borderless mode (aka "focus mode"),
//   this will return false. For fullscreen, this will return false unless the user
//   has enabled fullscreen tabs.
// Arguments:
// - <none>
// Return Value:
// - true iff the titlebar is visible
bool NonClientIslandWindow::_IsTitlebarVisible() const
{
    return !_borderless && (!_fullscreen || _showTabsFullscreen);
}

void NonClientIslandWindow::SetTitlebarBackground(winrt::Windows::UI::Xaml::Media::Brush brush)
{
    _titlebar.Background(brush);
}

void NonClientIslandWindow::SetMicaStyle(const winrt::Microsoft::Terminal::Settings::Model::MicaStyle newValue, const double titlebarOpacity)
{
    // Stash internally if we're using Mica. If we aren't, we don't want to
    // totally blow away our titlebar with DwmExtendFrameIntoClientArea,
    // especially on Windows 10
    _micaStyle = newValue;
    _titlebarOpacity = titlebarOpacity;

    IslandWindow::SetMicaStyle(newValue, titlebarOpacity);

    _UpdateFrameMargins();
}
