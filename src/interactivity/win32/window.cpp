// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "ConsoleControl.hpp"
#include "icon.hpp"
#include "menu.hpp"
#include "window.hpp"
#include "windowio.hpp"
#include "windowdpiapi.hpp"
#include "windowmetrics.hpp"

#include "../../inc/conint.h"

#include "../../host/globals.h"
#include "../../host/dbcs.h"
#include "../../host/getset.h"
#include "../../host/misc.h"
#include "../../host/_output.h"
#include "../../host/output.h"
#include "../../host/renderData.hpp"
#include "../../host/scrolling.hpp"
#include "../../host/srvinit.h"
#include "../../host/stream.h"
#include "../../host/telemetry.hpp"
#include "../../host/tracing.hpp"

#include "../../renderer/base/renderer.hpp"
#include "../../renderer/gdi/gdirenderer.hpp"

#ifndef __INSIDE_WINDOWS
#include "../../renderer/dx/DxRenderer.hpp"
#else
// Forward-declare this so we don't blow up later.
struct DxEngine;
#endif

#include "../inc/ServiceLocator.hpp"
#include "../../types/inc/Viewport.hpp"
#include "../interactivity/win32/windowUiaProvider.hpp"

// The following default masks are used in creating windows
// Make sure that these flags match when switching to fullscreen and back
#define CONSOLE_WINDOW_FLAGS (WS_OVERLAPPEDWINDOW | WS_HSCROLL | WS_VSCROLL)
#define CONSOLE_WINDOW_EX_FLAGS (WS_EX_WINDOWEDGE | WS_EX_ACCEPTFILES | WS_EX_APPWINDOW | WS_EX_LAYERED)

// Window class name
#define CONSOLE_WINDOW_CLASS (L"ConsoleWindowClass")

using namespace Microsoft::Console::Interactivity::Win32;
using namespace Microsoft::Console::Types;
using namespace Microsoft::Console::Interactivity;
using namespace Microsoft::Console::Render;

ATOM Window::s_atomWindowClass = 0;
Window* Window::s_Instance = nullptr;

Window::Window() :
    _fIsInFullscreen(false),
    _pSettings(nullptr),
    _hWnd(nullptr),
    _pUiaProvider(nullptr),
    _fWasMaximizedBeforeFullscreen(false),
    _dpiBeforeFullscreen(0)
{
    ZeroMemory((void*)&_rcClientLast, sizeof(_rcClientLast));
    ZeroMemory((void*)&_rcWindowBeforeFullscreen, sizeof(_rcWindowBeforeFullscreen));
    ZeroMemory((void*)&_rcWorkBeforeFullscreen, sizeof(_rcWorkBeforeFullscreen));
}

Window::~Window()
{
    if (ServiceLocator::LocateGlobals().pRender != nullptr)
    {
        delete ServiceLocator::LocateGlobals().pRender;
    }
}

// Routine Description:
// - This routine allocates and initializes a window for the console
// Arguments:
// - pSettings - All user-configurable settings related to the console host
// - pScreen - The initial screen rendering data to attach to (renders in the client area of this window)
// Return Value:
// - STATUS_SUCCESS or suitable NT error code
[[nodiscard]] NTSTATUS Window::CreateInstance(_In_ Settings* const pSettings,
                                              _In_ SCREEN_INFORMATION* const pScreen)
{
    NTSTATUS status = s_RegisterWindowClass();

    if (NT_SUCCESS(status))
    {
        Window* pNewWindow = new (std::nothrow) Window();

        status = NT_TESTNULL(pNewWindow);

        if (NT_SUCCESS(status))
        {
            status = pNewWindow->_MakeWindow(pSettings, pScreen);

            if (NT_SUCCESS(status))
            {
                Window::s_Instance = pNewWindow;
                LOG_IF_FAILED(ServiceLocator::SetConsoleWindowInstance(pNewWindow));
            }
        }
    }

    return status;
}

// Routine Description:
// - Registers the window class information with the system
// - Only should happen once for the entire lifetime of this class.
// Arguments:
// - <none>
// Return Value:
// - STATUS_SUCCESS or failure from loading icons/registering class with the system
[[nodiscard]] NTSTATUS Window::s_RegisterWindowClass()
{
    NTSTATUS status = STATUS_SUCCESS;

    // Today we never call this more than once.
    // In the future, if we need multiple windows (for tabs, etc.) we will need to make this thread-safe.
    // As such, the window class should always be 0 when we are entering this the first and only time.
    FAIL_FAST_IF(!(s_atomWindowClass == 0));

    // Only register if we haven't already registered
    if (s_atomWindowClass == 0)
    {
        // Prepare window class structure
        WNDCLASSEX wc = { 0 };
        wc.cbSize = sizeof(WNDCLASSEX);
        wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC | CS_DBLCLKS;
        wc.lpfnWndProc = s_ConsoleWindowProc;
        wc.cbClsExtra = 0;
        wc.cbWndExtra = GWL_CONSOLE_WNDALLOC;
        wc.hInstance = nullptr;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr; // We don't want the background painted. It will cause flickering.
        wc.lpszMenuName = nullptr;
        wc.lpszClassName = CONSOLE_WINDOW_CLASS;

        // Load icons
        status = Icon::Instance().GetIcons(&wc.hIcon, &wc.hIconSm);

        if (NT_SUCCESS(status))
        {
            s_atomWindowClass = RegisterClassExW(&wc);

            if (s_atomWindowClass == 0)
            {
                status = NTSTATUS_FROM_WIN32(GetLastError());
            }
        }
    }

    return status;
}

// Routine Description:
// - Updates some global system metrics when triggered.
// - Calls subroutines to update metrics for other relevant items
// - Example metrics include window borders, scroll size, timer values, etc.
// Arguments:
// - <none>
// Return Value:
// - <none>
void Window::_UpdateSystemMetrics() const
{
    WindowDpiApi* const dpiApi = ServiceLocator::LocateHighDpiApi<WindowDpiApi>();
    Globals& g = ServiceLocator::LocateGlobals();
    CONSOLE_INFORMATION& gci = g.getConsoleInformation();

    Scrolling::s_UpdateSystemMetrics();

    g.sVerticalScrollSize = (SHORT)dpiApi->GetSystemMetricsForDpi(SM_CXVSCROLL, g.dpi);
    g.sHorizontalScrollSize = (SHORT)dpiApi->GetSystemMetricsForDpi(SM_CYHSCROLL, g.dpi);

    gci.GetCursorBlinker().UpdateSystemMetrics();

    const auto sysConfig = ServiceLocator::LocateSystemConfigurationProvider();

    g.cursorPixelWidth = sysConfig->GetCursorWidth();
}

