// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "AppHost.h"
#include "../types/inc/Viewport.hpp"
#include "../types/inc/utils.hpp"
#include "../types/inc/User32Utils.hpp"
#include "../WinRTUtils/inc/WtExeUtils.h"
#include "resource.h"
#include "VirtualDesktopUtils.h"
#include "icon.h"

#include <TerminalThemeHelpers.h>

using namespace winrt::Windows::UI;
using namespace winrt::Windows::UI::Composition;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Hosting;
using namespace winrt::Windows::Foundation::Numerics;
using namespace winrt::Microsoft::Terminal;
using namespace winrt::Microsoft::Terminal::Settings::Model;
using namespace ::Microsoft::Console;
using namespace ::Microsoft::Console::Types;
using namespace std::chrono_literals;

// This magic flag is "documented" at https://msdn.microsoft.com/en-us/library/windows/desktop/ms646301(v=vs.85).aspx
// "If the high-order bit is 1, the key is down; otherwise, it is up."
static constexpr short KeyPressed{ gsl::narrow_cast<short>(0x8000) };

AppHost::AppHost(const winrt::TerminalApp::AppLogic& logic,
                 winrt::Microsoft::Terminal::Remoting::WindowRequestedArgs args,
                 const Remoting::WindowManager2& manager,
                 const Remoting::Peasant& peasant) noexcept :
    _windowManager2{ manager },
    _peasant{ peasant },
    _appLogic{ logic }, // don't make one, we're going to take a ref on app's
    _windowLogic{ nullptr },
    _window{ nullptr }
{
    _HandleCommandlineArgs();

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
    auto pfn = std::bind(&AppHost::_HandleCreateWindow,
                         this,
                         std::placeholders::_1,
                         std::placeholders::_2,
                         std::placeholders::_3);
    _window->SetCreateCallback(pfn);

    // Event handlers:
    // MAKE SURE THEY ARE ALL:
    // * winrt::auto_revoke
    // * revoked manually in the dtor before the window is nulled out.
    //
    // If you don't, then it's possible for them to get triggered as the app is
    // tearing down, after we've nulled out the window, during the dtor. That
    // can cause unexpected AV's everywhere.
    //
    // _window callbacks don't need to be treated this way, because:
    // * IslandWindow isn't a WinRT type (so it doesn't have neat revokers like this)
    // * This particular bug scenario applies when we've already freed the window.
    //
    // (Most of these events are actually set up in AppHost::Initialize)
    _window->MouseScrolled({ this, &AppHost::_WindowMouseWheeled });
    _window->WindowActivated({ this, &AppHost::_WindowActivated });
    _window->WindowMoved({ this, &AppHost::_WindowMoved });

    _window->ShouldExitFullscreen({ &_windowLogic, &winrt::TerminalApp::TerminalWindow::RequestExitFullscreen });

    _window->SetAlwaysOnTop(_windowLogic.GetInitialAlwaysOnTop());
    _window->SetAutoHideWindow(_windowLogic.AutoHideWindow());

    _window->MakeWindow();

    _GetWindowLayoutRequestedToken = _windowManager2.GetWindowLayoutRequested([this](auto&&,
                                                                                     const Remoting::GetWindowLayoutArgs& args) {
        // The peasants are running on separate threads, so they'll need to
        // swap what context they are in to the ui thread to get the actual layout.
        args.WindowLayoutJsonAsync(_GetWindowLayoutAsync());
    });
}

