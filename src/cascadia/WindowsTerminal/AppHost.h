// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "NonClientIslandWindow.h"
#include "NotificationIcon.h"
#include <ThrottledFunc.h>

class AppHost
{
public:
    AppHost() noexcept;
    virtual ~AppHost();

    void AppTitleChanged(const winrt::Windows::Foundation::IInspectable& sender, winrt::hstring newTitle);
    void LastTabClosed(const winrt::Windows::Foundation::IInspectable& sender, const winrt::TerminalApp::LastTabClosedEventArgs& args);
    void Initialize();
    bool OnDirectKeyEvent(const uint32_t vkey, const uint8_t scanCode, const bool down);
    void SetTaskbarProgress(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::Foundation::IInspectable& args);

    bool HasWindow();

private:
    std::unique_ptr<IslandWindow> _window;
    winrt::TerminalApp::App _app;
    winrt::TerminalApp::AppLogic _logic;
    winrt::Microsoft::Terminal::Remoting::WindowManager _windowManager{ nullptr };

    std::vector<winrt::Microsoft::Terminal::Settings::Model::GlobalSummonArgs> _hotkeys;
    winrt::com_ptr<IVirtualDesktopManager> _desktopManager{ nullptr };

    bool _shouldCreateWindow{ false };
    bool _useNonClientArea{ false };

    std::optional<til::throttled_func_trailing<>> _getWindowLayoutThrottler;
    std::shared_ptr<ThrottledFuncTrailing<bool>> _showHideWindowThrottler;
    winrt::Windows::Foundation::IAsyncAction _SaveWindowLayouts();
    winrt::fire_and_forget _SaveWindowLayoutsRepeat();

    void _HandleCommandlineArgs();
    winrt::Microsoft::Terminal::Settings::Model::LaunchPosition _GetWindowLaunchPosition();

    void _HandleCreateWindow(const HWND hwnd, til::rect proposedRect, winrt::Microsoft::Terminal::Settings::Model::LaunchMode& launchMode);
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
    void _RaiseVisualBell(const winrt::Windows::Foundation::IInspectable& sender,
                          const winrt::Windows::Foundation::IInspectable& arg);
    void _WindowMouseWheeled(const til::point coord, const int32_t delta);
    winrt::fire_and_forget _WindowActivated(bool activated);
    void _WindowMoved();

    void _DispatchCommandline(winrt::Windows::Foundation::IInspectable sender,
                              winrt::Microsoft::Terminal::Remoting::CommandlineArgs args);

    winrt::Windows::Foundation::IAsyncOperation<winrt::hstring> _GetWindowLayoutAsync();

    void _FindTargetWindow(const winrt::Windows::Foundation::IInspectable& sender,
                           const winrt::Microsoft::Terminal::Remoting::FindTargetWindowArgs& args);

    void _BecomeMonarch(const winrt::Windows::Foundation::IInspectable& sender,
                        const winrt::Windows::Foundation::IInspectable& args);
    void _GlobalHotkeyPressed(const long hotkeyIndex);
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

    void _listenForInboundConnections();
    winrt::fire_and_forget _setupGlobalHotkeys();
    winrt::fire_and_forget _createNewTerminalWindow(winrt::Microsoft::Terminal::Settings::Model::GlobalSummonArgs args);
    void _HandleSettingsChanged(const winrt::Windows::Foundation::IInspectable& sender,
                                const winrt::Windows::Foundation::IInspectable& args);

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

    void _QuitAllRequested(const winrt::Windows::Foundation::IInspectable& sender,
                           const winrt::Microsoft::Terminal::Remoting::QuitAllRequestedArgs& args);

    void _ShowWindowChanged(const winrt::Windows::Foundation::IInspectable& sender,
                            const winrt::Microsoft::Terminal::Control::ShowWindowArgs& args);

    void _CreateNotificationIcon();
    void _DestroyNotificationIcon();
    void _ShowNotificationIconRequested(const winrt::Windows::Foundation::IInspectable& sender,
                                        const winrt::Windows::Foundation::IInspectable& args);
    void _HideNotificationIconRequested(const winrt::Windows::Foundation::IInspectable& sender,
                                        const winrt::Windows::Foundation::IInspectable& args);

    void _updateTheme();

