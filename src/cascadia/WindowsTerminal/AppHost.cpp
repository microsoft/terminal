// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "AppHost.h"
#include "../types/inc/Viewport.hpp"
#include "../types/inc/utils.hpp"

using namespace winrt::Windows::UI;
using namespace winrt::Windows::UI::Composition;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Hosting;
using namespace winrt::Windows::Foundation::Numerics;
using namespace ::Microsoft::Console;
using namespace ::Microsoft::Console::Types;

AppHost::AppHost() noexcept :
    _app{},
    _logic{ nullptr }, // don't make one, we're going to take a ref on app's
    _window{ nullptr }
{
    _logic = _app.Logic(); // get a ref to app's logic

    _useNonClientArea = _logic.GetShowTabsInTitlebar();

    if (_useNonClientArea)
    {
        _window = std::make_unique<NonClientIslandWindow>(_logic.GetRequestedTheme());
    }
    else
    {
        _window = std::make_unique<IslandWindow>();
    }

    // Tell the window to callback to us when it's about to handle a WM_CREATE
    auto pfn = std::bind(&AppHost::_HandleCreateWindow,
                         this,
                         std::placeholders::_1,
                         std::placeholders::_2,
                         std::placeholders::_3);
    _window->SetCreateCallback(pfn);

    _window->MakeWindow();
}

AppHost::~AppHost()
{
    // destruction order is important for proper teardown here
    _window = nullptr;
    _app.Close();
    _app = nullptr;
}

// Method Description:
// - Initializes the XAML island, creates the terminal app, and sets the
//   island's content to that of the terminal app's content. Also registers some
//   callbacks with TermApp.
// !!! IMPORTANT!!!
// This must be called *AFTER* WindowsXamlManager::InitializeForCurrentThread.
// If it isn't, then we won't be able to create the XAML island.
// Arguments:
// - <none>
// Return Value:
// - <none>
void AppHost::Initialize()
{
    _window->Initialize();

    if (_useNonClientArea)
    {
        // Register our callbar for when the app's non-client content changes.
        // This has to be done _before_ App::Create, as the app might set the
        // content in Create.
        _logic.SetTitleBarContent({ this, &AppHost::_UpdateTitleBarContent });
    }

    // Register the 'X' button of the window for a warning experience of multiple
    // tabs opened, this is consistent with Alt+F4 closing
    _window->WindowCloseButtonClicked([this]() { _logic.WindowCloseButtonClicked(); });

    // Add an event handler to plumb clicks in the titlebar area down to the
    // application layer.
    _window->DragRegionClicked([this]() { _logic.TitlebarClicked(); });

    _logic.RequestedThemeChanged({ this, &AppHost::_UpdateTheme });
    _logic.ToggleFullscreen({ this, &AppHost::_ToggleFullscreen });

    _logic.Create();

    _logic.TitleChanged({ this, &AppHost::AppTitleChanged });
    _logic.LastTabClosed({ this, &AppHost::LastTabClosed });

    _window->UpdateTitle(_logic.Title());

    // Set up the content of the application. If the app has a custom titlebar,
    // set that content as well.
    _window->SetContent(_logic.GetRoot());
    _window->OnAppInitialized();
}

// Method Description:
// - Called when the app's title changes. Fires off a window message so we can
//   update the window's title on the main thread.
// Arguments:
// - sender: unused
// - newTitle: the string to use as the new window title
// Return Value:
// - <none>
void AppHost::AppTitleChanged(const winrt::Windows::Foundation::IInspectable& /*sender*/, winrt::hstring newTitle)
{
    _window->UpdateTitle(newTitle.c_str());
}

// Method Description:
// - Called when no tab is remaining to close the window.
// Arguments:
// - sender: unused
// - LastTabClosedEventArgs: unused
// Return Value:
// - <none>
void AppHost::LastTabClosed(const winrt::Windows::Foundation::IInspectable& /*sender*/, const winrt::TerminalApp::LastTabClosedEventArgs& /*args*/)
{
    _window->Close();
}

