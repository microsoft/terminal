// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "InteractivityFactory.hpp"

#include "../inc/ServiceLocator.hpp"

#ifdef BUILD_ONECORE_INTERACTIVITY
#include "..\onecore\AccessibilityNotifier.hpp"
#include "..\onecore\ConsoleControl.hpp"
#include "..\onecore\ConsoleInputThread.hpp"
#include "..\onecore\ConsoleWindow.hpp"
#include "..\onecore\ConIoSrvComm.hpp"
#include "..\onecore\SystemConfigurationProvider.hpp"
#include "..\onecore\WindowMetrics.hpp"
#endif

#include "../win32/AccessibilityNotifier.hpp"
#include "../win32/ConsoleControl.hpp"
#include "../win32/ConsoleInputThread.hpp"
#include "../win32/WindowDpiApi.hpp"
#include "../win32/WindowMetrics.hpp"
#include "../win32/SystemConfigurationProvider.hpp"

#pragma hdrstop

using namespace std;
using namespace Microsoft::Console::Interactivity;

#pragma region Public Methods

[[nodiscard]] NTSTATUS InteractivityFactory::CreateConsoleControl(_Inout_ std::unique_ptr<IConsoleControl>& control)
{
    auto status = STATUS_SUCCESS;

    ApiLevel level;
    status = ApiDetector::DetectNtUserWindow(&level);

    if (SUCCEEDED_NTSTATUS(status))
    {
        std::unique_ptr<IConsoleControl> newControl;
        try
        {
            switch (level)
            {
            case ApiLevel::Win32:
                newControl = std::make_unique<Microsoft::Console::Interactivity::Win32::ConsoleControl>();
                break;

#ifdef BUILD_ONECORE_INTERACTIVITY
            case ApiLevel::OneCore:
                newControl = std::make_unique<Microsoft::Console::Interactivity::OneCore::ConsoleControl>();
                break;
#endif
            default:
                status = STATUS_INVALID_LEVEL;
                break;
            }
        }
        catch (...)
        {
            status = NTSTATUS_FROM_HRESULT(wil::ResultFromCaughtException());
        }

        if (SUCCEEDED_NTSTATUS(status))
        {
            control.swap(newControl);
        }
    }

    return status;
}

[[nodiscard]] NTSTATUS InteractivityFactory::CreateConsoleInputThread(_Inout_ std::unique_ptr<IConsoleInputThread>& thread)
{
    auto status = STATUS_SUCCESS;

    ApiLevel level;
    status = ApiDetector::DetectNtUserWindow(&level);

    if (SUCCEEDED_NTSTATUS(status))
    {
        std::unique_ptr<IConsoleInputThread> newThread;
        try
        {
            switch (level)
            {
            case ApiLevel::Win32:
                newThread = std::make_unique<Microsoft::Console::Interactivity::Win32::ConsoleInputThread>();
                break;

#ifdef BUILD_ONECORE_INTERACTIVITY
            case ApiLevel::OneCore:
                newThread = std::make_unique<Microsoft::Console::Interactivity::OneCore::ConsoleInputThread>();
                break;
#endif
            default:
                status = STATUS_INVALID_LEVEL;
                break;
            }
        }
        catch (...)
        {
            status = NTSTATUS_FROM_HRESULT(wil::ResultFromCaughtException());
        }

        if (SUCCEEDED_NTSTATUS(status))
        {
            thread.swap(newThread);
        }
    }

    return status;
}

[[nodiscard]] NTSTATUS InteractivityFactory::CreateHighDpiApi(_Inout_ std::unique_ptr<IHighDpiApi>& api)
{
    auto status = STATUS_SUCCESS;

    ApiLevel level;
    status = ApiDetector::DetectNtUserWindow(&level);

    if (SUCCEEDED_NTSTATUS(status))
    {
        std::unique_ptr<IHighDpiApi> newApi;
        try
        {
            switch (level)
            {
            case ApiLevel::Win32:
                newApi = std::make_unique<Microsoft::Console::Interactivity::Win32::WindowDpiApi>();
                break;

#ifdef BUILD_ONECORE_INTERACTIVITY
            case ApiLevel::OneCore:
                newApi.reset();
                break;
#endif
            default:
                status = STATUS_INVALID_LEVEL;
                break;
            }
        }
        catch (...)
        {
            status = NTSTATUS_FROM_HRESULT(wil::ResultFromCaughtException());
        }

        if (SUCCEEDED_NTSTATUS(status))
        {
            api.swap(newApi);
        }
    }

    return status;
}

