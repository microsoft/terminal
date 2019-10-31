/********************************************************
*                                                       *
*   Copyright (C) Microsoft. All rights reserved.       *
*                                                       *
********************************************************/
#include "pch.h"
#include "NonClientIslandWindow.h"

extern "C" IMAGE_DOS_HEADER __ImageBase;

using namespace winrt::Windows::UI;
using namespace winrt::Windows::UI::Composition;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Hosting;
using namespace winrt::Windows::Foundation::Numerics;
using namespace ::Microsoft::Console::Types;

constexpr int RECT_WIDTH(const RECT* const pRect)
{
    return pRect->right - pRect->left;
}
constexpr int RECT_HEIGHT(const RECT* const pRect)
{
    return pRect->bottom - pRect->top;
}

NonClientIslandWindow::NonClientIslandWindow() noexcept :
    IslandWindow{},
    _isMaximized{ false }
{
}

NonClientIslandWindow::~NonClientIslandWindow()
{
}

// Method Description:
// - Called when the app's size changes. When that happens, the size of the drag
//   bar may have changed. If it has, we'll need to update the WindowRgn of the
//   interop window.
// Arguments:
// - <unused>
// Return Value:
// - <none>
void NonClientIslandWindow::OnDragBarSizeChanged(winrt::Windows::Foundation::IInspectable /*sender*/,
                                                 winrt::Windows::UI::Xaml::SizeChangedEventArgs /*eventArgs*/)
{
    _UpdateDragRegion();
}

void NonClientIslandWindow::OnAppInitialized()
{
    IslandWindow::OnAppInitialized();
}

void NonClientIslandWindow::Initialize()
{
    IslandWindow::Initialize();

    // Set up our grid of content. We'll use _rootGrid as our root element.
    // There will be two children of this grid - the TitlebarControl, and the
    // "client content"
    _rootGrid.Children().Clear();
    Controls::RowDefinition titlebarRow{};
    Controls::RowDefinition contentRow{};
    titlebarRow.Height(GridLengthHelper::Auto());

    _rootGrid.RowDefinitions().Append(titlebarRow);
    _rootGrid.RowDefinitions().Append(contentRow);

    // Create our titlebar control
    _titlebar = winrt::TerminalApp::TitlebarControl{ reinterpret_cast<uint64_t>(GetHandle()) };
    _dragBar = _titlebar.DragBar();

    _dragBar.SizeChanged({ this, &NonClientIslandWindow::OnDragBarSizeChanged });
    _rootGrid.SizeChanged({ this, &NonClientIslandWindow::OnDragBarSizeChanged });

    _rootGrid.Children().Append(_titlebar);

    Controls::Grid::SetRow(_titlebar, 0);
}

// Method Description:
// - Set the content of the "client area" of our window to the given content.
// Arguments:
// - content: the new UI element to use as the client content
// Return Value:
// - <none>
void NonClientIslandWindow::SetContent(winrt::Windows::UI::Xaml::UIElement content)
{
    _clientContent = content;

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
}

int NonClientIslandWindow::_GetTopBorderHeight()
{
    WINDOWPLACEMENT placement;
    winrt::check_bool(::GetWindowPlacement(_window.get(), &placement));
    bool isMaximized = placement.showCmd == SW_MAXIMIZE;
    if (isMaximized)
    {
        // no border when maximized
        return 0;
    }

    return static_cast<int>(1 * GetCurrentDpiScale());
}

