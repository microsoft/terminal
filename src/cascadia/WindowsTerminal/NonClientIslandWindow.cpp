/********************************************************
*                                                       *
*   Copyright (C) Microsoft. All rights reserved.       *
*                                                       *
********************************************************/
#include "pch.h"
#include "NonClientIslandWindow.h"
#include "../types/inc/ThemeUtils.h"

extern "C" IMAGE_DOS_HEADER __ImageBase;

using namespace winrt::Windows::UI;
using namespace winrt::Windows::UI::Composition;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Hosting;
using namespace winrt::Windows::Foundation::Numerics;
using namespace ::Microsoft::Console;
using namespace ::Microsoft::Console::Types;

NonClientIslandWindow::NonClientIslandWindow() noexcept :
    IslandWindow{},
    _oldIslandX{ -1 },
    _oldIslandY{ -1 },
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

    THROW_IF_FAILED(_UpdateFrameMargins());

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

// Method Description:
// - This method is computes the height of the frame border at the top of the
//   title bar and returns it. If the border is disabled, then this method will
//   return 0.
// Return Value:
// - the height of the frame border at the top of the title bar or zero to
//   disable it
int NonClientIslandWindow::GetTopBorderHeight() const noexcept
{
    if (_isMaximized)
    {
        // no border when maximized
        return 0;
    }

    return static_cast<int>(1 * GetCurrentDpiScale());
}

RECT NonClientIslandWindow::_GetDragAreaRect() const noexcept
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
// - Called when the size of the window changes for any reason. Updates the
//   Xaml Island to match our new sizing and also updates the maximize icon
//   if the window went from maximized to minimized or the opposite.
void NonClientIslandWindow::OnSize(const UINT width, const UINT height)
{
    _UpdateMaximizedState();

    if (_interopWindowHandle)
    {
        _UpdateIslandPosition(width, height);
    }
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
// - Called when the the windows goes from restored to maximized or from
//   maximized to restore. Updates the maximize button's icon and the frame
//   margins.
void NonClientIslandWindow::_OnMaximizeChange() noexcept
{
    if (_titlebar)
    {
        const auto windowStyle = GetWindowStyle(_window.get());
        const auto isIconified = WI_IsFlagSet(windowStyle, WS_ICONIC);

        const auto state = _isMaximized ? winrt::TerminalApp::WindowVisualState::WindowVisualStateMaximized :
                                          isIconified ? winrt::TerminalApp::WindowVisualState::WindowVisualStateIconified :
                                                        winrt::TerminalApp::WindowVisualState::WindowVisualStateNormal;

        try
        {
            _titlebar.SetWindowVisualState(state);
        }
        CATCH_LOG();
    }

    // no frame margin when maximized
    THROW_IF_FAILED(_UpdateFrameMargins());
}

// Method Description:
// - called when the size of the window changes for any reason. Updates the
//   sizes of our child Xaml Islands to match our new sizing.
void NonClientIslandWindow::_UpdateIslandPosition(const UINT windowWidth, const UINT windowHeight)
{
    const auto topBorderHeight = GetTopBorderHeight();

    const auto newIslandX = 0;
    const auto newIslandY = topBorderHeight;

    // I'm not sure that HWND_BOTTOM does anything different than HWND_TOP for us.
    winrt::check_bool(SetWindowPos(_interopWindowHandle,
                                   HWND_BOTTOM,
                                   newIslandX,
                                   newIslandY,
                                   windowWidth,
                                   windowHeight - topBorderHeight,
                                   SWP_SHOWWINDOW));

    // This happens when we go from maximized to restored or the opposite
    // because topBorderHeight changes.
    if (newIslandX != _oldIslandX || newIslandY != _oldIslandY)
    {
        // The drag bar's position changed compared to the client area because
        // the island moved but we will not be notified about this in the
        // NonClientIslandWindow::OnDragBarSizeChanged method because this
        // method is only called when the position of the drag bar changes
        // **inside** the island which is not the case here.
        _UpdateDragRegion();
    }

    _oldIslandX = newIslandX;
    _oldIslandY = newIslandY;
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
        RECT rcClient;
        winrt::check_bool(GetClientRect(_window.get(), &rcClient));

        const auto rcDragBar = _GetDragAreaRect();
        const auto topBorderHeight = GetTopBorderHeight();

        const auto clientRegion = wil::unique_hrgn(CreateRectRgn(rcClient.left, rcClient.top, rcClient.right, rcClient.bottom));
        const auto dragBarRegion = wil::unique_hrgn(CreateRectRgn(rcDragBar.left, rcDragBar.top, rcDragBar.right, rcDragBar.bottom));
        const auto clientWithoutDragBarRegion = wil::unique_hrgn(CreateRectRgn(0, 0, 0, 0));
        winrt::check_bool(CombineRgn(clientWithoutDragBarRegion.get(), clientRegion.get(), dragBarRegion.get(), RGN_DIFF));

        winrt::check_bool(SetWindowRgn(_interopWindowHandle, clientWithoutDragBarRegion.get(), true));
    }
}