// Routine Description:
// - This will call the system to create a window for the console, set
//   up settings, and prepare for rendering.
// Arguments:
// - pSettings - Load user-configurable settings from this structure
// - pScreen - Attach to this screen for rendering the client area of the window
// Return Value:
// - STATUS_SUCCESS, invalid parameters, or various potential errors from calling CreateWindow
[[nodiscard]] NTSTATUS Window::_MakeWindow(_In_ Settings* const pSettings,
                                           _In_ SCREEN_INFORMATION* const pScreen)
{
    Globals& g = ServiceLocator::LocateGlobals();
    CONSOLE_INFORMATION& gci = g.getConsoleInformation();
    NTSTATUS status = STATUS_SUCCESS;

    if (pSettings == nullptr)
    {
        status = STATUS_INVALID_PARAMETER_1;
    }
    else if (pScreen == nullptr)
    {
        status = STATUS_INVALID_PARAMETER_2;
    }

    // Ensure we have appropriate system metrics before we start constructing the window.
    _UpdateSystemMetrics();

    const bool useDx = pSettings->GetUseDx();
    GdiEngine* pGdiEngine = nullptr;
    [[maybe_unused]] DxEngine* pDxEngine = nullptr;
    try
    {
#ifndef __INSIDE_WINDOWS
        if (useDx)
        {
            pDxEngine = new DxEngine();
            // TODO: MSFT:21255595 make this less gross
            // Manually set the Dx Engine to Hwnd mode. When we're trying to
            // determine the initial window size, which happens BEFORE the
            // window is created, we'll want to make sure the DX engine does
            // math in the hwnd mode, not the Composition mode.
            THROW_IF_FAILED(pDxEngine->SetHwnd(nullptr));
            g.pRender->AddRenderEngine(pDxEngine);
        }
        else
#endif
        {
            pGdiEngine = new GdiEngine();
            g.pRender->AddRenderEngine(pGdiEngine);
        }
    }
    catch (...)
    {
        status = NTSTATUS_FROM_HRESULT(wil::ResultFromCaughtException());
    }

    if (NT_SUCCESS(status))
    {
        SCREEN_INFORMATION& siAttached = GetScreenInfo();

        siAttached.RefreshFontWithRenderer();

        // Save reference to settings
        _pSettings = pSettings;

        // Figure out coordinates and how big to make the window from the desired client viewport size
        // Put left, top, right and bottom into rectProposed for checking against monitor screens below
        RECT rectProposed = { pSettings->GetWindowOrigin().X, pSettings->GetWindowOrigin().Y, 0, 0 };
        _CalculateWindowRect(pSettings->GetWindowSize(), &rectProposed); //returns with rectangle filled out

        if (!WI_IsFlagSet(gci.Flags, CONSOLE_AUTO_POSITION))
        {
            //if launched from a shortcut, ensure window is visible on screen
            if (pSettings->IsStartupTitleIsLinkNameSet())
            {
                // if window would be fully OFFscreen, change position so it is ON screen.
                // This doesn't change the actual coordinates
                // stored in the link, just the starting position
                // of the window.
                // When the user reconnects the other monitor, the
                // window will be where he left it. Great for take
                // home laptop scenario.
                if (!MonitorFromRect(&rectProposed, MONITOR_DEFAULTTONULL))
                {
                    //Monitor we'll move to
                    HMONITOR hMon = MonitorFromRect(&rectProposed, MONITOR_DEFAULTTONEAREST);
                    MONITORINFO mi = { 0 };

                    //get origin of monitor's workarea
                    mi.cbSize = sizeof(MONITORINFO);
                    GetMonitorInfo(hMon, &mi);

                    //Adjust right and bottom to new positions, relative to monitor workarea's origin
                    //Need to do this before adjusting left/top so RECT_* calculations are correct
                    rectProposed.right = mi.rcWork.left + RECT_WIDTH(&rectProposed);
                    rectProposed.bottom = mi.rcWork.top + RECT_HEIGHT(&rectProposed);

                    // Move origin to top left of nearest
                    // monitor's WORKAREA (accounting for taskbar
                    // and any app toolbars)
                    rectProposed.left = mi.rcWork.left;
                    rectProposed.top = mi.rcWork.top;
                }
            }
        }

        // CreateWindowExW needs a null terminated string, so ensure
        // title is null terminated in a std::wstring here.
        // We don't mind the string copy here because making the window
        // should be infrequent.
        const std::wstring title{ gci.GetTitle() };

        // Attempt to create window
        HWND hWnd = CreateWindowExW(
            CONSOLE_WINDOW_EX_FLAGS,
            CONSOLE_WINDOW_CLASS,
            title.c_str(),
            CONSOLE_WINDOW_FLAGS,
            WI_IsFlagSet(gci.Flags, CONSOLE_AUTO_POSITION) ? CW_USEDEFAULT : rectProposed.left,
            rectProposed.top, // field is ignored if CW_USEDEFAULT was chosen above
            RECT_WIDTH(&rectProposed),
            RECT_HEIGHT(&rectProposed),
            HWND_DESKTOP,
            nullptr,
            nullptr,
            this // handle to this window class, passed to WM_CREATE to help dispatching to this instance
        );

        if (hWnd == nullptr)
        {
            DWORD const gle = GetLastError();
            RIPMSG1(RIP_WARNING, "CreateWindow failed with gle = 0x%x", gle);
            status = NTSTATUS_FROM_WIN32(gle);
        }

        if (NT_SUCCESS(status))
        {
            _hWnd = hWnd;

#ifndef __INSIDE_WINDOWS
            if (useDx)
            {
                status = NTSTATUS_FROM_WIN32(HRESULT_CODE((pDxEngine->SetHwnd(hWnd))));

                if (NT_SUCCESS(status))
                {
                    status = NTSTATUS_FROM_WIN32(HRESULT_CODE((pDxEngine->Enable())));
                }
            }
            else
#endif
            {
                status = NTSTATUS_FROM_WIN32(HRESULT_CODE((pGdiEngine->SetHwnd(hWnd))));
            }

            if (NT_SUCCESS(status))
            {
                // Set alpha on window if requested
                ApplyWindowOpacity();

                status = Menu::CreateInstance(hWnd);

                if (NT_SUCCESS(status))
                {
                    gci.ConsoleIme.RefreshAreaAttributes();

                    // Do WM_GETICON workaround. Must call WM_SETICON once or apps calling WM_GETICON will get null.
                    LOG_IF_FAILED(Icon::Instance().ApplyWindowMessageWorkaround(hWnd));

                    // Set up the hot key for this window.
                    if (gci.GetHotKey() != 0)
                    {
                        SendMessageW(hWnd, WM_SETHOTKEY, gci.GetHotKey(), 0);
                    }

                    // Post a window size update so that the new console window will size itself correctly once it's up and
                    // running. This works around chicken & egg cases involving window size calculations having to do with font
                    // sizes, DPI, and non-primary monitors (see MSFT #2367234).
                    siAttached.PostUpdateWindowSize();

                    // Locate window theming modules and try to set the dark mode.
                    LOG_IF_FAILED(Microsoft::Console::Internal::Theming::TrySetDarkMode(_hWnd));
                }
            }
        }
    }

    return status;
}

