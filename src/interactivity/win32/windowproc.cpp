// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "Clipboard.hpp"
#include "ConsoleControl.hpp"
#include "find.h"
#include "menu.hpp"
#include "window.hpp"
#include "windowdpiapi.hpp"
#include "windowime.hpp"
#include "windowio.hpp"
#include "windowmetrics.hpp"

#include "..\..\host\_output.h"
#include "..\..\host\output.h"
#include "..\..\host\dbcs.h"
#include "..\..\host\handle.h"
#include "..\..\host\input.h"
#include "..\..\host\misc.h"
#include "..\..\host\registry.hpp"
#include "..\..\host\scrolling.hpp"
#include "..\..\host\srvinit.h"

#include "..\inc\ServiceLocator.hpp"

#include "..\interactivity\win32\windowtheme.hpp"
#include "..\interactivity\win32\CustomWindowMessages.h"

#include "..\interactivity\win32\windowUiaProvider.hpp"

#include <iomanip>
#include <sstream>

using namespace Microsoft::Console::Interactivity::Win32;
using namespace Microsoft::Console::Types;

// The static and specific window procedures for this class are contained here
#pragma region Window Procedure

[[nodiscard]] LRESULT CALLBACK Window::s_ConsoleWindowProc(_In_ HWND hWnd, _In_ UINT Message, _In_ WPARAM wParam, _In_ LPARAM lParam)
{
    // Save the pointer here to the specific window instance when one is created
    if (Message == WM_CREATE)
    {
        const CREATESTRUCT* const pCreateStruct = reinterpret_cast<CREATESTRUCT*>(lParam);

        Window* const pWindow = reinterpret_cast<Window*>(pCreateStruct->lpCreateParams);
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pWindow));
    }

    // Dispatch the message to the specific class instance
    Window* const pWindow = reinterpret_cast<Window*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
    if (pWindow != nullptr)
    {
        return pWindow->ConsoleWindowProc(hWnd, Message, wParam, lParam);
    }

    // If we get this far, call the default window proc
    return DefWindowProcW(hWnd, Message, wParam, lParam);
}