    void _PropertyChangedHandler(const winrt::Windows::Foundation::IInspectable& sender,
                                 const winrt::Windows::UI::Xaml::Data::PropertyChangedEventArgs& args);

    void _initialResizeAndRepositionWindow(const HWND hwnd, RECT proposedRect, winrt::Microsoft::Terminal::Settings::Model::LaunchMode& launchMode);

    std::unique_ptr<NotificationIcon> _notificationIcon;
    winrt::event_token _ReAddNotificationIconToken;
    winrt::event_token _NotificationIconPressedToken;
    winrt::event_token _ShowNotificationIconContextMenuToken;
    winrt::event_token _NotificationIconMenuItemSelectedToken;
    winrt::event_token _GetWindowLayoutRequestedToken;
    winrt::event_token _WindowCreatedToken;
    winrt::event_token _WindowClosedToken;

    // Helper struct. By putting these all into one struct, we can revoke them
    // all at once, by assigning _revokers to a fresh Revokers instance. That'll
    // cause us to dtor the old one, which will immediately call revoke on all
    // the members as a part of their own dtors.
    struct Revokers
    {
        // Event handlers to revoke in ~AppHost, before calling App.Close
        winrt::Microsoft::Terminal::Remoting::WindowManager::BecameMonarch_revoker BecameMonarch;
        winrt::Microsoft::Terminal::Remoting::Peasant::ExecuteCommandlineRequested_revoker peasantExecuteCommandlineRequested;
        winrt::Microsoft::Terminal::Remoting::Peasant::SummonRequested_revoker peasantSummonRequested;
        winrt::Microsoft::Terminal::Remoting::Peasant::DisplayWindowIdRequested_revoker peasantDisplayWindowIdRequested;
        winrt::Microsoft::Terminal::Remoting::Peasant::QuitRequested_revoker peasantQuitRequested;
        winrt::TerminalApp::AppLogic::CloseRequested_revoker CloseRequested;
        winrt::TerminalApp::AppLogic::RequestedThemeChanged_revoker RequestedThemeChanged;
        winrt::TerminalApp::AppLogic::FullscreenChanged_revoker FullscreenChanged;
        winrt::TerminalApp::AppLogic::FocusModeChanged_revoker FocusModeChanged;
        winrt::TerminalApp::AppLogic::AlwaysOnTopChanged_revoker AlwaysOnTopChanged;
        winrt::TerminalApp::AppLogic::RaiseVisualBell_revoker RaiseVisualBell;
        winrt::TerminalApp::AppLogic::SystemMenuChangeRequested_revoker SystemMenuChangeRequested;
        winrt::TerminalApp::AppLogic::ChangeMaximizeRequested_revoker ChangeMaximizeRequested;
        winrt::TerminalApp::AppLogic::TitleChanged_revoker TitleChanged;
        winrt::TerminalApp::AppLogic::LastTabClosed_revoker LastTabClosed;
        winrt::TerminalApp::AppLogic::SetTaskbarProgress_revoker SetTaskbarProgress;
        winrt::TerminalApp::AppLogic::IdentifyWindowsRequested_revoker IdentifyWindowsRequested;
        winrt::TerminalApp::AppLogic::RenameWindowRequested_revoker RenameWindowRequested;
        winrt::TerminalApp::AppLogic::SettingsChanged_revoker SettingsChanged;
        winrt::TerminalApp::AppLogic::IsQuakeWindowChanged_revoker IsQuakeWindowChanged;
        winrt::TerminalApp::AppLogic::SummonWindowRequested_revoker SummonWindowRequested;
        winrt::TerminalApp::AppLogic::OpenSystemMenu_revoker OpenSystemMenu;
        winrt::TerminalApp::AppLogic::QuitRequested_revoker QuitRequested;
        winrt::TerminalApp::AppLogic::ShowWindowChanged_revoker ShowWindowChanged;
        winrt::Microsoft::Terminal::Remoting::WindowManager::ShowNotificationIconRequested_revoker ShowNotificationIconRequested;
        winrt::Microsoft::Terminal::Remoting::WindowManager::HideNotificationIconRequested_revoker HideNotificationIconRequested;
        winrt::Microsoft::Terminal::Remoting::WindowManager::QuitAllRequested_revoker QuitAllRequested;
        winrt::TerminalApp::AppLogic::PropertyChanged_revoker PropertyChanged;
    } _revokers{};
};