// Routine Description:
// - Called when the window is about to close
// - Right now, it just triggers the process list management to notify that we're closing
// Arguments:
// - <none>
// Return Value:
// - <none>
void Window::_CloseWindow() const
{
    // Pass on the notification to attached processes.
    // Since we only have one window for now, this will be the end of the host process as well.
    CloseConsoleProcessState();
}

// Routine Description:
// - Activates and shows this window based on the flags given.
// Arguments:
// - wShowWindow - See STARTUPINFO wShowWindow member: http://msdn.microsoft.com/en-us/library/windows/desktop/ms686331(v=vs.85).aspx
// Return Value:
// - STATUS_SUCCESS or system errors from activating the window and setting its show states
[[nodiscard]] NTSTATUS Window::ActivateAndShow(const WORD wShowWindow)
{
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    NTSTATUS status = STATUS_SUCCESS;
    HWND const hWnd = GetWindowHandle();

    // Only activate if the wShowWindow we were passed at process create doesn't explicitly tell us to remain inactive/hidden
    if (wShowWindow != SW_SHOWNOACTIVATE &&
        wShowWindow != SW_SHOWMINNOACTIVE &&
        wShowWindow != SW_HIDE)
    {
        // Do not check result. On some SKUs, such as WinPE, it's perfectly OK for NULL to be returned.
        SetActiveWindow(hWnd);
    }
    else if (wShowWindow == SW_SHOWMINNOACTIVE)
    {
        // If we're minimized and not the active window, set iconic to stop rendering
        gci.Flags |= CONSOLE_IS_ICONIC;
    }

    if (NT_SUCCESS(status))
    {
        ShowWindow(hWnd, wShowWindow);

        SCREEN_INFORMATION& siAttached = GetScreenInfo();
        siAttached.InternalUpdateScrollBars();
    }

    return status;
}

// Routine Description:
// - This routine sets the window origin.
// Arguments:
// - NewWindow: the inclusive rect to use as the new viewport in the buffer
// Return Value:
// <none>
void Window::ChangeViewport(const SMALL_RECT NewWindow)
{
    const CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    SCREEN_INFORMATION& ScreenInfo = GetScreenInfo();

    COORD const FontSize = ScreenInfo.GetScreenFontSize();

    if (WI_IsFlagClear(gci.Flags, CONSOLE_IS_ICONIC))
    {
        Selection* pSelection = &Selection::Instance();
        pSelection->HideSelection();

        // Fire off an event to let accessibility apps know we've scrolled.
        IAccessibilityNotifier* pNotifier = ServiceLocator::LocateAccessibilityNotifier();
        if (pNotifier != nullptr)
        {
            pNotifier->NotifyConsoleUpdateScrollEvent(ScreenInfo.GetViewport().Left() - NewWindow.Left,
                                                      ScreenInfo.GetViewport().Top() - NewWindow.Top);
        }

        // The new window is OK. Store it in screeninfo and refresh screen.
        ScreenInfo.SetViewport(Viewport::FromInclusive(NewWindow), false);
        Tracing::s_TraceWindowViewport(ScreenInfo.GetViewport());

        if (ServiceLocator::LocateGlobals().pRender != nullptr)
        {
            ServiceLocator::LocateGlobals().pRender->TriggerScroll();
        }

        pSelection->ShowSelection();
    }
    else
    {
        // we're iconic
        ScreenInfo.SetViewport(Viewport::FromInclusive(NewWindow), false);
        Tracing::s_TraceWindowViewport(ScreenInfo.GetViewport());
    }

    LOG_IF_FAILED(ConsoleImeResizeCompStrView());

    ScreenInfo.UpdateScrollBars();
}

// Routine Description:
// - Sends an update to the window size based on the character size requested.
// Arguments:
// - Size of the window in characters (relative to the current font)
// Return Value:
// - <none>
void Window::UpdateWindowSize(const COORD coordSizeInChars)
{
    GetScreenInfo().SetViewportSize(&coordSizeInChars);

    PostUpdateWindowSize();
}

// Routine Description:
// Arguments:
// Return Value:
void Window::UpdateWindowPosition(_In_ POINT const ptNewPos) const
{
    SetWindowPos(GetWindowHandle(),
                 nullptr,
                 ptNewPos.x,
                 ptNewPos.y,
                 0,
                 0,
                 SWP_NOSIZE | SWP_NOZORDER);
}

// This routine adds or removes the name to or from the beginning of the window title. The possible names are "Scroll", "Mark", and "Select"
void Window::UpdateWindowText()
{
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    const bool fInScrollMode = Scrolling::s_IsInScrollMode();

    Selection* pSelection = &Selection::Instance();
    const bool fInKeyboardMarkMode = pSelection->IsInSelectingState() && pSelection->IsKeyboardMarkSelection();
    const bool fInMouseSelectMode = pSelection->IsInSelectingState() && pSelection->IsMouseInitiatedSelection();

    // should have at most one active mode
    FAIL_FAST_IF(!((fInKeyboardMarkMode && !fInMouseSelectMode && !fInScrollMode) ||
                   (!fInKeyboardMarkMode && fInMouseSelectMode && !fInScrollMode) ||
                   (!fInKeyboardMarkMode && !fInMouseSelectMode && fInScrollMode) ||
                   (!fInKeyboardMarkMode && !fInMouseSelectMode && !fInScrollMode)));

    // determine which message, if any, we want to use
    DWORD dwMsgId = 0;
    if (fInKeyboardMarkMode)
    {
        dwMsgId = ID_CONSOLE_MSGMARKMODE;
    }
    else if (fInMouseSelectMode)
    {
        dwMsgId = ID_CONSOLE_MSGSELECTMODE;
    }
    else if (fInScrollMode)
    {
        dwMsgId = ID_CONSOLE_MSGSCROLLMODE;
    }

    // if we have a message, use it
    if (dwMsgId != 0)
    {
        // load mode string
        WCHAR szMsg[64];
        if (LoadStringW(ServiceLocator::LocateGlobals().hInstance, dwMsgId, szMsg, ARRAYSIZE(szMsg)) > 0)
        {
            gci.SetTitlePrefix(szMsg);
        }
    }
    else
    {
        // no mode-based message. set title back to original state.
        gci.SetTitlePrefix(L"");
    }
}

