// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "WindowEmperor.h"

#include <CoreWindow.h>
#include <LibraryResources.h>
#include <ScopedResourceLoader.h>
#include <WtExeUtils.h>
#include <til/hash.h>

#include "AppHost.h"
#include "resource.h"
#include "VirtualDesktopUtils.h"
#include "../../types/inc/User32Utils.hpp"
#include "../../types/inc/utils.hpp"

using namespace winrt;
using namespace winrt::Microsoft::Terminal;
using namespace winrt::Microsoft::Terminal::Settings::Model;
using namespace winrt::Windows::Foundation;
using namespace ::Microsoft::Console;
using namespace std::chrono_literals;
using VirtualKeyModifiers = winrt::Windows::System::VirtualKeyModifiers;

#ifdef _WIN64
static constexpr ULONG_PTR TERMINAL_HANDOFF_MAGIC = 0x4c414e494d524554; // 'TERMINAL'
#else
static constexpr ULONG_PTR TERMINAL_HANDOFF_MAGIC = 0x4d524554; // 'TERM'
#endif

extern "C" IMAGE_DOS_HEADER __ImageBase;

// A convenience function around CommandLineToArgv.
static std::vector<winrt::hstring> commandlineToArgArray(const wchar_t* commandLine)
{
    int argc = 0;
    const wil::unique_hlocal_ptr<LPWSTR> argv{ CommandLineToArgvW(commandLine, &argc) };
    argc = std::max(argc, 0);

    std::vector<winrt::hstring> args;
    args.reserve(argc);
    for (int i = 0; i < argc; i++)
    {
        args.emplace_back(argv.get()[i]);
    }
    return args;
}

// Returns the length of a double-null encoded string *excluding* the trailing double-null character.
static std::wstring_view stringFromDoubleNullTerminated(const wchar_t* beg)
{
    auto end = beg;

    for (; *end; end += wcsnlen(end, SIZE_T_MAX) + 1)
    {
    }

    return { beg, end };
}

// Appends an uint32_t to a byte vector.
static void serializeUint32(std::vector<uint8_t>& out, uint32_t value)
{
    out.insert(out.end(), reinterpret_cast<const uint8_t*>(&value), reinterpret_cast<const uint8_t*>(&value) + sizeof(value));
}

// Parses an uint32_t from the input iterator. Performs bounds-checks.
// Returns an iterator that points past it.
static const uint8_t* deserializeUint32(const uint8_t* it, const uint8_t* end, uint32_t& val)
{
    if (static_cast<size_t>(end - it) < sizeof(uint32_t))
    {
        throw std::out_of_range("Not enough data for uint32_t");
    }
    val = *reinterpret_cast<const uint32_t*>(it);
    return it + sizeof(uint32_t);
}

// Writes an uint32_t length prefix, followed by the string data, to the output vector.
static void serializeString(std::vector<uint8_t>& out, std::wstring_view str)
{
    const auto ptr = reinterpret_cast<const uint8_t*>(str.data());
    const auto len = gsl::narrow<uint32_t>(str.size());
    serializeUint32(out, len);
    out.insert(out.end(), ptr, ptr + len * sizeof(wchar_t));
}

// Parses the next string from the input iterator. Performs bounds-checks.
// Returns an iterator that points past it.
static const uint8_t* deserializeString(const uint8_t* it, const uint8_t* end, std::wstring_view& str)
{
    uint32_t len;
    it = deserializeUint32(it, end, len);

    if (static_cast<size_t>(end - it) < len * sizeof(wchar_t))
    {
        throw std::out_of_range("Not enough data for string content");
    }

    str = { reinterpret_cast<const wchar_t*>(it), len };
    return it + len * sizeof(wchar_t);
}

struct Handoff
{
    std::wstring_view args;
    std::wstring_view env;
    std::wstring_view cwd;
    uint32_t show;
};

// Serializes all relevant parameters to a byte blob for a WM_COPYDATA message.
// This allows us to hand off an invocation to an existing instance of the Terminal.
static std::vector<uint8_t> serializeHandoffPayload(int nCmdShow)
{
    const auto args = GetCommandLineW();
    const wil::unique_environstrings_ptr envMem{ GetEnvironmentStringsW() };
    const auto env = stringFromDoubleNullTerminated(envMem.get());
    const auto cwd = wil::GetCurrentDirectoryW<std::wstring>();

    std::vector<uint8_t> out;
    serializeString(out, args);
    serializeString(out, env);
    serializeString(out, cwd);
    serializeUint32(out, static_cast<uint32_t>(nCmdShow));
    return out;
}

// Counterpart to serializeHandoffPayload.
static Handoff deserializeHandoffPayload(const uint8_t* beg, const uint8_t* end)
{
    Handoff result{};
    auto it = beg;
    it = deserializeString(it, end, result.args);
    it = deserializeString(it, end, result.env);
    it = deserializeString(it, end, result.cwd);
    it = deserializeUint32(it, end, result.show);
    return result;
}

