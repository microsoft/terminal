// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "AppHost.h"

#include <dwmapi.h>
#include <TerminalThemeHelpers.h>
#include <til/latch.h>

#include "VirtualDesktopUtils.h"
#include "WindowEmperor.h"
#include "../types/inc/utils.hpp"

using namespace winrt::Windows::UI;
using namespace winrt::Windows::UI::Composition;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Hosting;
using namespace winrt::Windows::Foundation::Numerics;
using namespace winrt::Microsoft::Terminal;
using namespace winrt::Microsoft::Terminal::Settings::Model;
using namespace ::Microsoft::Console;
using namespace std::chrono_literals;

// This magic flag is "documented" at https://msdn.microsoft.com/en-us/library/windows/desktop/ms646301(v=vs.85).aspx
// "If the high-order bit is 1, the key is down; otherwise, it is up."
static constexpr short KeyPressed{ gsl::narrow_cast<short>(0x8000) };
static constexpr auto FrameUpdateInterval = std::chrono::milliseconds(16);

winrt::com_ptr<IVirtualDesktopManager> getDesktopManager()
{
    static til::shared_mutex<winrt::com_ptr<IVirtualDesktopManager>> s_desktopManager;

    if (const auto manager = *s_desktopManager.lock_shared())
    {
        return manager;
    }

    const auto guard = s_desktopManager.lock();
    if (!*guard)
    {
        *guard = winrt::try_create_instance<IVirtualDesktopManager>(__uuidof(VirtualDesktopManager));
    }
    return *guard;
}

AppHost::AppHost(WindowEmperor* manager, const winrt::TerminalApp::AppLogic& logic, winrt::TerminalApp::WindowRequestedArgs args) noexcept :
    _appLogic{ logic },
    _windowManager{ manager }
{
    _HandleCommandlineArgs(args);

    // _HandleCommandlineArgs will create a _windowLogic
    _useNonClientArea = _windowLogic.GetShowTabsInTitlebar();

    if (_useNonClientArea)
    {
        _window = std::make_unique<NonClientIslandWindow>(_windowLogic.GetRequestedTheme());
    }
    else
    {
        _window = std::make_unique<IslandWindow>();
    }

    // Update our own internal state tracking if we're in quake mode or not.
    _IsQuakeWindowChanged(nullptr, nullptr);

    _window->SetMinimizeToNotificationAreaBehavior(_windowLogic.GetMinimizeToNotificationArea());

    // Tell the window to callback to us when it's about to handle a WM_CREATE
    auto pfn = [this](auto&& PH1, auto&& PH2) { _HandleCreateWindow(std::forward<decltype(PH1)>(PH1), std::forward<decltype(PH2)>(PH2)); };
    _window->SetCreateCallback(pfn);

    _windowCallbacks.MouseScrolled = _window->MouseScrolled({ this, &AppHost::_WindowMouseWheeled });
    _windowCallbacks.WindowActivated = _window->WindowActivated({ this, &AppHost::_WindowActivated });
    _windowCallbacks.WindowMoved = _window->WindowMoved({ this, &AppHost::_WindowMoved });
    _windowCallbacks.ShouldExitFullscreen = _window->ShouldExitFullscreen({ &_windowLogic, &winrt::TerminalApp::TerminalWindow::RequestExitFullscreen });

    _window->MakeWindow();
}

bool AppHost::OnDirectKeyEvent(const uint32_t vkey, const uint8_t scanCode, const bool down)
{
    if (_windowLogic)
    {
        return _windowLogic.OnDirectKeyEvent(vkey, scanCode, down);
    }
    return false;
}

// Method Description:
// - Event handler to update the taskbar progress indicator
// - Upon receiving the event, we ask the underlying logic for the taskbar state/progress values
//   of the last active control
// Arguments:
// - sender: not used
// - args: not used
void AppHost::SetTaskbarProgress(const winrt::Windows::Foundation::IInspectable& /*sender*/,
                                 const winrt::Windows::Foundation::IInspectable& /*args*/)
{
    if (_windowLogic)
    {
        const auto state = _windowLogic.TaskbarState();
        _window->SetTaskbarProgress(gsl::narrow_cast<size_t>(state.State()),
                                    gsl::narrow_cast<size_t>(state.Progress()));
    }
}

