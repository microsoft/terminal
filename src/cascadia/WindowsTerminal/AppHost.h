// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "NonClientIslandWindow.h"
#include "TrayIcon.h"

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

    void _HandleCommandlineArgs();

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

    void _CreateTrayIcon();
    void _DestroyTrayIcon();
    void _ShowTrayIconRequested();
    void _HideTrayIconRequested();
    std::unique_ptr<TrayIcon> _trayIcon;
    winrt::event_token _ReAddTrayIconToken;
    winrt::event_token _TrayIconPressedToken;
    winrt::event_token _ShowTrayContextMenuToken;
    winrt::event_token _TrayMenuItemSelectedToken;
};
