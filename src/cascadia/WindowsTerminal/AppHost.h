// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "pch.h"
#include "NonClientIslandWindow.h"
#include "NotificationIcon.h"
#include <ThrottledFunc.h>

class AppHost : public std::enable_shared_from_this<AppHost>
{
public:
    static constexpr DWORD WM_REFRIGERATE = WM_APP + 0;

    AppHost(const winrt::TerminalApp::AppLogic& logic,
            winrt::Microsoft::Terminal::Remoting::WindowRequestedArgs args,
            const winrt::Microsoft::Terminal::Remoting::WindowManager& manager,
            const winrt::Microsoft::Terminal::Remoting::Peasant& peasant,
            std::unique_ptr<IslandWindow> window = nullptr) noexcept;

    void AppTitleChanged(const winrt::Windows::Foundation::IInspectable& sender, winrt::hstring newTitle);
    void Initialize();
    void Close();

    [[nodiscard]] std::unique_ptr<IslandWindow> Refrigerate();

    bool OnDirectKeyEvent(const uint32_t vkey, const uint8_t scanCode, const bool down);
    void SetTaskbarProgress(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::Foundation::IInspectable& args);

    winrt::TerminalApp::TerminalWindow Logic();

    static void s_DisplayMessageBox(const winrt::TerminalApp::ParseCommandlineResult& message);

    til::event<winrt::delegate<void()>> UpdateSettingsRequested;

private:
    std::unique_ptr<IslandWindow> _window;

    winrt::TerminalApp::AppLogic _appLogic;
    winrt::TerminalApp::TerminalWindow _windowLogic;

    winrt::Microsoft::Terminal::Remoting::WindowManager _windowManager{ nullptr };
    winrt::Microsoft::Terminal::Remoting::Peasant _peasant{ nullptr };

    winrt::com_ptr<IVirtualDesktopManager> _desktopManager{ nullptr };

    enum WindowInitializedState : uint32_t
    {
        NotInitialized = 0,
        Initializing = 1,
        Initialized = 2,
    };

    WindowInitializedState _isWindowInitialized{ WindowInitializedState::NotInitialized };
    bool _useNonClientArea{ false };
    winrt::Microsoft::Terminal::Settings::Model::LaunchMode _launchMode{};

    std::shared_ptr<ThrottledFuncTrailing<bool>> _showHideWindowThrottler;

    std::chrono::time_point<std::chrono::steady_clock> _started;
    SafeDispatcherTimer _frameTimer;

    uint32_t _launchShowWindowCommand{ SW_NORMAL };

    safe_void_coroutine _quit();
    void _revokeWindowCallbacks();

    void _HandleCommandlineArgs(const winrt::Microsoft::Terminal::Remoting::WindowRequestedArgs& args);
    void _HandleSessionRestore(const bool startedForContent);

    winrt::Microsoft::Terminal::Settings::Model::LaunchPosition _GetWindowLaunchPosition();

    void _HandleCreateWindow(const HWND hwnd, const til::rect& proposedRect);

    void _UpdateTitleBarContent(const winrt::Windows::Foundation::IInspectable& sender,
                                const winrt::Windows::UI::Xaml::UIElement& arg);
    void _UpdateTheme(const winrt::Windows::Foundation::IInspectable&,
                      const winrt::Microsoft::Terminal::Settings::Model::Theme& arg);
    void _FocusModeChanged(const winrt::Windows::Foundation::IInspectable& sender,
                           const winrt::Windows::Foundation::IInspectable& arg);
    void _FullscreenChanged(const winrt::Windows::Foundation::IInspectable& sender,
                            const winrt::Windows::Foundation::IInspectable& arg);
    void _ChangeMaximizeRequested(const winrt::Windows::Foundation::IInspectable& sender,
                                  const winrt::Windows::Foundation::IInspectable& arg);
    void _AlwaysOnTopChanged(const winrt::Windows::Foundation::IInspectable& sender,
                             const winrt::Windows::Foundation::IInspectable& arg);
    safe_void_coroutine _WindowInitializedHandler(const winrt::Windows::Foundation::IInspectable& sender,
                                                  const winrt::Windows::Foundation::IInspectable& arg);

    void _RaiseVisualBell(const winrt::Windows::Foundation::IInspectable& sender,
                          const winrt::Windows::Foundation::IInspectable& arg);
    void _WindowMouseWheeled(const til::point coord, const int32_t delta);
    void _WindowActivated(bool activated);
    safe_void_coroutine _peasantNotifyActivateWindow();
    void _WindowMoved();