[[nodiscard]] LRESULT CALLBACK Window::ConsoleWindowProc(_In_ HWND hWnd, _In_ UINT Message, _In_ WPARAM wParam, _In_ LPARAM lParam)
{
    Globals& g = ServiceLocator::LocateGlobals();
    CONSOLE_INFORMATION& gci = g.getConsoleInformation();
    LRESULT Status = 0;
    BOOL Unlock = TRUE;

    LockConsole();

    SCREEN_INFORMATION& ScreenInfo = GetScreenInfo();
    if (hWnd == nullptr) // TODO: this might not be possible anymore
    {
        if (Message == WM_CLOSE)
        {
            _CloseWindow();
            Status = 0;
        }
        else
        {
            Status = DefWindowProcW(hWnd, Message, wParam, lParam);
        }

        UnlockConsole();
        return Status;
    }

    switch (Message)
    {
    case WM_CREATE:
    {
        // Load all metrics we'll need.
        _UpdateSystemMetrics();

        // The system is not great and the window rect is wrong the first time for High DPI (WM_DPICHANGED scales strangely.)
        // So here we have to grab the DPI of the current window (now that we have a window).
        // Then we have to re-propose a window size for our window that is scaled to DPI and SetWindowPos.

        // First get the new DPI and update all the scaling factors in the console that are affected.

        // NOTE: GetWindowDpi and/or GetDpiForWindow can be *WRONG* at this point in time depending on monitor configuration.
        //       They won't be correct until the window is actually shown. So instead of using those APIs, figure out the DPI
        //       based on the rectangle that is about to be shown using the nearest monitor.

        // Get proposed window rect from create structure
        CREATESTRUCTW* pcs = (CREATESTRUCTW*)lParam;
        RECT rc;
        rc.left = pcs->x;
        rc.top = pcs->y;
        rc.right = rc.left + pcs->cx;
        rc.bottom = rc.top + pcs->cy;

        // Find nearest montitor.
        HMONITOR hmon = MonitorFromRect(&rc, MONITOR_DEFAULTTONEAREST);

        // This API guarantees that dpix and dpiy will be equal, but neither is an optional parameter so give two UINTs.
        UINT dpix = USER_DEFAULT_SCREEN_DPI;
        UINT dpiy = USER_DEFAULT_SCREEN_DPI;
        GetDpiForMonitor(hmon, MDT_EFFECTIVE_DPI, &dpix, &dpiy); // If this fails, we'll use the default of 96.

        // Pick one and set it to the global DPI.
        ServiceLocator::LocateGlobals().dpi = (int)dpix;

        _UpdateSystemMetrics(); // scroll bars and cursors and such.
        s_ReinitializeFontsForDPIChange(); // font sizes.

        // Now re-propose the window size with the same origin.
        RECT rectProposed = { rc.left, rc.top, 0, 0 };
        _CalculateWindowRect(_pSettings->GetWindowSize(), &rectProposed);

        SetWindowPos(hWnd, NULL, rectProposed.left, rectProposed.top, RECT_WIDTH(&rectProposed), RECT_HEIGHT(&rectProposed), SWP_NOACTIVATE | SWP_NOZORDER);

        // Save the proposed window rect dimensions here so we can adjust if the system comes back and changes them on what we asked for.
        ServiceLocator::LocateWindowMetrics<WindowMetrics>()->ConvertWindowRectToClientRect(&rectProposed);

        break;
    }

    case WM_DROPFILES:
    {
        _HandleDrop(wParam);
        break;
    }

    case WM_GETOBJECT:
    {
        Status = _HandleGetObject(hWnd, wParam, lParam);
        break;
    }

    case WM_DESTROY:
    {
        // signal to uia that they can disconnect our uia provider
        if (_pUiaProvider)
        {
            UiaReturnRawElementProvider(hWnd, 0, 0, NULL);
        }
        break;
    }

    case WM_SIZING:
    {
        // Signal that the user changed the window size, so we can return the value later for telemetry. By only
        // sending the data back if the size has changed, helps reduce the amount of telemetry being sent back.
        // WM_SIZING doesn't fire if they resize the window using Win-UpArrow, so we'll miss that scenario. We could
        // listen to the WM_SIZE message instead, but they can fire when the window is being restored from being
        // minimized, and not only when they resize the window.
        Telemetry::Instance().SetWindowSizeChanged();
        goto CallDefWin;
        break;
    }

    case WM_GETDPISCALEDSIZE:
    {
        // This message will send us the DPI we're about to be changed to.
        // Our goal is to use it to try to figure out the Window Rect that we'll need at that DPI to maintain
        // the same client rendering that we have now.

        // First retrieve the new DPI and the current DPI.
        DWORD const dpiProposed = (WORD)wParam;
        DWORD const dpiCurrent = g.dpi;

        // Now we need to get what the font size *would be* if we had this new DPI. We need to ask the renderer about that.
        const FontInfo& fiCurrent = ScreenInfo.GetCurrentFont();
        FontInfoDesired fiDesired(fiCurrent);
        FontInfo fiProposed(nullptr, 0, 0, { 0, 0 }, 0);

        const HRESULT hr = g.pRender->GetProposedFont(dpiProposed, fiDesired, fiProposed);
        // fiProposal will be updated by the renderer for this new font.
        // GetProposedFont can fail if there's no render engine yet.
        // This can happen if we're headless.
        // Just assume that the font is 1x1 in that case.
        const COORD coordFontProposed = SUCCEEDED(hr) ? fiProposed.GetSize() : COORD({ 1, 1 });

        // Then from that font size, we need to calculate the client area.
        // Then from the client area we need to calculate the window area (using the proposed DPI scalar here as well.)

        // Retrieve the additional parameters we need for the math call based on the current window & buffer properties.
        const Viewport viewport = ScreenInfo.GetViewport();
        COORD coordWindowInChars = viewport.Dimensions();

        const COORD coordBufferSize = ScreenInfo.GetTextBuffer().GetSize().Dimensions();

        // Now call the math calculation for our proposed size.
        RECT rectProposed = { 0 };
        s_CalculateWindowRect(coordWindowInChars, dpiProposed, coordFontProposed, coordBufferSize, hWnd, &rectProposed);

        // Prepare where we're going to keep our final suggestion.
        SIZE* const pSuggestionSize = (SIZE*)lParam;

        pSuggestionSize->cx = RECT_WIDTH(&rectProposed);
        pSuggestionSize->cy = RECT_HEIGHT(&rectProposed);

        // Format our final suggestion for consumption.
        UnlockConsole();
        return TRUE;
    }

    case WM_DPICHANGED:
    {
        _fInDPIChange = true;
        ServiceLocator::LocateGlobals().dpi = HIWORD(wParam);
        _UpdateSystemMetrics();
        s_ReinitializeFontsForDPIChange();

        if (IsInFullscreen())
        {
            // If we're a full screen window, completely ignore what the DPICHANGED says as it will be bigger than the monitor and
            // instead just ensure that the window is still taking up the full screen.
            SetIsFullscreen(true);
        }
        else
        {
            // this is the RECT that the system suggests.
            RECT* const prcNewScale = (RECT*)lParam;
            SetWindowPos(hWnd, HWND_TOP, prcNewScale->left, prcNewScale->top, RECT_WIDTH(prcNewScale), RECT_HEIGHT(prcNewScale), SWP_NOZORDER | SWP_NOACTIVATE);
        }

        _fInDPIChange = false;

        break;
    }

    case WM_ACTIVATE:
    {
        // if we're activated by a mouse click, remember it so
        // we don't pass the click on to the app.
        if (LOWORD(wParam) == WA_CLICKACTIVE)
        {
            gci.Flags |= CONSOLE_IGNORE_NEXT_MOUSE_INPUT;
        }
        goto CallDefWin;
        break;
    }

    case WM_SETFOCUS:
    {
        gci.ProcessHandleList.ModifyConsoleProcessFocus(TRUE);

        gci.Flags |= CONSOLE_HAS_FOCUS;

        gci.GetCursorBlinker().FocusStart();

        HandleFocusEvent(TRUE);

        // ActivateTextServices does nothing if already active so this is OK to be called every focus.
        ActivateTextServices(ServiceLocator::LocateConsoleWindow()->GetWindowHandle(), GetImeSuggestionWindowPos);

        // set the text area to have focus for accessibility consumers
        if (_pUiaProvider)
        {
            LOG_IF_FAILED(_pUiaProvider->SetTextAreaFocus());
        }

        break;
    }

    case WM_KILLFOCUS:
    {
        gci.ProcessHandleList.ModifyConsoleProcessFocus(FALSE);

        gci.Flags &= ~CONSOLE_HAS_FOCUS;

        // turn it off when we lose focus.
        gci.GetActiveOutputBuffer().GetTextBuffer().GetCursor().SetIsOn(false);
        gci.GetCursorBlinker().FocusEnd();

        HandleFocusEvent(FALSE);

        break;
    }

    case WM_PAINT:
    {
        // Since we handle our own minimized window state, we need to
        // check if we're minimized (iconic) and set our internal state flags accordingly.
        // http://msdn.microsoft.com/en-us/library/windows/desktop/dd162483(v=vs.85).aspx
        // NOTE: We will not get called to paint ourselves when minimized because we set an icon when registering the window class.
        //       That means this CONSOLE_IS_ICONIC is unnnecessary when/if we can decouple the drawing with D2D.
        if (IsIconic(hWnd))
        {
            WI_SetFlag(gci.Flags, CONSOLE_IS_ICONIC);
        }
        else
        {
            WI_ClearFlag(gci.Flags, CONSOLE_IS_ICONIC);
        }

        LOG_IF_FAILED(_HandlePaint());

        // NOTE: We cannot let the OS handle this message (meaning do NOT pass to DefWindowProc)
        // or it will cause missing painted regions in scenarios without a DWM (like Core Server SKU).
        // Ensure it is re-validated in this handler so we don't receive infinite WM_PAINTs after
        // we have stored the invalid region data for the next trip around the renderer thread.

        break;
    }

    case WM_ERASEBKGND:
    {
        break;
    }

    case WM_CLOSE:
    {
        // Write the final trace log during the WM_CLOSE message while the console process is still fully alive.
        // This gives us time to query the process for information.  We shouldn't really miss any useful
        // telemetry between now and when the process terminates.
        Telemetry::Instance().WriteFinalTraceLog();

        _CloseWindow();
        break;
    }

    case WM_SETTINGCHANGE:
    {
        try
        {
            WindowTheme theme;
            LOG_IF_FAILED(theme.TrySetDarkMode(hWnd));
        }
        CATCH_LOG();

        gci.GetCursorBlinker().SettingsChanged();
    }
        __fallthrough;

    case WM_DISPLAYCHANGE:
    {
        _UpdateSystemMetrics();
        break;
    }

    case WM_WINDOWPOSCHANGING:
    {
        // Enforce maximum size here instead of WM_GETMINMAXINFO.
        // If we return it in WM_GETMINMAXINFO, then it will be enforced when snapping across DPI boundaries (bad.)

        // Retrieve the suggested dimensions and make a rect and size.
        LPWINDOWPOS lpwpos = (LPWINDOWPOS)lParam;

        // We only need to apply restrictions if the size is changing.
        if (!WI_IsFlagSet(lpwpos->flags, SWP_NOSIZE))
        {
            // Figure out the suggested dimensions
            RECT rcSuggested;
            rcSuggested.left = lpwpos->x;
            rcSuggested.top = lpwpos->y;
            rcSuggested.right = rcSuggested.left + lpwpos->cx;
            rcSuggested.bottom = rcSuggested.top + lpwpos->cy;
            SIZE szSuggested;
            szSuggested.cx = RECT_WIDTH(&rcSuggested);
            szSuggested.cy = RECT_HEIGHT(&rcSuggested);

            // Figure out the current dimensions for comparison.
            RECT rcCurrent = GetWindowRect();

            // Determine whether we're being resized by someone dragging the edge or completely moved around.
            bool fIsEdgeResize = false;
            {
                // We can only be edge resizing if our existing rectangle wasn't empty. If it was empty, we're doing the initial create.
                if (!IsRectEmpty(&rcCurrent))
                {
                    // If one or two sides are changing, we're being edge resized.
                    unsigned int cSidesChanging = 0;
                    if (rcCurrent.left != rcSuggested.left)
                    {
                        cSidesChanging++;
                    }
                    if (rcCurrent.right != rcSuggested.right)
                    {
                        cSidesChanging++;
                    }
                    if (rcCurrent.top != rcSuggested.top)
                    {
                        cSidesChanging++;
                    }
                    if (rcCurrent.bottom != rcSuggested.bottom)
                    {
                        cSidesChanging++;
                    }

                    if (cSidesChanging == 1 || cSidesChanging == 2)
                    {
                        fIsEdgeResize = true;
                    }
                }
            }

            // If the window is maximized, let it do whatever it wants to do.
            // If not, then restrict it to our maximum possible window.
            if (!WI_IsFlagSet(GetWindowStyle(hWnd), WS_MAXIMIZE))
            {
                // Find the related monitor, the maximum pixel size,
                // and the dpi for the suggested rect.
                UINT dpiOfMaximum;
                RECT rcMaximum;

                if (fIsEdgeResize)
                {
                    // If someone's dragging from the edge to resize in one direction, we want to make sure we never grow past the current monitor.
                    rcMaximum = ServiceLocator::LocateWindowMetrics<WindowMetrics>()->GetMaxWindowRectInPixels(&rcCurrent, &dpiOfMaximum);
                }
                else
                {
                    // In other circumstances, assume we're snapping around or some other jump (TS).
                    // Just do whatever we're told using the new suggestion as the restriction monitor.
                    rcMaximum = ServiceLocator::LocateWindowMetrics<WindowMetrics>()->GetMaxWindowRectInPixels(&rcSuggested, &dpiOfMaximum);
                }

                // Only apply the maximum size restriction if the current DPI matches the DPI of the
                // maximum rect. This keeps us from applying the wrong restriction if the monitor
                // we're moving to has a different DPI but we've yet to get notified of that DPI
                // change. If we do apply it, then we'll restrict the console window BEFORE its
                // been resized for the DPI change, so we're likely to shrink the window too much
                // or worse yet, keep it from moving entirely. We'll get a WM_DPICHANGED,
                // resize the window, and then process the restriction in a few window messages.
                if (((int)dpiOfMaximum == g.dpi) &&
                    ((szSuggested.cx > RECT_WIDTH(&rcMaximum)) || (szSuggested.cy > RECT_HEIGHT(&rcMaximum))))
                {
                    lpwpos->cx = std::min(RECT_WIDTH(&rcMaximum), szSuggested.cx);
                    lpwpos->cy = std::min(RECT_HEIGHT(&rcMaximum), szSuggested.cy);

                    // We usually add SWP_NOMOVE so that if the user is dragging the left or top edge
                    // and hits the restriction, then the window just stops growing, it doesn't
                    // move with the mouse. However during DPI changes, we need to allow a move
                    // because the RECT from WM_DPICHANGED has been specially crafted by win32k
                    // to keep the mouse cursor from jumping away from the caption bar.
                    if (!_fInDPIChange)
                    {
                        lpwpos->flags |= SWP_NOMOVE;
                    }
                }
            }

            break;
        }
        else
        {
            goto CallDefWin;
        }
    }

    case WM_WINDOWPOSCHANGED:
    {
        // Only handle this if the DPI is the same as last time.
        // If the DPI is different, assume we're about to get a DPICHANGED notification
        // which will have a better suggested rectangle than this one.
        // NOTE: This stopped being possible in RS4 as the DPI now changes when and only when
        // we receive WM_DPICHANGED. We keep this check around so that we perform better downlevel.
        int const dpi = ServiceLocator::LocateHighDpiApi<WindowDpiApi>()->GetWindowDPI(hWnd);
        if (dpi == ServiceLocator::LocateGlobals().dpi)
        {
            _HandleWindowPosChanged(lParam);
        }
        break;
    }

    case WM_CONTEXTMENU:
    {
        Telemetry::Instance().SetContextMenuUsed();
        if (DefWindowProcW(hWnd, WM_NCHITTEST, 0, lParam) == HTCLIENT)
        {
            HMENU hHeirMenu = Menu::s_GetHeirMenuHandle();

            Unlock = FALSE;
            UnlockConsole();

            TrackPopupMenuEx(hHeirMenu,
                             TPM_RIGHTBUTTON | (GetSystemMetrics(SM_MENUDROPALIGNMENT) == 0 ? TPM_LEFTALIGN : TPM_RIGHTALIGN),
                             GET_X_LPARAM(lParam),
                             GET_Y_LPARAM(lParam),
                             hWnd,
                             nullptr);
        }
        else
        {
            goto CallDefWin;
        }
        break;
    }

    case WM_NCLBUTTONDOWN:
    {
        // allow user to move window even when bigger than the screen
        switch (wParam & 0x00FF)
        {
        case HTCAPTION:
            UnlockConsole();
            Unlock = FALSE;
            SetActiveWindow(hWnd);
            SendMessageTimeoutW(hWnd, WM_SYSCOMMAND, SC_MOVE | wParam, lParam, SMTO_NORMAL, INFINITE, nullptr);
            break;
        default:
            goto CallDefWin;
        }
        break;
    }

    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_CHAR:
    case WM_DEADCHAR:
    {
        HandleKeyEvent(hWnd, Message, wParam, lParam, &Unlock);
        break;
    }

    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
    case WM_SYSCHAR:
    case WM_SYSDEADCHAR:
    {
        if (HandleSysKeyEvent(hWnd, Message, wParam, lParam, &Unlock))
        {
            goto CallDefWin;
        }
        break;
    }

    case WM_COMMAND:
        // If this is an edit command from the context menu, treat it like a sys command.
        if ((wParam < ID_CONSOLE_COPY) || (wParam > ID_CONSOLE_SELECTALL))
        {
            break;
        }

        __fallthrough;

    case WM_SYSCOMMAND:
        if (wParam == ID_CONSOLE_MARK)
        {
            Selection::Instance().InitializeMarkSelection();
        }
        else if (wParam == ID_CONSOLE_COPY)
        {
            Clipboard::Instance().Copy();
        }
        else if (wParam == ID_CONSOLE_PASTE)
        {
            Clipboard::Instance().Paste();
        }
        else if (wParam == ID_CONSOLE_SCROLL)
        {
            Scrolling::s_DoScroll();
        }
        else if (wParam == ID_CONSOLE_FIND)
        {
            DoFind();
            Unlock = FALSE;
        }
        else if (wParam == ID_CONSOLE_SELECTALL)
        {
            Selection::Instance().SelectAll();
        }
        else if (wParam == ID_CONSOLE_CONTROL)
        {
            Menu::s_ShowPropertiesDialog(hWnd, FALSE);
        }
        else if (wParam == ID_CONSOLE_DEFAULTS)
        {
            Menu::s_ShowPropertiesDialog(hWnd, TRUE);
        }
        else
        {
            goto CallDefWin;
        }
        break;

    case WM_HSCROLL:
    {
        HorizontalScroll(LOWORD(wParam), HIWORD(wParam));
        break;
    }

    case WM_VSCROLL:
    {
        VerticalScroll(LOWORD(wParam), HIWORD(wParam));
        break;
    }

    case WM_INITMENU:
    {
        HandleMenuEvent(WM_INITMENU);
        Menu::Instance()->Initialize();
        break;
    }

    case WM_MENUSELECT:
    {
        if (HIWORD(wParam) == 0xffff)
        {
            HandleMenuEvent(WM_MENUSELECT);
        }
        break;
    }

    case WM_MOUSEMOVE:
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_LBUTTONDBLCLK:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
    case WM_RBUTTONDBLCLK:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
    case WM_MBUTTONDBLCLK:
    case WM_MOUSEWHEEL:
    case WM_MOUSEHWHEEL:
    {
        if (HandleMouseEvent(ScreenInfo, Message, wParam, lParam))
        {
            if (Message != WM_MOUSEWHEEL && Message != WM_MOUSEHWHEEL)
            {
                goto CallDefWin;
            }
        }
        else
        {
            break;
        }

        // Don't handle zoom.
        if (wParam & MK_CONTROL)
        {
            goto CallDefWin;
        }

        Status = 1;

        bool isMouseWheel = Message == WM_MOUSEWHEEL;
        bool isMouseHWheel = Message == WM_MOUSEHWHEEL;

        if (isMouseWheel || isMouseHWheel)
        {
            short wheelDelta = (short)HIWORD(wParam);
            bool hasShift = (wParam & MK_SHIFT) ? true : false;

            Scrolling::s_HandleMouseWheel(isMouseWheel,
                                          isMouseHWheel,
                                          wheelDelta,
                                          hasShift,
                                          ScreenInfo);
        }
        break;
    }

    case CM_SET_WINDOW_SIZE:
    {
        Status = _InternalSetWindowSize();
        break;
    }

    case CM_BEEP:
    {
        UnlockConsole();
        Unlock = FALSE;

        // Don't fall back to Beep() on win32 systems -- if the user configures their system for no sound, we should
        // respect that.
        PlaySoundW((LPCWSTR)SND_ALIAS_SYSTEMHAND, nullptr, SND_ALIAS_ID | SND_ASYNC | SND_SENTRY);
        break;
    }

    case CM_UPDATE_SCROLL_BARS:
    {
        ScreenInfo.InternalUpdateScrollBars();
        break;
    }

    case CM_UPDATE_TITLE:
    {
        SetWindowTextW(hWnd, gci.GetTitleAndPrefix().c_str());
        break;
    }

    case CM_UPDATE_EDITKEYS:
    {
        // Re-read the edit key settings from registry.
        Registry reg(&gci);
        reg.GetEditKeys(NULL);
        break;
    }

#ifdef DBG
    case CM_SET_KEY_STATE:
    {
        const int keyboardInputTableStateSize = 256;
        if (wParam < keyboardInputTableStateSize)
        {
            BYTE keyState[keyboardInputTableStateSize];
            GetKeyboardState(keyState);
            keyState[wParam] = static_cast<BYTE>(lParam);
            SetKeyboardState(keyState);
        }
        else
        {
            LOG_HR_MSG(E_INVALIDARG, "CM_SET_KEY_STATE invalid wParam");
        }
        break;
    }

    case CM_SET_KEYBOARD_LAYOUT:
    {
        try
        {
            std::wstringstream wss;
            wss << std::setfill(L'0') << std::setw(8) << wParam;
            std::wstring wstr(wss.str());
            LoadKeyboardLayout(wstr.c_str(), KLF_ACTIVATE);
        }
        catch (...)
        {
            LOG_HR_MSG(wil::ResultFromCaughtException(), "CM_SET_KEYBOARD_LAYOUT exception");
        }
        break;
    }
#endif DBG

    case EVENT_CONSOLE_CARET:
    case EVENT_CONSOLE_UPDATE_REGION:
    case EVENT_CONSOLE_UPDATE_SIMPLE:
    case EVENT_CONSOLE_UPDATE_SCROLL:
    case EVENT_CONSOLE_LAYOUT:
    case EVENT_CONSOLE_START_APPLICATION:
    case EVENT_CONSOLE_END_APPLICATION:
    {
        NotifyWinEvent(Message, hWnd, (LONG)wParam, (LONG)lParam);
        break;
    }

    default:
    CallDefWin:
    {
        if (Unlock)
        {
            UnlockConsole();
            Unlock = FALSE;
        }
        Status = DefWindowProcW(hWnd, Message, wParam, lParam);
        break;
    }
    }

    if (Unlock)
    {
        UnlockConsole();
    }

    return Status;
}