// Method Description:
// - Retrieve any commandline args passed on the commandline, and pass them to
//   the WindowManager, to ask if we should become a window process.
// - If we should create a window, then pass the arguments to the app logic for
//   processing.
// - If the logic determined there's an error while processing that commandline,
//   display a message box to the user with the text of the error, and exit.
//    * We display a message box because we're a Win32 application (not a
//      console app), and the shell has undoubtedly returned to the foreground
//      of the console. Text emitted here might mix unexpectedly with output
//      from the shell process.
// Arguments:
// - <none>
// Return Value:
// - <none>
void AppHost::_HandleCommandlineArgs(const winrt::TerminalApp::WindowRequestedArgs& windowArgs)
{
    // We did want to make a window, so let's instantiate it here.
    // We don't have XAML yet, but we do have other stuff.
    _windowLogic = _appLogic.CreateNewWindow();

    if (const auto content = windowArgs.Content(); !content.empty())
    {
        _windowLogic.SetStartupContent(content, windowArgs.InitialBounds());
        _launchShowWindowCommand = SW_NORMAL;
    }
    else
    {
        const auto args = windowArgs.Command();
        _windowLogic.SetStartupCommandline(args);
        _launchShowWindowCommand = args.ShowWindowCommand();
    }

    // This is a fix for GH#12190 and hopefully GH#12169.
    //
    // If the commandline we were provided is going to result in us only
    // opening elevated terminal instances, then we need to not even create
    // the window at all here. In that case, we're going through this
    // special escape hatch to dispatch all the calls to elevate-shim, and
    // then we're going to exit immediately.
    if (_windowLogic.ShouldImmediatelyHandoffToElevated())
    {
        _windowLogic.HandoffToElevated();
        return;
    }

    _windowLogic.WindowName(windowArgs.WindowName());
    _windowLogic.WindowId(windowArgs.Id());
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
    // You aren't allowed to do ANY XAML before this line!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    _window->Initialize();

    if (auto withWindow{ _windowLogic.try_as<IInitializeWithWindow>() })
    {
        // You aren't allowed to do anything with the TerminalPage before this line!!!!!!!
        withWindow->Initialize(_window->GetHandle());
    }

    if (_useNonClientArea)
    {
        // Register our callback for when the app's non-client content changes.
        // This has to be done _before_ App::Create, as the app might set the
        // content in Create.
        _windowLogic.SetTitleBarContent({ this, &AppHost::_UpdateTitleBarContent });
    }

    // These call APIs that are reentrant on the window message loop. If
    // you call them in the ctor, we might deadlock. The ctor for AppHost isn't
    // always called on the window thread - for reheated windows, it could be
    // called on a random COM thread.

    _window->SetAlwaysOnTop(_windowLogic.GetInitialAlwaysOnTop());
    _window->SetAutoHideWindow(_windowLogic.AutoHideWindow());
    _window->SetShowTabsFullscreen(_windowLogic.GetInitialShowTabsFullscreen());

    // MORE EVENT HANDLERS HERE!
    // MAKE SURE THEY ARE ALL:
    // * winrt::auto_revoke
    // * revoked manually in the dtor before the window is nulled out.
    //
    // If you don't, then it's possible for them to get triggered as the app is
    // tearing down, after we've nulled out the window, during the dtor. That
    // can cause unexpected AV's everywhere.
    //
    // _window callbacks are a little special:
    // * IslandWindow isn't a WinRT type (so it doesn't have neat revokers like
    //   this), so instead they go in their own special helper struct.
    // * they all need to be manually revoked in _revokeWindowCallbacks.

    // Register the 'X' button of the window for a warning experience of multiple
    // tabs opened, this is consistent with Alt+F4 closing
    _windowCallbacks.WindowCloseButtonClicked = _window->WindowCloseButtonClicked([this]() {
        _windowLogic.CloseWindow();
    });

    // Add an event handler to plumb clicks in the titlebar area down to the
    // application layer.
    _windowCallbacks.DragRegionClicked = _window->DragRegionClicked([this]() { _windowLogic.TitlebarClicked(); });

    _windowCallbacks.WindowVisibilityChanged = _window->WindowVisibilityChanged([this](bool showOrHide) { _windowLogic.WindowVisibilityChanged(showOrHide); });

    _revokers.Initialized = _windowLogic.Initialized(winrt::auto_revoke, { this, &AppHost::_WindowInitializedHandler });
    _revokers.RequestedThemeChanged = _windowLogic.RequestedThemeChanged(winrt::auto_revoke, { this, &AppHost::_UpdateTheme });
    _revokers.FullscreenChanged = _windowLogic.FullscreenChanged(winrt::auto_revoke, { this, &AppHost::_FullscreenChanged });
    _revokers.FocusModeChanged = _windowLogic.FocusModeChanged(winrt::auto_revoke, { this, &AppHost::_FocusModeChanged });
    _revokers.AlwaysOnTopChanged = _windowLogic.AlwaysOnTopChanged(winrt::auto_revoke, { this, &AppHost::_AlwaysOnTopChanged });
    _revokers.RaiseVisualBell = _windowLogic.RaiseVisualBell(winrt::auto_revoke, { this, &AppHost::_RaiseVisualBell });
    _revokers.SystemMenuChangeRequested = _windowLogic.SystemMenuChangeRequested(winrt::auto_revoke, { this, &AppHost::_SystemMenuChangeRequested });
    _revokers.ChangeMaximizeRequested = _windowLogic.ChangeMaximizeRequested(winrt::auto_revoke, { this, &AppHost::_ChangeMaximizeRequested });
    _revokers.RequestLaunchPosition = _windowLogic.RequestLaunchPosition(winrt::auto_revoke, { this, &AppHost::_HandleRequestLaunchPosition });

    _windowCallbacks.MaximizeChanged = _window->MaximizeChanged([this](bool newMaximize) {
        if (_windowLogic)
        {
            _windowLogic.Maximized(newMaximize);
        }
    });

    // Load bearing: make sure the PropertyChanged handler is added before we
    // call Create, so that when the app sets up the titlebar brush, we're
    // already prepared to listen for the change notification
    _revokers.PropertyChanged = _windowLogic.PropertyChanged(winrt::auto_revoke, { this, &AppHost::_PropertyChangedHandler });

    _appLogic.Create();
    _windowLogic.Create();

    _revokers.TitleChanged = _windowLogic.TitleChanged(winrt::auto_revoke, { this, &AppHost::_AppTitleChanged });
    _revokers.CloseWindowRequested = _windowLogic.CloseWindowRequested(winrt::auto_revoke, { this, &AppHost::_CloseRequested });
    _revokers.SetTaskbarProgress = _windowLogic.SetTaskbarProgress(winrt::auto_revoke, { this, &AppHost::SetTaskbarProgress });
    _revokers.IdentifyWindowsRequested = _windowLogic.IdentifyWindowsRequested(winrt::auto_revoke, { this, &AppHost::_IdentifyWindowsRequested });
    _revokers.WindowSizeChanged = _windowLogic.WindowSizeChanged(winrt::auto_revoke, { this, &AppHost::_WindowSizeChanged });

    // A note: make sure to listen to our _window_'s settings changed, not the
    // AppLogic's. We want to make sure the event has gone through the window
    // logic _before_ we handle it, so we can ask the window about it's newest
    // properties.
    _revokers.SettingsChanged = _windowLogic.SettingsChanged(winrt::auto_revoke, { this, &AppHost::_HandleSettingsChanged });

    _revokers.IsQuakeWindowChanged = _windowLogic.IsQuakeWindowChanged(winrt::auto_revoke, { this, &AppHost::_IsQuakeWindowChanged });
    _revokers.SummonWindowRequested = _windowLogic.SummonWindowRequested(winrt::auto_revoke, { this, &AppHost::_SummonWindowRequested });
    _revokers.OpenSystemMenu = _windowLogic.OpenSystemMenu(winrt::auto_revoke, { this, &AppHost::_OpenSystemMenu });
    _revokers.QuitRequested = _windowLogic.QuitRequested(winrt::auto_revoke, { this, &AppHost::_RequestQuitAll });
    _revokers.ShowWindowChanged = _windowLogic.ShowWindowChanged(winrt::auto_revoke, { this, &AppHost::_ShowWindowChanged });
    _revokers.RequestMoveContent = _windowLogic.RequestMoveContent(winrt::auto_revoke, { this, &AppHost::_handleMoveContent });
    _revokers.RequestReceiveContent = _windowLogic.RequestReceiveContent(winrt::auto_revoke, { this, &AppHost::_handleReceiveContent });

    // BODGY
    // On certain builds of Windows, when Terminal is set as the default
    // it will accumulate an unbounded amount of queued animations while
    // the screen is off and it is servicing window management for console
    // applications. This call into TerminalThemeHelpers will tell our
    // compositor to automatically complete animations that are scheduled
    // while the screen is off.
    TerminalTrySetAutoCompleteAnimationsWhenOccluded(static_cast<::IUnknown*>(winrt::get_abi(_windowLogic.GetRoot())), true);

    _window->SetSnapDimensionCallback([this](auto&& PH1, auto&& PH2) { return _windowLogic.CalcSnappedDimension(std::forward<decltype(PH1)>(PH1), std::forward<decltype(PH2)>(PH2)); });

    // Create a throttled function for updating the window state, to match the
    // one requested by the pty. A 200ms delay was chosen because it's the
    // typical animation timeout in Windows. This does result in a delay between
    // the PTY requesting a change to the window state and the Terminal
    // realizing it, but should mitigate issues where the Terminal and PTY get
    // de-sync'd.
    _showHideWindowThrottler = std::make_shared<ThrottledFuncTrailing<bool>>(
        winrt::Windows::System::DispatcherQueue::GetForCurrentThread(),
        std::chrono::milliseconds(200),
        [this](const bool show) {
            _window->ShowWindowChanged(show);
        });

    _window->UpdateTitle(_windowLogic.Title());

    // Set up the content of the application. If the app has a custom titlebar,
    // set that content as well.
    _window->SetContent(_windowLogic.GetRoot());
    _window->OnAppInitialized();
}