RECT NonClientIslandWindow::GetDragAreaRect() const noexcept
{
    if (_dragBar)
    {
        const auto scale = GetCurrentDpiScale();
        const auto transform = _dragBar.TransformToVisual(_rootGrid);
        const auto logicalDragBarRect = winrt::Windows::Foundation::Rect{
            0.0f,
            0.0f,
            static_cast<float>(_dragBar.ActualWidth()),
            static_cast<float>(_dragBar.ActualHeight())
        };
        const auto clientDragBarRect = transform.TransformBounds(logicalDragBarRect);
        RECT dragBarRect = {
            static_cast<LONG>(clientDragBarRect.X * scale),
            static_cast<LONG>(clientDragBarRect.Y * scale),
            static_cast<LONG>((clientDragBarRect.Width + clientDragBarRect.X) * scale),
            static_cast<LONG>((clientDragBarRect.Height + clientDragBarRect.Y) * scale),
        };
        return dragBarRect;
    }

    return RECT{};
}

// Method Description:
// - called when the size of the window changes for any reason. Updates the
//   sizes of our child Xaml Islands to match our new sizing.
void NonClientIslandWindow::OnSize(const UINT width, const UINT height)
{
    if (!_interopWindowHandle)
    {
        return;
    }

    const auto topBorderHeight = _GetTopBorderHeight();

    // I'm not sure that HWND_BOTTOM does anything different than HWND_TOP for us.
    winrt::check_bool(SetWindowPos(_interopWindowHandle,
                                   HWND_BOTTOM,
                                   0,
                                   topBorderHeight,
                                   width,
                                   height - topBorderHeight,
                                   SWP_SHOWWINDOW));
}

// Method Description:
// - Update the region of our window that is the draggable area. This happens in
//   response to a OnDragBarSizeChanged event. We'll calculate the areas of the
//   window that we want to display XAML content in, and set the window region
//   of our child xaml-island window to that region. That way, the parent window
//   will still get NCHITTEST'ed _outside_ the XAML content area, for things
//   like dragging and resizing.
// Arguments:
// - <none>
// Return Value:
// - <none>
void NonClientIslandWindow::_UpdateDragRegion()
{
    if (_dragBar)
    {
        // TODO:GH#1897 This is largely duplicated from OnSize, and we should do
        // better than that.
        const auto windowRect = GetWindowRect();
        const auto width = windowRect.right - windowRect.left;
        const auto height = windowRect.bottom - windowRect.top;

        const auto scale = GetCurrentDpiScale();
        const auto dpi = ::GetDpiForWindow(_window.get());

        const auto dragY = ::GetSystemMetricsForDpi(SM_CYDRAG, dpi);
        const auto dragX = ::GetSystemMetricsForDpi(SM_CXDRAG, dpi);

        // If we're maximized, we don't want to use the frame as our margins,
        // instead we want to use the margins from the maximization. If we included
        // the left&right sides of the frame in this calculation while maximized,
        // you' have a few pixels of the window border on the sides while maximized,
        // which most apps do not have.
        const auto bordersWidth = _isMaximized ?
                                      (_maximizedMargins.cxLeftWidth + _maximizedMargins.cxRightWidth) :
                                      (dragX * 2);
        const auto bordersHeight = _isMaximized ?
                                       (_maximizedMargins.cyBottomHeight + _maximizedMargins.cyTopHeight) :
                                       (dragY * 2);

        const auto windowsWidth = width - bordersWidth;
        const auto windowsHeight = height - bordersHeight;
        const auto xPos = _isMaximized ? _maximizedMargins.cxLeftWidth : dragX;
        const auto yPos = _isMaximized ? _maximizedMargins.cyTopHeight : dragY;

        const auto dragBarRect = GetDragAreaRect();
        const auto nonClientHeight = dragBarRect.bottom - dragBarRect.top;

        auto nonClientRegion = wil::unique_hrgn(CreateRectRgn(0, 0, 0, 0));
        auto nonClientLeftRegion = wil::unique_hrgn(CreateRectRgn(0, 0, dragBarRect.left, nonClientHeight));
        auto nonClientRightRegion = wil::unique_hrgn(CreateRectRgn(dragBarRect.right, 0, windowsWidth, nonClientHeight));
        winrt::check_bool(CombineRgn(nonClientRegion.get(), nonClientLeftRegion.get(), nonClientRightRegion.get(), RGN_OR));

        _dragBarRegion = wil::unique_hrgn(CreateRectRgn(0, 0, 0, 0));
        auto clientRegion = wil::unique_hrgn(CreateRectRgn(0, nonClientHeight, windowsWidth, windowsHeight));
        winrt::check_bool(CombineRgn(_dragBarRegion.get(), nonClientRegion.get(), clientRegion.get(), RGN_OR));
        winrt::check_bool(SetWindowRgn(_interopWindowHandle, _dragBarRegion.get(), true));
    }
}