// Either acquires unique ownership over the given `className` mutex,
// or attempts to pass the commandline to the existing instance.
static wil::unique_mutex acquireMutexOrAttemptHandoff(const wchar_t* className, int nCmdShow)
{
    // If the process that owns the mutex has not finished creating the window yet,
    // FindWindowW will return nullptr. We'll retry a few times just in case.
    // At the 1.5x growth rate, this will retry up to ~30s in total.
    for (DWORD sleep = 50; sleep < 10000; sleep += sleep / 2)
    {
        wil::unique_mutex mutex{ CreateMutexW(nullptr, TRUE, className) };
        if (GetLastError() == ERROR_ALREADY_EXISTS)
        {
            mutex.reset();
        }
        if (mutex)
        {
            return mutex;
        }

        // I found that FindWindow() with no other filters is substantially faster than
        // using FindWindowEx() and restricting the search to just HWND_MESSAGE windows.
        // In both cases it's quite fast though at ~1us/op vs ~3us/op (good old Win32 <3).
        if (const auto hwnd = FindWindowW(className, nullptr))
        {
            auto payload = serializeHandoffPayload(nCmdShow);
            const COPYDATASTRUCT cds{
                .dwData = TERMINAL_HANDOFF_MAGIC,
                .cbData = gsl::narrow<DWORD>(payload.size()),
                .lpData = payload.data(),
            };

            // Allow the existing instance to gain foreground rights.
            DWORD processId = 0;
            if (GetWindowThreadProcessId(hwnd, &processId) && processId)
            {
                AllowSetForegroundWindow(processId);
            }

            if (SendMessageTimeoutW(hwnd, WM_COPYDATA, 0, reinterpret_cast<LPARAM>(&cds), SMTO_ABORTIFHUNG | SMTO_ERRORONEXIT, 5000, nullptr))
            {
                return {};
            }
        }

        Sleep(sleep);
    }

    return {};
}

HWND WindowEmperor::GetMainWindow() const noexcept
{
    _assertIsMainThread();
    return _window.get();
}

AppHost* WindowEmperor::GetWindowById(uint64_t id) const noexcept
{
    _assertIsMainThread();

    for (const auto& window : _windows)
    {
        if (window->Logic().WindowProperties().WindowId() == id)
        {
            return window.get();
        }
    }
    return nullptr;
}

AppHost* WindowEmperor::GetWindowByName(std::wstring_view name) const noexcept
{
    _assertIsMainThread();

    for (const auto& window : _windows)
    {
        if (window->Logic().WindowProperties().WindowName() == name)
        {
            return window.get();
        }
    }
    return nullptr;
}

void WindowEmperor::CreateNewWindow(winrt::TerminalApp::WindowRequestedArgs args)
{
    _assertIsMainThread();

    uint64_t id = args.Id();
    bool needsNewId = id == 0;
    uint64_t newId = 0;

    for (const auto& host : _windows)
    {
        const auto existingId = host->Logic().WindowProperties().WindowId();
        newId = std::max(newId, existingId);
        needsNewId |= existingId == id;
    }

    if (needsNewId)
    {
        args.Id(newId + 1);
    }

    auto host = std::make_shared<AppHost>(this, _app.Logic(), std::move(args));
    host->Initialize();

    _windowCount += 1;
    _windows.emplace_back(std::move(host));
}

AppHost* WindowEmperor::_mostRecentWindow() const noexcept
{
    int64_t max = INT64_MIN;
    AppHost* mostRecent = nullptr;

    for (const auto& w : _windows)
    {
        const auto lastActivatedTime = w->GetLastActivatedTime();
        if (lastActivatedTime > max)
        {
            max = lastActivatedTime;
            mostRecent = w.get();
        }
    }

    return mostRecent;
}

