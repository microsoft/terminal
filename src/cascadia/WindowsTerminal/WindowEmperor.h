/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Class Name:
- WindowEmperor.h

Abstract:
- TODO!

--*/

#pragma once
#include "pch.h"

#include "WindowThread.h"

class WindowEmperor
{
public:
    WindowEmperor() noexcept;
    ~WindowEmperor();
    bool ShouldExit();
    void WaitForWindows();

    bool HandleCommandlineArgs();
    void CreateNewWindowThread(winrt::Microsoft::Terminal::Remoting::WindowRequestedArgs args, const bool firstWindow);

    LRESULT MessageHandler(UINT const message, WPARAM const wParam, LPARAM const lParam) noexcept;
    wil::unique_hwnd _window;

private:
    winrt::TerminalApp::App _app;
    winrt::Windows::System::DispatcherQueue _dispatcher{ nullptr };
    winrt::Microsoft::Terminal::Remoting::WindowManager2 _manager;

    std::vector<std::shared_ptr<WindowThread>> _windows;
    std::vector<std::thread> _threads;

    std::optional<til::throttled_func_trailing<>> _getWindowLayoutThrottler;

    winrt::event_token _WindowCreatedToken;
    winrt::event_token _WindowClosedToken;

    std::vector<winrt::Microsoft::Terminal::Settings::Model::GlobalSummonArgs> _hotkeys;

    std::unique_ptr<NotificationIcon> _notificationIcon;

    void _becomeMonarch();
    void _numberOfWindowsChanged(const winrt::Windows::Foundation::IInspectable&, const winrt::Windows::Foundation::IInspectable&);
    void _quitAllRequested(const winrt::Windows::Foundation::IInspectable&,
                           const winrt::Microsoft::Terminal::Remoting::QuitAllRequestedArgs&);

    winrt::Windows::Foundation::IAsyncAction _SaveWindowLayouts();
    winrt::fire_and_forget _SaveWindowLayoutsRepeat();

    void _createMessageWindow();

    void _hotkeyPressed(const long hotkeyIndex);
    bool _registerHotKey(const int index, const winrt::Microsoft::Terminal::Control::KeyChord& hotkey) noexcept;
    void _unregisterHotKey(const int index) noexcept;
    winrt::fire_and_forget _setupGlobalHotkeys();

    winrt::fire_and_forget _close();

    void _createNotificationIcon();
    void _destroyNotificationIcon();
    void _checkWindowsForNotificationIcon();
    void _showNotificationIconRequested();
    void _hideNotificationIconRequested();

    struct Revokers
    {
        winrt::Microsoft::Terminal::Remoting::WindowManager::ShowNotificationIconRequested_revoker ShowNotificationIconRequested;
        winrt::Microsoft::Terminal::Remoting::WindowManager::HideNotificationIconRequested_revoker HideNotificationIconRequested;
        winrt::Microsoft::Terminal::Remoting::WindowManager2::QuitAllRequested_revoker QuitAllRequested;
    } _revokers{};
};