// Method Description:
// Hit test the frame for resizing and moving.
// Method Description:
// - Hit test the frame for resizing and moving.
// Arguments:
// - ptMouse: the mouse point being tested, in absolute (NOT WINDOW) coordinates.
// Return Value:
// - one of the values from
//  https://docs.microsoft.com/en-us/windows/desktop/inputdev/wm-nchittest#return-value
//   corresponding to the area of the window that was hit
// NOTE:
// - Largely taken from code on:
// https://docs.microsoft.com/en-us/windows/desktop/dwm/customframe
[[nodiscard]] LRESULT NonClientIslandWindow::HitTestNCA(POINT ptMouse) const noexcept
{
    // This will handle the left, right and bottom parts of the frame because
    // we didn't change them.
    LPARAM lParam = MAKELONG(ptMouse.x, ptMouse.y);
    LRESULT result = DefWindowProc(_window.get(), WM_NCHITTEST, 0, lParam);

    if (result == HTCLIENT)
    {
        // The cursor might be in the title bar, because the client area was
        // extended to include the title bar (see
        // NonClientIslandWindow::_UpdateFrameMargins).

        // We have our own minimize, maximize and close buttons so we cannot
        // use DwmDefWindowProc.

        // TODO: return HTMINBUTTON, HTMAXBUTTON and HTCLOSE instead of
        //  HTCAPTION for the buttons?

        // At this point, we ruled out the left, right and bottom parts of
        // the frame and the minimize, maximize and close buttons in the
        // title bar. It has to be either the drag bar or something else in
        // the XAML island. But the XAML islands handles WM_NCHITTEST on its
        // own so actually it cannot be the XAML islands. Then it must be the
        // drag bar.

        RECT windowRc;
        winrt::check_bool(::GetWindowRect(_window.get(), &windowRc));

        const auto scale = GetCurrentDpiScale();

        // This doesn't appear to be consistent between all apps on Windows.
        // From Windows 18362.418 with 96 DPI:
        // - conhost.exe: 8 pixels
        // - explorer.exe: 9 pixels
        // - Settings app: 4 pixels
        // - Skype: 4 pixels
        const auto resizeBorderHeight = 4 * scale;

        if (ptMouse.y < windowRc.top + resizeBorderHeight)
        {
            result = HTTOP;
        }
        else
        {
            result = HTCAPTION;
        }
    }

    return result;
}

// Method Description:
// - Get the size of the borders we want to use. The sides and bottom will just
//   be big enough for resizing, but the top will be as big as we need for the
//   non-client content.
// Return Value:
// - A MARGINS struct containing the border dimensions we want.
MARGINS NonClientIslandWindow::GetFrameMargins() const noexcept
{
    const auto scale = GetCurrentDpiScale();
    const auto dpi = ::GetDpiForWindow(_window.get());
    const auto windowMarginSides = ::GetSystemMetricsForDpi(SM_CXDRAG, dpi);
    const auto windowMarginBottom = ::GetSystemMetricsForDpi(SM_CXDRAG, dpi);

    const auto dragBarRect = GetDragAreaRect();
    const auto nonClientHeight = dragBarRect.bottom - dragBarRect.top;

    MARGINS margins{ 0 };
    margins.cxLeftWidth = windowMarginSides;
    margins.cxRightWidth = windowMarginSides;
    margins.cyBottomHeight = windowMarginBottom;
    margins.cyTopHeight = nonClientHeight + windowMarginBottom;

    return margins;
}

