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

    winrt::fire_and_forget _QuitRequested(const winrt::Windows::Foundation::IInspectable& sender,
                                          const winrt::Windows::Foundation::IInspectable& args);

    void _RequestQuitAll(const winrt::Windows::Foundation::IInspectable& sender,
                         const winrt::Windows::Foundation::IInspectable& args);

    void _QuitAllRequested(const winrt::Windows::Foundation::IInspectable& sender,
                           const winrt::Microsoft::Terminal::Remoting::QuitAllRequestedArgs& args);

    void _CreateNotificationIcon();
    void _DestroyNotificationIcon();
    void _ShowNotificationIconRequested();
    void _HideNotificationIconRequested();
    std::unique_ptr<NotificationIcon> _notificationIcon;
    winrt::event_token _ReAddNotificationIconToken;
    winrt::event_token _NotificationIconPressedToken;
    winrt::event_token _ShowNotificationIconContextMenuToken;
    winrt::event_token _NotificationIconMenuItemSelectedToken;
};