void Window::CaptureMouse()
{
    SetCapture(_hWnd);
}

BOOL Window::ReleaseMouse()
{
    return ReleaseCapture();
}

// Routine Description:
// - Adjusts the outer window frame size. Does not move the position.
// Arguments:
// - sizeNew - The X and Y dimensions
// Return Value:
// - <none>
void Window::_UpdateWindowSize(const SIZE sizeNew)
{
    const CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    SCREEN_INFORMATION& ScreenInfo = GetScreenInfo();

    if (WI_IsFlagClear(gci.Flags, CONSOLE_IS_ICONIC))
    {
        ScreenInfo.InternalUpdateScrollBars();

        SetWindowPos(GetWindowHandle(),
                     nullptr,
                     0,
                     0,
                     sizeNew.cx,
                     sizeNew.cy,
                     SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_DRAWFRAME);
    }
}

// Routine Description:
// - Triggered when the buffer dimensions/viewport is changed.
// - This function recalculates what size the window should be in order to host the given buffer and viewport
// - Then it will trigger an actual adjustment of the outer window frame
// Arguments:
// - <none> - All state is read from the attached screen buffer
// Return Value:
// - STATUS_SUCCESS or suitable error code
[[nodiscard]] NTSTATUS Window::_InternalSetWindowSize()
{
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    SCREEN_INFORMATION& siAttached = GetScreenInfo();

    WI_ClearFlag(gci.Flags, CONSOLE_SETTING_WINDOW_SIZE);

    if (!IsInFullscreen() && !IsInMaximized())
    {
        // Figure out how big to make the window, given the desired client area size.
        siAttached.ResizingWindow++;

        // First get the buffer viewport size
        const auto WindowDimensions = siAttached.GetViewport().Dimensions();

        // We'll use the font to convert characters to pixels
        const auto ScreenFontSize = siAttached.GetScreenFontSize();

        // Now do the multiplication of characters times pixels per char. This is the client area pixel size.
        SIZE WindowSize;
        WindowSize.cx = WindowDimensions.X * ScreenFontSize.X;
        WindowSize.cy = WindowDimensions.Y * ScreenFontSize.Y;

        // Fill a rectangle to call the system to adjust the client rect into a window rect
        RECT rectSizeTemp = { 0 };
        rectSizeTemp.right = WindowSize.cx;
        rectSizeTemp.bottom = WindowSize.cy;
        FAIL_FAST_IF(!(rectSizeTemp.top == 0 && rectSizeTemp.left == 0));
        ServiceLocator::LocateWindowMetrics<WindowMetrics>()->ConvertClientRectToWindowRect(&rectSizeTemp);

        // Measure the adjusted rectangle dimensions and fill up the size variable
        WindowSize.cx = RECT_WIDTH(&rectSizeTemp);
        WindowSize.cy = RECT_HEIGHT(&rectSizeTemp);

        if (WindowDimensions.Y != 0)
        {
            // We want the alt to have scroll bars if the main has scroll bars.
            // The bars are disabled, but they're still there.
            // This keeps the window, viewport, and SB size from changing when swapping.
            if (!siAttached.GetMainBuffer().IsMaximizedX())
            {
                WindowSize.cy += ServiceLocator::LocateGlobals().sHorizontalScrollSize;
            }

            if (!siAttached.GetMainBuffer().IsMaximizedY())
            {
                WindowSize.cx += ServiceLocator::LocateGlobals().sVerticalScrollSize;
            }
        }

        // Only attempt to actually change the window size if the difference between the size we just calculated
        // and the size of the current window is substantial enough to make a rendering difference.
        // This is an issue now in the V2 console because we allow sub-character window sizes
        // such that there isn't leftover space around the window when snapping.

        // To figure out if it's substantial, calculate what the window size would be if it were one character larger than what we just proposed
        SIZE WindowSizeMax;
        WindowSizeMax.cx = WindowSize.cx + ScreenFontSize.X;
        WindowSizeMax.cy = WindowSize.cy + ScreenFontSize.Y;

        // And figure out the current window size as well.
        RECT const rcWindowCurrent = GetWindowRect();
        SIZE WindowSizeCurrent;
        WindowSizeCurrent.cx = RECT_WIDTH(&rcWindowCurrent);
        WindowSizeCurrent.cy = RECT_HEIGHT(&rcWindowCurrent);

        // If the current window has a few extra sub-character pixels between the proposed size (WindowSize) and the next size up (WindowSizeMax), then don't change anything.
        bool const fDeltaXSubstantial = !(WindowSizeCurrent.cx >= WindowSize.cx && WindowSizeCurrent.cx < WindowSizeMax.cx);
        bool const fDeltaYSubstantial = !(WindowSizeCurrent.cy >= WindowSize.cy && WindowSizeCurrent.cy < WindowSizeMax.cy);

        // If either change was substantial, update the window accordingly to the newly proposed value.
        if (fDeltaXSubstantial || fDeltaYSubstantial)
        {
            _UpdateWindowSize(WindowSize);
        }
        else
        {
            // If the change wasn't substantial, we may still need to update scrollbar positions. Note that PSReadLine
            // scrolls the window via Console.SetWindowPosition, which ultimately calls down to SetConsoleWindowInfo,
            // which ends up in this function.
            siAttached.InternalUpdateScrollBars();
        }

        // MSFT: 12092729
        // To fix an issue with 3rd party applications that wrap our console, notify that the screen buffer size changed
        // when the window viewport is updated.
        // ---
        // - The specific scenario that this impacts is ConEmu (wrapping our console) to use Bash in WSL.
        // - The reason this is a problem is because ConEmu has to programmatically manipulate our buffer and window size
        //   one after another to get our dimensions to change.
        // - The WSL layer watches our Buffer change message to know when to get the new Window size and send it into the
        //   WSL environment. This isn't technically correct to use a Buffer message to know when Window changes, but
        //   it's not totally their fault because we do not provide a Window changed message at all.
        // - If our window is adjusted directly, the Buffer and Window dimensions are both updated simultaneously under lock
        //   and WSL gets one message and updates appropriately.
        // - If ConEmu updates it via the API, one is updated, then the other with an unlocked time interval.
        //   The WSL layer will potentially get the Window size that hasn't been updated yet or is out of sync before the
        //   other API call is completed which results in the application in the WSL environment thinking the window is
        //   a different size and outputting VT sequences with an invalid assumption.
        // - If we make it so a Window change also emits a Buffer change message, then WSL will be notified appropriately
        //   and can pass that information into the WSL environment.
        // - To Windows apps that weren't expecting this information, it should cause no harm because they should just receive
        //   an additional Buffer message with the same size again and do nothing special.
        ScreenBufferSizeChange(siAttached.GetActiveBuffer().GetBufferSize().Dimensions());

        siAttached.ResizingWindow--;
    }

    LOG_IF_FAILED(ConsoleImeResizeCompStrView());

    return STATUS_SUCCESS;
}

