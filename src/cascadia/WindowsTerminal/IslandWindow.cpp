// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "IslandWindow.h"
#include "../types/inc/Viewport.hpp"

extern "C" IMAGE_DOS_HEADER __ImageBase;

using namespace winrt::Windows::UI;
using namespace winrt::Windows::UI::Composition;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Hosting;
using namespace winrt::Windows::Foundation::Numerics;
using namespace ::Microsoft::Console::Types;

#define XAML_HOSTING_WINDOW_CLASS_NAME L"CASCADIA_HOSTING_WINDOW_CLASS"

IslandWindow::IslandWindow() noexcept :
    _currentWidth{ 0 },
    _currentHeight{ 0 },
    _interopWindowHandle{ nullptr },
    _rootGrid{ nullptr },
    _source{ nullptr },
    _pfnCreateCallback{ nullptr }
{
}

IslandWindow::~IslandWindow()
{
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
    RegisterClass(&wc);
    WINRT_ASSERT(!_window);

    // Create the window with the default size here - During the creation of the
    // window, the system will give us a chance to set it's size in WM_CREATE.
    // WM_CREATE will be handled synchronously, before CreateWindow returns.
    WINRT_VERIFY(CreateWindow(wc.lpszClassName,
        L"Windows Terminal",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        nullptr, nullptr, wc.hInstance, this));

    WINRT_ASSERT(_window);

}

// Method Description:
// - Set a callback to be called when we process a WM_CREATE message. This gives
//   the AppHost a chance to resize the window to the propoer size.
// Arguments:
// - pfn: a function to be called during the handling of WM_CREATE. It takes two
//   parameters:
//      * HWND: the HWND of the window that's being created.
//      * RECT: The position on the screen that the system has proposed for our
//        window.
// Return Value:
// - <none>
void IslandWindow::SetCreateCallback(std::function<void(const HWND, const RECT)> pfn) noexcept
{
    _pfnCreateCallback = pfn;
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

    if (_pfnCreateCallback)
    {
        _pfnCreateCallback(_window, rc);
    }

    ShowWindow(_window, SW_SHOW);
    UpdateWindow(_window);
}

void IslandWindow::Initialize()
{
    const bool initialized = (_interopWindowHandle != nullptr);

    _source = DesktopWindowXamlSource{};

    auto interop = _source.as<IDesktopWindowXamlSourceNative>();
    winrt::check_hresult(interop->AttachToWindow(_window));

    // stash the child interop handle so we can resize it when the main hwnd is resized
    interop->get_WindowHandle(&_interopWindowHandle);

    _rootGrid = winrt::Windows::UI::Xaml::Controls::Grid();
    _source.Content(_rootGrid);

    // Do a quick resize to force the island to paint
    OnSize();
}

double IslandWindow::GetCurrentDpiScale()
{
    // setup a root grid that will be used to apply DPI scaling
    winrt::Windows::UI::Xaml::Media::ScaleTransform dpiScaleTransform;
    winrt::Windows::UI::Xaml::Controls::Grid dpiAdjustmentGrid;

    const auto dpi = GetDpiForWindow(_window);
    const double scale = double(dpi) / double(USER_DEFAULT_SCREEN_DPI);
    return scale;
}


void IslandWindow::OnSize()
{
    // update the interop window size
    SetWindowPos(_interopWindowHandle, 0, 0, 0, _currentWidth, _currentHeight, SWP_SHOWWINDOW);

    if (_rootGrid)
    {
        const auto dpi = GetCurrentDpiScale();
        const auto logicalWidth = (_currentWidth / dpi) + 0.5;
        _rootGrid.Width(logicalWidth);
        const auto logicalHeigth = (_currentHeight / dpi) + 0.5;
        _rootGrid.Height(logicalHeigth);
    }
}

LRESULT IslandWindow::MessageHandler(UINT const message, WPARAM const wparam, LPARAM const lparam) noexcept
{
    switch (message) {

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
    _currentWidth = width;
    _currentHeight = height;
    if (nullptr != _rootGrid)
    {
        OnSize();
    }
}

// Method Description:
// - Called when the window is minimized to the taskbar.
void IslandWindow::OnMinimize()
{
    // TODO MSFT#21315817 Stop rendering island content when the app is minimized.
}

// Method Description:
// - Called when the window is restored from having been minimized.
void IslandWindow::OnRestore()
{
    // TODO MSFT#21315817 Stop rendering island content when the app is minimized.
}

void IslandWindow::SetRootContent(winrt::Windows::UI::Xaml::UIElement content)
{
    _rootGrid.Children().Clear();
    _rootGrid.Children().Append(content);
}