[[nodiscard]] LRESULT NonClientIslandWindow::_OnNcCalcSize(const WPARAM wParam, const LPARAM lParam) noexcept
{
    if (wParam == false)
    {
        return 0;
    }

    NCCALCSIZE_PARAMS* params = reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam);

    // Store the original top before the default window proc applies the
    // default frame.
    const auto originalTop = params->rgrc[0].top;

    // apply the default frame
    auto ret = DefWindowProc(_window.get(), WM_NCCALCSIZE, wParam, lParam);
    if (ret != 0)
    {
        return ret;
    }

    auto newTop = originalTop;

    // WM_NCCALCSIZE is called before WM_SIZE
    _UpdateMaximizedState();

    if (_isMaximized)
    {
        // When a window is maximized, its size is actually a little
        // bit more than the monitor work area. The window borders'
        // size is added to the window size so that the borders do not
        // appear on the monitor.

        // We only have to do this for the title bar because it's the
        // only part of the frame that we change.
        const auto resizeBorderHeight =
            // there isn't a SM_CYPADDEDBORDER for the Y axis
            GetSystemMetricsForDpi(SM_CXPADDEDBORDER, _currentDpi) +
            GetSystemMetricsForDpi(SM_CYFRAME, _currentDpi);
        newTop += resizeBorderHeight;
    }

    // only modify the top of the frame to remove the title bar
    params->rgrc[0].top = newTop;

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
    LRESULT result = DefWindowProc(_window.get(), WM_NCHITTEST, 0, lParam);

    if (result == HTCLIENT)
    {
        // The cursor might be in the title bar, because the client area was
        // extended to include the title bar (see
        // NonClientIslandWindow::_UpdateFrameMargins).

        // We have our own minimize, maximize and close buttons so we cannot
        // use DwmDefWindowProc.

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
        const auto resizeBorderHeight = 8 * scale;
        const auto isOnResizeBorder = ptMouse.y < windowRc.top + resizeBorderHeight;

        // no top resize handle when maximized
        if (!_isMaximized && isOnResizeBorder)
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
// - Updates the borders of our window frame, using DwmExtendFrameIntoClientArea.
// Arguments:
// - <none>
// Return Value:
// - the HRESULT returned by DwmExtendFrameIntoClientArea.
[[nodiscard]] HRESULT NonClientIslandWindow::_UpdateFrameMargins() const noexcept
{
    const auto topBorderHeight = GetTopBorderHeight();

    // We removed the whole top part of the frame (see handling of
    // WM_NCCALCSIZE) so the top border is missing now. We add it back here.
    // Note: You might wonder why we don't remove just the title bar (the part
    //  that we don't want) instead of removing the whole top part of the frame
    //  and then adding the border (what we want) back. I tried to do this but
    //  it didn't work: DWM drew the whole title bar anyways. It seems that DWM
    //  only wants to draw either nothing or the whole title bar at the top.
    MARGINS margins = { 0, 0, topBorderHeight, 0 };

    // Extend the frame into the client area.
    return DwmExtendFrameIntoClientArea(_window.get(), &margins);
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
    case WM_NCCALCSIZE:
        return _OnNcCalcSize(wParam, lParam);
    case WM_NCHITTEST:
        return _OnNcHitTest({ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) });
    case WM_PAINT:
        return _OnPaint();
    }

    return IslandWindow::MessageHandler(message, wParam, lParam);
}

[[nodiscard]] LRESULT NonClientIslandWindow::_OnPaint() noexcept
{
    if (!_titlebar)
    {
        return 0;
    }

    PAINTSTRUCT ps{ 0 };
    const auto hdc = wil::BeginPaint(_window.get(), &ps);
    if (hdc.get())
    {
        const auto backgroundBrush = _titlebar.Background();
        const auto backgroundSolidBrush = backgroundBrush.as<Media::SolidColorBrush>();
        const auto backgroundColor = backgroundSolidBrush.Color();
        const auto color = RGB(backgroundColor.R, backgroundColor.G, backgroundColor.B);

        if (!_backgroundBrush || color != _backgroundBrushColor)
        {
            // Create brush for titlebar color.
            _backgroundBrush = wil::unique_hbrush(CreateSolidBrush(color));
        }

        ::FillRect(hdc.get(), &ps.rcPaint, _backgroundBrush.get());
    }

    return 0;
}

// Method Description:
// - This method is called when the window receives the WM_NCCREATE message.
// Return Value:
// - <none>
void NonClientIslandWindow::_OnNcCreate() noexcept
{
    IslandWindow::_OnNcCreate();

    // Set the frame's theme before it is rendered (WM_NCPAINT) so that it is
    // rendered with the correct theme.
    _UpdateFrameTheme();
}

// Method Description:
// - Updates the window frame's theme depending on the application theme (light
//   or dark). If this is called after the frame is rendered, the old frame is
//   not invalidated so the frame doesn't update until the user resizes or
//   focuses/unfocuses the window.
// Return Value:
// - <none>
void NonClientIslandWindow::_UpdateFrameTheme()
{
    const auto isDarkMode = Application::Current().RequestedTheme() == ApplicationTheme::Dark;

    if (FAILED(ThemeUtils::SetDwmImmersiveDarkMode(_window.get(), isDarkMode)))
    {
        LOG_LAST_ERROR();
    }
}