// Routine Description:
// - Adjusts the window contents in response to vertical scrolling
// Arguments:
// - ScrollCommand - The relevant command (one line, one page, etc.)
// - AbsoluteChange - The magnitude of the change (for tracking commands)
// Return Value:
// - <none>
void Window::VerticalScroll(const WORD wScrollCommand, const WORD wAbsoluteChange)
{
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    COORD NewOrigin;
    SCREEN_INFORMATION& ScreenInfo = GetScreenInfo();

    // Log a telemetry event saying the user interacted with the Console
    Telemetry::Instance().SetUserInteractive();

    const auto& viewport = ScreenInfo.GetViewport();

    NewOrigin = viewport.Origin();

    const SHORT sScreenBufferSizeY = ScreenInfo.GetBufferSize().Height();

    switch (wScrollCommand)
    {
    case SB_LINEUP:
    {
        NewOrigin.Y--;
        break;
    }

    case SB_LINEDOWN:
    {
        NewOrigin.Y++;
        break;
    }

    case SB_PAGEUP:
    {
        NewOrigin.Y -= viewport.Height() - 1;
        break;
    }

    case SB_PAGEDOWN:
    {
        NewOrigin.Y += viewport.Height() - 1;
        break;
    }

    case SB_THUMBTRACK:
    {
        gci.Flags |= CONSOLE_SCROLLBAR_TRACKING;
        NewOrigin.Y = wAbsoluteChange;
        break;
    }

    case SB_THUMBPOSITION:
    {
        UnblockWriteConsole(CONSOLE_SCROLLBAR_TRACKING);
        NewOrigin.Y = wAbsoluteChange;
        break;
    }

    case SB_TOP:
    {
        NewOrigin.Y = 0;
        break;
    }

    case SB_BOTTOM:
    {
        NewOrigin.Y = sScreenBufferSizeY - viewport.Height();
        break;
    }

    default:
    {
        return;
    }
    }

    NewOrigin.Y = std::clamp(NewOrigin.Y, 0i16, gsl::narrow<SHORT>(sScreenBufferSizeY - viewport.Height()));
    LOG_IF_FAILED(ScreenInfo.SetViewportOrigin(true, NewOrigin, false));
}

// Routine Description:
// - Adjusts the window contents in response to horizontal scrolling
// Arguments:
// - ScrollCommand - The relevant command (one line, one page, etc.)
// - AbsoluteChange - The magnitude of the change (for tracking commands)
// Return Value:
// - <none>
void Window::HorizontalScroll(const WORD wScrollCommand, const WORD wAbsoluteChange)
{
    // Log a telemetry event saying the user interacted with the Console
    Telemetry::Instance().SetUserInteractive();

    SCREEN_INFORMATION& ScreenInfo = GetScreenInfo();
    const SHORT sScreenBufferSizeX = ScreenInfo.GetBufferSize().Width();
    const auto& viewport = ScreenInfo.GetViewport();
    COORD NewOrigin = viewport.Origin();

    switch (wScrollCommand)
    {
    case SB_LINEUP:
    {
        NewOrigin.X--;
        break;
    }

    case SB_LINEDOWN:
    {
        NewOrigin.X++;
        break;
    }

    case SB_PAGEUP:
    {
        NewOrigin.X -= viewport.Width() - 1;
        break;
    }

    case SB_PAGEDOWN:
    {
        NewOrigin.X += viewport.Width() - 1;
        break;
    }

    case SB_THUMBTRACK:
    case SB_THUMBPOSITION:
    {
        NewOrigin.X = wAbsoluteChange;
        break;
    }

    case SB_TOP:
    {
        NewOrigin.X = 0;
        break;
    }

    case SB_BOTTOM:
    {
        NewOrigin.X = (WORD)(sScreenBufferSizeX - viewport.Width());
        break;
    }

    default:
    {
        return;
    }
    }
    NewOrigin.X = std::clamp(NewOrigin.X, 0i16, gsl::narrow<SHORT>(sScreenBufferSizeX - viewport.Width()));
    LOG_IF_FAILED(ScreenInfo.SetViewportOrigin(true, NewOrigin, false));
}

BOOL Window::EnableBothScrollBars()
{
    return EnableScrollBar(_hWnd, SB_BOTH, ESB_ENABLE_BOTH);
}

int Window::UpdateScrollBar(bool isVertical,
                            bool isAltBuffer,
                            UINT pageSize,
                            int maxSize,
                            int viewportPosition)
{
    SCROLLINFO si;
    si.cbSize = sizeof(si);
    si.fMask = isAltBuffer ? SIF_ALL | SIF_DISABLENOSCROLL : SIF_ALL;
    si.nPage = pageSize;
    si.nMin = 0;
    si.nMax = maxSize;
    si.nPos = viewportPosition;

    return SetScrollInfo(_hWnd, isVertical ? SB_VERT : SB_HORZ, &si, TRUE);
}

// Routine Description:
// - Converts a window position structure (sent to us when the window moves) into a window rectangle (the outside edge dimensions)
// Arguments:
// - lpWindowPos - position structure received via Window message
// - prc - rectangle to fill
// Return Value:
// - <none>
void Window::s_ConvertWindowPosToWindowRect(const LPWINDOWPOS lpWindowPos, _Out_ RECT* const prc)
{
    prc->left = lpWindowPos->x;
    prc->top = lpWindowPos->y;
    prc->right = lpWindowPos->x + lpWindowPos->cx;
    prc->bottom = lpWindowPos->y + lpWindowPos->cy;
}