    void _DispatchCommandline(winrt::Windows::Foundation::IInspectable sender,
                              winrt::Microsoft::Terminal::Remoting::CommandlineArgs args);

    void _HandleSummon(const winrt::Windows::Foundation::IInspectable& sender,
                       const winrt::Microsoft::Terminal::Remoting::SummonWindowBehavior& args);

    safe_void_coroutine _IdentifyWindowsRequested(const winrt::Windows::Foundation::IInspectable sender,
                                                  const winrt::Windows::Foundation::IInspectable args);
    void _DisplayWindowId(const winrt::Windows::Foundation::IInspectable& sender,
                          const winrt::Windows::Foundation::IInspectable& args);
    safe_void_coroutine _RenameWindowRequested(const winrt::Windows::Foundation::IInspectable sender,
                                               const winrt::TerminalApp::RenameWindowRequestedArgs args);

    void _HandleSettingsChanged(const winrt::Windows::Foundation::IInspectable& sender,
                                const winrt::TerminalApp::SettingsLoadEventArgs& args);

    void _IsQuakeWindowChanged(const winrt::Windows::Foundation::IInspectable& sender,
                               const winrt::Windows::Foundation::IInspectable& args);

    void _SummonWindowRequested(const winrt::Windows::Foundation::IInspectable& sender,
                                const winrt::Windows::Foundation::IInspectable& args);

    void _OpenSystemMenu(const winrt::Windows::Foundation::IInspectable& sender,
                         const winrt::Windows::Foundation::IInspectable& args) const;

    void _SystemMenuChangeRequested(const winrt::Windows::Foundation::IInspectable& sender,
                                    const winrt::TerminalApp::SystemMenuChangeArgs& args);

    void _QuitRequested(const winrt::Windows::Foundation::IInspectable& sender,
                        const winrt::Windows::Foundation::IInspectable& args);

    void _RequestQuitAll(const winrt::Windows::Foundation::IInspectable& sender,
                         const winrt::Windows::Foundation::IInspectable& args);
    void _CloseRequested(const winrt::Windows::Foundation::IInspectable& sender,
                         const winrt::Windows::Foundation::IInspectable& args);

    void _ShowWindowChanged(const winrt::Windows::Foundation::IInspectable& sender,
                            const winrt::Microsoft::Terminal::Control::ShowWindowArgs& args);

    void _WindowSizeChanged(const winrt::Windows::Foundation::IInspectable& sender,
                            const winrt::Microsoft::Terminal::Control::WindowSizeChangedEventArgs& args);

    void _updateTheme();

    void _PropertyChangedHandler(const winrt::Windows::Foundation::IInspectable& sender,
                                 const winrt::Windows::UI::Xaml::Data::PropertyChangedEventArgs& args);

    void _initialResizeAndRepositionWindow(const HWND hwnd, til::rect proposedRect, winrt::Microsoft::Terminal::Settings::Model::LaunchMode& launchMode);

    void _resizeWindow(const HWND hwnd, til::size newSize);

    void _handleMoveContent(const winrt::Windows::Foundation::IInspectable& sender,
                            winrt::TerminalApp::RequestMoveContentArgs args);
    void _handleAttach(const winrt::Windows::Foundation::IInspectable& sender,
                       winrt::Microsoft::Terminal::Remoting::AttachRequest args);

    void _requestUpdateSettings();

    // Page -> us -> monarch
    void _handleReceiveContent(const winrt::Windows::Foundation::IInspectable& sender,
                               winrt::TerminalApp::RequestReceiveContentArgs args);

    // monarch -> us -> Page
    void _handleSendContent(const winrt::Windows::Foundation::IInspectable& sender,
                            winrt::Microsoft::Terminal::Remoting::RequestReceiveContentArgs args);

    void _startFrameTimer();
    void _stopFrameTimer();
    void _updateFrameColor(const winrt::Windows::Foundation::IInspectable&, const winrt::Windows::Foundation::IInspectable&);

    void _HandleRequestLaunchPosition(const winrt::Windows::Foundation::IInspectable& sender,
                                      winrt::TerminalApp::LaunchPositionRequest args);

