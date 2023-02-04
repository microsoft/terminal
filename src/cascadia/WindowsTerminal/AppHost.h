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

    void AppTitleChanged(const WF::IInspectable& sender, winrt::hstring newTitle);
    void LastTabClosed(const WF::IInspectable& sender, const MTApp::LastTabClosedEventArgs& args);
    void Initialize();
    bool OnDirectKeyEvent(const uint32_t vkey, const uint8_t scanCode, const bool down);
    void SetTaskbarProgress(const WF::IInspectable& sender, const WF::IInspectable& args);

    bool HasWindow();

private:
    std::unique_ptr<IslandWindow> _window;
    MTApp::App _app;
    MTApp::AppLogic _logic;
    winrt::Microsoft::Terminal::Remoting::WindowManager _windowManager{ nullptr };

    std::vector<MTSM::GlobalSummonArgs> _hotkeys;
    winrt::com_ptr<IVirtualDesktopManager> _desktopManager{ nullptr };

    bool _shouldCreateWindow{ false };
    bool _useNonClientArea{ false };

    std::optional<til::throttled_func_trailing<>> _getWindowLayoutThrottler;
    std::shared_ptr<ThrottledFuncTrailing<bool>> _showHideWindowThrottler;
    WF::IAsyncAction _SaveWindowLayouts();
    winrt::fire_and_forget _SaveWindowLayoutsRepeat();

    void _HandleCommandlineArgs();
    MTSM::LaunchPosition _GetWindowLaunchPosition();

    void _HandleCreateWindow(const HWND hwnd, til::rect proposedRect, MTSM::LaunchMode& launchMode);
    void _UpdateTitleBarContent(const WF::IInspectable& sender,
                                const WUX::UIElement& arg);
    void _UpdateTheme(const WF::IInspectable&,
                      const MTSM::Theme& arg);
    void _FocusModeChanged(const WF::IInspectable& sender,
                           const WF::IInspectable& arg);
    void _FullscreenChanged(const WF::IInspectable& sender,
                            const WF::IInspectable& arg);
    void _ChangeMaximizeRequested(const WF::IInspectable& sender,
                                  const WF::IInspectable& arg);
    void _AlwaysOnTopChanged(const WF::IInspectable& sender,
                             const WF::IInspectable& arg);
    void _RaiseVisualBell(const WF::IInspectable& sender,
                          const WF::IInspectable& arg);
    void _WindowMouseWheeled(const til::point coord, const int32_t delta);
    winrt::fire_and_forget _WindowActivated(bool activated);
    void _WindowMoved();

    void _DispatchCommandline(WF::IInspectable sender,
                              winrt::Microsoft::Terminal::Remoting::CommandlineArgs args);

    WF::IAsyncOperation<winrt::hstring> _GetWindowLayoutAsync();

    void _FindTargetWindow(const WF::IInspectable& sender,
                           const winrt::Microsoft::Terminal::Remoting::FindTargetWindowArgs& args);

    void _BecomeMonarch(const WF::IInspectable& sender,
                        const WF::IInspectable& args);
    void _GlobalHotkeyPressed(const long hotkeyIndex);
    void _HandleSummon(const WF::IInspectable& sender,
                       const winrt::Microsoft::Terminal::Remoting::SummonWindowBehavior& args);

    winrt::fire_and_forget _IdentifyWindowsRequested(const WF::IInspectable sender,
                                                     const WF::IInspectable args);
    void _DisplayWindowId(const WF::IInspectable& sender,
                          const WF::IInspectable& args);
    winrt::fire_and_forget _RenameWindowRequested(const WF::IInspectable sender,
                                                  const MTApp::RenameWindowRequestedArgs args);

    GUID _CurrentDesktopGuid();

    bool _LazyLoadDesktopManager();

    void _listenForInboundConnections();
    winrt::fire_and_forget _setupGlobalHotkeys();
    winrt::fire_and_forget _createNewTerminalWindow(MTSM::GlobalSummonArgs args);
    void _HandleSettingsChanged(const WF::IInspectable& sender,
                                const WF::IInspectable& args);

    void _IsQuakeWindowChanged(const WF::IInspectable& sender,
                               const WF::IInspectable& args);

    void _SummonWindowRequested(const WF::IInspectable& sender,
                                const WF::IInspectable& args);

    void _OpenSystemMenu(const WF::IInspectable& sender,
                         const WF::IInspectable& args);

    void _SystemMenuChangeRequested(const WF::IInspectable& sender,
                                    const MTApp::SystemMenuChangeArgs& args);

    winrt::fire_and_forget _QuitRequested(const WF::IInspectable& sender,
                                          const WF::IInspectable& args);

    void _RequestQuitAll(const WF::IInspectable& sender,
                         const WF::IInspectable& args);
    void _CloseRequested(const WF::IInspectable& sender,
                         const WF::IInspectable& args);

    void _QuitAllRequested(const WF::IInspectable& sender,
                           const winrt::Microsoft::Terminal::Remoting::QuitAllRequestedArgs& args);

    void _ShowWindowChanged(const WF::IInspectable& sender,
                            const MTControl::ShowWindowArgs& args);

    void _CreateNotificationIcon();
    void _DestroyNotificationIcon();
    void _ShowNotificationIconRequested(const WF::IInspectable& sender,
                                        const WF::IInspectable& args);
    void _HideNotificationIconRequested(const WF::IInspectable& sender,
                                        const WF::IInspectable& args);

    void _updateTheme();

    void _PropertyChangedHandler(const WF::IInspectable& sender,
                                 const WUX::Data::PropertyChangedEventArgs& args);

    void _initialResizeAndRepositionWindow(const HWND hwnd, RECT proposedRect, MTSM::LaunchMode& launchMode);

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
        MTApp::AppLogic::CloseRequested_revoker CloseRequested;
        MTApp::AppLogic::RequestedThemeChanged_revoker RequestedThemeChanged;
        MTApp::AppLogic::FullscreenChanged_revoker FullscreenChanged;
        MTApp::AppLogic::FocusModeChanged_revoker FocusModeChanged;
        MTApp::AppLogic::AlwaysOnTopChanged_revoker AlwaysOnTopChanged;
        MTApp::AppLogic::RaiseVisualBell_revoker RaiseVisualBell;
        MTApp::AppLogic::SystemMenuChangeRequested_revoker SystemMenuChangeRequested;
        MTApp::AppLogic::ChangeMaximizeRequested_revoker ChangeMaximizeRequested;
        MTApp::AppLogic::TitleChanged_revoker TitleChanged;
        MTApp::AppLogic::LastTabClosed_revoker LastTabClosed;
        MTApp::AppLogic::SetTaskbarProgress_revoker SetTaskbarProgress;
        MTApp::AppLogic::IdentifyWindowsRequested_revoker IdentifyWindowsRequested;
        MTApp::AppLogic::RenameWindowRequested_revoker RenameWindowRequested;
        MTApp::AppLogic::SettingsChanged_revoker SettingsChanged;
        MTApp::AppLogic::IsQuakeWindowChanged_revoker IsQuakeWindowChanged;
        MTApp::AppLogic::SummonWindowRequested_revoker SummonWindowRequested;
        MTApp::AppLogic::OpenSystemMenu_revoker OpenSystemMenu;
        MTApp::AppLogic::QuitRequested_revoker QuitRequested;
        MTApp::AppLogic::ShowWindowChanged_revoker ShowWindowChanged;
        winrt::Microsoft::Terminal::Remoting::WindowManager::ShowNotificationIconRequested_revoker ShowNotificationIconRequested;
        winrt::Microsoft::Terminal::Remoting::WindowManager::HideNotificationIconRequested_revoker HideNotificationIconRequested;
        winrt::Microsoft::Terminal::Remoting::WindowManager::QuitAllRequested_revoker QuitAllRequested;
        MTApp::AppLogic::PropertyChanged_revoker PropertyChanged;
    } _revokers{};
};
