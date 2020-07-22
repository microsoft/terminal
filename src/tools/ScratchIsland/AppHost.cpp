// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "AppHost.h"
#include "../types/inc/Viewport.hpp"
#include "../types/inc/utils.hpp"
#include "../types/inc/User32Utils.hpp"

#include "resource.h"

using namespace winrt::Windows::UI;
using namespace winrt::Windows::UI::Composition;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Controls;
using namespace winrt::Windows::UI::Xaml::Hosting;
using namespace winrt::Windows::Foundation::Numerics;
using namespace ::Microsoft::Console;
using namespace ::Microsoft::Console::Types;

AppHost::AppHost() noexcept :
    _window{ nullptr }
{
    _window = std::make_unique<IslandWindow>();

    // Tell the window to callback to us when it's about to handle a WM_CREATE
    auto pfn = std::bind(&AppHost::_HandleCreateWindow,
                         this,
                         std::placeholders::_1,
                         std::placeholders::_2);
    _window->SetCreateCallback(pfn);

    _window->MakeWindow();
}

AppHost::~AppHost()
{
    // destruction order is important for proper teardown here
    _window = nullptr;
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

    ////////////////////////////////////////////////////////////////////////////
    // Initialize the UI
    ////////////////////////////////////////////////////////////////////////////
    _rootGrid = Grid();
    _swapchainsGrid = Grid();
    _swp0 = SwapChainPanel();
    _swp1 = SwapChainPanel();
    _swp2 = SwapChainPanel();
    _swp3 = SwapChainPanel();
    // Set up the content of the application. If the app has a custom titlebar,
    // set that content as well.
    {
        auto firstRowDef = Controls::RowDefinition();
        firstRowDef.Height(GridLengthHelper::FromValueAndType(1, GridUnitType::Star));

        auto secondRowDef = Controls::RowDefinition();
        secondRowDef.Height(GridLengthHelper::FromValueAndType(9, GridUnitType::Star));

        _rootGrid.RowDefinitions().Append(firstRowDef);
        _rootGrid.RowDefinitions().Append(secondRowDef);
    }
    {
        auto firstRowDef = Controls::RowDefinition();
        firstRowDef.Height(GridLengthHelper::FromValueAndType(1, GridUnitType::Star));

        auto secondRowDef = Controls::RowDefinition();
        secondRowDef.Height(GridLengthHelper::FromValueAndType(1, GridUnitType::Star));

        _swapchainsGrid.RowDefinitions().Append(firstRowDef);
        _swapchainsGrid.RowDefinitions().Append(secondRowDef);

        auto firstColDef = Controls::ColumnDefinition();
        firstColDef.Width(GridLengthHelper::FromValueAndType(1, GridUnitType::Star));

        auto secondColDef = Controls::ColumnDefinition();
        secondColDef.Width(GridLengthHelper::FromValueAndType(1, GridUnitType::Star));

        _swapchainsGrid.ColumnDefinitions().Append(firstColDef);
        _swapchainsGrid.ColumnDefinitions().Append(secondColDef);
    }

    _rootGrid.Children().Append(_swapchainsGrid);
    Controls::Grid::SetRow(_swapchainsGrid, 1);

    {
        Media::SolidColorBrush solidColor{};
        til::color newBgColor{ 0xFFFF0000 };
        solidColor.Color(newBgColor);
        _rootGrid.Background(solidColor);
    }
    {
        Media::SolidColorBrush solidColor{};
        til::color newBgColor{ 0xFF00FF00 };
        solidColor.Color(newBgColor);
        _swapchainsGrid.Background(solidColor);
    }

    winrt::Windows::UI::Xaml::Thickness newMargin = ThicknessHelper::FromUniformLength(4);
    _swp0.Margin(newMargin);
    _swp1.Margin(newMargin);
    _swp2.Margin(newMargin);
    _swp3.Margin(newMargin);

    _rootGrid.Children().Append(_swp0);
    _rootGrid.Children().Append(_swp1);
    _rootGrid.Children().Append(_swp2);
    _rootGrid.Children().Append(_swp3);

    Controls::Grid::SetRow(_swp0, 0);
    Controls::Grid::SetColumn(_swp0, 0);

    Controls::Grid::SetRow(_swp1, 0);
    Controls::Grid::SetColumn(_swp1, 1);

    Controls::Grid::SetRow(_swp2, 1);
    Controls::Grid::SetColumn(_swp2, 0);

    Controls::Grid::SetRow(_swp3, 1);
    Controls::Grid::SetColumn(_swp3, 1);
    _window->SetContent(_rootGrid);
    ////////////////////////////////////////////////////////////////////////////

    _createHost();

    _window->OnAppInitialized();
}