[[nodiscard]] NTSTATUS InteractivityFactory::CreateWindowMetrics(_Inout_ std::unique_ptr<IWindowMetrics>& metrics)
{
    auto status = STATUS_SUCCESS;

    ApiLevel level;
    status = ApiDetector::DetectNtUserWindow(&level);

    if (SUCCEEDED_NTSTATUS(status))
    {
        std::unique_ptr<IWindowMetrics> newMetrics;
        try
        {
            switch (level)
            {
            case ApiLevel::Win32:
                newMetrics = std::make_unique<Microsoft::Console::Interactivity::Win32::WindowMetrics>();
                break;

#ifdef BUILD_ONECORE_INTERACTIVITY
            case ApiLevel::OneCore:
                newMetrics = std::make_unique<Microsoft::Console::Interactivity::OneCore::WindowMetrics>();
                break;
#endif
            default:
                status = STATUS_INVALID_LEVEL;
                break;
            }
        }
        catch (...)
        {
            status = NTSTATUS_FROM_HRESULT(wil::ResultFromCaughtException());
        }

        if (SUCCEEDED_NTSTATUS(status))
        {
            metrics.swap(newMetrics);
        }
    }

    return status;
}

[[nodiscard]] NTSTATUS InteractivityFactory::CreateAccessibilityNotifier(_Inout_ std::unique_ptr<IAccessibilityNotifier>& notifier)
{
    auto status = STATUS_SUCCESS;

    ApiLevel level;
    status = ApiDetector::DetectNtUserWindow(&level);

    if (SUCCEEDED_NTSTATUS(status))
    {
        std::unique_ptr<IAccessibilityNotifier> newNotifier;
        try
        {
            switch (level)
            {
            case ApiLevel::Win32:
                newNotifier = std::make_unique<Microsoft::Console::Interactivity::Win32::AccessibilityNotifier>();
                break;

#ifdef BUILD_ONECORE_INTERACTIVITY
            case ApiLevel::OneCore:
                newNotifier = std::make_unique<Microsoft::Console::Interactivity::OneCore::AccessibilityNotifier>();
                break;
#endif
            default:
                status = STATUS_INVALID_LEVEL;
                break;
            }
        }
        catch (...)
        {
            status = NTSTATUS_FROM_HRESULT(wil::ResultFromCaughtException());
        }

        if (SUCCEEDED_NTSTATUS(status))
        {
            notifier.swap(newNotifier);
        }
    }

    return status;
}

[[nodiscard]] NTSTATUS InteractivityFactory::CreateSystemConfigurationProvider(_Inout_ std::unique_ptr<ISystemConfigurationProvider>& provider)
{
    auto status = STATUS_SUCCESS;

    ApiLevel level;
    status = ApiDetector::DetectNtUserWindow(&level);

    if (SUCCEEDED_NTSTATUS(status))
    {
        std::unique_ptr<ISystemConfigurationProvider> NewProvider;
        try
        {
            switch (level)
            {
            case ApiLevel::Win32:
                NewProvider = std::make_unique<Microsoft::Console::Interactivity::Win32::SystemConfigurationProvider>();
                break;

#ifdef BUILD_ONECORE_INTERACTIVITY
            case ApiLevel::OneCore:
                NewProvider = std::make_unique<Microsoft::Console::Interactivity::OneCore::SystemConfigurationProvider>();
                break;
#endif
            default:
                status = STATUS_INVALID_LEVEL;
                break;
            }
        }
        catch (...)
        {
            status = NTSTATUS_FROM_HRESULT(wil::ResultFromCaughtException());
        }

        if (SUCCEEDED_NTSTATUS(status))
        {
            provider.swap(NewProvider);
        }
    }

    return status;
}