void AppHost::Close()
{
    // After calling _window->Close() we should avoid creating more WinUI related actions.
    // I suspect WinUI wouldn't like that very much. As such unregister all event handlers first.
    _revokers = {};
    _frameTimer.Destroy();
    _showHideWindowThrottler.reset();

    _revokeWindowCallbacks();

    _window->Close();

    if (_windowLogic)
    {
        _windowLogic.DismissDialog();
        _windowLogic = nullptr;
    }
}

int64_t AppHost::GetLastActivatedTime() const noexcept
{
    return _lastActivatedTime.QuadPart;
}

// Lazily gets the virtual desktop ID for this window.
winrt::Windows::Foundation::IAsyncOperation<winrt::guid> AppHost::GetVirtualDesktopId()
{
    static constexpr winrt::guid null_guid{};

    if (_virtualDesktopId != null_guid)
    {
        co_return _virtualDesktopId;
    }

    const auto dispatcher = _windowLogic.GetRoot().Dispatcher();
    const auto hwnd = _window->GetHandle();
    const auto weakThis = weak_from_this();
    const auto desktopManager = getDesktopManager();

    if (!hwnd || !desktopManager)
    {
        co_return null_guid;
    }

    // The amazing IVirtualDesktopManager API is cross-process COM into explorer.exe,
    // so we can't call it on the UI thread (= slow & reentrant = bugs/freezes).
    // Fun fact: GetWindowDesktopId() is O(n) over all HWNDs. :)
    co_await winrt::resume_background();

    GUID id;
    if (FAILED_LOG(desktopManager->GetWindowDesktopId(hwnd, &id)))
    {
        co_return null_guid;
    }

    co_await wil::resume_foreground(dispatcher);

    const auto strongThis = weakThis.lock();
    if (!strongThis)
    {
        co_return null_guid;
    }

    _virtualDesktopId = winrt::guid{ id };
    co_return _virtualDesktopId;
}

IslandWindow* AppHost::GetWindow() const noexcept
{
    return _window.get();
}

void AppHost::_revokeWindowCallbacks()
{
    // You'll recall, IslandWindow isn't a WinRT type so it can't have auto-revokers.
    //
    // Instead, we need to manually remove our callbacks we registered on the window object.
    _window->MouseScrolled(_windowCallbacks.MouseScrolled);
    _window->WindowActivated(_windowCallbacks.WindowActivated);
    _window->WindowMoved(_windowCallbacks.WindowMoved);
    _window->ShouldExitFullscreen(_windowCallbacks.ShouldExitFullscreen);
    _window->WindowCloseButtonClicked(_windowCallbacks.WindowCloseButtonClicked);
    _window->DragRegionClicked(_windowCallbacks.DragRegionClicked);
    _window->WindowVisibilityChanged(_windowCallbacks.WindowVisibilityChanged);
    _window->MaximizeChanged(_windowCallbacks.MaximizeChanged);
}

// Method Description:
// - Called every time when the active tab's title changes. We'll also fire off
//   a window message so we can update the window's title on the main thread,
//   though we'll only do so if the settings are configured for that.
// Arguments:
// - sender: unused
// - newTitle: the string to use as the new window title
// Return Value:
// - <none>
void AppHost::_AppTitleChanged(const winrt::Windows::Foundation::IInspectable& /*sender*/, winrt::hstring newTitle)
{
    if (_windowLogic.GetShowTitleInTitlebar())
    {
        _window->UpdateTitle(newTitle);
    }
}

// The terminal page is responsible for persisting its own state, but it does
// need to ask us where exactly on the screen the window is.
void AppHost::_HandleRequestLaunchPosition(const winrt::Windows::Foundation::IInspectable& /*sender*/,
                                           winrt::TerminalApp::LaunchPositionRequest args)
{
    args.Position(_GetWindowLaunchPosition());
}

LaunchPosition AppHost::_GetWindowLaunchPosition()
{
    LaunchPosition pos{};
    // If we started saving before closing, but didn't resume the event handler
    // until after _window might be a nullptr.
    if (!_window)
    {
        return pos;
    }

    try
    {
        // Get the position of the current window. This includes the
        // non-client already.
        const auto window = _window->GetWindowRect();

        const auto dpi = _window->GetCurrentDpi();
        const auto nonClientArea = _window->GetNonClientFrame(dpi);

        // The nonClientArea adjustment is negative, so subtract that out.
        // This way we save the user-visible location of the terminal.
        pos.X = window.left - nonClientArea.left;
        pos.Y = window.top;
    }
    CATCH_LOG();

    return pos;
}

// Method Description:
// - Callback for when the window is first being created (during WM_CREATE).
//   Stash the proposed size for later. We'll need that once we're totally
//   initialized, so that we can show the window in the right position *when we
//   want to show it*. If we did the _initialResizeAndRepositionWindow work now,
//   it would have no effect, because the window is not yet visible.
// Arguments:
// - hwnd: The HWND of the window we're about to create.
// - proposedRect: The location and size of the window that we're about to
//   create. We'll use this rect to determine which monitor the window is about
//   to appear on.
void AppHost::_HandleCreateWindow(const HWND /* hwnd */, const til::rect& proposedRect)
{
    // GH#11561: Hide the window until we're totally done being initialized.
    // More commentary in TerminalPage::_CompleteInitialization
    _initialResizeAndRepositionWindow(_window->GetHandle(), proposedRect, _launchMode);
}