void WindowEmperor::HandleCommandlineArgs(int nCmdShow)
{
    std::wstring windowClassName;
    windowClassName.reserve(47); // "Windows Terminal Preview Admin 0123456789012345"
#if defined(WT_BRANDING_RELEASE)
    windowClassName.append(L"Windows Terminal");
#elif defined(WT_BRANDING_PREVIEW)
    windowClassName.append(L"Windows Terminal Preview");
#elif defined(WT_BRANDING_CANARY)
    windowClassName.append(L"Windows Terminal Canary");
#else
    windowClassName.append(L"Windows Terminal Dev");
#endif
    if (Utils::IsRunningElevated())
    {
        windowClassName.append(L" Admin");
    }
    if (!IsPackaged())
    {
        const auto path = wil::GetModuleFileNameW<std::wstring>(nullptr);
        const auto hash = til::hash(path);
#ifdef _WIN64
        fmt::format_to(std::back_inserter(windowClassName), FMT_COMPILE(L" {:016x}"), hash);
#else
        fmt::format_to(std::back_inserter(windowClassName), FMT_COMPILE(L" {:08x}"), hash);
#endif
    }

    // Windows Terminal is a single-instance application. Either acquire ownership
    // over the mutex, or hand off the command line to the existing instance.
    const auto mutex = acquireMutexOrAttemptHandoff(windowClassName.c_str(), nCmdShow);
    if (!mutex)
    {
        // The command line has been handed off. We can now exit.
        // We do it with TerminateProcess() primarily to avoid WinUI shutdown issues.
        TerminateProcess(GetCurrentProcess(), gsl::narrow_cast<UINT>(0));
        __assume(false);
    }

    _app = winrt::TerminalApp::App{};
    _app.Logic().ReloadSettings();

    _createMessageWindow(windowClassName.c_str());
    _setupGlobalHotkeys();
    _checkWindowsForNotificationIcon();

    // When the settings change, we'll want to update our global hotkeys
    // and our notification icon based on the new settings.
    _app.Logic().SettingsChanged([this](auto&&, const TerminalApp::SettingsLoadEventArgs& args) {
        if (SUCCEEDED(args.Result()))
        {
            _assertIsMainThread();
            _setupGlobalHotkeys();
            _checkWindowsForNotificationIcon();
        }
    });

    {
        const wil::unique_environstrings_ptr envMem{ GetEnvironmentStringsW() };
        const auto env = stringFromDoubleNullTerminated(envMem.get());
        const auto cwd = wil::GetCurrentDirectoryW<std::wstring>();
        const auto showCmd = gsl::narrow_cast<uint32_t>(nCmdShow);

        // Restore persisted windows.
        const auto state = ApplicationState::SharedInstance();
        const auto layouts = state.PersistedWindowLayouts();
        if (layouts && layouts.Size() > 0)
        {
            _needsPersistenceCleanup = true;

            uint32_t startIdx = 0;
            for (const auto layout : layouts)
            {
                hstring args[] = { L"wt", L"-w", L"new", L"-s", winrt::to_hstring(startIdx) };
                _dispatchCommandline({ args, cwd, showCmd, env });
                startIdx += 1;
            }
        }

        // Create another window if needed: There aren't any yet, or we got an explicit command line.
        const auto args = commandlineToArgArray(GetCommandLineW());
        if (_windows.empty() || args.size() != 1)
        {
            _dispatchCommandline({ args, cwd, showCmd, env });
        }

        // If we created no windows, e.g. because the args are "/?" we can just exit now.
        _postQuitMessageIfNeeded();
    }

    // ALWAYS change the _real_ CWD of the Terminal to system32,
    // so that we don't lock the directory we were spawned in.
    if (std::wstring system32; SUCCEEDED_LOG(wil::GetSystemDirectoryW(system32)))
    {
        LOG_IF_WIN32_BOOL_FALSE(SetCurrentDirectoryW(system32.c_str()));
    }

    // The first CoreWindow is created implicitly by XAML and parented to the
    // first XAML island. We parent it to our initial window for 2 reasons:
    // * On Windows 10 the CoreWindow will show up as a visible window on the taskbar
    //   due to a WinUI bug, and this will hide it, because our initial window is hidden.
    // * When we DestroyWindow() the island it will destroy the CoreWindow,
    //   and it's not possible to recreate it. That's also a WinUI bug.
    if (const auto coreWindow = winrt::Windows::UI::Core::CoreWindow::GetForCurrentThread())
    {
        if (const auto interop = coreWindow.try_as<ICoreWindowInterop>())
        {
            HWND coreHandle = nullptr;
            if (SUCCEEDED(interop->get_WindowHandle(&coreHandle)) && coreHandle)
            {
                SetParent(coreHandle, _window.get());
            }
        }
    }

    // Main message loop. It pumps all windows.
    bool loggedInteraction = false;
    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0))
    {
        // This is `if (WM_KEYDOWN || WM_KEYUP || WM_SYSKEYDOWN || WM_SYSKEYUP)`.
        // It really doesn't need to be written that obtuse, but it at
        // least nicely mirrors our `keyDown = msg.message & 1` logic.
        // FYI: For the key-down/up messages the lowest bit indicates if it's up.
        if ((msg.message & ~1) == WM_KEYDOWN || (msg.message & ~1) == WM_SYSKEYDOWN)
        {
            if (!loggedInteraction)
            {
                TraceLoggingWrite(
                    g_hWindowsTerminalProvider,
                    "SessionBecameInteractive",
                    TraceLoggingDescription("Event emitted when the session was interacted with"),
                    TraceLoggingKeyword(MICROSOFT_KEYWORD_MEASURES),
                    TelemetryPrivacyDataTag(PDT_ProductAndServiceUsage));
                loggedInteraction = true;
            }

            const bool keyDown = (msg.message & 1) == 0;
            if (
                // GH#638: The Xaml input stack doesn't allow an application to suppress the "caret browsing"
                // dialog experience triggered when you press F7. Official recommendation from the Xaml
                // team is to catch F7 before we hand it off.
                (msg.wParam == VK_F7 && keyDown) ||
                // GH#6421: System XAML will never send an Alt KeyUp event. Same thing here.
                (msg.wParam == VK_MENU && !keyDown) ||
                // GH#7125: System XAML will show a system dialog on Alt Space.
                // We want to handle it ourselves and there's no way to suppress it.
                // It almost seems like there's a pattern here...
                (msg.wParam == VK_SPACE && msg.message == WM_SYSKEYDOWN))
            {
                _dispatchSpecialKey(msg);
                continue;
            }

            if (msg.message == WM_KEYDOWN)
            {
                IslandWindow::HideCursor();
            }
        }

        if (IslandWindow::IsCursorHidden())
        {
            IslandWindow::ShowCursorMaybe(msg.message);
        }

        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    _finalizeSessionPersistence();

    if (_notificationIconShown)
    {
        Shell_NotifyIconW(NIM_DELETE, &_notificationIcon);
    }

    // There's a mysterious crash in XAML on Windows 10 if you just let _app get destroyed (GH#15410).
    // We also need to ensure that all UI threads exit before WindowEmperor leaves the scope on the main thread (MSFT:46744208).
    // Both problems can be solved and the shutdown accelerated by using TerminateProcess.
    // std::exit(), etc., cannot be used here, because those use ExitProcess for unpackaged applications.
    TerminateProcess(GetCurrentProcess(), gsl::narrow_cast<UINT>(0));
    __assume(false);
}

void WindowEmperor::_dispatchSpecialKey(const MSG& msg) const
{
    // Each CoreInput window is a child of our IslandWindow.
    // We can figure out the IslandWindow HWND via GetAncestor().
    const auto hwnd = GetAncestor(msg.hwnd, GA_ROOT);
    AppHost* window = nullptr;

    for (const auto& h : _windows)
    {
        const auto w = h->GetWindow();
        if (w && w->GetHandle() == hwnd)
        {
            window = h.get();
            break;
        }
    }

    // Fallback.
    if (!window)
    {
        window = _mostRecentWindow();
        if (!window)
        {
            return;
        }
    }

    const auto vkey = gsl::narrow_cast<uint32_t>(msg.wParam);
    const auto scanCode = gsl::narrow_cast<uint8_t>(msg.lParam >> 16);
    const bool keyDown = (msg.message & 1) == 0;
    window->OnDirectKeyEvent(vkey, scanCode, keyDown);
}

