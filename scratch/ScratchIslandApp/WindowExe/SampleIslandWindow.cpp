// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "SampleIslandWindow.h"
#include "../types/inc/Viewport.hpp"
#include "resource.h"
#include "icon.h"

extern "C" IMAGE_DOS_HEADER __ImageBase;

using namespace winrt::Windows::UI;
using namespace winrt::Windows::UI::Composition;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Hosting;
using namespace winrt::Windows::Foundation::Numerics;
using namespace ::Microsoft::Console::Types;

#define XAML_HOSTING_WINDOW_CLASS_NAME L"SCRATCH_HOSTING_WINDOW_CLASS"

SampleIslandWindow::SampleIslandWindow() noexcept :
    _interopWindowHandle{ nullptr },
    _rootGrid{ nullptr },
    _source{ nullptr }
{
}

SampleIslandWindow::~SampleIslandWindow()
{
    _source.Close();
}

// Method Description:
// - Create the actual window that we'll use for the application.
// Arguments:
// - <none>
// Return Value:
// - <none>
void SampleIslandWindow::MakeWindow() noexcept
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
    WINRT_VERIFY(CreateWindowEx(0,
                                wc.lpszClassName,
                                L"ScratchApp",
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
void SampleIslandWindow::Close()
{
    PostQuitMessage(0);
}

// Method Description:
// - Handles a WM_CREATE message. Calls our create callback, if one's been set.
// Arguments:
// - wParam: unused
// - lParam: the lParam of a WM_CREATE, which is a pointer to a CREATESTRUCTW
// Return Value:
// - <none>
void SampleIslandWindow::_HandleCreateWindow(const WPARAM, const LPARAM lParam) noexcept
{
    // Get proposed window rect from create structure
    CREATESTRUCTW* pcs = reinterpret_cast<CREATESTRUCTW*>(lParam);
    RECT rc;
    rc.left = pcs->x;
    rc.top = pcs->y;
    rc.right = rc.left + pcs->cx;
    rc.bottom = rc.top + pcs->cy;

    ShowWindow(_window.get(), SW_SHOW);

    UpdateWindow(_window.get());

    UpdateWindowIconForActiveMetrics(_window.get());
}

void SampleIslandWindow::Initialize()
{
    _source = DesktopWindowXamlSource{};

    auto interop = _source.as<IDesktopWindowXamlSourceNative>();
    winrt::check_hresult(interop->AttachToWindow(_window.get()));

    // stash the child interop handle so we can resize it when the main hwnd is resized
    interop->get_WindowHandle(&_interopWindowHandle);

    _rootGrid = winrt::Windows::UI::Xaml::Controls::Grid();
    _source.Content(_rootGrid);
}

void SampleIslandWindow::OnSize(const UINT width, const UINT height)
{
    // update the interop window size
    SetWindowPos(_interopWindowHandle, nullptr, 0, 0, width, height, SWP_SHOWWINDOW | SWP_NOACTIVATE);

    if (_rootGrid)
    {
        const auto size = GetLogicalSize();
        _rootGrid.Width(size.Width);
        _rootGrid.Height(size.Height);
    }
}

[[nodiscard]] LRESULT SampleIslandWindow::MessageHandler(UINT const message, WPARAM const wparam, LPARAM const lparam) noexcept
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
            return 0;
        }
        break;
    }
    case WM_MENUCHAR:
    {
        // GH#891: return this LRESULT here to prevent the app from making a
        // bell when alt+key is pressed. A menu is active and the user presses a
        // key that does not correspond to any mnemonic or accelerator key,
        return MAKELRESULT(0, MNC_CLOSE);
    }
    case WM_THEMECHANGED:
        UpdateWindowIconForActiveMetrics(_window.get());
        return 0;
    }

    return base_type::MessageHandler(message, wparam, lparam);
}

// Method Description:
// - Called when the window has been resized (or maximized)
// Arguments:
// - width: the new width of the window _in pixels_
// - height: the new height of the window _in pixels_
void SampleIslandWindow::OnResize(const UINT width, const UINT height)
{
    if (_interopWindowHandle)
    {
        OnSize(width, height);
    }
}

// Method Description:
// - Called when the window is minimized to the taskbar.
void SampleIslandWindow::OnMinimize()
{
}

// Method Description:
// - Called when the window is restored from having been minimized.
void SampleIslandWindow::OnRestore()
{
}

void SampleIslandWindow::SetContent(winrt::Windows::UI::Xaml::UIElement content)
{
    _rootGrid.Children().Clear();
    _rootGrid.Children().Append(content);
}

void SampleIslandWindow::OnAppInitialized()
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
void SampleIslandWindow::OnApplicationThemeChanged(const winrt::Windows::UI::Xaml::ElementTheme& requestedTheme)
{
    _rootGrid.RequestedTheme(requestedTheme);
    // Invalidate the window rect, so that we'll repaint any elements we're
    // drawing ourselves to match the new theme
    ::InvalidateRect(_window.get(), nullptr, false);
}