// Method Description:
// - Resize the window we're about to create to the appropriate dimensions, as
//   specified in the settings. This is called once the app has finished it's
//   initial setup, once we have created all the tabs, panes, etc. We'll load
//   the settings for the app, then get the proposed size of the terminal from
//   the app. Using that proposed size, we'll resize the window we're creating,
//   so that it'll match the values in the settings.
// Arguments:
// - hwnd: The HWND of the window we're about to create.
// - proposedRect: The location and size of the window that we're about to
//   create. We'll use this rect to determine which monitor the window is about
//   to appear on.
// - launchMode: A LaunchMode enum reference that indicates the launch mode
// Return Value:
// - None
void AppHost::_initialResizeAndRepositionWindow(const HWND hwnd, til::rect proposedRect, LaunchMode& launchMode)
{
    launchMode = _windowLogic.GetLaunchMode();

    // Acquire the actual initial position
    auto initialPos = _windowLogic.GetInitialPosition(proposedRect.left, proposedRect.top);
    const auto centerOnLaunch = _windowLogic.CenterOnLaunch();
    proposedRect.left = gsl::narrow<til::CoordType>(initialPos.X);
    proposedRect.top = gsl::narrow<til::CoordType>(initialPos.Y);

    long adjustedHeight = 0;
    long adjustedWidth = 0;

    // Find nearest monitor.
    auto hmon = MonitorFromRect(proposedRect.as_win32_rect(), MONITOR_DEFAULTTONEAREST);

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

    auto isTitlebarIntersectWithMonitors = false;
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

    auto initialSize = _windowLogic.GetLaunchDimensions(dpix);

    const auto islandWidth = Utils::ClampToShortMax(lrintf(initialSize.Width), 1);
    const auto islandHeight = Utils::ClampToShortMax(lrintf(initialSize.Height), 1);

    // Get the size of a window we'd need to host that client rect. This will
    // add the titlebar space.
    const til::size nonClientSize{ _window->GetTotalNonClientExclusiveSize(dpix) };
    const til::rect nonClientFrame{ _window->GetNonClientFrame(dpix) };
    adjustedWidth = islandWidth + nonClientSize.width;
    adjustedHeight = islandHeight + nonClientSize.height;

    til::size dimensions{ Utils::ClampToShortMax(adjustedWidth, 1),
                          Utils::ClampToShortMax(adjustedHeight, 1) };

    // Find nearest monitor for the position that we've actually settled on
    auto hMonNearest = MonitorFromRect(proposedRect.as_win32_rect(), MONITOR_DEFAULTTONEAREST);
    MONITORINFO nearestMonitorInfo;
    nearestMonitorInfo.cbSize = sizeof(MONITORINFO);
    // Get monitor dimensions:
    GetMonitorInfo(hMonNearest, &nearestMonitorInfo);
    const til::size desktopDimensions{ nearestMonitorInfo.rcWork.right - nearestMonitorInfo.rcWork.left,
                                       nearestMonitorInfo.rcWork.bottom - nearestMonitorInfo.rcWork.top };

    // GH#10583 - Adjust the position of the rectangle to account for the size
    // of the invisible borders on the left/right. We DON'T want to adjust this
    // for the top here - the IslandWindow includes the titlebar in
    // nonClientFrame.top, so adjusting for that would actually place the
    // titlebar _off_ the monitor.
    til::point origin{ (proposedRect.left + nonClientFrame.left),
                       (proposedRect.top) };

    if (_windowLogic.IsQuakeWindow())
    {
        // If we just use rcWork by itself, we'll fail to account for the invisible
        // space reserved for the resize handles. So retrieve that size here.
        const auto availableSpace = desktopDimensions + nonClientSize;

        origin = {
            (nearestMonitorInfo.rcWork.left - (nonClientSize.width / 2)),
            (nearestMonitorInfo.rcWork.top)
        };
        dimensions = {
            availableSpace.width,
            availableSpace.height / 2
        };
        launchMode = LaunchMode::FocusMode;
    }
    else if (centerOnLaunch)
    {
        // Move our proposed location into the center of that specific monitor.
        origin = {
            (nearestMonitorInfo.rcWork.left + ((desktopDimensions.width / 2) - (dimensions.width / 2))),
            (nearestMonitorInfo.rcWork.top + ((desktopDimensions.height / 2) - (dimensions.height / 2)))
        };
    }

    const til::rect newRect{ origin, dimensions };
    bool succeeded = SetWindowPos(hwnd,
                                  nullptr,
                                  newRect.left,
                                  newRect.top,
                                  newRect.width(),
                                  newRect.height(),
                                  SWP_NOACTIVATE | SWP_NOZORDER);

    // Refresh the dpi of HWND because the dpi where the window will launch may be different
    // at this time
    _window->RefreshCurrentDPI();

    // If we can't resize the window, that's really okay. We can just go on with
    // the originally proposed window size.
    LOG_LAST_ERROR_IF(!succeeded);
}

// Method Description:
// - Resize the window when window size changed signal is received.
// Arguments:
// - hwnd: The HWND of the window we're about to resize.
// - newSize: The new size of the window in pixels.
// Return Value:
// - None
void AppHost::_resizeWindow(const HWND hwnd, til::size newSize)
{
    til::rect windowRect{ _window->GetWindowRect() };
    UINT dpix = _window->GetCurrentDpi();

    const auto islandWidth = Utils::ClampToShortMax(newSize.width, 1);
    const auto islandHeight = Utils::ClampToShortMax(newSize.height, 1);

    // Get the size of a window we'd need to host that client rect. This will
    // add the titlebar space.
    const til::size nonClientSize{ _window->GetTotalNonClientExclusiveSize(dpix) };
    long adjustedWidth = islandWidth + nonClientSize.width;
    long adjustedHeight = islandHeight + nonClientSize.height;

    til::size dimensions{ Utils::ClampToShortMax(adjustedWidth, 1),
                          Utils::ClampToShortMax(adjustedHeight, 1) };
    til::point origin{ windowRect.left, windowRect.top };

    const til::rect newRect{ origin, dimensions };
    bool succeeded = SetWindowPos(hwnd,
                                  nullptr,
                                  newRect.left,
                                  newRect.top,
                                  newRect.width(),
                                  newRect.height(),
                                  SWP_NOACTIVATE | SWP_NOZORDER);

    // If we can't resize the window, that's really okay. We can just go on with
    // the originally proposed window size.
    LOG_LAST_ERROR_IF(!succeeded);
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
        auto nonClientWindow{ static_cast<NonClientIslandWindow*>(_window.get()) };
        nonClientWindow->SetTitlebarContent(arg);
        nonClientWindow->SetTitlebarBackground(_windowLogic.TitlebarBrush());
    }

    _updateTheme();
}