void WindowEmperor::_dispatchCommandline(winrt::TerminalApp::CommandlineArgs args)
{
    const auto exitCode = args.ExitCode();

    if (const auto msg = args.ExitMessage(); !msg.empty())
    {
        _showMessageBox(msg, exitCode != 0);
        return;
    }

    if (exitCode != 0)
    {
        return;
    }

    const auto parsedTarget = args.TargetWindow();
    WindowingMode windowingBehavior = WindowingMode::UseNew;
    uint64_t windowId = 0;
    winrt::hstring windowName;

    // Figure out the windowing behavior the caller wants
    // and get the sanitized window ID (if any) and window name (if any).
    if (parsedTarget.empty())
    {
        windowingBehavior = _app.Logic().Settings().GlobalSettings().WindowingBehavior();
    }
    else if (const auto opt = til::parse_signed<int64_t>(parsedTarget, 10))
    {
        // Negative window IDs map to WindowingMode::UseNew.
        if (*opt > 0)
        {
            windowId = gsl::narrow_cast<uint64_t>(*opt);
        }
        else if (*opt == 0)
        {
            windowingBehavior = WindowingMode::UseExisting;
        }
    }
    else if (parsedTarget == L"last")
    {
        windowingBehavior = WindowingMode::UseExisting;
    }
    // A window name of "new" maps to WindowingMode::UseNew.
    else if (parsedTarget != L"new")
    {
        windowName = parsedTarget;
    }

    AppHost* window = nullptr;

    // Map from the windowing behavior, ID, and name to a window.
    if (windowId)
    {
        window = GetWindowById(windowId);
    }
    else if (!windowName.empty())
    {
        window = GetWindowByName(windowName);
    }
    else
    {
        switch (windowingBehavior)
        {
        case WindowingMode::UseAnyExisting:
            window = _mostRecentWindow();
            break;
        case WindowingMode::UseExisting:
            _dispatchCommandlineCurrentDesktop(std::move(args));
            return;
        default:
            break;
        }
    }

    if (window)
    {
        window->DispatchCommandline(std::move(args));
    }
    else
    {
        winrt::TerminalApp::WindowRequestedArgs request{ windowId, std::move(args) };
        request.WindowName(std::move(windowName));
        CreateNewWindow(std::move(request));
    }
}

// This is an implementation-detail of _dispatchCommandline().
safe_void_coroutine WindowEmperor::_dispatchCommandlineCurrentDesktop(winrt::TerminalApp::CommandlineArgs args)
{
    std::weak_ptr<AppHost> mostRecentWeak;

    if (winrt::guid currentDesktop; VirtualDesktopUtils::GetCurrentVirtualDesktopId(reinterpret_cast<GUID*>(&currentDesktop)))
    {
        int64_t max = INT64_MIN;
        for (const auto& w : _windows)
        {
            const auto lastActivatedTime = w->GetLastActivatedTime();
            const auto desktopId = co_await w->GetVirtualDesktopId();
            if (desktopId == currentDesktop && lastActivatedTime > max)
            {
                max = lastActivatedTime;
                mostRecentWeak = w;
            }
        }
    }

    // GetVirtualDesktopId(), as current implemented, should always return on the main thread.
    _assertIsMainThread();

    const auto mostRecent = mostRecentWeak.lock();
    auto window = mostRecent.get();

    if (!window)
    {
        window = _mostRecentWindow();
    }

    if (window)
    {
        window->DispatchCommandline(std::move(args));
    }
    else
    {
        CreateNewWindow(winrt::TerminalApp::WindowRequestedArgs{ 0, std::move(args) });
    }
}

bool WindowEmperor::_summonWindow(const SummonWindowSelectionArgs& args) const
{
    AppHost* window = nullptr;

    if (args.WindowID)
    {
        for (const auto& w : _windows)
        {
            if (w->Logic().WindowProperties().WindowId() == args.WindowID)
            {
                window = w.get();
                break;
            }
        }
    }
    else if (!args.WindowName.empty())
    {
        for (const auto& w : _windows)
        {
            if (w->Logic().WindowProperties().WindowName() == args.WindowName)
            {
                window = w.get();
                break;
            }
        }
    }
    else
    {
        window = _mostRecentWindow();
    }

    if (!window)
    {
        return false;
    }

    window->HandleSummon(args.SummonBehavior);
    return true;
}

void WindowEmperor::_summonAllWindows() const
{
    TerminalApp::SummonWindowBehavior args;
    args.ToggleVisibility(false);

    for (const auto& window : _windows)
    {
        window->HandleSummon(args);
    }
}

#pragma region WindowProc

static WindowEmperor* GetThisFromHandle(HWND const window) noexcept
{
    const auto data = GetWindowLongPtrW(window, GWLP_USERDATA);
    return reinterpret_cast<WindowEmperor*>(data);
}

[[nodiscard]] LRESULT __stdcall WindowEmperor::_wndProc(HWND const window, UINT const message, WPARAM const wparam, LPARAM const lparam) noexcept
{
    if (const auto that = GetThisFromHandle(window))
    {
        return that->_messageHandler(window, message, wparam, lparam);
    }

    if (WM_NCCREATE == message)
    {
        const auto cs = reinterpret_cast<CREATESTRUCT*>(lparam);
        const auto that = static_cast<WindowEmperor*>(cs->lpCreateParams);
        that->_window.reset(window);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(that));
    }

    return DefWindowProcW(window, message, wparam, lparam);
}

