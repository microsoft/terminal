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

using namespace winrt::Windows::UI;
using namespace winrt::Windows::UI::Composition;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Hosting;
using namespace winrt::Windows::Foundation::Numerics;
using namespace winrt::Microsoft::Terminal;
using namespace winrt::Microsoft::Terminal::Settings::Model;
using namespace ::Microsoft::Console;
using namespace ::Microsoft::Console::Types;

// This magic flag is "documented" at https://msdn.microsoft.com/en-us/library/windows/desktop/ms646301(v=vs.85).aspx
// "If the high-order bit is 1, the key is down; otherwise, it is up."
static constexpr short KeyPressed{ gsl::narrow_cast<short>(0x8000) };

AppHost::AppHost() noexcept :
    _app{},
    _windowManager{},
    _logic{ nullptr }, // don't make one, we're going to take a ref on app's
    _window{ nullptr }
{
    _logic = _app.Logic(); // get a ref to app's logic

    // Inform the WindowManager that it can use us to find the target window for
    // a set of commandline args. This needs to be done before
    // _HandleCommandlineArgs, because WE might end up being the monarch. That
    // would mean we'd need to be responsible for looking that up.
    _windowManager.FindTargetWindowRequested({ this, &AppHost::_FindTargetWindow });

    // If there were commandline args to our process, try and process them here.
    // Do this before AppLogic::Create, otherwise this will have no effect.
    //
    // This will send our commandline to the Monarch, to ask if we should make a
    // new window or not. If not, exit immediately.
    _HandleCommandlineArgs();
    if (!_shouldCreateWindow)
    {
        return;
    }

    _useNonClientArea = _logic.GetShowTabsInTitlebar();
    if (_useNonClientArea)
    {
        _window = std::make_unique<NonClientIslandWindow>(_logic.GetRequestedTheme());
    }
    else
    {
        _window = std::make_unique<IslandWindow>();
    }

    // Update our own internal state tracking if we're in quake mode or not.
    _IsQuakeWindowChanged(nullptr, nullptr);

    // Tell the window to callback to us when it's about to handle a WM_CREATE
    auto pfn = std::bind(&AppHost::_HandleCreateWindow,
                         this,
                         std::placeholders::_1,
                         std::placeholders::_2,
                         std::placeholders::_3);
    _window->SetCreateCallback(pfn);

    _window->SetSnapDimensionCallback(std::bind(&winrt::TerminalApp::AppLogic::CalcSnappedDimension,
                                                _logic,
                                                std::placeholders::_1,
                                                std::placeholders::_2));
    _window->MouseScrolled({ this, &AppHost::_WindowMouseWheeled });
    _window->WindowActivated({ this, &AppHost::_WindowActivated });
    _window->HotkeyPressed({ this, &AppHost::_GlobalHotkeyPressed });
    _window->SetAlwaysOnTop(_logic.GetInitialAlwaysOnTop());
    _window->MakeWindow();

    _windowManager.BecameMonarch({ this, &AppHost::_BecomeMonarch });
    if (_windowManager.IsMonarch())
    {
        _BecomeMonarch(nullptr, nullptr);
    }
}

AppHost::~AppHost()
{
    // destruction order is important for proper teardown here

    _window = nullptr;
    _app.Close();
    _app = nullptr;
}