winrt::fire_and_forget AppHost::_createHost()
{
    // {
    //     auto nativePanel2 = _swp0.as<ISwapChainPanelNative2>();
    //     nativePanel2;
    // }

    co_await winrt::resume_background();
    // {
    //     auto nativePanel2 = _swp0.as<ISwapChainPanelNative2>();
    //     nativePanel2;
    // }

    auto host0 = _manager.CreateHost();
    host0.CreateSwapChain(_swp0);

    // wil::unique_handle _swapChainHandle;
    // DCompositionCreateSurfaceHandle(GENERIC_ALL, nullptr, &_swapChainHandle);
    // wil::unique_handle otherProcess = OpenProcess(contentProcessPid);
    // HANDLE otherGuysHandleId = INVALID_HANDLE_VALUE;
    // BOOL ret = DuplicateHandle(GetCurrentProcessHandle(), swapChainHandle.get(), otherProcess.get(), &otherGuysHandleId, 0, FALSE, DUPLICATE_SAME_ACCESS)
    // HANDLE otherGuysHandleId = DuplicateHandle(otherProcess.get(), _swapChainHandle.get());
    // StupidWinRtApiThatTakesInt64(otherGuysHandleId);

    // host0.Attach(_swp0);

    co_await winrt::resume_foreground(_swp0.Dispatcher());

    _swp0_layoutUpdatedRevoker = _swp0.LayoutUpdated(winrt::auto_revoke, [this, host0](auto /*s*/, auto /*e*/) {
        // This event fires every time the layout changes, but it is always the last one to fire
        // in any layout change chain. That gives us great flexibility in finding the right point
        // at which to initialize our renderer (and our terminal).
        // Any earlier than the last layout update and we may not know the terminal's starting size.

        host0.Host().BeginRendering();

        // if (_InitializeTerminal())
        // {
        // Only let this succeed once.
        _swp0_layoutUpdatedRevoker.revoke();
        // }
    });
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
void AppHost::_HandleCreateWindow(const HWND hwnd, RECT proposedRect)
{
    // Acquire the actual initial position
    winrt::Windows::Foundation::Point initialPosition{ (float)proposedRect.left, (float)proposedRect.top };
    proposedRect.left = gsl::narrow_cast<long>(initialPosition.X);
    proposedRect.top = gsl::narrow_cast<long>(initialPosition.Y);

    long adjustedHeight = 0;
    long adjustedWidth = 0;

    // Find nearest monitor.
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

    winrt::Windows::Foundation::Size initialSize{ 800, 600 };

    const short islandWidth = Utils::ClampToShortMax(
        static_cast<long>(ceil(initialSize.Width)), 1);
    const short islandHeight = Utils::ClampToShortMax(
        static_cast<long>(ceil(initialSize.Height)), 1);

    // Get the size of a window we'd need to host that client rect. This will
    // add the titlebar space.
    const auto nonClientSize = _window->GetTotalNonClientExclusiveSize(dpix);
    adjustedWidth = islandWidth + nonClientSize.cx;
    adjustedHeight = islandHeight + nonClientSize.cy;

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

    // Refresh the dpi of HWND because the dpi where the window will launch may be different
    // at this time
    _window->RefreshCurrentDPI();

    // If we can't resize the window, that's really okay. We can just go on with
    // the originally proposed window size.
    LOG_LAST_ERROR_IF(!succeeded);
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