void WindowEmperor::_createMessageWindow(const wchar_t* className)
{
    const auto instance = reinterpret_cast<HINSTANCE>(&__ImageBase);
    const auto icon = LoadIconW(instance, MAKEINTRESOURCEW(IDI_APPICON));

    const WNDCLASS wc{
        .lpfnWndProc = &_wndProc,
        .hInstance = instance,
        .hIcon = icon,
        .lpszClassName = className,
    };
    RegisterClassW(&wc);

    WM_TASKBARCREATED = RegisterWindowMessageW(L"TaskbarCreated");

    // NOTE: This cannot be a HWND_MESSAGE window as otherwise we don't
    // receive any HWND_BROADCAST messages, like WM_QUERYENDSESSION.
    // NOTE: Before CreateWindowExW() returns it invokes our WM_NCCREATE
    // message handler, which then stores the HWND in this->_window.
    WINRT_VERIFY(CreateWindowExW(
        /* dwExStyle    */ 0,
        /* lpClassName  */ className,
        /* lpWindowName */ L"Windows Terminal",
        /* dwStyle      */ 0,
        /* X            */ 0,
        /* Y            */ 0,
        /* nWidth       */ 0,
        /* nHeight      */ 0,
        /* hWndParent   */ nullptr,
        /* hMenu        */ nullptr,
        /* hInstance    */ instance,
        /* lpParam      */ this));

    _notificationIcon.cbSize = sizeof(NOTIFYICONDATA);
    _notificationIcon.hWnd = _window.get();
    _notificationIcon.uID = 1;
    _notificationIcon.uFlags = NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP | NIF_ICON;
    _notificationIcon.uCallbackMessage = WM_NOTIFY_FROM_NOTIFICATION_AREA;
    _notificationIcon.hIcon = icon;
    _notificationIcon.uVersion = NOTIFYICON_VERSION_4;

    // AppName happens to be in the ContextMenu's Resources, see GH#12264
    const ScopedResourceLoader loader{ L"TerminalApp/ContextMenu" };
    const auto appNameLoc = loader.GetLocalizedString(L"AppName");
    StringCchCopy(_notificationIcon.szTip, ARRAYSIZE(_notificationIcon.szTip), appNameLoc.c_str());
}

// Posts a WM_QUIT as soon as we have no reason to exist anymore.
// That basically means no windows and no message boxes.
void WindowEmperor::_postQuitMessageIfNeeded() const
{
    if (
        _messageBoxCount <= 0 &&
        _windowCount <= 0 &&
        !_app.Logic().Settings().GlobalSettings().AllowHeadless())
    {
        PostQuitMessage(0);
    }
}

safe_void_coroutine WindowEmperor::_showMessageBox(winrt::hstring message, bool error)
{
    // Prevent the main loop from exiting until the message box is closed.
    // Once the loop exits, the app exits, and the message box will be closed.
    _messageBoxCount += 1;
    const auto decrement = wil::scope_exit([hwnd = _window.get()]() noexcept {
        PostMessageW(hwnd, WM_MESSAGE_BOX_CLOSED, 0, 0);
    });

    // We must yield to a background thread, because MessageBoxW() is a blocking call, and we can't
    // block the main thread. That would prevent us from servicing WM_COPYDATA messages and similar.
    co_await winrt::resume_background();

    const auto messageTitle = error ? IDS_ERROR_DIALOG_TITLE : IDS_HELP_DIALOG_TITLE;
    const auto messageIcon = error ? MB_ICONERROR : MB_ICONWARNING;
    // The dialog cannot have our _window as the parent, because that one is always hidden/invisible.
    // TODO:GH#4134: polish this dialog more, to make the text more like msiexec /?
    MessageBoxW(nullptr, message.c_str(), GetStringResource(messageTitle).c_str(), MB_OK | messageIcon);
}