// Method Description:
// - Updates the borders of our window frame, using DwmExtendFrameIntoClientArea.
// Arguments:
// - <none>
// Return Value:
// - the HRESULT returned by DwmExtendFrameIntoClientArea.
[[nodiscard]] HRESULT NonClientIslandWindow::_UpdateFrameMargins() const noexcept
{
    const auto dpi = ::GetDpiForWindow(_window.get());
    if (dpi == 0)
    {
        winrt::throw_last_error();
    }

    RECT frameRc = {};
    ::AdjustWindowRectExForDpi(&frameRc, WS_OVERLAPPEDWINDOW, FALSE, 0, dpi);

    // We removed the titlebar from the non client area (see handling of
    // WM_NCCALCSIZE) and now we add it back, but into the client area to paint
    // over it with our XAML island.
    MARGINS margins = { 0, 0, -frameRc.top, 0 };

    // Extend the frame into the client area.
    return DwmExtendFrameIntoClientArea(_window.get(), &margins);
}

// Routine Description:
// - Gets the maximum possible window rectangle in pixels. Based on the monitor
//   the window is on or the primary monitor if no window exists yet.
// Arguments:
// - prcSuggested - If we were given a suggested rectangle for where the window
//                  is going, we can pass it in here to find out the max size
//                  on that monitor.
//                - If this value is zero and we had a valid window handle,
//                  we'll use that instead. Otherwise the value of 0 will make
//                  us use the primary monitor.
// - pDpiSuggested - The dpi that matches the suggested rect. We will attempt to
//                   compute this during the function, but if we fail for some
//                   reason, the original value passed in will be left untouched.
// Return Value:
// - RECT containing the left, right, top, and bottom positions from the desktop
//   origin in pixels. Measures the outer edges of the potential window.
// NOTE:
// Heavily taken from WindowMetrics::GetMaxWindowRectInPixels in conhost.
RECT NonClientIslandWindow::GetMaxWindowRectInPixels(const RECT* const prcSuggested, _Out_opt_ UINT* pDpiSuggested)
{
    // prepare rectangle
    RECT rc = *prcSuggested;

    // use zero rect to compare.
    RECT rcZero;
    SetRectEmpty(&rcZero);

    // First get the monitor pointer from either the active window or the default location (0,0,0,0)
    HMONITOR hMonitor = nullptr;

    // NOTE: We must use the nearest monitor because sometimes the system moves
    // the window around into strange spots while performing snap and Win+D
    // operations. Those operations won't work correctly if we use
    // MONITOR_DEFAULTTOPRIMARY.
    if (!EqualRect(&rc, &rcZero))
    {
        // For invalid window handles or when we were passed a non-zero
        // suggestion rectangle, get the monitor from the rect.
        hMonitor = MonitorFromRect(&rc, MONITOR_DEFAULTTONEAREST);
    }
    else
    {
        // Otherwise, get the monitor from the window handle.
        hMonitor = MonitorFromWindow(_window.get(), MONITOR_DEFAULTTONEAREST);
    }

    // If for whatever reason there is no monitor, we're going to give back
    // whatever we got since we can't figure anything out. We won't adjust the
    // DPI either. That's OK. DPI doesn't make much sense with no display.
    if (nullptr == hMonitor)
    {
        return rc;
    }

    // Now obtain the monitor pixel dimensions
    MONITORINFO MonitorInfo = { 0 };
    MonitorInfo.cbSize = sizeof(MONITORINFO);

    GetMonitorInfoW(hMonitor, &MonitorInfo);

    // We have to make a correction to the work area. If we actually consume the
    // entire work area (by maximizing the window). The window manager will
    // render the borders off-screen. We need to pad the work rectangle with the
    // border dimensions to represent the actual max outer edges of the window
    // rect.
    WINDOWINFO wi = { 0 };
    wi.cbSize = sizeof(WINDOWINFO);
    GetWindowInfo(_window.get(), &wi);

    // In non-full screen, we want to only use the work area (avoiding the task bar space)
    rc = MonitorInfo.rcWork;

    if (pDpiSuggested != nullptr)
    {
        UINT monitorDpiX;
        UINT monitorDpiY;
        if (SUCCEEDED(GetDpiForMonitor(hMonitor, MDT_EFFECTIVE_DPI, &monitorDpiX, &monitorDpiY)))
        {
            *pDpiSuggested = monitorDpiX;
        }
        else
        {
            *pDpiSuggested = GetDpiForWindow(_window.get());
        }
    }

    return rc;
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
    case WM_ACTIVATE:
    {
        _HandleActivateWindow();
        break;
    }
    case WM_NCCALCSIZE:
    {
        if (wParam == false)
        {
            return 0;
        }
        // Handle the non-client size message.
        if (wParam == TRUE && lParam)
        {
            // Calculate new NCCALCSIZE_PARAMS based on custom NCA inset.
            NCCALCSIZE_PARAMS* pncsp = reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam);

            const auto dpi = ::GetDpiForWindow(_window.get());
            if (dpi == 0)
            {
                winrt::throw_last_error();
            }

            RECT frameRc = {};
            ::AdjustWindowRectExForDpi(&frameRc, WS_OVERLAPPEDWINDOW, FALSE, 0, dpi);

            // keep the standard frame, except ...
            pncsp->rgrc[0].left = pncsp->rgrc[0].left - frameRc.left;
            pncsp->rgrc[0].top = pncsp->rgrc[0].top + 0; // ... remove the titlebar
            pncsp->rgrc[0].right = pncsp->rgrc[0].right - frameRc.right;
            pncsp->rgrc[0].bottom = pncsp->rgrc[0].bottom - frameRc.bottom;

            return 0;
        }
        break;
    }
    case WM_NCHITTEST:
    {
        return HitTestNCA({ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) });
    }

    case WM_EXITSIZEMOVE:
    {
        ForceResize();
        break;
    }

    case WM_PAINT:
    {
        if (!_dragBar)
        {
            return 0;
        }

        PAINTSTRUCT ps{ 0 };
        const auto hdc = wil::BeginPaint(_window.get(), &ps);
        if (hdc.get())
        {
            const auto topBorderHeight = _GetTopBorderHeight();

            RECT rcTopBorder = ps.rcPaint;
            rcTopBorder.top = 0;
            rcTopBorder.bottom = topBorderHeight;
            ::FillRect(hdc.get(), &rcTopBorder, GetStockBrush(BLACK_BRUSH));

            if (ps.rcPaint.bottom > topBorderHeight)
            {
                // Create brush for borders, titlebar color.
                const auto backgroundBrush = _titlebar.Background();
                const auto backgroundSolidBrush = backgroundBrush.as<Media::SolidColorBrush>();
                const auto backgroundColor = backgroundSolidBrush.Color();
                const auto color = RGB(backgroundColor.R, backgroundColor.G, backgroundColor.B);
                _backgroundBrush = wil::unique_hbrush(CreateSolidBrush(color));

                RECT rcRest = ps.rcPaint;
                rcRest.top = topBorderHeight;

                HDC opaqueDc;
                BP_PAINTPARAMS params = { sizeof(params), BPPF_NOCLIP | BPPF_ERASE };
                HPAINTBUFFER buf = BeginBufferedPaint(hdc.get(), &rcRest, BPBF_TOPDOWNDIB, &params, &opaqueDc);
                if (!buf || !opaqueDc)
                {
                    winrt::throw_last_error();
                }

                ::FillRect(opaqueDc, &rcRest, _backgroundBrush.get());
                ::BufferedPaintSetAlpha(buf, NULL, 255); // set to opaque to draw over the default title bar
                ::EndBufferedPaint(buf, TRUE);
            }
        }

        return 0;
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

    case WM_WINDOWPOSCHANGING:
    {
        // Enforce maximum size here instead of WM_GETMINMAXINFO. If we return
        // it in WM_GETMINMAXINFO, then it will be enforced when snapping across
        // DPI boundaries (bad.)
        LPWINDOWPOS lpwpos = reinterpret_cast<LPWINDOWPOS>(lParam);
        if (lpwpos == nullptr)
        {
            break;
        }
        if (_HandleWindowPosChanging(lpwpos))
        {
            return 0;
        }
        else
        {
            break;
        }
    }
    case WM_DPICHANGED:
    {
        auto lprcNewScale = reinterpret_cast<RECT*>(lParam);
        OnSize(RECT_WIDTH(lprcNewScale), RECT_HEIGHT(lprcNewScale));
        break;
    }
    }

    return IslandWindow::MessageHandler(message, wParam, lParam);
}