// Method Description:
// - Attempts to instantiate a "pseudo window" for when we're operating in
//      pseudoconsole mode. There are some tools (cygwin & derivatives) that use
//      the GetConsoleWindow API to uniquely identify console sessions. This
//      function is used to create an invisible window for that scenario, so
//      that GetConsoleWindow returns a real value.
// Arguments:
// - hwnd: Receives the value of the newly created window's HWND.
// - owner: the HWND that should be the initial owner of the pseudo window.
// Return Value:
// - STATUS_SUCCESS on success, otherwise an appropriate error.
[[nodiscard]] NTSTATUS InteractivityFactory::CreatePseudoWindow(HWND& hwnd, const HWND owner)
{
    hwnd = nullptr;
    ApiLevel level;
    auto status = ApiDetector::DetectNtUserWindow(&level);

    if (SUCCEEDED_NTSTATUS(status))
    {
        try
        {
            static const auto PSEUDO_WINDOW_CLASS = L"PseudoConsoleWindow";
            WNDCLASSEXW pseudoClass{ 0 };
            switch (level)
            {
            case ApiLevel::Win32:
            {
                pseudoClass.cbSize = sizeof(WNDCLASSEXW);
                pseudoClass.lpszClassName = PSEUDO_WINDOW_CLASS;
                pseudoClass.lpfnWndProc = s_PseudoWindowProc;
                pseudoClass.cbWndExtra = GWL_CONSOLE_WNDALLOC; // this is required to store the owning thread/process override in NTUSER
                auto windowClassAtom{ RegisterClassExW(&pseudoClass) };

                // Note that because we're not specifying WS_CHILD, this window
                // will become an _owned_ window, not a _child_ window. This is
                // important - child windows report their position as relative
                // to their parent window, while owned windows are still
                // relative to the desktop. (there are other subtleties as well
                // as far as the difference between parent/child and owner/owned
                // windows). Evan K said we should do it this way, and he
                // definitely knows.
                //
                // GH#13066: Load-bearing: Make sure to set WS_POPUP. If you
                // don't, then GetAncestor(GetConsoleWindow(), GA_ROOTOWNER)
                // will return the console handle again, not the owning
                // terminal's handle. It's not entirely clear why, but WS_POPUP
                // is absolutely vital for this to work correctly.
                const auto windowStyle = WS_OVERLAPPEDWINDOW | WS_POPUP;
                const auto exStyles = WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_NOACTIVATE;

                // Attempt to create window.
                hwnd = CreateWindowExW(exStyles,
                                       reinterpret_cast<LPCWSTR>(windowClassAtom),
                                       nullptr,
                                       windowStyle,
                                       0,
                                       0,
                                       0,
                                       0,
                                       owner,
                                       nullptr,
                                       nullptr,
                                       this);

                if (hwnd == nullptr)
                {
                    const auto gle = GetLastError();
                    status = NTSTATUS_FROM_WIN32(gle);
                }
                _pseudoConsoleWindowHwnd = hwnd;
                break;
            }
#ifdef BUILD_ONECORE_INTERACTIVITY
            case ApiLevel::OneCore:
                hwnd = 0;
                status = STATUS_SUCCESS;
                break;
#endif
            default:
                status = STATUS_INVALID_LEVEL;
                break;
            }
        }
        catch (...)
        {
            status = NTSTATUS_FROM_HRESULT(wil::ResultFromCaughtException());
        }
    }

    return status;
}

// Method Description:
// - Static window procedure for pseudo console windows
// - Processes set-up on create to stow the "this" pointer to specific instantiations and routes
//   to the specific object on future calls.
// Arguments:
// - hWnd - Associated window handle from message
// - Message - ID of message in queue
// - wParam - Variable wParam depending on message type
// - lParam - Variable lParam depending on message type
// Return Value:
// - 0 if we processed this message. See details on how a WindowProc is implemented.
[[nodiscard]] LRESULT CALLBACK InteractivityFactory::s_PseudoWindowProc(_In_ HWND hWnd, _In_ UINT Message, _In_ WPARAM wParam, _In_ LPARAM lParam)
{
    // Save the pointer here to the specific window instance when one is created
    if (Message == WM_CREATE)
    {
        const CREATESTRUCT* const pCreateStruct = reinterpret_cast<CREATESTRUCT*>(lParam);

        InteractivityFactory* const pFactory = reinterpret_cast<InteractivityFactory*>(pCreateStruct->lpCreateParams);
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pFactory));
    }

    // Dispatch the message to the specific class instance
    InteractivityFactory* const pFactory = reinterpret_cast<InteractivityFactory*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
    if (pFactory != nullptr)
    {
        return pFactory->PseudoWindowProc(hWnd, Message, wParam, lParam);
    }

    // If we get this far, call the default window proc
    return DefWindowProcW(hWnd, Message, wParam, lParam);
}