bool AppHost::OnDirectKeyEvent(const uint32_t vkey, const uint8_t scanCode, const bool down)
{
    if (_logic)
    {
        return _logic.OnDirectKeyEvent(vkey, scanCode, down);
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
void AppHost::SetTaskbarProgress(const winrt::Windows::Foundation::IInspectable& /*sender*/, const winrt::Windows::Foundation::IInspectable& /*args*/)
{
    if (_logic)
    {
        const auto state = gsl::narrow_cast<size_t>(_logic.GetLastActiveControlTaskbarState());
        const auto progress = gsl::narrow_cast<size_t>(_logic.GetLastActiveControlTaskbarProgress());
        _window->SetTaskbarProgress(state, progress);
    }
}

void _buildArgsFromCommandline(std::vector<winrt::hstring>& args)
{
    if (auto commandline{ GetCommandLineW() })
    {
        int argc = 0;

        // Get the argv, and turn them into a hstring array to pass to the app.
        wil::unique_any<LPWSTR*, decltype(&::LocalFree), ::LocalFree> argv{ CommandLineToArgvW(commandline, &argc) };
        if (argv)
        {
            for (auto& elem : wil::make_range(argv.get(), argc))
            {
                args.emplace_back(elem);
            }
        }
    }
    if (args.empty())
    {
        args.emplace_back(L"wt.exe");
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
    std::vector<winrt::hstring> args;
    _buildArgsFromCommandline(args);
    std::wstring cwd{ wil::GetCurrentDirectoryW<std::wstring>() };

    Remoting::CommandlineArgs eventArgs{ { args }, { cwd } };
    _windowManager.ProposeCommandline(eventArgs);

    _shouldCreateWindow = _windowManager.ShouldCreateWindow();
    if (!_shouldCreateWindow)
    {
        return;
    }

    if (auto peasant{ _windowManager.CurrentWindow() })
    {
        if (auto args{ peasant.InitialArgs() })
        {
            const auto result = _logic.SetStartupCommandline(args.Commandline());
            const auto message = _logic.ParseCommandlineMessage();
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

                if (_logic.ShouldExitEarly())
                {
                    ExitProcess(result);
                }
            }
        }

        // After handling the initial args, hookup the callback for handling
        // future commandline invocations. When our peasant is told to execute a
        // commandline (in the future), it'll trigger this callback, that we'll
        // use to send the actions to the app.
        peasant.ExecuteCommandlineRequested({ this, &AppHost::_DispatchCommandline });
        peasant.SummonRequested({ this, &AppHost::_HandleSummon });

        peasant.DisplayWindowIdRequested({ this, &AppHost::_DisplayWindowId });

        _logic.WindowName(peasant.WindowName());
        _logic.WindowId(peasant.GetID());
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
    _window->Initialize();

    if (auto withWindow{ _logic.try_as<IInitializeWithWindow>() })
    {
        withWindow->Initialize(_window->GetHandle());
    }

    if (_useNonClientArea)
    {
        // Register our callback for when the app's non-client content changes.
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
    _logic.FullscreenChanged({ this, &AppHost::_FullscreenChanged });
    _logic.FocusModeChanged({ this, &AppHost::_FocusModeChanged });
    _logic.AlwaysOnTopChanged({ this, &AppHost::_AlwaysOnTopChanged });
    _logic.RaiseVisualBell({ this, &AppHost::_RaiseVisualBell });

    _logic.Create();

    _logic.TitleChanged({ this, &AppHost::AppTitleChanged });
    _logic.LastTabClosed({ this, &AppHost::LastTabClosed });
    _logic.SetTaskbarProgress({ this, &AppHost::SetTaskbarProgress });
    _logic.IdentifyWindowsRequested({ this, &AppHost::_IdentifyWindowsRequested });
    _logic.RenameWindowRequested({ this, &AppHost::_RenameWindowRequested });
    _logic.SettingsChanged({ this, &AppHost::_HandleSettingsChanged });
    _logic.IsQuakeWindowChanged({ this, &AppHost::_IsQuakeWindowChanged });

    _window->UpdateTitle(_logic.Title());

    // Set up the content of the application. If the app has a custom titlebar,
    // set that content as well.
    _window->SetContent(_logic.GetRoot());
    _window->OnAppInitialized();

    // THIS IS A HACK
    //
    // We've got a weird crash that happens terribly inconsistently, but pretty
    // readily on migrie's laptop, only in Debug mode. Apparently, there's some
    // weird ref-counting magic that goes on during teardown, and our
    // Application doesn't get closed quite right, which can cause us to crash
    // into the debugger. This of course, only happens on exit, and happens
    // somewhere in the XamlHost.dll code.
    //
    // Crazily, if we _manually leak the Application_ here, then the crash
    // doesn't happen. This doesn't matter, because we really want the
    // Application to live for _the entire lifetime of the process_, so the only
    // time when this object would actually need to get cleaned up is _during
    // exit_. So we can safely leak this Application object, and have it just
    // get cleaned up normally when our process exits.
    ::winrt::TerminalApp::App a{ _app };
    ::winrt::detach_abi(a);
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
    _window->UpdateTitle(newTitle);
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
void AppHost::_HandleCreateWindow(const HWND hwnd, RECT proposedRect, LaunchMode& launchMode)
{
    launchMode = _logic.GetLaunchMode();

    // Acquire the actual initial position
    auto initialPos = _logic.GetInitialPosition(proposedRect.left, proposedRect.top);
    const auto centerOnLaunch = _logic.CenterOnLaunch();
    proposedRect.left = static_cast<long>(initialPos.X);
    proposedRect.top = static_cast<long>(initialPos.Y);

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

    auto initialSize = _logic.GetLaunchDimensions(dpix);

    const short islandWidth = Utils::ClampToShortMax(
        static_cast<long>(ceil(initialSize.Width)), 1);
    const short islandHeight = Utils::ClampToShortMax(
        static_cast<long>(ceil(initialSize.Height)), 1);

    // Get the size of a window we'd need to host that client rect. This will
    // add the titlebar space.
    const til::size nonClientSize = _window->GetTotalNonClientExclusiveSize(dpix);
    adjustedWidth = islandWidth + nonClientSize.width<long>();
    adjustedHeight = islandHeight + nonClientSize.height<long>();

    til::size dimensions{ Utils::ClampToShortMax(adjustedWidth, 1),
                          Utils::ClampToShortMax(adjustedHeight, 1) };

    // Find nearest monitor for the position that we've actually settled on
    HMONITOR hMonNearest = MonitorFromRect(&proposedRect, MONITOR_DEFAULTTONEAREST);
    MONITORINFO nearestMonitorInfo;
    nearestMonitorInfo.cbSize = sizeof(MONITORINFO);
    // Get monitor dimensions:
    GetMonitorInfo(hMonNearest, &nearestMonitorInfo);
    const til::size desktopDimensions{ gsl::narrow<short>(nearestMonitorInfo.rcWork.right - nearestMonitorInfo.rcWork.left),
                                       gsl::narrow<short>(nearestMonitorInfo.rcWork.bottom - nearestMonitorInfo.rcWork.top) };

    til::point origin{ (proposedRect.left),
                       (proposedRect.top) };

    if (_logic.IsQuakeWindow())
    {
        // If we just use rcWork by itself, we'll fail to account for the invisible
        // space reserved for the resize handles. So retrieve that size here.
        const til::size ncSize{ _window->GetTotalNonClientExclusiveSize(dpix) };
        const til::size availableSpace = desktopDimensions + nonClientSize;

        origin = til::point{
            ::base::ClampSub<long>(nearestMonitorInfo.rcWork.left, (nonClientSize.width() / 2)),
            (nearestMonitorInfo.rcWork.top)
        };
        dimensions = til::size{
            availableSpace.width(),
            availableSpace.height() / 2
        };
        launchMode = LaunchMode::FocusMode;
    }
    else if (centerOnLaunch)
    {
        // Move our proposed location into the center of that specific monitor.
        origin = til::point{
            (nearestMonitorInfo.rcWork.left + ((desktopDimensions.width() / 2) - (dimensions.width() / 2))),
            (nearestMonitorInfo.rcWork.top + ((desktopDimensions.height() / 2) - (dimensions.height() / 2)))
        };
    }

    const til::rectangle newRect{ origin, dimensions };
    bool succeeded = SetWindowPos(hwnd,
                                  nullptr,
                                  newRect.left<int>(),
                                  newRect.top<int>(),
                                  newRect.width<int>(),
                                  newRect.height<int>(),
                                  SWP_NOACTIVATE | SWP_NOZORDER);

    // Refresh the dpi of HWND because the dpi where the window will launch may be different
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

void AppHost::_FocusModeChanged(const winrt::Windows::Foundation::IInspectable&,
                                const winrt::Windows::Foundation::IInspectable&)
{
    _window->FocusModeChanged(_logic.FocusMode());
}

void AppHost::_FullscreenChanged(const winrt::Windows::Foundation::IInspectable&,
                                 const winrt::Windows::Foundation::IInspectable&)
{
    _window->FullscreenChanged(_logic.Fullscreen());
}

void AppHost::_AlwaysOnTopChanged(const winrt::Windows::Foundation::IInspectable&,
                                  const winrt::Windows::Foundation::IInspectable&)
{
    _window->SetAlwaysOnTop(_logic.AlwaysOnTop());
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
    if (_logic)
    {
        // Find all the elements that are underneath the mouse
        auto elems = winrt::Windows::UI::Xaml::Media::VisualTreeHelper::FindElementsInHostCoordinates(coord, _logic.GetRoot());
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
                    const til::point controlOrigin{ til::math::flooring, transform.TransformPoint(til::point{ 0, 0 }) };

                    const til::point offsetPoint = coord - controlOrigin;

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
void AppHost::_DispatchCommandline(winrt::Windows::Foundation::IInspectable /*sender*/,
                                   Remoting::CommandlineArgs args)
{
    const Remoting::SummonWindowBehavior summonArgs{};
    summonArgs.MoveToCurrentDesktop(false);
    summonArgs.DropdownDuration(0);
    summonArgs.ToMonitor(Remoting::MonitorBehavior::InPlace);
    // Summon the window whenever we dispatch a commandline to it. This will
    // make it obvious when a new tab/pane is created in a window.
    _window->SummonWindow(summonArgs);
    _logic.ExecuteCommandline(args.Commandline(), args.CurrentDirectory());
}

// Method Description:
// - Event handler for the WindowManager::FindTargetWindowRequested event. The
//   manager will ask us how to figure out what the target window is for a set
//   of commandline arguments. We'll take those arguments, and ask AppLogic to
//   parse them for us. We'll then set ResultTargetWindow in the given args, so
//   the sender can use that result.
// Arguments:
// - args: the bundle of a commandline and working directory to find the correct target window for.
// Return Value:
// - <none>
void AppHost::_FindTargetWindow(const winrt::Windows::Foundation::IInspectable& /*sender*/,
                                const Remoting::FindTargetWindowArgs& args)
{
    const auto targetWindow = _logic.FindTargetWindow(args.Args().Commandline());
    args.ResultTargetWindow(targetWindow.WindowId());
    args.ResultTargetWindowName(targetWindow.WindowName());
}

winrt::fire_and_forget AppHost::_WindowActivated()
{
    co_await winrt::resume_background();

    if (auto peasant{ _windowManager.CurrentWindow() })
    {
        const auto currentDesktopGuid{ _CurrentDesktopGuid() };

        // TODO: projects/5 - in the future, we'll want to actually get the
        // desktop GUID in IslandWindow, and bubble that up here, then down to
        // the Peasant. For now, we're just leaving space for it.
        Remoting::WindowActivatedArgs args{ peasant.GetID(),
                                            (uint64_t)_window->GetHandle(),
                                            currentDesktopGuid,
                                            winrt::clock().now() };
        peasant.ActivateWindow(args);
    }
}

void AppHost::_BecomeMonarch(const winrt::Windows::Foundation::IInspectable& /*sender*/,
                             const winrt::Windows::Foundation::IInspectable& /*args*/)
{
    _setupGlobalHotkeys();
}

winrt::fire_and_forget AppHost::_setupGlobalHotkeys()
{
    // The hotkey MUST be registered on the main thread. It will fail otherwise!
    co_await winrt::resume_foreground(_logic.GetRoot().Dispatcher(),
                                      winrt::Windows::UI::Core::CoreDispatcherPriority::Normal);

    // Remove all the already registered hotkeys before setting up the new ones.
    _window->UnsetHotkeys(_hotkeys);

    _hotkeyActions = _logic.GlobalHotkeys();
    _hotkeys.clear();
    for (const auto& [k, v] : _hotkeyActions)
    {
        if (k != nullptr)
        {
            _hotkeys.push_back(k);
        }
    }

    _window->SetGlobalHotkeys(_hotkeys);
}

// Method Description:
// - Called whenever a registered hotkey is pressed. We'll look up the
//   GlobalSummonArgs for the specified hotkey, then dispatch a call to the
//   Monarch with the selection information.
// - If the monarch finds a match for the window name (or no name was provided),
//   it'll set FoundMatch=true.
// - If FoundMatch is false, and a name was provided, then we should create a
//   new window with the given name.
// Arguments:
// - hotkeyIndex: the index of the entry in _hotkeys that was pressed.
// Return Value:
// - <none>
void AppHost::_GlobalHotkeyPressed(const long hotkeyIndex)
{
    if (hotkeyIndex < 0 || static_cast<size_t>(hotkeyIndex) > _hotkeys.size())
    {
        return;
    }
    // Lookup the matching keychord
    Control::KeyChord kc = _hotkeys.at(hotkeyIndex);
    // Get the stored Command for that chord
    if (const auto& cmd{ _hotkeyActions.Lookup(kc) })
    {
        if (const auto& summonArgs{ cmd.ActionAndArgs().Args().try_as<Settings::Model::GlobalSummonArgs>() })
        {
            Remoting::SummonWindowSelectionArgs args{ summonArgs.Name() };

            // desktop:any - MoveToCurrentDesktop=false, OnCurrentDesktop=false
            // desktop:toCurrent - MoveToCurrentDesktop=true, OnCurrentDesktop=false
            // desktop:onCurrent - MoveToCurrentDesktop=false, OnCurrentDesktop=true
            args.OnCurrentDesktop(summonArgs.Desktop() == Settings::Model::DesktopBehavior::OnCurrent);
            args.SummonBehavior().MoveToCurrentDesktop(summonArgs.Desktop() == Settings::Model::DesktopBehavior::ToCurrent);
            args.SummonBehavior().ToggleVisibility(summonArgs.ToggleVisibility());
            args.SummonBehavior().DropdownDuration(summonArgs.DropdownDuration());

            switch (summonArgs.Monitor())
            {
            case Settings::Model::MonitorBehavior::Any:
                args.SummonBehavior().ToMonitor(Remoting::MonitorBehavior::InPlace);
                break;
            case Settings::Model::MonitorBehavior::ToCurrent:
                args.SummonBehavior().ToMonitor(Remoting::MonitorBehavior::ToCurrent);
                break;
            case Settings::Model::MonitorBehavior::ToMouse:
                args.SummonBehavior().ToMonitor(Remoting::MonitorBehavior::ToMouse);
                break;
            }

            _windowManager.SummonWindow(args);
            if (args.FoundMatch())
            {
                // Excellent, the window was found. We have nothing else to do here.
            }
            else
            {
                // We should make the window ourselves.
                _createNewTerminalWindow(summonArgs);
            }
        }
    }
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

    if (auto peasant{ _windowManager.CurrentWindow() })
    {
        peasant.RequestIdentifyWindows();
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
    _logic.IdentifyWindow();
}

winrt::fire_and_forget AppHost::_RenameWindowRequested(const winrt::Windows::Foundation::IInspectable /*sender*/,
                                                       const winrt::TerminalApp::RenameWindowRequestedArgs args)
{
    // Capture calling context.
    winrt::apartment_context ui_thread;

    // Switch to the BG thread - anything x-proc must happen on a BG thread
    co_await winrt::resume_background();

    if (auto peasant{ _windowManager.CurrentWindow() })
    {
        Remoting::RenameRequestArgs requestArgs{ args.ProposedName() };

        peasant.RequestRename(requestArgs);

        // Switch back to the UI thread. Setting the WindowName needs to happen
        // on the UI thread, because it'll raise a PropertyChanged event
        co_await ui_thread;

        if (requestArgs.Succeeded())
        {
            _logic.WindowName(args.ProposedName());
        }
        else
        {
            _logic.RenameFailed();
        }
    }
}

void AppHost::_HandleSettingsChanged(const winrt::Windows::Foundation::IInspectable& /*sender*/,
                                     const winrt::Windows::Foundation::IInspectable& /*args*/)
{
    _setupGlobalHotkeys();
}

void AppHost::_IsQuakeWindowChanged(const winrt::Windows::Foundation::IInspectable&,
                                    const winrt::Windows::Foundation::IInspectable&)
{
    _window->IsQuakeWindow(_logic.IsQuakeWindow());
}
