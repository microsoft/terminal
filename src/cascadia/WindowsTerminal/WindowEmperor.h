/*++
Copyright (c) Microsoft Corporation Licensed under the MIT license.

Class Name:
- WindowEmperor.h

Abstract:
- The WindowEmperor is our class for managing the single Terminal process
  with all our windows. It will be responsible for handling the commandline
  arguments. It will initially try to find another terminal process to
  communicate with. If it does, it'll hand off to the existing process.
- If it determines that it should create a window, it will set up a new thread
  for that window, and a message loop on the main thread for handling global
  state, such as hotkeys and the notification icon.

--*/

#pragma once
#include "pch.h"

#include "WindowThread.h"

class WindowEmperor : public std::enable_shared_from_this<WindowEmperor>
{
public:
    WindowEmperor() noexcept;
    ~WindowEmperor();
    void WaitForWindows();

    bool HandleCommandlineArgs();

private:
    void _createNewWindowThread(const winrt::Microsoft::Terminal::Remoting::WindowRequestedArgs& args);

    [[nodiscard]] static LRESULT __stdcall _wndProc(HWND const window, UINT const message, WPARAM const wparam, LPARAM const lparam) noexcept;
    LRESULT _messageHandler(UINT const message, WPARAM const wParam, LPARAM const lParam) noexcept;
    wil::unique_hwnd _window;

    winrt::TerminalApp::App _app;
    winrt::Windows::System::DispatcherQueue _dispatcher{ nullptr };
    winrt::Microsoft::Terminal::Remoting::WindowManager _manager;

    til::shared_mutex<std::vector<std::shared_ptr<WindowThread>>> _windows;
    std::atomic<uint32_t> _windowThreadInstances;

    til::shared_mutex<std::vector<std::shared_ptr<WindowThread>>> _oldThreads;

    std::optional<til::throttled_func_trailing<>> _getWindowLayoutThrottler;

    winrt::event_token _WindowCreatedToken;
    winrt::event_token _WindowClosedToken;

    std::vector<winrt::Microsoft::Terminal::Settings::Model::GlobalSummonArgs> _hotkeys;

    std::unique_ptr<NotificationIcon> _notificationIcon;

    bool _quitting{ false };

    void _windowStartedHandlerPostXAML(const std::shared_ptr<WindowThread>& sender);
    void _removeWindow(uint64_t senderID);
    void _decrementWindowCount();

    void _becomeMonarch();
    void _numberOfWindowsChanged(const winrt::Windows::Foundation::IInspectable&, const winrt::Windows::Foundation::IInspectable&);
    void _quitAllRequested(const winrt::Windows::Foundation::IInspectable&,
                           const winrt::Microsoft::Terminal::Remoting::QuitAllRequestedArgs&);

    winrt::fire_and_forget _windowIsQuakeWindowChanged(winrt::Windows::Foundation::IInspectable sender, winrt::Windows::Foundation::IInspectable args);
    winrt::fire_and_forget _windowRequestUpdateSettings();

    winrt::Windows::Foundation::IAsyncAction _saveWindowLayouts();
    winrt::fire_and_forget _saveWindowLayoutsRepeat();

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
        winrt::Microsoft::Terminal::Remoting::WindowManager::WindowCreated_revoker WindowCreated;
        winrt::Microsoft::Terminal::Remoting::WindowManager::WindowClosed_revoker WindowClosed;
        winrt::Microsoft::Terminal::Remoting::WindowManager::QuitAllRequested_revoker QuitAllRequested;
    } _revokers{};
};