#pragma endregion

// Helper handler methods for specific cases within the large window procedure are in this section
#pragma region Message Handlers

void Window::_HandleWindowPosChanged(const LPARAM lParam)
{
    HWND hWnd = GetWindowHandle();
    SCREEN_INFORMATION& ScreenInfo = GetScreenInfo();

    LPWINDOWPOS const lpWindowPos = (LPWINDOWPOS)lParam;

    // If the frame changed, update the system metrics.
    if (WI_IsFlagSet(lpWindowPos->flags, SWP_FRAMECHANGED))
    {
        _UpdateSystemMetrics();
    }

    // This message is sent as the result of someone calling SetWindowPos(). We use it here to set/clear the
    // CONSOLE_IS_ICONIC bit appropriately. doing so in the WM_SIZE handler is incorrect because the WM_SIZE
    // comes after the WM_ERASEBKGND during SetWindowPos() processing, and the WM_ERASEBKGND needs to know if
    // the console window is iconic or not.
    if (!ScreenInfo.ResizingWindow && (lpWindowPos->cx || lpWindowPos->cy) && !IsIconic(hWnd))
    {
        // calculate the dimensions for the newly proposed window rectangle
        RECT rcNew;
        s_ConvertWindowPosToWindowRect(lpWindowPos, &rcNew);
        ServiceLocator::LocateWindowMetrics<WindowMetrics>()->ConvertWindowRectToClientRect(&rcNew);

        // If the window is not being resized, including a DPI change, then
        // don't do anything except update our windowrect
        if (!WI_IsFlagSet(lpWindowPos->flags, SWP_NOSIZE) || _fInDPIChange)
        {
            ScreenInfo.ProcessResizeWindow(&rcNew, &_rcClientLast);
        }

        // now that operations are complete, save the new rectangle size as the last seen value
        _rcClientLast = rcNew;
    }
}

