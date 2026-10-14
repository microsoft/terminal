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

class AppHost;
struct TerminalProtocolComServer;
struct WindowTarget;

class WindowEmperor
{
public:
    enum UserMessages : UINT
    {
        WM_CLOSE_TERMINAL_WINDOW = WM_USER,
        WM_MESSAGE_BOX_CLOSED,
        WM_IDENTIFY_ALL_WINDOWS,
        WM_NOTIFY_FROM_NOTIFICATION_AREA,
        WM_GET_WINDOW_LIST,
        WM_CREATE_PROTOCOL_WINDOW,
    };

    // Used by WM_GET_WINDOW_LIST.  Callers allocate a vector on their
    // stack and pass a pointer through LPARAM; the emperor fills it in
    // synchronously via SendMessage.
    struct WindowListEntry
    {
        uint64_t Id;
        std::wstring Name;
    };

    // Used by WM_CREATE_PROTOCOL_WINDOW. The Terminal Protocol COM server runs
    // on an MTA thread, but windows can only be created on the main/UI thread,
    // so it allocates one of these on its stack and passes a pointer through
    // LPARAM. SendMessage blocks until the main thread has filled in the result.
    struct ProtocolWindowRequest
    {
        // in: the parsed -w target. Only New and (unmatched) Name reach here.
        const WindowTarget* Target;
        // in: for window's first tab.
        winrt::Microsoft::Terminal::Settings::Model::NewTerminalArgs Args;
        // out: 0 if the window could not be created.
        uint64_t CreatedWindowId;
    };

    WindowEmperor();
    ~WindowEmperor();

    HWND GetMainWindow() const noexcept;
    AppHost* GetWindowById(uint64_t id) const noexcept;
    AppHost* GetWindowByName(std::wstring_view name) const noexcept;
    void CreateNewWindow(winrt::TerminalApp::WindowRequestedArgs args);
    void HandleCommandlineArgs(int nCmdShow);
    void FocusTabInAnyWindow(const winrt::TerminalApp::Tab& tab) const;
    // OpenWindow is used for opening a new window or summoning an existing window by name.
    void OpenWindow(const winrt::hstring& name);

    // Protocol server access
    const std::vector<std::shared_ptr<::AppHost>>& GetWindows() const noexcept { return _windows; }
    AppHost* GetMostRecentWindow() const noexcept { return _mostRecentWindow(); }
    AppHost* ResolveWindowTarget(const WindowTarget& target) const noexcept;
    HWND GetMainWindowForProtocol() const noexcept { return _window.get(); }

private:
    struct SummonWindowSelectionArgs
    {
        uint64_t WindowID = 0;
        std::wstring_view WindowName;
        bool OnCurrentDesktop = false;
        winrt::TerminalApp::SummonWindowBehavior SummonBehavior;
    };

    [[nodiscard]] static LRESULT __stdcall _wndProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) noexcept;

    AppHost* _mostRecentWindow() const noexcept;
    void _createWindowMaybeRestoringWorkspace(uint64_t windowId, const winrt::hstring& windowName, winrt::TerminalApp::CommandlineArgs args, const winrt::Windows::Foundation::Collections::IVector<winrt::Microsoft::Terminal::Settings::Model::ActionAndArgs>& startupActions = nullptr);
    uint64_t _createProtocolWindow(const WindowTarget& target, const winrt::Microsoft::Terminal::Settings::Model::NewTerminalArgs& args);
    bool _summonWindow(const SummonWindowSelectionArgs& args) const;
    void _summonAllWindows() const;
    void _dispatchSpecialKey(const MSG& msg) const;
    void _dispatchCommandline(winrt::TerminalApp::CommandlineArgs args);
    void _dispatchCommandlineCommon(winrt::array_view<const winrt::hstring> args, wil::zwstring_view currentDirectory, wil::zwstring_view envString, uint32_t showWindowCommand);
    safe_void_coroutine _dispatchCommandlineCurrentDesktop(winrt::TerminalApp::CommandlineArgs args);
    LRESULT _messageHandler(HWND window, UINT message, WPARAM wParam, LPARAM lParam) noexcept;
    void _createMessageWindow(const wchar_t* className);
    void _postQuitMessageIfNeeded() const;
    safe_void_coroutine _showMessageBox(winrt::hstring message, bool error);
    void _notificationAreaMenuRequested(WPARAM wParam);
    void _notificationAreaMenuClicked(WPARAM wParam, LPARAM lParam) const;
    void _hotkeyPressed(long hotkeyIndex);
    void _registerHotKey(int index, const winrt::Microsoft::Terminal::Control::KeyChord& hotkey) noexcept;
    void _unregisterHotKey(int index) noexcept;
    void _setupGlobalHotkeys();
    void _setupSessionPersistence(bool enabled);
    void _persistState(const winrt::Microsoft::Terminal::Settings::Model::ApplicationState& state) const;
    void _finalizeSessionPersistence() const;
    void _checkWindowsForNotificationIcon();
    void _setupAumid(const std::wstring& aumid);

    wil::unique_hwnd _window;
    winrt::TerminalApp::App _app{ nullptr };
    std::vector<std::shared_ptr<::AppHost>> _windows;

    // Protocol server for AI CLI integration
    void _initializeProtocolServer();
    std::vector<winrt::Microsoft::Terminal::Settings::Model::GlobalSummonArgs> _hotkeys;
    NOTIFYICONDATA _notificationIcon{};
    UINT WM_TASKBARCREATED = 0;
    HMENU _currentWindowMenu = nullptr;
    bool _notificationIconShown = false;
    bool _skipPersistence = false;
    bool _needsPersistenceCleanup = false;
    SafeDispatcherTimer _persistStateTimer;
    std::optional<bool> _currentSystemThemeIsDark;
    int32_t _windowCount = 0;
    int32_t _messageBoxCount = 0;
    std::wstring _pendingAumidLnkPath;
    std::wstring _pendingAumid;

#if 0 // #ifdef NDEBUG
    static constexpr void _assertIsMainThread() noexcept
    {
    }
#else
    void _assertIsMainThread() const noexcept
    {
        WI_ASSERT_MSG(_mainThreadId == GetCurrentThreadId(), "This part of WindowEmperor must be accessed from the UI thread");
    }
    DWORD _mainThreadId = GetCurrentThreadId();
#endif
};
