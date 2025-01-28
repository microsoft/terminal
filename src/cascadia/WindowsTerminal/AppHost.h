// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "pch.h"
#include "NonClientIslandWindow.h"
#include <ThrottledFunc.h>

class WindowEmperor;

class AppHost : public std::enable_shared_from_this<AppHost>
{
public:
    AppHost(WindowEmperor* manager, const winrt::TerminalApp::AppLogic& logic, winrt::TerminalApp::WindowRequestedArgs args) noexcept;

    void Initialize();
    void Close();

    int64_t GetLastActivatedTime() const noexcept;
    winrt::Windows::Foundation::IAsyncOperation<winrt::guid> GetVirtualDesktopId();
    IslandWindow* GetWindow() const noexcept;
    winrt::TerminalApp::TerminalWindow Logic();

    bool OnDirectKeyEvent(uint32_t vkey, uint8_t scanCode, bool down);
    void SetTaskbarProgress(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::Foundation::IInspectable& args);
    safe_void_coroutine HandleSummon(winrt::TerminalApp::SummonWindowBehavior args) const;
    void DispatchCommandline(winrt::TerminalApp::CommandlineArgs args);

private:
    enum class WindowInitializedState : uint32_t
    {
        NotInitialized = 0,
        Initializing = 1,
        Initialized = 2,
    };

    WindowEmperor* _windowManager = nullptr;
    std::unique_ptr<IslandWindow> _window;
    winrt::TerminalApp::AppLogic _appLogic{ nullptr };
    winrt::TerminalApp::TerminalWindow _windowLogic{ nullptr };
    std::shared_ptr<ThrottledFuncTrailing<bool>> _showHideWindowThrottler;
    SafeDispatcherTimer _frameTimer;
    LARGE_INTEGER _lastActivatedTime{};
    winrt::guid _virtualDesktopId{};
    WindowInitializedState _isWindowInitialized{ WindowInitializedState::NotInitialized };
    winrt::Microsoft::Terminal::Settings::Model::LaunchMode _launchMode{};
    uint32_t _launchShowWindowCommand{ SW_NORMAL };
    bool _useNonClientArea{ false };

    void _revokeWindowCallbacks();

    void _HandleCommandlineArgs(const winrt::TerminalApp::WindowRequestedArgs& args);

    winrt::Microsoft::Terminal::Settings::Model::LaunchPosition _GetWindowLaunchPosition();

    void _HandleCreateWindow(HWND hwnd, const til::rect& proposedRect);

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
    void _WindowMouseWheeled(const winrt::Windows::Foundation::Point coord, const int32_t delta);
    void _WindowActivated(bool activated);
    void _WindowMoved();

    void _IdentifyWindowsRequested(winrt::Windows::Foundation::IInspectable sender,
                                   winrt::Windows::Foundation::IInspectable args);
    void _DisplayWindowId(const winrt::Windows::Foundation::IInspectable& sender,
                          const winrt::Windows::Foundation::IInspectable& args);

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

    void _initialResizeAndRepositionWindow(HWND hwnd, til::rect proposedRect, winrt::Microsoft::Terminal::Settings::Model::LaunchMode& launchMode);

    void _resizeWindow(HWND hwnd, til::size newSize);

    void _handleMoveContent(const winrt::Windows::Foundation::IInspectable& sender,
                            winrt::TerminalApp::RequestMoveContentArgs args);

    void _handleReceiveContent(const winrt::Windows::Foundation::IInspectable& sender,
                               winrt::TerminalApp::RequestReceiveContentArgs args);

    void _startFrameTimer();
    void _stopFrameTimer();
    void _updateFrameColor(const winrt::Windows::Foundation::IInspectable&, const winrt::Windows::Foundation::IInspectable&);

    void _AppTitleChanged(const winrt::Windows::Foundation::IInspectable& sender, winrt::hstring newTitle);
    void _HandleRequestLaunchPosition(const winrt::Windows::Foundation::IInspectable& sender,
                                      winrt::TerminalApp::LaunchPositionRequest args);

    // Helper struct. By putting these all into one struct, we can revoke them
    // all at once, by assigning _revokers to a fresh Revokers instance. That'll
    // cause us to dtor the old one, which will immediately call revoke on all
    // the members as a part of their own dtors.
    struct Revokers
    {
        winrt::TerminalApp::TerminalWindow::Initialized_revoker Initialized;
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
        winrt::event_token MaximizeChanged;
        // LOAD BEARING!!
        //If you add events here, make sure they're revoked in AppHost::_revokeWindowCallbacks
    } _windowCallbacks{};
};