// Routine Description:
// - This helper method for the window procedure will handle the WM_PAINT message
// - It will retrieve the invalid rectangle and dispatch that information to the attached renderer
//   (if available). It will then attempt to validate/finalize the paint to appease the system
//   and prevent more WM_PAINTs from coming back (until of course something else causes an invalidation).
// Arguments:
// - <none>
// Return Value:
// - S_OK if we succeeded. ERROR_INVALID_HANDLE if there is no HWND. E_FAIL if GDI failed for some reason.
[[nodiscard]] HRESULT Window::_HandlePaint() const
{
    HWND const hwnd = GetWindowHandle();
    RETURN_HR_IF_NULL(HRESULT_FROM_WIN32(ERROR_INVALID_HANDLE), hwnd);

    // We have to call BeginPaint to retrieve the invalid rectangle state
    // BeginPaint/EndPaint does a bunch of other magic in the system level
    // that we can't sufficiently replicate with GetInvalidRect/ValidateRect.
    // ---
    // We've tried in the past to not call BeginPaint/EndPaint
    // and under certain circumstances (windows with SW_HIDE, SKUs without DWM, etc.)
    // the system either sends WM_PAINT messages ad nauseum or fails to redraw everything correctly.
    PAINTSTRUCT ps;
    HDC const hdc = BeginPaint(hwnd, &ps);
    RETURN_HR_IF_NULL(E_FAIL, hdc);

    if (ServiceLocator::LocateGlobals().pRender != nullptr)
    {
        // In lieu of actually painting right now, we're just going to aggregate this information in the renderer
        // and let it paint whenever it feels appropriate.
        RECT const rcUpdate = ps.rcPaint;
        ServiceLocator::LocateGlobals().pRender->TriggerSystemRedraw(&rcUpdate);
    }

    LOG_IF_WIN32_BOOL_FALSE(EndPaint(hwnd, &ps));

    return S_OK;
}

