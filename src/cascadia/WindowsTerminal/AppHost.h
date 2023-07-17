// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "NonClientIslandWindow.h"
#include "NotificationIcon.h"
#include <ThrottledFunc.h>

class AppHost
{
public:
    AppHost(const winrt::TerminalApp::AppLogic& logic,
            winrt::Microsoft::Terminal::Remoting::WindowRequestedArgs args,
            const winrt::Microsoft::Terminal::Remoting::WindowManager& manager,
            const winrt::Microsoft::Terminal::Remoting::Peasant& peasant,
            std::unique_ptr<IslandWindow> window = nullptr) noexcept;

    void AppTitleChanged(const winrt::Windows::Foundation::IInspectable& sender, winrt::hstring newTitle);
    void LastTabClosed(const winrt::Windows::Foundation::IInspectable& sender, const winrt::TerminalApp::LastTabClosedEventArgs& args);
    void Initialize();
    void Close();

    [[nodiscard]] std::unique_ptr<IslandWindow> Refrigerate();

    bool OnDirectKeyEvent(const uint32_t vkey, const uint8_t scanCode, const bool down);
    void SetTaskbarProgress(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::Foundation::IInspectable& args);

    winrt::TerminalApp::TerminalWindow Logic();

    static void s_DisplayMessageBox(const winrt::TerminalApp::ParseCommandlineResult& message);

    WINRT_CALLBACK(UpdateSettingsRequested, winrt::delegate<void()>);

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
    winrt::Windows::UI::Xaml::DispatcherTimer _frameTimer{ nullptr };

    uint32_t _launchShowWindowCommand{ SW_NORMAL };

    void _preInit();

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
    winrt::fire_and_forget _WindowInitializedHandler(const winrt::Windows::Foundation::IInspectable& sender,
                                                     const winrt::Windows::Foundation::IInspectable& arg);

    void _RaiseVisualBell(const winrt::Windows::Foundation::IInspectable& sender,
                          const winrt::Windows::Foundation::IInspectable& arg);
    void _WindowMouseWheeled(const til::point coord, const int32_t delta);
    void _WindowActivated(bool activated);
    winrt::fire_and_forget _peasantNotifyActivateWindow();
    void _WindowMoved();

    void _DispatchCommandline(winrt::Windows::Foundation::IInspectable sender,
                              winrt::Microsoft::Terminal::Remoting::CommandlineArgs args);

    winrt::Windows::Foundation::IAsyncOperation<winrt::hstring> _GetWindowLayoutAsync();

    void _HandleSummon(const winrt::Windows::Foundation::IInspectable& sender,
                       const winrt::Microsoft::Terminal::Remoting::SummonWindowBehavior& args);

    winrt::fire_and_forget _IdentifyWindowsRequested(const winrt::Windows::Foundation::IInspectable sender,
                                                     const winrt::Windows::Foundation::IInspectable args);
    void _DisplayWindowId(const winrt::Windows::Foundation::IInspectable& sender,
                          const winrt::Windows::Foundation::IInspectable& args);
    winrt::fire_and_forget _RenameWindowRequested(const winrt::Windows::Foundation::IInspectable sender,
                                                  const winrt::TerminalApp::RenameWindowRequestedArgs args);

    GUID _CurrentDesktopGuid();

    bool _LazyLoadDesktopManager();

    void _HandleSettingsChanged(const winrt::Windows::Foundation::IInspectable& sender,
                                const winrt::TerminalApp::SettingsLoadEventArgs& args);

    void _IsQuakeWindowChanged(const winrt::Windows::Foundation::IInspectable& sender,
                               const winrt::Windows::Foundation::IInspectable& args);

    void _SummonWindowRequested(const winrt::Windows::Foundation::IInspectable& sender,
                                const winrt::Windows::Foundation::IInspectable& args);

    void _OpenSystemMenu(const winrt::Windows::Foundation::IInspectable& sender,
                         const winrt::Windows::Foundation::IInspectable& args);

    void _SystemMenuChangeRequested(const winrt::Windows::Foundation::IInspectable& sender,
                                    const winrt::TerminalApp::SystemMenuChangeArgs& args);

    winrt::fire_and_forget _QuitRequested(const winrt::Windows::Foundation::IInspectable& sender,
                                          const winrt::Windows::Foundation::IInspectable& args);

    void _RequestQuitAll(const winrt::Windows::Foundation::IInspectable& sender,
                         const winrt::Windows::Foundation::IInspectable& args);
    void _CloseRequested(const winrt::Windows::Foundation::IInspectable& sender,
                         const winrt::Windows::Foundation::IInspectable& args);

    void _ShowWindowChanged(const winrt::Windows::Foundation::IInspectable& sender,
                            const winrt::Microsoft::Terminal::Control::ShowWindowArgs& args);

    void _updateTheme();

    void _PropertyChangedHandler(const winrt::Windows::Foundation::IInspectable& sender,
                                 const winrt::Windows::UI::Xaml::Data::PropertyChangedEventArgs& args);

    void _initialResizeAndRepositionWindow(const HWND hwnd, til::rect proposedRect, winrt::Microsoft::Terminal::Settings::Model::LaunchMode& launchMode);

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

    winrt::event_token _GetWindowLayoutRequestedToken;
    winrt::event_token _frameTimerToken;

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
        winrt::TerminalApp::TerminalWindow::LastTabClosed_revoker LastTabClosed;
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
        winrt::TerminalApp::TerminalWindow::PropertyChanged_revoker PropertyChanged;
        winrt::TerminalApp::TerminalWindow::SettingsChanged_revoker SettingsChanged;

        winrt::Microsoft::Terminal::Remoting::WindowManager::QuitAllRequested_revoker QuitAllRequested;
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