// Method Description:
// - Handle a WM_ACTIVATE message. Called during the creation of the window, and
//   used as an opprotunity to get the dimensions of the caption buttons (the
//   min, max, close buttons). We'll use these dimensions to help size the
//   non-client area of the window.
void NonClientIslandWindow::_HandleActivateWindow()
{
    THROW_IF_FAILED(_UpdateFrameMargins());
}

// Method Description:
// - Handle a WM_WINDOWPOSCHANGING message. When the window is changing, or the
//   dpi is changing, this handler is triggered to give us a chance to adjust
//   the window size and position manually. We use this handler during a maxiize
//   to figure out by how much the window will overhang the edges of the
//   monitor, and set up some padding to adjust for that.
// Arguments:
// - windowPos: A pointer to a proposed window location and size. Should we wish
//   to manually position the window, we could change the values of this struct.
// Return Value:
// - true if we handled this message, false otherwise. If we return false, the
//   message should instead be handled by DefWindowProc
// Note:
// Largely taken from the conhost WM_WINDOWPOSCHANGING handler.
bool NonClientIslandWindow::_HandleWindowPosChanging(WINDOWPOS* const windowPos)
{
    // We only need to apply restrictions if the size is changing.
    if (WI_IsFlagSet(windowPos->flags, SWP_NOSIZE))
    {
        return false;
    }

    const auto windowStyle = GetWindowStyle(_window.get());
    const auto isMaximized = WI_IsFlagSet(windowStyle, WS_MAXIMIZE);
    const auto isIconified = WI_IsFlagSet(windowStyle, WS_ICONIC);

    if (_titlebar)
    {
        _titlebar.SetWindowVisualState(isMaximized ? winrt::TerminalApp::WindowVisualState::WindowVisualStateMaximized :
                                                     isIconified ? winrt::TerminalApp::WindowVisualState::WindowVisualStateIconified :
                                                                   winrt::TerminalApp::WindowVisualState::WindowVisualStateNormal);
    }

    // Figure out the suggested dimensions
    RECT rcSuggested;
    rcSuggested.left = windowPos->x;
    rcSuggested.top = windowPos->y;
    rcSuggested.right = rcSuggested.left + windowPos->cx;
    rcSuggested.bottom = rcSuggested.top + windowPos->cy;
    SIZE szSuggested;
    szSuggested.cx = RECT_WIDTH(&rcSuggested);
    szSuggested.cy = RECT_HEIGHT(&rcSuggested);

    // Figure out the current dimensions for comparison.
    RECT rcCurrent = GetWindowRect();

    // Determine whether we're being resized by someone dragging the edge or
    // completely moved around.
    bool fIsEdgeResize = false;
    {
        // We can only be edge resizing if our existing rectangle wasn't empty.
        // If it was empty, we're doing the initial create.
        if (!IsRectEmpty(&rcCurrent))
        {
            // If one or two sides are changing, we're being edge resized.
            unsigned int cSidesChanging = 0;
            if (rcCurrent.left != rcSuggested.left)
            {
                cSidesChanging++;
            }
            if (rcCurrent.right != rcSuggested.right)
            {
                cSidesChanging++;
            }
            if (rcCurrent.top != rcSuggested.top)
            {
                cSidesChanging++;
            }
            if (rcCurrent.bottom != rcSuggested.bottom)
            {
                cSidesChanging++;
            }

            if (cSidesChanging == 1 || cSidesChanging == 2)
            {
                fIsEdgeResize = true;
            }
        }
    }

    // If we're about to maximize the window, determine how much we're about to
    // overhang by, and adjust for that.
    // We need to do this because maximized windows will typically overhang the
    // actual monitor bounds by roughly the size of the old "thick: window
    // borders. For normal windows, this is fine, but because we're using
    // DwmExtendFrameIntoClientArea, that means some of our client content will
    // now overhang, and get cut off.
    if (isMaximized)
    {
        // Find the related monitor, the maximum pixel size,
        // and the dpi for the suggested rect.
        UINT dpiOfMaximum;
        RECT rcMaximum;

        if (fIsEdgeResize)
        {
            // If someone's dragging from the edge to resize in one direction,
            // we want to make sure we never grow past the current monitor.
            rcMaximum = GetMaxWindowRectInPixels(&rcCurrent, &dpiOfMaximum);
        }
        else
        {
            // In other circumstances, assume we're snapping around or some
            // other jump (TS). Just do whatever we're told using the new
            // suggestion as the restriction monitor.
            rcMaximum = GetMaxWindowRectInPixels(&rcSuggested, &dpiOfMaximum);
        }

        const auto suggestedWidth = szSuggested.cx;
        const auto suggestedHeight = szSuggested.cy;

        const auto maxWidth = RECT_WIDTH(&rcMaximum);
        const auto maxHeight = RECT_HEIGHT(&rcMaximum);

        // Only apply the maximum size restriction if the current DPI matches
        // the DPI of the maximum rect. This keeps us from applying the wrong
        // restriction if the monitor we're moving to has a different DPI but
        // we've yet to get notified of that DPI change. If we do apply it, then
        // we'll restrict the console window BEFORE its been resized for the DPI
        // change, so we're likely to shrink the window too much or worse yet,
        // keep it from moving entirely. We'll get a WM_DPICHANGED, resize the
        // window, and then process the restriction in a few window messages.
        if (((int)dpiOfMaximum == _currentDpi) &&
            ((suggestedWidth > maxWidth) ||
             (suggestedHeight > maxHeight)))
        {
            RECT frame{};
            // Calculate the maxmized window overhang by getting the size of the window frame.
            // We use the style without WS_CAPTION otherwise the caption height is included.
            // Only remove WS_DLGFRAME since WS_CAPTION = WS_DLGFRAME | WS_BORDER,
            // but WS_BORDER is needed as it modifies the calculation of the width of the frame.
            const auto targetStyle = windowStyle & ~WS_DLGFRAME;
            AdjustWindowRectExForDpi(&frame, targetStyle, false, GetWindowExStyle(_window.get()), _currentDpi);

            // Frame left and top will be negative
            _maximizedMargins.cxLeftWidth = frame.left * -1;
            _maximizedMargins.cyTopHeight = frame.top * -1;
            _maximizedMargins.cxRightWidth = frame.right;
            _maximizedMargins.cyBottomHeight = frame.bottom;

            _isMaximized = true;
            THROW_IF_FAILED(_UpdateFrameMargins());
        }
    }
    else
    {
        // Clear our maximization state
        _maximizedMargins = { 0 };

        // Immediately after resoring down, don't update our frame margins. If
        // you do this here, then a small gap will appear between the titlebar
        // and the content, until the window is moved. However, we do need to
        // keep this here _in general_ for dragging across DPI boundaries.
        if (!_isMaximized)
        {
            THROW_IF_FAILED(_UpdateFrameMargins());
        }

        _isMaximized = false;
    }
    return true;
}