LRESULT WindowEmperor::_messageHandler(HWND window, UINT const message, WPARAM const wParam, LPARAM const lParam) noexcept
{
    try
    {
        switch (message)
        {
        case WM_CLOSE_TERMINAL_WINDOW:
        {
            const auto globalSettings = _app.Logic().Settings().GlobalSettings();
            // Keep the last window in the array so that we can persist it on exit.
            // We check for AllowHeadless(), as that being true prevents us from ever quitting in the first place.
            // (= If we avoided closing the last window you wouldn't be able to reach a headless state.)
            const auto shouldKeepWindow =
                _windows.size() == 1 &&
                globalSettings.ShouldUsePersistedLayout() &&
                !globalSettings.AllowHeadless();

            if (!shouldKeepWindow)
            {
                // Did the window counter get out of sync? It shouldn't.
                assert(_windowCount == gsl::narrow_cast<int32_t>(_windows.size()));

                const auto host = reinterpret_cast<AppHost*>(lParam);
                auto it = _windows.begin();
                const auto end = _windows.end();

                for (; it != end; ++it)
                {
                    if (host == it->get())
                    {
                        host->Close();
                        _windows.erase(it);
                        break;
                    }
                }
            }

            // Counterpart specific to CreateNewWindow().
            _windowCount -= 1;
            _postQuitMessageIfNeeded();
            return 0;
        }
        case WM_MESSAGE_BOX_CLOSED:
            // Counterpart specific to _showMessageBox().
            _messageBoxCount -= 1;
            _postQuitMessageIfNeeded();
            return 0;
        case WM_IDENTIFY_ALL_WINDOWS:
            for (const auto& host : _windows)
            {
                host->Logic().IdentifyWindow();
            }
            return 0;
        case WM_NOTIFY_FROM_NOTIFICATION_AREA:
            switch (LOWORD(lParam))
            {
            case NIN_SELECT:
            case NIN_KEYSELECT:
            {
                SummonWindowSelectionArgs args;
                args.SummonBehavior.MoveToCurrentDesktop(false);
                args.SummonBehavior.ToMonitor(winrt::TerminalApp::MonitorBehavior::InPlace);
                args.SummonBehavior.ToggleVisibility(false);
                std::ignore = _summonWindow(std::move(args));
                break;
            }
            case WM_CONTEXTMENU:
                _notificationAreaMenuRequested(wParam);
                break;
            default:
                break;
            }
            return 0;
        case WM_WINDOWPOSCHANGING:
        {
            // No, we really don't want this window to be visible. It hides the buggy CoreWindow that XAML creates.
            // We can't make it a HWND_MESSAGE window, because then we don't get WM_QUERYENDSESSION messages.
            const auto wp = reinterpret_cast<WINDOWPOS*>(lParam);
            wp->flags &= ~SWP_SHOWWINDOW;
            return 0;
        }
        case WM_MENUCOMMAND:
            _notificationAreaMenuClicked(wParam, lParam);
            return 0;
        case WM_SETTINGCHANGE:
            // Currently, we only support checking when the OS theme changes. In that case, wParam is 0.
            // GH#15102: Re-evaluate when we decide to reload env vars.
            //
            // ImmersiveColorSet seems to be the notification that the OS theme
            // changed. If that happens, let the app know, so it can hot-reload
            // themes, color schemes that might depend on the OS theme
            if (wParam == 0 && lParam != 0 && wcscmp(reinterpret_cast<const wchar_t*>(lParam), L"ImmersiveColorSet") == 0)
            {
                // GH#15732: Don't update the settings, unless the theme
                // _actually_ changed. ImmersiveColorSet gets sent more often
                // than just on a theme change. It notably gets sent when the PC
                // is locked, or the UAC prompt opens.
                const auto isCurrentlyDark = Theme::IsSystemInDarkTheme();
                if (isCurrentlyDark != _currentSystemThemeIsDark)
                {
                    _currentSystemThemeIsDark = isCurrentlyDark;
                    _app.Logic().ReloadSettings();
                }
            }
            return 0;
        case WM_COPYDATA:
            if (const auto cds = reinterpret_cast<COPYDATASTRUCT*>(lParam); cds->dwData == TERMINAL_HANDOFF_MAGIC)
            {
                const auto handoff = deserializeHandoffPayload(static_cast<const uint8_t*>(cds->lpData), static_cast<const uint8_t*>(cds->lpData) + cds->cbData);
                const winrt::hstring args{ handoff.args };
                const winrt::hstring env{ handoff.env };
                const winrt::hstring cwd{ handoff.cwd };
                const auto argv = commandlineToArgArray(args.c_str());
                _dispatchCommandline({ argv, cwd, gsl::narrow_cast<uint32_t>(handoff.show), env });
            }
            return 0;
        case WM_HOTKEY:
            _hotkeyPressed(static_cast<long>(wParam));
            return 0;
        case WM_QUERYENDSESSION:
            // For WM_QUERYENDSESSION and WM_ENDSESSION, refer to:
            // https://docs.microsoft.com/en-us/windows/win32/rstmgr/guidelines-for-applications
            // ENDSESSION_CLOSEAPP: The application is using a file that must be replaced,
            // the system is being serviced, or system resources are exhausted.
            RegisterApplicationRestart(nullptr, RESTART_NO_CRASH | RESTART_NO_HANG);
            return TRUE;
        case WM_ENDSESSION:
            _forcePersistence = true;
            PostQuitMessage(0);
            return 0;
        default:
            // We'll want to receive this message when explorer.exe restarts
            // so that we can re-add our icon to the notification area.
            // This unfortunately isn't a switch case because we register the
            // message at runtime.
            if (message == WM_TASKBARCREATED)
            {
                _notificationIconShown = false;
                _checkWindowsForNotificationIcon();
                return 0;
            }
        }
    }
    catch (...)
    {
        LOG_CAUGHT_EXCEPTION();
    }

    return DefWindowProcW(window, message, wParam, lParam);
}

void WindowEmperor::_finalizeSessionPersistence() const
{
    const auto state = ApplicationState::SharedInstance();

    if (_forcePersistence || _app.Logic().Settings().GlobalSettings().ShouldUsePersistedLayout())
    {
        state.PersistedWindowLayouts(nullptr);
        for (const auto& w : _windows)
        {
            w->Logic().PersistState();
        }
    }

    // Ensure to write the state.json before we TerminateProcess()
    state.Flush();

    if (_needsPersistenceCleanup)
    {
        // Get the "buffer_{guid}.txt" files that we expect to be there
        std::unordered_set<winrt::guid> sessionIds;
        if (const auto layouts = state.PersistedWindowLayouts())
        {
            for (const auto& windowLayout : layouts)
            {
                for (const auto& actionAndArgs : windowLayout.TabLayout())
                {
                    const auto args = actionAndArgs.Args();
                    NewTerminalArgs terminalArgs{ nullptr };

                    if (const auto tabArgs = args.try_as<NewTabArgs>())
                    {
                        terminalArgs = tabArgs.ContentArgs().try_as<NewTerminalArgs>();
                    }
                    else if (const auto paneArgs = args.try_as<SplitPaneArgs>())
                    {
                        terminalArgs = paneArgs.ContentArgs().try_as<NewTerminalArgs>();
                    }

                    if (terminalArgs)
                    {
                        sessionIds.emplace(terminalArgs.SessionId());
                    }
                }
            }
        }

        // Remove the "buffer_{guid}.txt" files that shouldn't be there
        // e.g. "buffer_FD40D746-163E-444C-B9B2-6A3EA2B26722.txt"
        {
            const std::filesystem::path settingsDirectory{ std::wstring_view{ CascadiaSettings::SettingsDirectory() } };
            const auto filter = settingsDirectory / L"buffer_*";
            WIN32_FIND_DATAW ffd;

            // This could also use std::filesystem::directory_iterator.
            // I was just slightly bothered by how it doesn't have a O(1) .filename()
            // function, even though the underlying Win32 APIs provide it for free.
            // Both work fine.
            const wil::unique_hfind handle{ FindFirstFileExW(filter.c_str(), FindExInfoBasic, &ffd, FindExSearchNameMatch, nullptr, FIND_FIRST_EX_LARGE_FETCH) };
            if (!handle)
            {
                return;
            }

            do
            {
                const auto nameLen = wcsnlen_s(&ffd.cFileName[0], ARRAYSIZE(ffd.cFileName));
                const std::wstring_view name{ &ffd.cFileName[0], nameLen };

                if (nameLen != 47)
                {
                    continue;
                }

                wchar_t guidStr[39];
                guidStr[0] = L'{';
                memcpy(&guidStr[1], name.data() + 7, 36 * sizeof(wchar_t));
                guidStr[37] = L'}';
                guidStr[38] = L'\0';

                const auto id = Utils::GuidFromString(&guidStr[0]);
                if (!sessionIds.contains(id))
                {
                    std::filesystem::remove(settingsDirectory / name);
                }
            } while (FindNextFileW(handle.get(), &ffd));
        }
    }
}