// Routine Description:
// - Converts character counts of the viewport (client area, screen buffer) into the outer pixel dimensions of the window using the current window for context
// Arguments:
// - coordWindowInChars - The size of the viewport
// - prectWindow - rectangle to fill with pixel positions of the outer edge rectangle for this window
// Return Value:
// - <none>
void Window::_CalculateWindowRect(const COORD coordWindowInChars, _Inout_ RECT* const prectWindow) const
{
    auto& g = ServiceLocator::LocateGlobals();
    const SCREEN_INFORMATION& siAttached = GetScreenInfo();
    const COORD coordFontSize = siAttached.GetScreenFontSize();
    const HWND hWnd = GetWindowHandle();
    const COORD coordBufferSize = siAttached.GetBufferSize().Dimensions();
    const int iDpi = g.dpi;

    s_CalculateWindowRect(coordWindowInChars, iDpi, coordFontSize, coordBufferSize, hWnd, prectWindow);
}

// Routine Description:
// - Converts character counts of the viewport (client area, screen buffer) into
//      the outer pixel dimensions of the window
// Arguments:
// - coordWindowInChars - The size of the viewport
// - iDpi - The DPI of the monitor on which this window will reside
//      (used to get DPI-scaled system metrics)
// - coordFontSize - the size in pixels of the font on the monitor
//      (this should be already scaled for DPI)
// - coordBufferSize - the character count of the buffer rectangle in X by Y
// - hWnd - If available, a handle to the window we would change so we can
//      retrieve its class information for border/titlebar/etc metrics.
// - prectWindow - rectangle to fill with pixel positions of the outer edge
//      rectangle for this window
// Return Value:
// - <none>
void Window::s_CalculateWindowRect(const COORD coordWindowInChars,
                                   const int iDpi,
                                   const COORD coordFontSize,
                                   const COORD coordBufferSize,
                                   _In_opt_ HWND const hWnd,
                                   _Inout_ RECT* const prectWindow)
{
    SIZE sizeWindow;

    // Initially use the given size in characters * font size to get client area pixel size
    sizeWindow.cx = coordWindowInChars.X * coordFontSize.X;
    sizeWindow.cy = coordWindowInChars.Y * coordFontSize.Y;

    // Create a proposed rectangle
    RECT rectProposed = { prectWindow->left, prectWindow->top, prectWindow->left + sizeWindow.cx, prectWindow->top + sizeWindow.cy };

    // Now adjust the client area into a window size
    // 1. Start with default window style
    DWORD dwStyle = CONSOLE_WINDOW_FLAGS;
    DWORD dwExStyle = CONSOLE_WINDOW_EX_FLAGS;
    BOOL fMenu = FALSE;

    // 2. if we already have a window handle, check if the style has been updated
    if (hWnd != nullptr)
    {
        dwStyle = GetWindowStyle(hWnd);
        dwExStyle = GetWindowExStyle(hWnd);
    }

    // 3. Perform adjustment
    // NOTE: This may adjust the position of the window as well as the size. This is why we use rectProposed in the interim.
    ServiceLocator::LocateWindowMetrics<WindowMetrics>()->AdjustWindowRectEx(&rectProposed, dwStyle, fMenu, dwExStyle, iDpi);

    // Finally compensate for scroll bars

    // If the window is smaller than the buffer in width, add space at the bottom for a horizontal scroll bar
    if (coordWindowInChars.X < coordBufferSize.X)
    {
        rectProposed.bottom += (SHORT)ServiceLocator::LocateHighDpiApi<WindowDpiApi>()->GetSystemMetricsForDpi(SM_CYHSCROLL, iDpi);
    }

    // If the window is smaller than the buffer in height, add space at the right for a vertical scroll bar
    if (coordWindowInChars.Y < coordBufferSize.Y)
    {
        rectProposed.right += (SHORT)ServiceLocator::LocateHighDpiApi<WindowDpiApi>()->GetSystemMetricsForDpi(SM_CXVSCROLL, iDpi);
    }

    // Apply the calculated sizes to the existing window pointer
    // We do this at the end so we can preserve the positioning of the window and just change the size.
    prectWindow->right = prectWindow->left + RECT_WIDTH(&rectProposed);
    prectWindow->bottom = prectWindow->top + RECT_HEIGHT(&rectProposed);
}

RECT Window::GetWindowRect() const noexcept
{
    RECT rc = { 0 };
    ::GetWindowRect(GetWindowHandle(), &rc);
    return rc;
}

HWND Window::GetWindowHandle() const
{
    return _hWnd;
}

SCREEN_INFORMATION& Window::GetScreenInfo()
{
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    return gci.GetActiveOutputBuffer();
}

const SCREEN_INFORMATION& Window::GetScreenInfo() const
{
    const CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    return gci.GetActiveOutputBuffer();
}

// Routine Description:
// - Gets the window opacity (alpha channel)
// Arguments:
// - <none>
// Return Value:
// - The level of opacity. 0xff should represent 100% opaque and 0x00 should be 100% transparent. (Used for Alpha channel in drawing.)
BYTE Window::GetWindowOpacity() const
{
    return _pSettings->GetWindowAlpha();
}

// Routine Description:
// - Sets the window opacity (alpha channel) with the given value
// - Will restrict to within the valid range. Invalid values will use 0% transparency/100% opaque.
// Arguments:
// - bOpacity - 0xff/100% opacity = opaque window. 0xb2/70% opacity = 30% transparent window.
// Return Value:
// - <none>
void Window::SetWindowOpacity(const BYTE bOpacity)
{
    _pSettings->SetWindowAlpha(bOpacity);
}

// Routine Description:
// - Calls the operating system to apply the current window opacity settings to the active console window.
// Arguments:
// - <none>
// Return Value:
// - <none>
void Window::ApplyWindowOpacity() const
{
    const BYTE bAlpha = GetWindowOpacity();
    HWND const hWnd = GetWindowHandle();

    // See: http://msdn.microsoft.com/en-us/library/ms997507.aspx
    SetLayeredWindowAttributes(hWnd, 0, bAlpha, LWA_ALPHA);
}

// Routine Description:
// - Changes the window opacity by a specified delta.
// - This will update the internally stored value by the given delta (within boundaries)
//   and then will have the new value applied to the actual window.
// - Values that would make the opacity greater than 100% will be fixed to 100%.
// - Values that would bring the opacity below the minimum threshold will be fixed to the minimum threshold.
// Arguments:
// - sOpacityDelta - How much to modify the current window opacity. Positive = more opaque. Negative = more transparent.
// Return Value:
// - <none>
void Window::ChangeWindowOpacity(const short sOpacityDelta)
{
    // Window Opacity is always a BYTE (unsigned char, 1 byte)
    // Delta is a short (signed short, 2 bytes)

    int iAlpha = GetWindowOpacity(); // promote unsigned char to fit into a signed int (4 bytes)

    iAlpha += sOpacityDelta; // performing signed math of 2 byte delta into 4 bytes will not under/overflow.

    // comparisons are against 1 byte values and are ok.
    if (iAlpha > BYTE_MAX)
    {
        iAlpha = BYTE_MAX;
    }
    else if (iAlpha < MIN_WINDOW_OPACITY)
    {
        iAlpha = MIN_WINDOW_OPACITY;
    }

    //Opacity bool is set to true when keyboard or mouse short cut used.
    SetWindowOpacity((BYTE)iAlpha); // cast to fit is guaranteed to be within byte bounds by the checks above.
    ApplyWindowOpacity();
}