// Method Description:
// - Called when the app wants to change its theme. We'll forward this to the
//   IslandWindow, so it can update the root UI element of the entire XAML tree.
// Arguments:
// - sender: unused
// - arg: the ElementTheme to use as the new theme for the UI
// Return Value:
// - <none>
void AppHost::_UpdateTheme(const winrt::Windows::Foundation::IInspectable&,
                           const winrt::Microsoft::Terminal::Settings::Model::Theme& /*arg*/)
{
    _updateTheme();
}

void AppHost::_FocusModeChanged(const winrt::Windows::Foundation::IInspectable&,
                                const winrt::Windows::Foundation::IInspectable&)
{
    _window->FocusModeChanged(_windowLogic.FocusMode());
}

void AppHost::_FullscreenChanged(const winrt::Windows::Foundation::IInspectable&,
                                 const winrt::Windows::Foundation::IInspectable&)
{
    _window->FullscreenChanged(_windowLogic.Fullscreen());
}

void AppHost::_ChangeMaximizeRequested(const winrt::Windows::Foundation::IInspectable&,
                                       const winrt::Windows::Foundation::IInspectable&)
{
    if (const auto handle = _window->GetHandle())
    {
        // Shamelessly copied from TitlebarControl::_OnMaximizeOrRestore
        // since there doesn't seem to be another way to handle this
        POINT point1 = {};
        ::GetCursorPos(&point1);
        const auto lParam = MAKELPARAM(point1.x, point1.y);
        WINDOWPLACEMENT placement = { sizeof(placement) };
        ::GetWindowPlacement(handle, &placement);
        if (placement.showCmd == SW_SHOWNORMAL)
        {
            ::PostMessage(handle, WM_SYSCOMMAND, SC_MAXIMIZE, lParam);
        }
        else if (placement.showCmd == SW_SHOWMAXIMIZED)
        {
            ::PostMessage(handle, WM_SYSCOMMAND, SC_RESTORE, lParam);
        }
    }
}

void AppHost::_AlwaysOnTopChanged(const winrt::Windows::Foundation::IInspectable&,
                                  const winrt::Windows::Foundation::IInspectable&)
{
    // MSFT:34662459
    //
    // Although we're manually revoking the event handler now in the dtor before
    // we null out the window, let's be extra careful and check JUST IN CASE.
    if (_window == nullptr)
    {
        return;
    }

    _window->SetAlwaysOnTop(_windowLogic.AlwaysOnTop());
}

// Method Description
// - Called when the app wants to flash the taskbar, indicating to the user that
//   something needs their attention
// Arguments
// - <unused>
void AppHost::_RaiseVisualBell(const winrt::Windows::Foundation::IInspectable&,
                               const winrt::Windows::Foundation::IInspectable&)
{
    _window->FlashTaskbar();
}

// Method Description:
// - Called when the IslandWindow has received a WM_MOUSEWHEEL message. This can
//   happen on some laptops, where their trackpads won't scroll inactive windows
//   _ever_.
// - We're going to take that message and manually plumb it through to our
//   TermControl's, or anything else that implements IMouseWheelListener.
// - See GH#979 for more details.
// Arguments:
// - coord: The Window-relative, logical coordinates location of the mouse during this event.
// - delta: the wheel delta that triggered this event.
// Return Value:
// - <none>
void AppHost::_WindowMouseWheeled(const winrt::Windows::Foundation::Point coord, const int32_t delta)
{
    if (_windowLogic)
    {
        // Find all the elements that are underneath the mouse
        auto elems = Xaml::Media::VisualTreeHelper::FindElementsInHostCoordinates(coord, _windowLogic.GetRoot());
        for (const auto& e : elems)
        {
            // If that element has implemented IMouseWheelListener, call OnMouseWheel on that element.
            if (auto control{ e.try_as<Control::IMouseWheelListener>() })
            {
                try
                {
                    // Translate the event to the coordinate space of the control
                    // we're attempting to dispatch it to
                    const auto transform = e.TransformToVisual(nullptr);
                    const auto controlOrigin = transform.TransformPoint({});

                    const winrt::Windows::Foundation::Point offsetPoint{
                        coord.X - controlOrigin.X,
                        coord.Y - controlOrigin.Y,
                    };

                    const auto lButtonDown = WI_IsFlagSet(GetKeyState(VK_LBUTTON), KeyPressed);
                    const auto mButtonDown = WI_IsFlagSet(GetKeyState(VK_MBUTTON), KeyPressed);
                    const auto rButtonDown = WI_IsFlagSet(GetKeyState(VK_RBUTTON), KeyPressed);

                    if (control.OnMouseWheel(offsetPoint, delta, lButtonDown, mButtonDown, rButtonDown))
                    {
                        // If the element handled the mouse wheel event, don't
                        // continue to iterate over the remaining controls.
                        break;
                    }
                }
                CATCH_LOG();
            }
        }
    }
}

// Method Description:
// - Event handler for the Peasant::ExecuteCommandlineRequested event. Take the
//   provided commandline args, and attempt to parse them and perform the
//   actions immediately. The parsing is performed by AppLogic.
// - This is invoked when another wt.exe instance runs something like `wt -w 1
//   new-tab`, and the Monarch delegates the commandline to this instance.
// Arguments:
// - args: the bundle of a commandline and working directory to use for this invocation.
// Return Value:
// - <none>
void AppHost::DispatchCommandline(winrt::TerminalApp::CommandlineArgs args)
{
    winrt::TerminalApp::SummonWindowBehavior summonArgs{};
    summonArgs.MoveToCurrentDesktop(false);
    summonArgs.DropdownDuration(0);
    summonArgs.ToMonitor(winrt::TerminalApp::MonitorBehavior::InPlace);
    summonArgs.ToggleVisibility(false); // Do not toggle, just make visible.
    // Summon the window whenever we dispatch a commandline to it. This will
    // make it obvious when a new tab/pane is created in a window.
    HandleSummon(std::move(summonArgs));
    _windowLogic.ExecuteCommandline(std::move(args));
}

void AppHost::_WindowActivated(bool activated)
{
    _windowLogic.WindowActivated(activated);

    if (activated)
    {
        QueryPerformanceCounter(&_lastActivatedTime);
        _virtualDesktopId = {};
    }
}

