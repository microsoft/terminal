// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "NonClientIslandWindow.h"
#include "NotificationIcon.h"
#include <til/throttled_func.h>

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
    winrt::Windows::Foundation::IAsyncAction _SaveWindowLayouts();
    winrt::fire_and_forget _SaveWindowLayoutsRepeat();

    void _HandleCommandlineArgs();
    winrt::Microsoft::Terminal::Settings::Model::LaunchPosition _GetWindowLaunchPosition();

    void _HandleCreateWindow(const HWND hwnd, RECT proposedRect, winrt::Microsoft::Terminal::Settings::Model::LaunchMode& launchMode);
    void _UpdateTitleBarContent(const winrt::Windows::Foundation::IInspectable& sender,
                                const winrt::Windows::UI::Xaml::UIElement& arg);
    void _UpdateTheme(const winrt::Windows::Foundation::IInspectable&,
                      const winrt::Windows::UI::Xaml::ElementTheme& arg);
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
    winrt::fire_and_forget _WindowActivated();
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

    void _CreateNotificationIcon();
    void _DestroyNotificationIcon();
    void _ShowNotificationIconRequested(const winrt::Windows::Foundation::IInspectable& sender,
                                        const winrt::Windows::Foundation::IInspectable& args);
    void _HideNotificationIconRequested(const winrt::Windows::Foundation::IInspectable& sender,
                                        const winrt::Windows::Foundation::IInspectable& args);
    std::unique_ptr<NotificationIcon> _notificationIcon;
    winrt::event_token _ReAddNotificationIconToken;
    winrt::event_token _NotificationIconPressedToken;
    winrt::event_token _ShowNotificationIconContextMenuToken;
    winrt::event_token _NotificationIconMenuItemSelectedToken;
    winrt::event_token _GetWindowLayoutRequestedToken;
    winrt::event_token _WindowCreatedToken;
    winrt::event_token _WindowClosedToken;

    // winrt::event_revoker _MouseScrolledRevoker;
    // winrt::event_revoker _WindowActivatedRevoker;
    // winrt::event_revoker _WindowMovedRevoker;
    // winrt::event_revoker _HotkeyPressedRevoker;
    // winrt::event_revoker _SetAlwaysOnTopRevoker;
    // winrt::event_revoker _ShouldExitFullscreenRevoker;
    winrt::Microsoft::Terminal::Remoting::WindowManager::BecameMonarch_revoker _BecameMonarchRevoker;
    winrt::Microsoft::Terminal::Remoting::Peasant::ExecuteCommandlineRequested_revoker _peasantExecuteCommandlineRequestedRevoker;
    winrt::Microsoft::Terminal::Remoting::Peasant::SummonRequested_revoker _peasantSummonRequestedRevoker;
    winrt::Microsoft::Terminal::Remoting::Peasant::DisplayWindowIdRequested_revoker _peasantDisplayWindowIdRequestedRevoker;
    winrt::Microsoft::Terminal::Remoting::Peasant::QuitRequested_revoker _peasantQuitRequestedRevoker;
    winrt::TerminalApp::AppLogic::CloseRequested_revoker _CloseRequestedRevoker;
    winrt::TerminalApp::AppLogic::RequestedThemeChanged_revoker _RequestedThemeChangedRevoker;
    winrt::TerminalApp::AppLogic::FullscreenChanged_revoker _FullscreenChangedRevoker;
    winrt::TerminalApp::AppLogic::FocusModeChanged_revoker _FocusModeChangedRevoker;
    winrt::TerminalApp::AppLogic::AlwaysOnTopChanged_revoker _AlwaysOnTopChangedRevoker;
    winrt::TerminalApp::AppLogic::RaiseVisualBell_revoker _RaiseVisualBellRevoker;
    winrt::TerminalApp::AppLogic::SystemMenuChangeRequested_revoker _SystemMenuChangeRequestedRevoker;
    winrt::TerminalApp::AppLogic::ChangeMaximizeRequested_revoker _ChangeMaximizeRequestedRevoker;
    winrt::TerminalApp::AppLogic::TitleChanged_revoker _TitleChangedRevoker;
    winrt::TerminalApp::AppLogic::LastTabClosed_revoker _LastTabClosedRevoker;
    winrt::TerminalApp::AppLogic::SetTaskbarProgress_revoker _SetTaskbarProgressRevoker;
    winrt::TerminalApp::AppLogic::IdentifyWindowsRequested_revoker _IdentifyWindowsRequestedRevoker;
    winrt::TerminalApp::AppLogic::RenameWindowRequested_revoker _RenameWindowRequestedRevoker;
    winrt::TerminalApp::AppLogic::SettingsChanged_revoker _SettingsChangedRevoker;
    winrt::TerminalApp::AppLogic::IsQuakeWindowChanged_revoker _IsQuakeWindowChangedRevoker;
    winrt::TerminalApp::AppLogic::SummonWindowRequested_revoker _SummonWindowRequestedRevoker;
    winrt::TerminalApp::AppLogic::OpenSystemMenu_revoker _OpenSystemMenuRevoker;
    winrt::TerminalApp::AppLogic::QuitRequested_revoker _QuitRequestedRevoker;
    winrt::Microsoft::Terminal::Remoting::WindowManager::ShowNotificationIconRequested_revoker _ShowNotificationIconRequestedRevoker;
    winrt::Microsoft::Terminal::Remoting::WindowManager::HideNotificationIconRequested_revoker _HideNotificationIconRequestedRevoker;
    winrt::Microsoft::Terminal::Remoting::WindowManager::QuitAllRequested_revoker _QuitAllRequestedRevoker;
};