    // Helper struct. By putting these all into one struct, we can revoke them
    // all at once, by assigning _revokers to a fresh Revokers instance. That'll
    // cause us to dtor the old one, which will immediately call revoke on all
    // the members as a part of their own dtors.
    struct Revokers
    {
        // Event handlers to revoke in ~AppHost, before calling App.Close
        winrt::Microsoft::Terminal::Remoting::Peasant::ExecuteCommandlineRequested_revoker peasantExecuteCommandlineRequested;
        winrt::Microsoft::Terminal::Remoting::Peasant::SummonRequested_revoker peasantSummonRequested;
        winrt::Microsoft::Terminal::Remoting::Peasant::DisplayWindowIdRequested_revoker peasantDisplayWindowIdRequested;
        winrt::Microsoft::Terminal::Remoting::Peasant::QuitRequested_revoker peasantQuitRequested;

        winrt::Microsoft::Terminal::Remoting::Peasant::AttachRequested_revoker AttachRequested;

        winrt::TerminalApp::TerminalWindow::Initialized_revoker Initialized;
        winrt::TerminalApp::TerminalWindow::CloseRequested_revoker CloseRequested;
        winrt::TerminalApp::TerminalWindow::RequestedThemeChanged_revoker RequestedThemeChanged;
        winrt::TerminalApp::TerminalWindow::FullscreenChanged_revoker FullscreenChanged;
        winrt::TerminalApp::TerminalWindow::FocusModeChanged_revoker FocusModeChanged;
        winrt::TerminalApp::TerminalWindow::AlwaysOnTopChanged_revoker AlwaysOnTopChanged;
        winrt::TerminalApp::TerminalWindow::RaiseVisualBell_revoker RaiseVisualBell;
        winrt::TerminalApp::TerminalWindow::SystemMenuChangeRequested_revoker SystemMenuChangeRequested;
        winrt::TerminalApp::TerminalWindow::ChangeMaximizeRequested_revoker ChangeMaximizeRequested;
        winrt::TerminalApp::TerminalWindow::TitleChanged_revoker TitleChanged;
        winrt::TerminalApp::TerminalWindow::CloseWindowRequested_revoker CloseWindowRequested;
        winrt::TerminalApp::TerminalWindow::SetTaskbarProgress_revoker SetTaskbarProgress;
        winrt::TerminalApp::TerminalWindow::IdentifyWindowsRequested_revoker IdentifyWindowsRequested;
        winrt::TerminalApp::TerminalWindow::RenameWindowRequested_revoker RenameWindowRequested;
        winrt::TerminalApp::TerminalWindow::IsQuakeWindowChanged_revoker IsQuakeWindowChanged;
        winrt::TerminalApp::TerminalWindow::SummonWindowRequested_revoker SummonWindowRequested;
        winrt::TerminalApp::TerminalWindow::OpenSystemMenu_revoker OpenSystemMenu;
        winrt::TerminalApp::TerminalWindow::QuitRequested_revoker QuitRequested;
        winrt::TerminalApp::TerminalWindow::ShowWindowChanged_revoker ShowWindowChanged;
        winrt::TerminalApp::TerminalWindow::RequestMoveContent_revoker RequestMoveContent;
        winrt::TerminalApp::TerminalWindow::RequestReceiveContent_revoker RequestReceiveContent;
        winrt::TerminalApp::TerminalWindow::RequestLaunchPosition_revoker RequestLaunchPosition;
        winrt::TerminalApp::TerminalWindow::PropertyChanged_revoker PropertyChanged;
        winrt::TerminalApp::TerminalWindow::SettingsChanged_revoker SettingsChanged;
        winrt::TerminalApp::TerminalWindow::WindowSizeChanged_revoker WindowSizeChanged;

        winrt::Microsoft::Terminal::Remoting::Peasant::SendContentRequested_revoker SendContentRequested;
    } _revokers{};

    // our IslandWindow is not a WinRT type. It can't make auto_revokers like
    // the above. We also need to make sure to unregister ourself from the
    // window when we refrigerate the window thread so that the window can later
    // be re-used.
    struct WindowRevokers
    {
        winrt::event_token MouseScrolled;
        winrt::event_token WindowActivated;
        winrt::event_token WindowMoved;
        winrt::event_token ShouldExitFullscreen;
        winrt::event_token WindowCloseButtonClicked;
        winrt::event_token DragRegionClicked;
        winrt::event_token WindowVisibilityChanged;
        winrt::event_token UpdateSettingsRequested;
        winrt::event_token MaximizeChanged;
        winrt::event_token AutomaticShutdownRequested;
        // LOAD BEARING!!
        //If you add events here, make sure they're revoked in AppHost::_revokeWindowCallbacks
    } _windowCallbacks{};
};