safe_void_coroutine AppHost::HandleSummon(const winrt::TerminalApp::SummonWindowBehavior args) const
{
    _window->SummonWindow(args);

    if (!args || !args.MoveToCurrentDesktop())
    {
        co_return;
    }

    const auto desktopManager = getDesktopManager();
    if (!desktopManager)
    {
        co_return;
    }

    // Just like AppHost::GetVirtualDesktopId:
    // IVirtualDesktopManager is cross-process COM into explorer.exe,
    // and we can't use that on the UI thread.
    co_await winrt::resume_background();

    // First thing - make sure that we're not on the current desktop. If
    // we are, then don't call MoveWindowToDesktop. This is to mitigate
    // MSFT:33035972
    BOOL onCurrentDesktop{ false };
    if (SUCCEEDED(desktopManager->IsWindowOnCurrentVirtualDesktop(_window->GetHandle(), &onCurrentDesktop)) && onCurrentDesktop)
    {
        // If we succeeded, and the window was on the current desktop, then do nothing.
    }
    else
    {
        // Here, we either failed to check if the window is on the
        // current desktop, or it wasn't on that desktop. In both those
        // cases, just move the window.

        GUID currentlyActiveDesktop{ 0 };
        if (VirtualDesktopUtils::GetCurrentVirtualDesktopId(&currentlyActiveDesktop))
        {
            LOG_IF_FAILED(desktopManager->MoveWindowToDesktop(_window->GetHandle(), currentlyActiveDesktop));
        }
        // If GetCurrentVirtualDesktopId failed, then just leave the window
        // where it is. Nothing else to be done :/
    }
}

// Method Description:
// - Called when this window wants _all_ windows to display their
//   identification. We'll hop to the BG thread, and raise an event (eventually
//   handled by the monarch) to bubble this request to all the Terminal windows.
// Arguments:
// - <unused>
// Return Value:
// - <none>
void AppHost::_IdentifyWindowsRequested(const winrt::Windows::Foundation::IInspectable /*sender*/,
                                        const winrt::Windows::Foundation::IInspectable /*args*/)
{
    PostMessageW(_windowManager->GetMainWindow(), WindowEmperor::WM_IDENTIFY_ALL_WINDOWS, 0, 0);
}

// Method Description:
// - Called when the monarch wants us to display our window ID. We'll call down
//   to the app layer to display the toast.
// Arguments:
// - <unused>
// Return Value:
// - <none>
void AppHost::_DisplayWindowId(const winrt::Windows::Foundation::IInspectable& /*sender*/,
                               const winrt::Windows::Foundation::IInspectable& /*args*/)
{
    _windowLogic.IdentifyWindow();
}

static double _opacityFromBrush(const winrt::Windows::UI::Xaml::Media::Brush& brush)
{
    if (auto acrylic = brush.try_as<winrt::Windows::UI::Xaml::Media::AcrylicBrush>())
    {
        return acrylic.TintOpacity();
    }
    else if (auto solidColor = brush.try_as<winrt::Windows::UI::Xaml::Media::SolidColorBrush>())
    {
        return solidColor.Opacity();
    }
    return 1.0;
}

static bool _isActuallyDarkTheme(const auto requestedTheme)
{
    switch (requestedTheme)
    {
    case winrt::Windows::UI::Xaml::ElementTheme::Light:
        return false;
    case winrt::Windows::UI::Xaml::ElementTheme::Dark:
        return true;
    case winrt::Windows::UI::Xaml::ElementTheme::Default:
    default:
        return Theme::IsSystemInDarkTheme();
    }
}

// DwmSetWindowAttribute(... DWMWA_BORDER_COLOR.. ) doesn't work on Windows 10,
// but it _will_ spew to the debug console. This helper just no-ops the call on
// Windows 10, so that we don't even get that spew
void _frameColorHelper(const HWND h, const COLORREF color)
{
    if (Utils::IsWindows11())
    {
        LOG_IF_FAILED(DwmSetWindowAttribute(h, DWMWA_BORDER_COLOR, &color, sizeof(color)));
    }
}

void AppHost::_updateTheme()
{
    auto theme = _appLogic.Settings().GlobalSettings().CurrentTheme();

    _window->OnApplicationThemeChanged(theme.RequestedTheme());

    const auto windowTheme{ theme.Window() };

    const auto b = _windowLogic.TitlebarBrush();
    const auto color = ThemeColor::ColorFromBrush(b);
    const auto colorOpacity = b ? color.A / 255.0 : 0.0;
    const auto brushOpacity = _opacityFromBrush(b);
    const auto opacity = std::min(colorOpacity, brushOpacity);
    _window->UseMica(windowTheme ? windowTheme.UseMica() : false, opacity);

    // This is a hack to make the window borders dark instead of light.
    // It must be done before WM_NCPAINT so that the borders are rendered with
    // the correct theme.
    // For more information, see GH#6620.
    _window->UseDarkTheme(_isActuallyDarkTheme(theme.RequestedTheme()));

    // Update the window frame. If `rainbowFrame:true` is enabled, then that
    // will be used. Otherwise we'll try to use the `FrameBrush` set in the
    // terminal window, as that will have the right color for the ThemeColor for
    // this setting. If that value is null, then revert to the default frame
    // color.
    if (windowTheme)
    {
        if (windowTheme.RainbowFrame())
        {
            _startFrameTimer();
        }
        else if (const auto b{ _windowLogic.FrameBrush() })
        {
            _stopFrameTimer();
            const auto color = ThemeColor::ColorFromBrush(b);
            COLORREF ref{ til::color{ color } };
            _frameColorHelper(_window->GetHandle(), ref);
        }
        else
        {
            _stopFrameTimer();
            // DWMWA_COLOR_DEFAULT is the magic "reset to the default" value
            _frameColorHelper(_window->GetHandle(), DWMWA_COLOR_DEFAULT);
        }
    }
}

void AppHost::_startFrameTimer()
{
    // Instantiate the frame color timer, if we haven't already. We'll only ever
    // create one instance of this. We'll set up the callback for the timers as
    // _updateFrameColor, which will actually handle setting the colors. If we
    // already have a timer, just start that one.

    _frameTimer.Tick({ this, &AppHost::_updateFrameColor });
    _frameTimer.Interval(FrameUpdateInterval);
    _frameTimer.Start();
}

void AppHost::_stopFrameTimer()
{
    if (_frameTimer)
    {
        _frameTimer.Stop();
    }
}