// Routine Description:
// - This routine is called when ConsoleWindowProc receives a WM_DROPFILES message.
// - It initially calls DragQueryFile() to calculate the number of files dropped and then DragQueryFile() is called to retrieve the filename.
// - DoStringPaste() pastes the filename to the console window
// Arguments:
// - wParam  - Identifies the structure containing the filenames of the dropped files.
// - Console - Pointer to CONSOLE_INFORMATION structure
// Return Value:
// - <none>
void Window::_HandleDrop(const WPARAM wParam) const
{
    WCHAR szPath[MAX_PATH];
    BOOL fAddQuotes;

    if (DragQueryFile((HDROP)wParam, 0, szPath, ARRAYSIZE(szPath)) != 0)
    {
        // Log a telemetry flag saying the user interacted with the Console
        // Only log when DragQueryFile succeeds, because if we don't when the console starts up, we're seeing
        // _HandleDrop get called multiple times (and DragQueryFile fail),
        // which can incorrectly mark this console session as interactive.
        Telemetry::Instance().SetUserInteractive();

        fAddQuotes = (wcschr(szPath, L' ') != nullptr);
        if (fAddQuotes)
        {
            Clipboard::Instance().StringPaste(L"\"", 1);
        }

        Clipboard::Instance().StringPaste(szPath, wcslen(szPath));

        if (fAddQuotes)
        {
            Clipboard::Instance().StringPaste(L"\"", 1);
        }
    }
}