// Routine Description:
// - Shorthand for checking if the current window has the maximized property set
// - Uses internally stored window handle
// Return Value:
// - True if maximized. False otherwise.
bool Window::IsInMaximized() const
{
    return IsMaximized(_hWnd);
}

bool Window::IsInFullscreen() const
{
    return _fIsInFullscreen;
}

// Routine Description:
// - Called when entering fullscreen, with the window's current monitor rect and work area.
// - The current window position, dpi, work area, and maximized state are stored, and the
//   window is positioned to the monitor rect.
void Window::_SetFullscreenPosition(const RECT rcMonitor, const RECT rcWork)
{
    ::GetWindowRect(GetWindowHandle(), &_rcWindowBeforeFullscreen);
    _dpiBeforeFullscreen = GetDpiForWindow(GetWindowHandle());
    _fWasMaximizedBeforeFullscreen = IsZoomed(GetWindowHandle());
    _rcWorkBeforeFullscreen = rcWork;

    SetWindowPos(GetWindowHandle(),
                 HWND_TOP,
                 rcMonitor.left,
                 rcMonitor.top,
                 rcMonitor.right - rcMonitor.left,
                 rcMonitor.bottom - rcMonitor.top,
                 SWP_FRAMECHANGED);
}

// Routine Description:
// - Called when exiting fullscreen, with the window's current monitor work area.
// - The window is restored to its previous position, migrating that previous position to the
//   window's current monitor (if the current work area or window DPI have changed).
// - A fullscreen window's monitor can be changed by win+shift+left/right hotkeys or monitor
//   topology changes (for example unplugging a monitor or disconnecting a remote session).
void Window::_RestoreFullscreenPosition(const RECT rcWork)
{
    // If the window was previously maximized, re-maximize the window.
    if (_fWasMaximizedBeforeFullscreen)
    {
        ShowWindow(GetWindowHandle(), SW_SHOWMAXIMIZED);
        SetWindowPos(GetWindowHandle(), HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

        return;
    }

    // Start with the stored window position.
    RECT rcRestore = _rcWindowBeforeFullscreen;

    // If the window DPI has changed, re-size the stored position by the change in DPI. This
    // ensures the window restores to the same logical size (even if to a monitor with a different
    // DPI/ scale factor).
    UINT dpiWindow = GetDpiForWindow(GetWindowHandle());
    rcRestore.right = rcRestore.left + MulDiv(rcRestore.right - rcRestore.left, dpiWindow, _dpiBeforeFullscreen);
    rcRestore.bottom = rcRestore.top + MulDiv(rcRestore.bottom - rcRestore.top, dpiWindow, _dpiBeforeFullscreen);

    // Offset the stored position by the difference in work area.
    OffsetRect(&rcRestore,
               rcWork.left - _rcWorkBeforeFullscreen.left,
               rcWork.top - _rcWorkBeforeFullscreen.top);

    // Enforce that our position is entirely within the bounds of our work area.
    // Prefer the top-left be on-screen rather than bottom-right (right before left, bottom before top).
    if (rcRestore.right > rcWork.right)
    {
        OffsetRect(&rcRestore, rcWork.right - rcRestore.right, 0);
    }
    if (rcRestore.left < rcWork.left)
    {
        OffsetRect(&rcRestore, rcWork.left - rcRestore.left, 0);
    }
    if (rcRestore.bottom > rcWork.bottom)
    {
        OffsetRect(&rcRestore, 0, rcWork.bottom - rcRestore.bottom);
    }
    if (rcRestore.top < rcWork.top)
    {
        OffsetRect(&rcRestore, 0, rcWork.top - rcRestore.top);
    }

    // Show the window at the computed position.
    SetWindowPos(GetWindowHandle(),
                 HWND_TOP,
                 rcRestore.left,
                 rcRestore.top,
                 rcRestore.right - rcRestore.left,
                 rcRestore.bottom - rcRestore.top,
                 SWP_SHOWWINDOW | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
}

void Window::SetIsFullscreen(const bool fFullscreenEnabled)
{
    const bool fChangingFullscreen = (fFullscreenEnabled != _fIsInFullscreen);
    _fIsInFullscreen = fFullscreenEnabled;

    HWND const hWnd = GetWindowHandle();

    // First, modify regular window styles as appropriate
    LONG dwWindowStyle = GetWindowLongW(hWnd, GWL_STYLE);
    if (_fIsInFullscreen)
    {
        // moving to fullscreen. remove WS_OVERLAPPEDWINDOW, which specifies styles for non-fullscreen windows (e.g.
        // caption bar). add the WS_POPUP style to allow us to size ourselves to the monitor size.
        WI_ClearAllFlags(dwWindowStyle, WS_OVERLAPPEDWINDOW);
        WI_SetFlag(dwWindowStyle, WS_POPUP);
    }
    else
    {
        // coming back from fullscreen. undo what we did to get in to fullscreen in the first place.
        WI_ClearFlag(dwWindowStyle, WS_POPUP);
        WI_SetAllFlags(dwWindowStyle, WS_OVERLAPPEDWINDOW);
    }
    SetWindowLongW(hWnd, GWL_STYLE, dwWindowStyle);

    // Now modify extended window styles as appropriate
    LONG dwExWindowStyle = GetWindowLongW(hWnd, GWL_EXSTYLE);
    if (_fIsInFullscreen)
    {
        // moving to fullscreen. remove the window edge style to avoid an ugly border when not focused.
        WI_ClearFlag(dwExWindowStyle, WS_EX_WINDOWEDGE);
    }
    else
    {
        // coming back from fullscreen.
        WI_SetFlag(dwExWindowStyle, WS_EX_WINDOWEDGE);
    }
    SetWindowLongW(hWnd, GWL_EXSTYLE, dwExWindowStyle);

    // Only change the window position if changing fullscreen state.
    if (fChangingFullscreen)
    {
        // Get the monitor info for the window's current monitor.
        MONITORINFO mi = {};
        mi.cbSize = sizeof(mi);
        GetMonitorInfo(MonitorFromWindow(GetWindowHandle(), MONITOR_DEFAULTTONEAREST), &mi);

        if (_fIsInFullscreen)
        {
            // Store the window's current position and size the window to the monitor.
            _SetFullscreenPosition(mi.rcMonitor, mi.rcWork);
        }
        else
        {
            // Restore the stored window position.
            _RestoreFullscreenPosition(mi.rcWork);

            SCREEN_INFORMATION& siAttached = GetScreenInfo();
            siAttached.MakeCurrentCursorVisible();
        }
    }
}

void Window::ToggleFullscreen()
{
    SetIsFullscreen(!IsInFullscreen());
}

void Window::s_ReinitializeFontsForDPIChange()
{
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    gci.GetActiveOutputBuffer().RefreshFontWithRenderer();
}

[[nodiscard]] LRESULT Window::s_RegPersistWindowPos(_In_ PCWSTR const pwszTitle,
                                                    const BOOL fAutoPos,
                                                    const Window* const pWindow)
{
    const CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    HKEY hCurrentUserKey, hConsoleKey, hTitleKey;
    // Open the current user registry key.
    NTSTATUS Status = RegistrySerialization::s_OpenCurrentUserConsoleTitleKey(pwszTitle, &hCurrentUserKey, &hConsoleKey, &hTitleKey);
    if (NT_SUCCESS(Status))
    {
        // Save window size
        auto windowRect = pWindow->GetWindowRect();
        const auto windowDimensions = gci.GetActiveOutputBuffer().GetViewport().Dimensions();
        DWORD dwValue = MAKELONG(windowDimensions.X, windowDimensions.Y);
        Status = RegistrySerialization::s_UpdateValue(hConsoleKey,
                                                      hTitleKey,
                                                      CONSOLE_REGISTRY_WINDOWSIZE,
                                                      REG_DWORD,
                                                      reinterpret_cast<BYTE*>(&dwValue),
                                                      static_cast<DWORD>(sizeof(dwValue)));
        if (NT_SUCCESS(Status))
        {
            const COORD coordScreenBufferSize = gci.GetActiveOutputBuffer().GetBufferSize().Dimensions();
            auto screenBufferWidth = coordScreenBufferSize.X;
            auto screenBufferHeight = coordScreenBufferSize.Y;
            dwValue = MAKELONG(screenBufferWidth, screenBufferHeight);
            Status = RegistrySerialization::s_UpdateValue(hConsoleKey,
                                                          hTitleKey,
                                                          CONSOLE_REGISTRY_BUFFERSIZE,
                                                          REG_DWORD,
                                                          reinterpret_cast<BYTE*>(&dwValue),
                                                          static_cast<DWORD>(sizeof(dwValue)));
            if (NT_SUCCESS(Status))
            {
                // Save window position
                if (fAutoPos)
                {
                    Status = RegistrySerialization::s_DeleteValue(hTitleKey, CONSOLE_REGISTRY_WINDOWPOS);
                }
                else
                {
                    dwValue = MAKELONG(windowRect.left, windowRect.top);
                    Status = RegistrySerialization::s_UpdateValue(hConsoleKey,
                                                                  hTitleKey,
                                                                  CONSOLE_REGISTRY_WINDOWPOS,
                                                                  REG_DWORD,
                                                                  reinterpret_cast<BYTE*>(&dwValue),
                                                                  static_cast<DWORD>(sizeof(dwValue)));
                }
            }
        }

        if (hTitleKey != hConsoleKey)
        {
            RegCloseKey(hTitleKey);
        }

        RegCloseKey(hConsoleKey);
        RegCloseKey(hCurrentUserKey);
    }

    return Status;
}

[[nodiscard]] LRESULT Window::s_RegPersistWindowOpacity(_In_ PCWSTR const pwszTitle, const Window* const pWindow)
{
    HKEY hCurrentUserKey, hConsoleKey, hTitleKey;

    // Open the current user registry key.
    NTSTATUS Status = RegistrySerialization::s_OpenCurrentUserConsoleTitleKey(pwszTitle, &hCurrentUserKey, &hConsoleKey, &hTitleKey);
    if (NT_SUCCESS(Status))
    {
        // Save window opacity
        DWORD dwValue;
        dwValue = pWindow->GetWindowOpacity();
        Status = RegistrySerialization::s_UpdateValue(hConsoleKey,
                                                      hTitleKey,
                                                      CONSOLE_REGISTRY_WINDOWALPHA,
                                                      REG_DWORD,
                                                      reinterpret_cast<BYTE*>(&dwValue),
                                                      static_cast<DWORD>(sizeof(dwValue)));

        if (hTitleKey != hConsoleKey)
        {
            RegCloseKey(hTitleKey);
        }
        RegCloseKey(hConsoleKey);
        RegCloseKey(hCurrentUserKey);
    }
    return Status;
}

// Routine Description:
// - Creates/retrieves a handle to the UI Automation provider COM interfaces
// Arguments:
// - <none>
// Return Value:
// - Pointer to UI Automation provider class/interfaces.
IRawElementProviderSimple* Window::_GetUiaProvider()
{
    if (nullptr == _pUiaProvider)
    {
        LOG_IF_FAILED(WRL::MakeAndInitialize<WindowUiaProvider>(&_pUiaProvider, this));
    }

    return _pUiaProvider.Get();
}

[[nodiscard]] HRESULT Window::SignalUia(_In_ EVENTID id)
{
    if (_pUiaProvider != nullptr)
    {
        return _pUiaProvider->Signal(id);
    }
    return S_FALSE;
}

[[nodiscard]] HRESULT Window::UiaSetTextAreaFocus()
{
    if (_pUiaProvider != nullptr)
    {
        LOG_IF_FAILED(_pUiaProvider->SetTextAreaFocus());
        return S_OK;
    }
    return S_FALSE;
}

void Window::SetOwner()
{
    SetConsoleWindowOwner(_hWnd, nullptr);
}

BOOL Window::GetCursorPosition(_Out_ LPPOINT lpPoint)
{
    return GetCursorPos(lpPoint);
}

BOOL Window::GetClientRectangle(_Out_ LPRECT lpRect)
{
    return GetClientRect(_hWnd, lpRect);
}

int Window::MapPoints(_Inout_updates_(cPoints) LPPOINT lpPoints, _In_ UINT cPoints)
{
    return MapWindowPoints(_hWnd, nullptr, lpPoints, cPoints);
}

BOOL Window::ConvertScreenToClient(_Inout_ LPPOINT lpPoint)
{
    return ScreenToClient(_hWnd, lpPoint);
}