AppHost::~AppHost()
{
    // destruction order is important for proper teardown here

    // revoke ALL our event handlers. There's a large class of bugs where we
    // might get a callback to one of these when we call app.Close() below. Make
    // sure to revoke these first, so we won't get any more callbacks, then null
    // out the window, then close the app.
    _revokers = {};

    _showHideWindowThrottler.reset();

    _window = nullptr;
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
// - If we shouldn't become a window, set _shouldCreateWindow to false and exit
//   immediately.
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
void AppHost::_HandleCommandlineArgs()
{
    // We did want to make a window, so let's instantiate it here.
    // We don't have XAML yet, but we do have other stuff.
    _windowLogic = _appLogic.CreateNewWindow();

    if (_peasant)
    {
        const auto& args{ _peasant.InitialArgs() };
        if (args)
        {
            const auto result = _windowLogic.SetStartupCommandline(args.Commandline());
            const auto message = _windowLogic.ParseCommandlineMessage();
            if (!message.empty())
            {
                const auto displayHelp = result == 0;
                const auto messageTitle = displayHelp ? IDS_HELP_DIALOG_TITLE : IDS_ERROR_DIALOG_TITLE;
                const auto messageIcon = displayHelp ? MB_ICONWARNING : MB_ICONERROR;
                // TODO:GH#4134: polish this dialog more, to make the text more
                // like msiexec /?
                MessageBoxW(nullptr,
                            message.data(),
                            GetStringResource(messageTitle).data(),
                            MB_OK | messageIcon);

                if (_windowLogic.ShouldExitEarly())
                {
                    ExitProcess(result);
                }
            }
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
            _shouldCreateWindow = false;
            _windowLogic.HandoffToElevated();
            return;
        }

        // After handling the initial args, hookup the callback for handling
        // future commandline invocations. When our peasant is told to execute a
        // commandline (in the future), it'll trigger this callback, that we'll
        // use to send the actions to the app.
        //
        // MORE EVENT HANDLERS, same rules as the ones above.
        _revokers.peasantExecuteCommandlineRequested = _peasant.ExecuteCommandlineRequested(winrt::auto_revoke, { this, &AppHost::_DispatchCommandline });
        _revokers.peasantSummonRequested = _peasant.SummonRequested(winrt::auto_revoke, { this, &AppHost::_HandleSummon });
        _revokers.peasantDisplayWindowIdRequested = _peasant.DisplayWindowIdRequested(winrt::auto_revoke, { this, &AppHost::_DisplayWindowId });
        _revokers.peasantQuitRequested = _peasant.QuitRequested(winrt::auto_revoke, { this, &AppHost::_QuitRequested });

        // This is logic that almost seems like it belongs on the WindowEmperor.
        // It probably does. However, it needs to muck with our own window so
        // much, that there was no reasonable way of moving this. Moving it also
        // seemed to reorder bits of init so much that everything broke. So
        // we'll leave it here.
        const auto numPeasants = _windowManager2.GetNumberOfPeasants();
        if (numPeasants == 1)
        {
            const auto layouts = ApplicationState::SharedInstance().PersistedWindowLayouts();
            if (_appLogic.ShouldUsePersistedLayout() &&
                layouts &&
                layouts.Size() > 0)
            {
                uint32_t startIdx = 0;
                // We want to create a window for every saved layout.
                // If we are the only window, and no commandline arguments were provided
                // then we should just use the current window to load the first layout.
                // Otherwise create this window normally with its commandline, and create
                // a new window using the first saved layout information.
                // The 2nd+ layout will always get a new window.
                if (!_windowLogic.HasCommandlineArguments() &&
                    !_appLogic.HasSettingsStartupActions())
                {
                    _windowLogic.SetPersistedLayoutIdx(startIdx);
                    startIdx += 1;
                }

                // Create new windows for each of the other saved layouts.
                for (const auto size = layouts.Size(); startIdx < size; startIdx += 1)
                {
                    auto newWindowArgs = fmt::format(L"{0} -w new -s {1}", args.Commandline()[0], startIdx);

                    STARTUPINFO si;
                    memset(&si, 0, sizeof(si));
                    si.cb = sizeof(si);
                    wil::unique_process_information pi;

                    LOG_IF_WIN32_BOOL_FALSE(CreateProcessW(nullptr,
                                                           newWindowArgs.data(),
                                                           nullptr, // lpProcessAttributes
                                                           nullptr, // lpThreadAttributes
                                                           false, // bInheritHandles
                                                           DETACHED_PROCESS | CREATE_UNICODE_ENVIRONMENT, // doCreationFlags
                                                           nullptr, // lpEnvironment
                                                           nullptr, // lpStartingDirectory
                                                           &si, // lpStartupInfo
                                                           &pi // lpProcessInformation
                                                           ));
                }
            }
        }
        _windowLogic.SetNumberOfOpenWindows(numPeasants);

        _windowLogic.WindowName(_peasant.WindowName());
        _windowLogic.WindowId(_peasant.GetID());
    }
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

    // MORE EVENT HANDLERS HERE!
    // MAKE SURE THEY ARE ALL:
    // * winrt::auto_revoke
    // * revoked manually in the dtor before the window is nulled out.
    //
    // If you don't, then it's possible for them to get triggered as the app is
    // tearing down, after we've nulled out the window, during the dtor. That
    // can cause unexpected AV's everywhere.
    //
    // _window callbacks don't need to be treated this way, because:
    // * IslandWindow isn't a WinRT type (so it doesn't have neat revokers like this)
    // * This particular bug scenario applies when we've already freed the window.

    // Register the 'X' button of the window for a warning experience of multiple
    // tabs opened, this is consistent with Alt+F4 closing
    _window->WindowCloseButtonClicked([this]() {
        _CloseRequested(nullptr, nullptr);
    });
    // If the user requests a close in another way handle the same as if the 'X'
    // was clicked.
    _revokers.CloseRequested = _windowLogic.CloseRequested(winrt::auto_revoke, { this, &AppHost::_CloseRequested });

    // Add an event handler to plumb clicks in the titlebar area down to the
    // application layer.
    _window->DragRegionClicked([this]() { _windowLogic.TitlebarClicked(); });

    _window->WindowVisibilityChanged([this](bool showOrHide) { _windowLogic.WindowVisibilityChanged(showOrHide); });
    _window->UpdateSettingsRequested([this]() { _appLogic.ReloadSettings(); });

    _revokers.RequestedThemeChanged = _windowLogic.RequestedThemeChanged(winrt::auto_revoke, { this, &AppHost::_UpdateTheme });
    _revokers.FullscreenChanged = _windowLogic.FullscreenChanged(winrt::auto_revoke, { this, &AppHost::_FullscreenChanged });
    _revokers.FocusModeChanged = _windowLogic.FocusModeChanged(winrt::auto_revoke, { this, &AppHost::_FocusModeChanged });
    _revokers.AlwaysOnTopChanged = _windowLogic.AlwaysOnTopChanged(winrt::auto_revoke, { this, &AppHost::_AlwaysOnTopChanged });
    _revokers.RaiseVisualBell = _windowLogic.RaiseVisualBell(winrt::auto_revoke, { this, &AppHost::_RaiseVisualBell });
    _revokers.SystemMenuChangeRequested = _windowLogic.SystemMenuChangeRequested(winrt::auto_revoke, { this, &AppHost::_SystemMenuChangeRequested });
    _revokers.ChangeMaximizeRequested = _windowLogic.ChangeMaximizeRequested(winrt::auto_revoke, { this, &AppHost::_ChangeMaximizeRequested });

    _window->MaximizeChanged([this](bool newMaximize) {
        if (_appLogic)
        {
            _windowLogic.Maximized(newMaximize);
        }
    });

    _window->AutomaticShutdownRequested([this]() {
        // Raised when the OS is beginning an update of the app. We will quit,
        // to save our state, before the OS manually kills us.
        _windowManager2.RequestQuitAll(_peasant);
    });

    // Load bearing: make sure the PropertyChanged handler is added before we
    // call Create, so that when the app sets up the titlebar brush, we're
    // already prepared to listen for the change notification
    _revokers.PropertyChanged = _windowLogic.PropertyChanged(winrt::auto_revoke, { this, &AppHost::_PropertyChangedHandler });

    _appLogic.Create();
    _windowLogic.Create();

    _revokers.TitleChanged = _windowLogic.TitleChanged(winrt::auto_revoke, { this, &AppHost::AppTitleChanged });
    _revokers.LastTabClosed = _windowLogic.LastTabClosed(winrt::auto_revoke, { this, &AppHost::LastTabClosed });
    _revokers.SetTaskbarProgress = _windowLogic.SetTaskbarProgress(winrt::auto_revoke, { this, &AppHost::SetTaskbarProgress });
    _revokers.IdentifyWindowsRequested = _windowLogic.IdentifyWindowsRequested(winrt::auto_revoke, { this, &AppHost::_IdentifyWindowsRequested });
    _revokers.RenameWindowRequested = _windowLogic.RenameWindowRequested(winrt::auto_revoke, { this, &AppHost::_RenameWindowRequested });

    // A note: make sure to listen to our _window_'s settings changed, not the
    // applogic's. We want to make sure the event has gone through the window
    // logic _before_ we handle it, so we can ask the window about it's newest
    // properties.
    _revokers.SettingsChanged = _windowLogic.SettingsChanged(winrt::auto_revoke, { this, &AppHost::_HandleSettingsChanged });

    _revokers.IsQuakeWindowChanged = _windowLogic.IsQuakeWindowChanged(winrt::auto_revoke, { this, &AppHost::_IsQuakeWindowChanged });
    _revokers.SummonWindowRequested = _windowLogic.SummonWindowRequested(winrt::auto_revoke, { this, &AppHost::_SummonWindowRequested });
    _revokers.OpenSystemMenu = _windowLogic.OpenSystemMenu(winrt::auto_revoke, { this, &AppHost::_OpenSystemMenu });
    _revokers.QuitRequested = _windowLogic.QuitRequested(winrt::auto_revoke, { this, &AppHost::_RequestQuitAll });
    _revokers.ShowWindowChanged = _windowLogic.ShowWindowChanged(winrt::auto_revoke, { this, &AppHost::_ShowWindowChanged });

    // BODGY
    // On certain builds of Windows, when Terminal is set as the default
    // it will accumulate an unbounded amount of queued animations while
    // the screen is off and it is servicing window management for console
    // applications. This call into TerminalThemeHelpers will tell our
    // compositor to automatically complete animations that are scheduled
    // while the screen is off.
    TerminalTrySetAutoCompleteAnimationsWhenOccluded(static_cast<::IUnknown*>(winrt::get_abi(_windowLogic.GetRoot())), true);

    _window->SetSnapDimensionCallback(std::bind(&winrt::TerminalApp::TerminalWindow::CalcSnappedDimension,
                                                _windowLogic,
                                                std::placeholders::_1,
                                                std::placeholders::_2));

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

// Method Description:
// - Called every time when the active tab's title changes. We'll also fire off
//   a window message so we can update the window's title on the main thread,
//   though we'll only do so if the settings are configured for that.
// Arguments:
// - sender: unused
// - newTitle: the string to use as the new window title
// Return Value:
// - <none>
void AppHost::AppTitleChanged(const winrt::Windows::Foundation::IInspectable& /*sender*/, winrt::hstring newTitle)
{
    if (_windowLogic.GetShowTitleInTitlebar())
    {
        _window->UpdateTitle(newTitle);
    }
    _windowManager2.UpdateActiveTabTitle(newTitle, _peasant);
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
    // TODO!
    // if (_windowManager2.IsMonarch() && _notificationIcon)
    // {
    //     _DestroyNotificationIcon();
    // }
    // else if (_window->IsQuakeWindow())
    // {
    //     _HideNotificationIconRequested(nullptr, nullptr);
    // }

    // We don't want to try to save layouts if we are about to close.
    _windowManager2.GetWindowLayoutRequested(_GetWindowLayoutRequestedToken);

    // Remove ourself from the list of peasants so that we aren't included in
    // any future requests. This will also mean we block until any existing
    // event handler finishes.
    _windowManager2.SignalClose(_peasant);

    _window->Close();
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
void AppHost::_HandleCreateWindow(const HWND hwnd, til::rect proposedRect, LaunchMode& launchMode)
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

    const auto islandWidth = Utils::ClampToShortMax(
        static_cast<long>(ceil(initialSize.Width)), 1);
    const auto islandHeight = Utils::ClampToShortMax(
        static_cast<long>(ceil(initialSize.Height)), 1);

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
void AppHost::_WindowMouseWheeled(const til::point coord, const int32_t delta)
{
    if (_appLogic)
    {
        // Find all the elements that are underneath the mouse
        auto elems = winrt::Windows::UI::Xaml::Media::VisualTreeHelper::FindElementsInHostCoordinates(coord.to_winrt_point(), _windowLogic.GetRoot());
        for (const auto& e : elems)
        {
            // If that element has implemented IMouseWheelListener, call OnMouseWheel on that element.
            if (auto control{ e.try_as<winrt::Microsoft::Terminal::Control::IMouseWheelListener>() })
            {
                try
                {
                    // Translate the event to the coordinate space of the control
                    // we're attempting to dispatch it to
                    const auto transform = e.TransformToVisual(nullptr);
                    const til::point controlOrigin{ til::math::flooring, transform.TransformPoint({}) };

                    const auto offsetPoint = coord - controlOrigin;

                    const auto lButtonDown = WI_IsFlagSet(GetKeyState(VK_LBUTTON), KeyPressed);
                    const auto mButtonDown = WI_IsFlagSet(GetKeyState(VK_MBUTTON), KeyPressed);
                    const auto rButtonDown = WI_IsFlagSet(GetKeyState(VK_RBUTTON), KeyPressed);

                    if (control.OnMouseWheel(offsetPoint.to_winrt_point(), delta, lButtonDown, mButtonDown, rButtonDown))
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

bool AppHost::HasWindow()
{
    return _shouldCreateWindow;
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
void AppHost::_DispatchCommandline(winrt::Windows::Foundation::IInspectable sender,
                                   Remoting::CommandlineArgs args)
{
    const Remoting::SummonWindowBehavior summonArgs{};
    summonArgs.MoveToCurrentDesktop(false);
    summonArgs.DropdownDuration(0);
    summonArgs.ToMonitor(Remoting::MonitorBehavior::InPlace);
    summonArgs.ToggleVisibility(false); // Do not toggle, just make visible.
    // Summon the window whenever we dispatch a commandline to it. This will
    // make it obvious when a new tab/pane is created in a window.
    _HandleSummon(sender, summonArgs);
    _windowLogic.ExecuteCommandline(args.Commandline(), args.CurrentDirectory());
}

winrt::fire_and_forget AppHost::_WindowActivated(bool activated)
{
    _windowLogic.WindowActivated(activated);

    if (!activated)
    {
        co_return;
    }

    co_await winrt::resume_background();

    if (_peasant)
    {
        const auto currentDesktopGuid{ _CurrentDesktopGuid() };

        // TODO: projects/5 - in the future, we'll want to actually get the
        // desktop GUID in IslandWindow, and bubble that up here, then down to
        // the Peasant. For now, we're just leaving space for it.
        Remoting::WindowActivatedArgs args{ _peasant.GetID(),
                                            (uint64_t)_window->GetHandle(),
                                            currentDesktopGuid,
                                            winrt::clock().now() };
        _peasant.ActivateWindow(args);
    }
}

// Method Description:
// - Asynchronously get the window layout from the current page. This is
//   done async because we need to switch between the ui thread and the calling
//   thread.
// - NB: The peasant calling this must not be running on the UI thread, otherwise
//   they will crash since they just call .get on the async operation.
// Arguments:
// - <none>
// Return Value:
// - The window layout as a json string.
winrt::Windows::Foundation::IAsyncOperation<winrt::hstring> AppHost::_GetWindowLayoutAsync()
{
    winrt::apartment_context peasant_thread;

    winrt::hstring layoutJson = L"";
    // Use the main thread since we are accessing controls.
    co_await wil::resume_foreground(_windowLogic.GetRoot().Dispatcher());
    try
    {
        const auto pos = _GetWindowLaunchPosition();
        layoutJson = _windowLogic.GetWindowLayoutJson(pos);
    }
    CATCH_LOG()

    // go back to give the result to the peasant.
    co_await peasant_thread;

    co_return layoutJson;
}

// Method Description:
// - Called when the monarch failed to summon a window for a given set of
//   SummonWindowSelectionArgs. In this case, we should create the specified
//   window ourselves.
// - This is to support the scenario like `globalSummon(Name="_quake")` being
//   used to summon the window if it already exists, or create it if it doesn't.
// Arguments:
// - args: Contains information on how we should name the window
// Return Value:
// - <none>
winrt::fire_and_forget AppHost::_createNewTerminalWindow(Settings::Model::GlobalSummonArgs args)
{
    // Hop to the BG thread
    co_await winrt::resume_background();

    // This will get us the correct exe for dev/preview/release. If you
    // don't stick this in a local, it'll get mangled by ShellExecute. I
    // have no idea why.
    const auto exePath{ GetWtExePath() };

    // If we weren't given a name, then just use new to force the window to be
    // unnamed.
    winrt::hstring cmdline{
        fmt::format(L"-w {}",
                    args.Name().empty() ? L"new" :
                                          args.Name())
    };

    SHELLEXECUTEINFOW seInfo{ 0 };
    seInfo.cbSize = sizeof(seInfo);
    seInfo.fMask = SEE_MASK_NOASYNC;
    seInfo.lpVerb = L"open";
    seInfo.lpFile = exePath.c_str();
    seInfo.lpParameters = cmdline.c_str();
    seInfo.nShow = SW_SHOWNORMAL;
    LOG_IF_WIN32_BOOL_FALSE(ShellExecuteExW(&seInfo));

    co_return;
}

// Method Description:
// - Helper to initialize our instance of IVirtualDesktopManager. If we already
//   got one, then this will just return true. Otherwise, we'll try and init a
//   new instance of one, and store that.
// - This will return false if we weren't able to initialize one, which I'm not
//   sure is actually possible.
// Arguments:
// - <none>
// Return Value:
// - true iff _desktopManager points to a non-null instance of IVirtualDesktopManager
bool AppHost::_LazyLoadDesktopManager()
{
    if (_desktopManager == nullptr)
    {
        try
        {
            _desktopManager = winrt::create_instance<IVirtualDesktopManager>(__uuidof(VirtualDesktopManager));
        }
        CATCH_LOG();
    }

    return _desktopManager != nullptr;
}

void AppHost::_HandleSummon(const winrt::Windows::Foundation::IInspectable& /*sender*/,
                            const Remoting::SummonWindowBehavior& args)
{
    _window->SummonWindow(args);

    if (args != nullptr && args.MoveToCurrentDesktop())
    {
        if (_LazyLoadDesktopManager())
        {
            // First thing - make sure that we're not on the current desktop. If
            // we are, then don't call MoveWindowToDesktop. This is to mitigate
            // MSFT:33035972
            BOOL onCurrentDesktop{ false };
            if (SUCCEEDED(_desktopManager->IsWindowOnCurrentVirtualDesktop(_window->GetHandle(), &onCurrentDesktop)) && onCurrentDesktop)
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
                    LOG_IF_FAILED(_desktopManager->MoveWindowToDesktop(_window->GetHandle(), currentlyActiveDesktop));
                }
                // If GetCurrentVirtualDesktopId failed, then just leave the window
                // where it is. Nothing else to be done :/
            }
        }
    }
}

// Method Description:
// - This gets the GUID of the desktop our window is currently on. It does NOT
//   get the GUID of the desktop that's currently active.
// Arguments:
// - <none>
// Return Value:
// - the GUID of the desktop our window is currently on
GUID AppHost::_CurrentDesktopGuid()
{
    GUID currentDesktopGuid{ 0 };
    if (_LazyLoadDesktopManager())
    {
        LOG_IF_FAILED(_desktopManager->GetWindowDesktopId(_window->GetHandle(), &currentDesktopGuid));
    }
    return currentDesktopGuid;
}

// Method Description:
// - Called when this window wants _all_ windows to display their
//   identification. We'll hop to the BG thread, and raise an event (eventually
//   handled by the monarch) to bubble this request to all the Terminal windows.
// Arguments:
// - <unused>
// Return Value:
// - <none>
winrt::fire_and_forget AppHost::_IdentifyWindowsRequested(const winrt::Windows::Foundation::IInspectable /*sender*/,
                                                          const winrt::Windows::Foundation::IInspectable /*args*/)
{
    // We'll be raising an event that may result in a RPC call to the monarch -
    // make sure we're on the background thread, or this will silently fail
    co_await winrt::resume_background();

    if (_peasant)
    {
        _peasant.RequestIdentifyWindows();
    }
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

winrt::fire_and_forget AppHost::_RenameWindowRequested(const winrt::Windows::Foundation::IInspectable /*sender*/,
                                                       const winrt::TerminalApp::RenameWindowRequestedArgs args)
{
    // Capture calling context.
    winrt::apartment_context ui_thread;

    // Switch to the BG thread - anything x-proc must happen on a BG thread
    co_await winrt::resume_background();

    if (_peasant)
    {
        Remoting::RenameRequestArgs requestArgs{ args.ProposedName() };

        _peasant.RequestRename(requestArgs);

        // Switch back to the UI thread. Setting the WindowName needs to happen
        // on the UI thread, because it'll raise a PropertyChanged event
        co_await ui_thread;

        if (requestArgs.Succeeded())
        {
            _windowLogic.WindowName(args.ProposedName());
        }
        else
        {
            _windowLogic.RenameFailed();
        }
    }
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

void AppHost::_updateTheme()
{
    auto theme = _appLogic.Theme();

    _window->OnApplicationThemeChanged(theme.RequestedTheme());

    const auto b = _windowLogic.TitlebarBrush();
    const auto color = ThemeColor::ColorFromBrush(b);
    const auto colorOpacity = b ? color.A / 255.0 : 0.0;
    const auto brushOpacity = _opacityFromBrush(b);
    const auto opacity = std::min(colorOpacity, brushOpacity);
    _window->UseMica(theme.Window() ? theme.Window().UseMica() : false, opacity);

    // This is a hack to make the window borders dark instead of light.
    // It must be done before WM_NCPAINT so that the borders are rendered with
    // the correct theme.
    // For more information, see GH#6620.
    LOG_IF_FAILED(TerminalTrySetDarkTheme(_window->GetHandle(), _isActuallyDarkTheme(theme.RequestedTheme())));
}

void AppHost::_HandleSettingsChanged(const winrt::Windows::Foundation::IInspectable& /*sender*/,
                                     const winrt::TerminalApp::SettingsLoadEventArgs& /*args*/)
{
    // We don't need to call in to windowLogic here - it has its own SettingsChanged handler

    // TODO! tray icon
    //
    // // If we're monarch, we need to check some conditions to show the notification icon.
    // // If there's a Quake window somewhere, we'll want to keep the notification icon.
    // // There's two settings - MinimizeToNotificationArea and AlwaysShowNotificationIcon. If either
    // // one of them are true, we want to make sure there's a notification icon.
    // // If both are false, we want to remove our icon from the notification area.
    // // When we remove our icon from the notification area, we'll also want to re-summon
    // // any hidden windows, but right now we're not keeping track of who's hidden,
    // // so just summon them all. Tracking the work to do a "summon all minimized" in
    // // GH#10448
    // if (_windowManager2.IsMonarch())
    // {
    //     if (!_windowManager2.DoesQuakeWindowExist())
    //     {
    //         if (!_notificationIcon && (_windowLogic.GetMinimizeToNotificationArea() || _windowLogic.GetAlwaysShowNotificationIcon()))
    //         {
    //             _CreateNotificationIcon();
    //         }
    //         else if (_notificationIcon && !_windowLogic.GetMinimizeToNotificationArea() && !_windowLogic.GetAlwaysShowNotificationIcon())
    //         {
    //             _windowManager2.SummonAllWindows();
    //             _DestroyNotificationIcon();
    //         }
    //     }
    // }

    _window->SetMinimizeToNotificationAreaBehavior(_windowLogic.GetMinimizeToNotificationArea());
    _window->SetAutoHideWindow(_windowLogic.AutoHideWindow());
    _updateTheme();
}

void AppHost::_IsQuakeWindowChanged(const winrt::Windows::Foundation::IInspectable&,
                                    const winrt::Windows::Foundation::IInspectable&)
{
    // We want the quake window to be accessible through the notification icon.
    // This means if there's a quake window _somewhere_, we want the notification icon
    // to show regardless of the notification icon settings.
    // This also means we'll need to destroy the notification icon if it was created
    // specifically for the quake window. If not, it should not be destroyed.
    if (!_window->IsQuakeWindow() && _windowLogic.IsQuakeWindow())
    {
        _ShowNotificationIconRequested(nullptr, nullptr);
    }
    else if (_window->IsQuakeWindow() && !_windowLogic.IsQuakeWindow())
    {
        _HideNotificationIconRequested(nullptr, nullptr);
    }

    _window->IsQuakeWindow(_windowLogic.IsQuakeWindow());
}

// Raised from our Peasant. We handle by propogating the call to our terminal window.
winrt::fire_and_forget AppHost::_QuitRequested(const winrt::Windows::Foundation::IInspectable&,
                                               const winrt::Windows::Foundation::IInspectable&)
{
    // Need to be on the main thread to close out all of the tabs.
    co_await wil::resume_foreground(_windowLogic.GetRoot().Dispatcher());

    _windowLogic.Quit();
}

// Raised from TerminalWindow. We handle by bubbling the request to the window manager.
void AppHost::_RequestQuitAll(const winrt::Windows::Foundation::IInspectable&,
                              const winrt::Windows::Foundation::IInspectable&)
{
    _windowManager2.RequestQuitAll(_peasant);
}

void AppHost::_ShowWindowChanged(const winrt::Windows::Foundation::IInspectable&,
                                 const winrt::Microsoft::Terminal::Control::ShowWindowArgs& args)
{
    // GH#13147: Enqueue a throttled update to our window state. Throttling
    // should prevent scenarios where the Terminal window state and PTY window
    // state get de-sync'd, and cause the window to minimize/restore constantly
    // in a loop.
    if (_showHideWindowThrottler)
    {
        _showHideWindowThrottler->Run(args.ShowOrHide());
    }
}

void AppHost::_SummonWindowRequested(const winrt::Windows::Foundation::IInspectable& sender,
                                     const winrt::Windows::Foundation::IInspectable&)
{
    const Remoting::SummonWindowBehavior summonArgs{};
    summonArgs.MoveToCurrentDesktop(false);
    summonArgs.DropdownDuration(0);
    summonArgs.ToMonitor(Remoting::MonitorBehavior::InPlace);
    summonArgs.ToggleVisibility(false); // Do not toggle, just make visible.
    _HandleSummon(sender, summonArgs);
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
// - Creates a Notification Icon and hooks up its handlers
// Arguments:
// - <none>
// Return Value:
// - <none>
void AppHost::_CreateNotificationIcon()
{
    _notificationIcon = std::make_unique<NotificationIcon>(_window->GetHandle());

    // Hookup the handlers, save the tokens for revoking if settings change.
    _ReAddNotificationIconToken = _window->NotifyReAddNotificationIcon([this]() { _notificationIcon->ReAddNotificationIcon(); });
    _NotificationIconPressedToken = _window->NotifyNotificationIconPressed([this]() { _notificationIcon->NotificationIconPressed(); });
    _ShowNotificationIconContextMenuToken = _window->NotifyShowNotificationIconContextMenu([this](til::point coord) { _notificationIcon->ShowContextMenu(coord, _windowManager2.GetPeasantInfos()); });
    _NotificationIconMenuItemSelectedToken = _window->NotifyNotificationIconMenuItemSelected([this](HMENU hm, UINT idx) { _notificationIcon->MenuItemSelected(hm, idx); });
    _notificationIcon->SummonWindowRequested([this](auto& args) { _windowManager2.SummonWindow(args); });
}

// Method Description:
// - Deletes our notification icon if we have one.
// Arguments:
// - <none>
// Return Value:
// - <none>
void AppHost::_DestroyNotificationIcon()
{
    _window->NotifyReAddNotificationIcon(_ReAddNotificationIconToken);
    _window->NotifyNotificationIconPressed(_NotificationIconPressedToken);
    _window->NotifyShowNotificationIconContextMenu(_ShowNotificationIconContextMenuToken);
    _window->NotifyNotificationIconMenuItemSelected(_NotificationIconMenuItemSelectedToken);

    _notificationIcon->RemoveIconFromNotificationArea();
    _notificationIcon = nullptr;
}

void AppHost::_ShowNotificationIconRequested(const winrt::Windows::Foundation::IInspectable& /*sender*/,
                                             const winrt::Windows::Foundation::IInspectable& /*args*/)
{
    // TODO! tray icon
    //
    // if (_windowManager2.IsMonarch())
    // {
    //     if (!_notificationIcon)
    //     {
    //         _CreateNotificationIcon();
    //     }
    // }
    // else
    // {
    //     _windowManager2.RequestShowNotificationIcon();
    // }
}

void AppHost::_HideNotificationIconRequested(const winrt::Windows::Foundation::IInspectable& /*sender*/,
                                             const winrt::Windows::Foundation::IInspectable& /*args*/)
{
    // TODO! tray icon
    //
    // if (_windowManager2.IsMonarch())
    // {
    //     // Destroy it only if our settings allow it
    //     if (_notificationIcon &&
    //         !_windowLogic.GetAlwaysShowNotificationIcon() &&
    //         !_windowLogic.GetMinimizeToNotificationArea())
    //     {
    //         _DestroyNotificationIcon();
    //     }
    // }
    // else
    // {
    //     _windowManager2.RequestHideNotificationIcon();
    // }
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
    const auto pos = _GetWindowLaunchPosition();
    _windowLogic.CloseWindow(pos);
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
}

winrt::TerminalApp::TerminalWindow AppHost::Logic()
{
    return _windowLogic;
}