[[nodiscard]] LRESULT Window::_HandleGetObject(const HWND hwnd, const WPARAM wParam, const LPARAM lParam)
{
    LRESULT retVal = 0;

    // If we are receiving a request from Microsoft UI Automation framework, then return the basic UIA COM interface.
    if (static_cast<long>(lParam) == static_cast<long>(UiaRootObjectId))
    {
        // NOTE: Deliverable MSFT: 10881045 is required before this will work properly.
        // The UIAutomationCore.dll cannot currently handle the fact that our HWND is assigned to the child PID.
        // It will attempt to set up events/pipes on the wrong PID/HWND combination when called here.
        // A temporary workaround until that is delivered is to disable window handle reparenting using
        // ConsoleControl's ConsoleSetWindowOwner call.
        retVal = UiaReturnRawElementProvider(hwnd, wParam, lParam, _GetUiaProvider());
    }
    // Otherwise, return 0. We don't implement MS Active Accessibility (the other framework that calls WM_GETOBJECT).

    return retVal;
}

#pragma endregion

// Dispatchers are used to post or send a window message into the queue from other portions of the codebase without accessing internal properties directly
#pragma region Dispatchers

BOOL Window::PostUpdateWindowSize() const
{
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    const SCREEN_INFORMATION& ScreenInfo = GetScreenInfo();

    if (ScreenInfo.ConvScreenInfo != nullptr)
    {
        return FALSE;
    }

    if (gci.Flags & CONSOLE_SETTING_WINDOW_SIZE)
    {
        return FALSE;
    }

    gci.Flags |= CONSOLE_SETTING_WINDOW_SIZE;
    return PostMessageW(GetWindowHandle(), CM_SET_WINDOW_SIZE, (WPARAM)&ScreenInfo, 0);
}

BOOL Window::SendNotifyBeep() const
{
    return SendNotifyMessageW(GetWindowHandle(), CM_BEEP, 0, 0);
}

BOOL Window::PostUpdateScrollBars() const
{
    return PostMessageW(GetWindowHandle(), CM_UPDATE_SCROLL_BARS, (WPARAM)&GetScreenInfo(), 0);
}

BOOL Window::PostUpdateExtendedEditKeys() const
{
    return PostMessageW(GetWindowHandle(), CM_UPDATE_EDITKEYS, 0, 0);
}

#pragma endregion