// Method Description:
// - Resize the window we're about to create to the appropriate dimensions, as
//   specified in the settings. This will be called during the handling of
//   WM_CREATE. We'll load the settings for the app, then get the proposed size
//   of the terminal from the app. Using that proposed size, we'll resize the
//   window we're creating, so that it'll match the values in the settings.
// Arguments:
// - hwnd: The HWND of the window we're about to create.
// - proposedRect: The location and size of the window that we're about to
//   create. We'll use this rect to determine which monitor the window is about
//   to appear on.
// - launchMode: A LaunchMode enum reference that indicates the launch mode
// Return Value:
// - None
void AppHost::_HandleCreateWindow(const HWND hwnd, RECT proposedRect, winrt::TerminalApp::LaunchMode& launchMode)
{
    launchMode = _logic.GetLaunchMode();

    // Acquire the actual intial position
    winrt::Windows::Foundation::Point initialPosition = _logic.GetLaunchInitialPositions(proposedRect.left, proposedRect.top);
    proposedRect.left = gsl::narrow_cast<long>(initialPosition.X);
    proposedRect.top = gsl::narrow_cast<long>(initialPosition.Y);

    long adjustedHeight = 0;
    long adjustedWidth = 0;
    if (launchMode == winrt::TerminalApp::LaunchMode::DefaultMode)
    {
        // Find nearest montitor.
        HMONITOR hmon = MonitorFromRect(&proposedRect, MONITOR_DEFAULTTONEAREST);

        // Get nearest monitor information
        MONITORINFO monitorInfo;
        monitorInfo.cbSize = sizeof(MONITORINFO);
        GetMonitorInfo(hmon, &monitorInfo);

        // This API guarantees that dpix and dpiy will be equal, but neither is an
        // optional parameter so give two UINTs.
        UINT dpix = USER_DEFAULT_SCREEN_DPI;
        UINT dpiy = USER_DEFAULT_SCREEN_DPI;
        // If this fails, we'll use the default of 96.
        GetDpiForMonitor(hmon, MDT_EFFECTIVE_DPI, &dpix, &dpiy);

        // We need to check if the top left point of the titlebar of the window is within any screen
        RECT offScreenTestRect;
        offScreenTestRect.left = proposedRect.left;
        offScreenTestRect.top = proposedRect.top;
        offScreenTestRect.right = offScreenTestRect.left + 1;
        offScreenTestRect.bottom = offScreenTestRect.top + 1;

        bool isTitlebarIntersectWithMonitors = false;
        EnumDisplayMonitors(
            nullptr, &offScreenTestRect, [](HMONITOR, HDC, LPRECT, LPARAM lParam) -> BOOL {
                auto intersectWithMonitor = reinterpret_cast<bool*>(lParam);
                *intersectWithMonitor = true;
                // Continue the enumeration
                return FALSE;
            },
            reinterpret_cast<LPARAM>(&isTitlebarIntersectWithMonitors));

        if (!isTitlebarIntersectWithMonitors)
        {
            // If the title bar is out-of-screen, we set the initial position to
            // the top left corner of the nearest monitor
            proposedRect.left = monitorInfo.rcWork.left;
            proposedRect.top = monitorInfo.rcWork.top;
        }

        auto initialSize = _logic.GetLaunchDimensions(dpix);

        const short islandWidth = Utils::ClampToShortMax(
            static_cast<long>(ceil(initialSize.X)), 1);
        const short islandHeight = Utils::ClampToShortMax(
            static_cast<long>(ceil(initialSize.Y)), 1);

        RECT islandFrame = {};
        bool succeeded = AdjustWindowRectExForDpi(&islandFrame, WS_OVERLAPPEDWINDOW, false, 0, dpix);
        // If we failed to get the correct window size for whatever reason, log
        // the error and go on. We'll use whatever the control proposed as the
        // size of our window, which will be at least close.
        LOG_LAST_ERROR_IF(!succeeded);

        if (_useNonClientArea)
        {
            islandFrame.top = -NonClientIslandWindow::topBorderVisibleHeight;
        }

        adjustedWidth = -islandFrame.left + islandWidth + islandFrame.right;
        adjustedHeight = -islandFrame.top + islandHeight + islandFrame.bottom;
    }

    const COORD origin{ gsl::narrow<short>(proposedRect.left),
                        gsl::narrow<short>(proposedRect.top) };
    const COORD dimensions{ Utils::ClampToShortMax(adjustedWidth, 1),
                            Utils::ClampToShortMax(adjustedHeight, 1) };

    const auto newPos = Viewport::FromDimensions(origin, dimensions);
    bool succeeded = SetWindowPos(hwnd,
                                  nullptr,
                                  newPos.Left(),
                                  newPos.Top(),
                                  newPos.Width(),
                                  newPos.Height(),
                                  SWP_NOACTIVATE | SWP_NOZORDER);

    // Refresh the dpi of HWND becuase the dpi where the window will launch may be different
    // at this time
    _window->RefreshCurrentDPI();

    // If we can't resize the window, that's really okay. We can just go on with
    // the originally proposed window size.
    LOG_LAST_ERROR_IF(!succeeded);

    TraceLoggingWrite(
        g_hWindowsTerminalProvider,
        "WindowCreated",
        TraceLoggingDescription("Event emitted upon creating the application window"),
        TraceLoggingKeyword(MICROSOFT_KEYWORD_MEASURES),
        TelemetryPrivacyDataTag(PDT_ProductAndServicePerformance));
}

// Method Description:
// - Called when the app wants to set its titlebar content. We'll take the
//   UIElement and set the Content property of our Titlebar that element.
// Arguments:
// - sender: unused
// - arg: the UIElement to use as the new Titlebar content.
// Return Value:
// - <none>
void AppHost::_UpdateTitleBarContent(const winrt::Windows::Foundation::IInspectable&, const winrt::Windows::UI::Xaml::UIElement& arg)
{
    if (_useNonClientArea)
    {
        (static_cast<NonClientIslandWindow*>(_window.get()))->SetTitlebarContent(arg);
    }
}

// Method Description:
// - Called when the app wants to change its theme. We'll forward this to the
//   IslandWindow, so it can update the root UI element of the entire XAML tree.
// Arguments:
// - sender: unused
// - arg: the ElementTheme to use as the new theme for the UI
// Return Value:
// - <none>
void AppHost::_UpdateTheme(const winrt::Windows::Foundation::IInspectable&, const winrt::Windows::UI::Xaml::ElementTheme& arg)
{
    _window->OnApplicationThemeChanged(arg);
}

void AppHost::_ToggleFullscreen(const winrt::Windows::Foundation::IInspectable&,
                                const winrt::TerminalApp::ToggleFullscreenEventArgs&)
{
    _window->ToggleFullscreen();
}