// Method Description:
// - Updates the color of the window frame to cycle through all the colors. This
//   is called as the `_frameTimer` Tick callback, roughly 60 times per second.
void AppHost::_updateFrameColor(const winrt::Windows::Foundation::IInspectable&, const winrt::Windows::Foundation::IInspectable&)
{
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);

    const auto period = freq.QuadPart * 4;
    const auto mod = counter.QuadPart % period;
    const auto hue = static_cast<float>(mod) / static_cast<float>(period);
    const auto color = til::color::from_hue(hue);

    _frameColorHelper(_window->GetHandle(), color);
}

void AppHost::_HandleSettingsChanged(const winrt::Windows::Foundation::IInspectable& /*sender*/,
                                     const winrt::TerminalApp::SettingsLoadEventArgs& /*args*/)
{
    // We don't need to call in to windowLogic here - it has its own SettingsChanged handler

    _window->SetMinimizeToNotificationAreaBehavior(_windowLogic.GetMinimizeToNotificationArea());
    _window->SetAutoHideWindow(_windowLogic.AutoHideWindow());
    _window->SetShowTabsFullscreen(_windowLogic.ShowTabsFullscreen());
    _updateTheme();
}

void AppHost::_IsQuakeWindowChanged(const winrt::Windows::Foundation::IInspectable&,
                                    const winrt::Windows::Foundation::IInspectable&)
{
    _window->IsQuakeWindow(_windowLogic.IsQuakeWindow());
}

// Raised from TerminalWindow. We handle by bubbling the request to the window manager.
void AppHost::_RequestQuitAll(const winrt::Windows::Foundation::IInspectable&,
                              const winrt::Windows::Foundation::IInspectable&)
{
    PostQuitMessage(0);
}

void AppHost::_ShowWindowChanged(const winrt::Windows::Foundation::IInspectable&,
                                 const winrt::Microsoft::Terminal::Control::ShowWindowArgs& args)
{
    // GH#13147: Enqueue a throttled update to our window state. Throttling
    // should prevent scenarios where the Terminal window state and PTY window
    // state get de-sync'd, and cause the window to minimize/restore constantly
    // in a loop.
    _showHideWindowThrottler->Run(args.ShowOrHide());
}

void AppHost::_WindowSizeChanged(const winrt::Windows::Foundation::IInspectable& /*sender*/,
                                 const winrt::Microsoft::Terminal::Control::WindowSizeChangedEventArgs& args)
{
    _resizeWindow(_window->GetHandle(), { args.Width(), args.Height() });
}

void AppHost::_SummonWindowRequested(const winrt::Windows::Foundation::IInspectable&,
                                     const winrt::Windows::Foundation::IInspectable&)
{
    winrt::TerminalApp::SummonWindowBehavior summonArgs;
    summonArgs.MoveToCurrentDesktop(false);
    summonArgs.DropdownDuration(0);
    summonArgs.ToMonitor(winrt::TerminalApp::MonitorBehavior::InPlace);
    summonArgs.ToggleVisibility(false); // Do not toggle, just make visible.
    HandleSummon(std::move(summonArgs));
}

void AppHost::_OpenSystemMenu(const winrt::Windows::Foundation::IInspectable&,
                              const winrt::Windows::Foundation::IInspectable&)
{
    _window->OpenSystemMenu(std::nullopt, std::nullopt);
}

void AppHost::_SystemMenuChangeRequested(const winrt::Windows::Foundation::IInspectable&, const winrt::TerminalApp::SystemMenuChangeArgs& args)
{
    switch (args.Action())
    {
    case winrt::TerminalApp::SystemMenuChangeAction::Add:
    {
        auto handler = args.Handler();
        _window->AddToSystemMenu(args.Name(), [handler]() { handler(); });
        break;
    }
    case winrt::TerminalApp::SystemMenuChangeAction::Remove:
    {
        _window->RemoveFromSystemMenu(args.Name());
        break;
    }
    default:
    {
    }
    }
}

// Method Description:
// - BODGY workaround for GH#9320. When the window moves, dismiss all the popups
//   in the UI tree. Xaml Islands unfortunately doesn't do this for us, see
//   microsoft/microsoft-ui-xaml#4554
// Arguments:
// - <none>
// Return Value:
// - <none>
void AppHost::_WindowMoved()
{
    if (_isWindowInitialized < WindowInitializedState::Initialized)
    {
        return;
    }
    if (_windowLogic)
    {
        // Ensure any open ContentDialog is dismissed.
        // Closing the popup in the UI tree as done below is not sufficient because
        // it does not terminate the dialog's async operation.
        _windowLogic.DismissDialog();

        const auto root{ _windowLogic.GetRoot() };
        if (root && root.XamlRoot())
        {
            try
            {
                // This is basically DismissAllPopups which is also in
                // TerminalSettingsEditor/Utils.h
                // There isn't a good place that's shared between these two files, but
                // it's only 5 LOC so whatever.
                const auto popups{ Media::VisualTreeHelper::GetOpenPopupsForXamlRoot(root.XamlRoot()) };
                for (const auto& p : popups)
                {
                    p.IsOpen(false);
                }
            }
            catch (...)
            {
                // We purposely don't log here, because this is exceptionally noisy,
                // especially on startup, when we're moving the window into place
                // but might not have a real xamlRoot yet.
            }
        }
    }
}

void AppHost::_CloseRequested(const winrt::Windows::Foundation::IInspectable& /*sender*/,
                              const winrt::Windows::Foundation::IInspectable& /*args*/)
{
    PostMessageW(_windowManager->GetMainWindow(), WindowEmperor::WM_CLOSE_TERMINAL_WINDOW, 0, reinterpret_cast<LPARAM>(this));
}

void AppHost::_PropertyChangedHandler(const winrt::Windows::Foundation::IInspectable& /*sender*/,
                                      const winrt::Windows::UI::Xaml::Data::PropertyChangedEventArgs& e)
{
    if (e.PropertyName() == L"TitlebarBrush")
    {
        if (_useNonClientArea)
        {
            auto nonClientWindow{ static_cast<NonClientIslandWindow*>(_window.get()) };
            nonClientWindow->SetTitlebarBackground(_windowLogic.TitlebarBrush());
            _updateTheme();
        }
    }
    else if (e.PropertyName() == L"FrameBrush")
    {
        _updateTheme();
    }
}