// Method Description:
// - Per-object window procedure for pseudo console windows
// Arguments:
// - hWnd - Associated window handle from message
// - Message - ID of message in queue
// - wParam - Variable wParam depending on message type
// - lParam - Variable lParam depending on message type
// Return Value:
// - 0 if we processed this message. See details on how a WindowProc is implemented.
[[nodiscard]] LRESULT CALLBACK InteractivityFactory::PseudoWindowProc(_In_ HWND hWnd, _In_ UINT Message, _In_ WPARAM wParam, _In_ LPARAM lParam)
{
    switch (Message)
    {
    // NOTE: To the future reader, all window messages that are talked about but unused were tested
    //       during prototyping and didn't give quite the results needed to determine show/hide window
    //       state. The notes are left here for future expeditions into message queues.
    // case WM_QUERYOPEN:
    // It can be fun to toggle WM_QUERYOPEN but DefWindowProc returns TRUE.
    case WM_SIZE:
    {
        // Curiously, at least on Windows 10 (and rarely on Windows 11), if you
        // minimize the Terminal by clicking on the taskbar, then alt-tab to try
        // and restore the window, the Taskbar will decide to call
        // SwitchToWindow on us, the invisible, owned window of the main window.
        // When that happens, we'll get a WM_SIZE(SIZE_RESTORED, lParam=0). The
        // main window will NOT get a SwitchToWindow called. If we don't
        // actually inform the hosting process about this, then the main HWND
        // might stay hidden. Refer to GH#13589

        if (wParam == SIZE_RESTORED)
        {
            _WritePseudoWindowCallback(true);
        }

        if (wParam == SIZE_MINIMIZED)
        {
            _WritePseudoWindowCallback(false);
        }
        break;
    }
    // case WM_WINDOWPOSCHANGING:
    //     As long as user32 didn't eat the `ShowWindow` call because the window state requested
    //     matches the existing WS_VISIBLE state of the HWND... we should hear from it in WM_WINDOWPOSCHANGING.
    //     WM_WINDOWPOSCHANGING can tell us a bunch through the flags fields.
    //     We can also check IsIconic/IsZoomed on the HWND during the message
    //     and we could suppress the change to prevent things from happening.
    // case WM_SYSCOMMAND:
    //   WM_SYSCOMMAND will not come through. Don't try.
    // WM_SHOWWINDOW does come through on some of the messages.
    case WM_SHOWWINDOW:
    {
        if (0 == lParam) // Someone explicitly called ShowWindow on us.
        {
            _WritePseudoWindowCallback((bool)wParam);
        }
    }
    case WM_GETOBJECT:
    {
        if (static_cast<long>(lParam) == static_cast<long>(UiaRootObjectId))
        {
            if (nullptr == _pPseudoConsoleUiaProvider)
            {
                LOG_IF_FAILED(WRL::MakeAndInitialize<PseudoConsoleWindowAccessibilityProvider>(&_pPseudoConsoleUiaProvider, _pseudoConsoleWindowHwnd));
            }
            return UiaReturnRawElementProvider(hWnd, wParam, lParam, _pPseudoConsoleUiaProvider.Get());
        }
        return 0;
    }
    }
    // If we get this far, call the default window proc
    return DefWindowProcW(hWnd, Message, wParam, lParam);
}

// Method Description:
// - Helper for the pseudo console message loop to send a notification
//   when it realizes we should be showing or hiding the window.
// - Simply skips if no callback is installed.
// Arguments:
// - showOrHide: Show is true; hide is false.
// Return Value:
// - <none>
void InteractivityFactory::_WritePseudoWindowCallback(bool showOrHide)
{
    // IMPORTANT!
    //
    // A hosting terminal window should only "restore" itself in response to
    // this message, if it's already minimized. If the window is maximized a
    // restore will restore-down the window instead.
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    if (const auto io = gci.GetVtIo())
    {
        io->SetWindowVisibility(showOrHide);
    }
}

#pragma endregion