void WindowEmperor::_notificationAreaMenuRequested(const WPARAM wParam)
{
    const auto menu = CreatePopupMenu();
    if (!menu)
    {
        assert(false);
        return;
    }

    static constexpr MENUINFO mi{
        .cbSize = sizeof(MENUINFO),
        .fMask = MIM_STYLE | MIM_APPLYTOSUBMENUS | MIM_MENUDATA,
        .dwStyle = MNS_NOTIFYBYPOS,
    };
    SetMenuInfo(menu, &mi);

    // The "Focus Terminal" menu item.
    AppendMenuW(menu, MF_STRING, 0, RS_(L"NotificationIconFocusTerminal").c_str());
    AppendMenuW(menu, MF_SEPARATOR, 0, L"");

    // A submenu to focus a specific window. Lists all windows that we manage.
    if (const auto submenu = CreatePopupMenu())
    {
        static constexpr MENUINFO submenuInfo{
            .cbSize = sizeof(MENUINFO),
            .fMask = MIM_MENUDATA,
            .dwStyle = MNS_NOTIFYBYPOS,
        };
        SetMenuInfo(submenu, &submenuInfo);

        std::wstring displayText;
        displayText.reserve(64);

        for (const auto& host : _windows)
        {
            const auto logic = host->Logic();
            const auto props = logic.WindowProperties();
            const auto id = props.WindowId();

            displayText.clear();
            fmt::format_to(std::back_inserter(displayText), L"#{}", id);
            if (const auto title = logic.Title(); !title.empty())
            {
                fmt::format_to(std::back_inserter(displayText), L": {}", title);
            }
            if (const auto name = props.WindowName(); !name.empty())
            {
                fmt::format_to(std::back_inserter(displayText), L" [{}]", name);
            }

            AppendMenuW(submenu, MF_STRING, gsl::narrow_cast<UINT_PTR>(id), displayText.c_str());
        }

        AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(submenu), RS_(L"NotificationIconWindowSubmenu").c_str());
    }

    // We'll need to set our window to the foreground before calling
    // TrackPopupMenuEx or else the menu won't dismiss when clicking away.
    SetForegroundWindow(_window.get());

    // User can select menu items with the left and right buttons.
    const auto rightAlign = GetSystemMetrics(SM_MENUDROPALIGNMENT) != 0;
    const UINT uFlags = TPM_RIGHTBUTTON | (rightAlign ? TPM_RIGHTALIGN : TPM_LEFTALIGN);
    TrackPopupMenuEx(menu, uFlags, GET_X_LPARAM(wParam), GET_Y_LPARAM(wParam), _window.get(), nullptr);

    if (_currentWindowMenu)
    {
        // This will recursively destroy the submenu(s) as well.
        DestroyMenu(_currentWindowMenu);
    }
    _currentWindowMenu = menu;
}

void WindowEmperor::_notificationAreaMenuClicked(const WPARAM wParam, const LPARAM lParam) const
{
    const auto menu = reinterpret_cast<HMENU>(lParam);
    const auto menuItemIndex = LOWORD(wParam);
    const auto windowId = GetMenuItemID(menu, menuItemIndex);

    // _notificationAreaMenuRequested constructs each menu item with an ID
    // that is either 0 for "Focus Terminal" or >0 for a specific window ID.
    // This works well for us because valid window IDs are always >0.
    SummonWindowSelectionArgs args;
    args.WindowID = windowId;
    args.SummonBehavior.ToggleVisibility(false);
    args.SummonBehavior.MoveToCurrentDesktop(false);
    args.SummonBehavior.ToMonitor(winrt::TerminalApp::MonitorBehavior::InPlace);
    std::ignore = _summonWindow(std::move(args));
}

#pragma endregion
#pragma region GlobalHotkeys