safe_void_coroutine AppHost::_WindowInitializedHandler(const winrt::Windows::Foundation::IInspectable& /*sender*/,
                                                       const winrt::Windows::Foundation::IInspectable& /*arg*/)
{
    _isWindowInitialized = WindowInitializedState::Initializing;

    // GH#11561: We're totally done being initialized. Resize the window to
    // match the initial settings, and then call ShowWindow to finally make us
    // visible.

    // Use the visibility that we were originally requested with as a base. We
    // can't just use SW_SHOWDEFAULT, because that is set on a per-process
    // basis. That means that a second window needs to have its STARTUPINFO's
    // wShowCmd passed into the original process.
    auto nCmdShow = _launchShowWindowCommand;

    if (WI_IsFlagSet(_launchMode, LaunchMode::MaximizedMode))
    {
        nCmdShow = SW_MAXIMIZE;
    }

    // Delay ShowWindow() until after XAML's initial layout pass is complete.
    auto weakThis{ weak_from_this() };
    co_await wil::resume_foreground(_windowLogic.GetRoot().Dispatcher(), winrt::Windows::UI::Core::CoreDispatcherPriority::Low);
    const auto strongThis = weakThis.lock();
    if (!strongThis || _window == nullptr)
    {
        co_return;
    }

    ShowWindow(_window->GetHandle(), nCmdShow);

    // If we didn't start the window hidden (in one way or another), then try to
    // pull ourselves to the foreground. Don't necessarily do a whole "summon",
    // we don't really want to STEAL foreground if someone rightfully took it

    const bool noForeground = nCmdShow == SW_SHOWMINIMIZED ||
                              nCmdShow == SW_SHOWNOACTIVATE ||
                              nCmdShow == SW_SHOWMINNOACTIVE ||
                              nCmdShow == SW_SHOWNA ||
                              nCmdShow == SW_FORCEMINIMIZE;
    if (!noForeground)
    {
        SetForegroundWindow(_window->GetHandle());
    }

    // Don't set our state to Initialized until after the call to ShowWindow.
    // When we call ShowWindow, the OS will also send us a WM_MOVE, which we'll
    // then use to try and dismiss an open dialog. This creates the unintended
    // side effect of immediately dismissing the initial warning dialog, if
    // there were settings load warnings.
    //
    // In AppHost::_WindowMoved, we'll make sure we're at least initialized
    // before dismissing open dialogs.
    _isWindowInitialized = WindowInitializedState::Initialized;
}

winrt::TerminalApp::TerminalWindow AppHost::Logic()
{
    return _windowLogic;
}

// Method Description:
// - Raised from Page -> us -> manager -> monarch
// - Called when the user attempts to move a tab or pane to another window.
//   `args` will contain info about the structure of the content being moved,
//   and where it should go.
// - If the WindowPosition is filled in, then the user was dragging a tab out of
//   this window and dropping it in empty space, indicating it should create a
//   new window. In that case, we'll make some adjustments using that info and
//   our own window info, so that the new window will be created in the right
//   place and with the same size.
void AppHost::_handleMoveContent(const winrt::Windows::Foundation::IInspectable& /*sender*/,
                                 winrt::TerminalApp::RequestMoveContentArgs args)
{
    winrt::Windows::Foundation::IReference<winrt::Windows::Foundation::Rect> windowBoundsReference{ nullptr };

    if (args.WindowPosition() && _window)
    {
        // The WindowPosition is in DIPs. We need to convert it to pixels.
        const auto dragPositionInDips = args.WindowPosition().Value();
        const auto scale = _window->GetCurrentDpiScale();

        auto dragPositionInPixels = dragPositionInDips;
        dragPositionInPixels.X *= scale;
        dragPositionInPixels.Y *= scale;

        // Fortunately, the window position is already in pixels.
        til::rect windowBoundsInPixels{ _window->GetWindowRect() };
        til::size windowSize{ windowBoundsInPixels.size() };

        const auto dpi = _window->GetCurrentDpi();
        const auto nonClientFrame = _window->GetNonClientFrame(dpi);

        // If this window is maximized, you don't _really_ want the new window
        // showing up at the same size (the size of a maximized window). You
        // want it to just make a normal-sized window. This logic was taken out
        // of AppHost::_HandleCreateWindow.
        if (IsZoomed(_window->GetHandle()))
        {
            const auto initialSize = _windowLogic.GetLaunchDimensions(dpi);

            const auto islandWidth = Utils::ClampToShortMax(lrintf(initialSize.Width), 1);
            const auto islandHeight = Utils::ClampToShortMax(lrintf(initialSize.Height), 1);

            // Get the size of a window we'd need to host that client rect. This will
            // add the titlebar space.
            const til::size nonClientSize{ _window->GetTotalNonClientExclusiveSize(dpi) };

            const auto adjustedWidth = islandWidth + nonClientSize.width;
            const auto adjustedHeight = islandHeight + nonClientSize.height;

            windowSize = til::size{ Utils::ClampToShortMax(adjustedWidth, 1),
                                    Utils::ClampToShortMax(adjustedHeight, 1) };
        }

        // Adjust for the non-client bounds
        dragPositionInPixels.X -= nonClientFrame.left;
        dragPositionInPixels.Y -= nonClientFrame.top;
        windowSize = windowSize - nonClientFrame.size();

        // Convert to DIPs for the size, so that dragging across a DPI boundary
        // retains the correct dimensions.
        // Use the drag event as the new position, and the size of the actual window.
        const auto inverseScale = 1.0f / scale;
        windowBoundsReference = winrt::Windows::Foundation::Rect{
            dragPositionInPixels.X * inverseScale,
            dragPositionInPixels.Y * inverseScale,
            windowSize.width * inverseScale,
            windowSize.height * inverseScale,
        };
    }

    const auto windowName = args.Window();
    winrt::hstring sanitizedWindowName;
    AppHost* target = nullptr;

    if (const auto id = til::parse_signed<int32_t>(windowName))
    {
        if (*id > 0)
        {
            target = _windowManager->GetWindowById(*id);
        }
    }
    else if (windowName != L"new")
    {
        target = _windowManager->GetWindowByName(windowName);
        sanitizedWindowName = windowName;
    }

    if (target)
    {
        target->_windowLogic.AttachContent(args.Content(), args.TabIndex());
    }
    else
    {
        _windowManager->CreateNewWindow(winrt::TerminalApp::WindowRequestedArgs{ sanitizedWindowName, args.Content(), windowBoundsReference });
    }
}

// Page -> us -> manager -> monarch
// The page wants to tell the monarch that it was the drop target for a drag drop.
// The manager will tell the monarch to tell the _other_ window to send its content to us.
void AppHost::_handleReceiveContent(const winrt::Windows::Foundation::IInspectable& /* sender */,
                                    winrt::TerminalApp::RequestReceiveContentArgs args)
{
    if (const auto target = _windowManager->GetWindowById(args.SourceWindow()))
    {
        target->_windowLogic.SendContentToOther(winrt::TerminalApp::RequestReceiveContentArgs{ args.SourceWindow(), args.TargetWindow(), args.TabIndex() });
    }
}