void WindowEmperor::_hotkeyPressed(const long hotkeyIndex)
{
    if (hotkeyIndex < 0 || static_cast<size_t>(hotkeyIndex) > _hotkeys.size())
    {
        return;
    }

    const auto& summonArgs = til::at(_hotkeys, hotkeyIndex);

    // desktop:any - MoveToCurrentDesktop=false, OnCurrentDesktop=false
    // desktop:toCurrent - MoveToCurrentDesktop=true, OnCurrentDesktop=false
    // desktop:onCurrent - MoveToCurrentDesktop=false, OnCurrentDesktop=true
    SummonWindowSelectionArgs args;
    args.WindowName = summonArgs.Name();
    args.OnCurrentDesktop = summonArgs.Desktop() == DesktopBehavior::OnCurrent;
    args.SummonBehavior.MoveToCurrentDesktop(summonArgs.Desktop() == DesktopBehavior::ToCurrent);
    args.SummonBehavior.ToggleVisibility(summonArgs.ToggleVisibility());
    args.SummonBehavior.DropdownDuration(summonArgs.DropdownDuration());

    switch (summonArgs.Monitor())
    {
    case MonitorBehavior::Any:
        args.SummonBehavior.ToMonitor(TerminalApp::MonitorBehavior::InPlace);
        break;
    case MonitorBehavior::ToCurrent:
        args.SummonBehavior.ToMonitor(TerminalApp::MonitorBehavior::ToCurrent);
        break;
    case MonitorBehavior::ToMouse:
        args.SummonBehavior.ToMonitor(TerminalApp::MonitorBehavior::ToMouse);
        break;
    }

    if (_summonWindow(args))
    {
        return;
    }

    hstring argv[] = { L"wt", L"-w", summonArgs.Name() };
    if (argv[2].empty())
    {
        argv[2] = L"new";
    }

    const wil::unique_environstrings_ptr envMem{ GetEnvironmentStringsW() };
    const auto env = stringFromDoubleNullTerminated(envMem.get());
    const auto cwd = wil::GetCurrentDirectoryW<std::wstring>();
    _dispatchCommandline({ argv, cwd, SW_SHOWDEFAULT, std::move(env) });
}

void WindowEmperor::_registerHotKey(const int index, const winrt::Microsoft::Terminal::Control::KeyChord& hotkey) noexcept
{
    const auto vkey = hotkey.Vkey();
    auto hotkeyFlags = MOD_NOREPEAT;
    {
        const auto modifiers = hotkey.Modifiers();
        WI_SetFlagIf(hotkeyFlags, MOD_WIN, WI_IsFlagSet(modifiers, VirtualKeyModifiers::Windows));
        WI_SetFlagIf(hotkeyFlags, MOD_ALT, WI_IsFlagSet(modifiers, VirtualKeyModifiers::Menu));
        WI_SetFlagIf(hotkeyFlags, MOD_CONTROL, WI_IsFlagSet(modifiers, VirtualKeyModifiers::Control));
        WI_SetFlagIf(hotkeyFlags, MOD_SHIFT, WI_IsFlagSet(modifiers, VirtualKeyModifiers::Shift));
    }

    // TODO GH#8888: We should display a warning of some kind if this fails.
    // This can fail if something else already bound this hotkey.
    LOG_IF_WIN32_BOOL_FALSE(::RegisterHotKey(_window.get(), index, hotkeyFlags, vkey));
}

// Method Description:
// - Call UnregisterHotKey once for each previously registered hotkey.
// Return Value:
// - <none>
void WindowEmperor::_unregisterHotKey(const int index) noexcept
{
    LOG_IF_WIN32_BOOL_FALSE(::UnregisterHotKey(_window.get(), index));
}

void WindowEmperor::_setupGlobalHotkeys()
{
    // Unregister all previously registered hotkeys.
    //
    // RegisterHotKey(), will not unregister hotkeys automatically.
    // If a hotkey with a given HWND and ID combination already exists
    // then a duplicate one will be added, which we don't want.
    // (Additionally we want to remove hotkeys that were removed from the settings.)
    for (auto i = 0, count = gsl::narrow_cast<int>(_hotkeys.size()); i < count; ++i)
    {
        _unregisterHotKey(i);
    }

    _hotkeys.clear();

    // Re-register all current hotkeys.
    for (const auto& [keyChord, cmd] : _app.Logic().GlobalHotkeys())
    {
        if (auto summonArgs = cmd.ActionAndArgs().Args().try_as<Settings::Model::GlobalSummonArgs>())
        {
            const auto index = gsl::narrow_cast<int>(_hotkeys.size());
            _registerHotKey(index, keyChord);
            _hotkeys.emplace_back(summonArgs);
        }
    }
}

#pragma endregion

#pragma region NotificationIcon

void WindowEmperor::_checkWindowsForNotificationIcon()
{
    // We need to check some conditions to show the notification icon.
    //
    // * If there's a Quake window somewhere, we'll want to keep the
    //   notification icon.
    // * There's two settings - MinimizeToNotificationArea and
    //   AlwaysShowNotificationIcon. If either one of them are true, we want to
    //   make sure there's a notification icon.
    //
    // If both are false, we want to remove our icon from the notification area.
    // When we remove our icon from the notification area, we'll also want to
    // re-summon any hidden windows, but right now we're not keeping track of
    // who's hidden, so just summon them all. Tracking the work to do a "summon
    // all minimized" in GH#10448
    //
    // To avoid races between us thinking the settings updated, and the windows
    // themselves getting the new settings, only ask the app logic for the
    // RequestsTrayIcon setting value, and combine that with the result of each
    // window (which won't change during a settings reload).
    const auto globals = _app.Logic().Settings().GlobalSettings();
    auto needsIcon = globals.AlwaysShowNotificationIcon() || globals.MinimizeToNotificationArea();
    if (!needsIcon)
    {
        for (const auto& host : _windows)
        {
            needsIcon |= host->Logic().IsQuakeWindow();
        }
    }

    if (_notificationIconShown == needsIcon)
    {
        return;
    }

    if (needsIcon)
    {
        Shell_NotifyIconW(NIM_ADD, &_notificationIcon);
        Shell_NotifyIconW(NIM_SETVERSION, &_notificationIcon);
    }
    else
    {
        Shell_NotifyIconW(NIM_DELETE, &_notificationIcon);
        // If we no longer want the tray icon, but we did have one, then quick
        // re-summon all our windows, so they don't get lost when the icon
        // disappears forever.
        _summonAllWindows();
    }

    _notificationIconShown = needsIcon;
}

#pragma endregion
