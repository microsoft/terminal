// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "window.hpp"

#include "clipboard.hpp"
#include "find.h"
#include "menu.hpp"
#include "windowdpiapi.hpp"
#include "windowio.hpp"
#include "windowmetrics.hpp"
#include "../../host/handle.h"
#include "../../host/registry.hpp"
#include "../../host/scrolling.hpp"
#include "../../inc/conint.h"
#include "../inc/ServiceLocator.hpp"
#include "../interactivity/win32/CustomWindowMessages.h"
#include "../interactivity/win32/windowUiaProvider.hpp"

using namespace Microsoft::Console::Interactivity::Win32;
using namespace Microsoft::Console::Types;

// NOTE: We put this struct into a `static constexpr` (= ".rodata", read-only data segment), which means it
// cannot have any mutable members right now. If you need any, you have to make it a non-const `static`.
struct TsfDataProvider : Microsoft::Console::TSF::IDataProvider
{
    virtual ~TsfDataProvider() = default;

    STDMETHODIMP QueryInterface(REFIID, void**) noexcept override
    {
        return E_NOTIMPL;
    }

    ULONG STDMETHODCALLTYPE AddRef() noexcept override
    {
        return 1;
    }

    ULONG STDMETHODCALLTYPE Release() noexcept override
    {
        return 1;
    }

    HWND GetHwnd() override
    {
        return Microsoft::Console::Interactivity::ServiceLocator::LocateConsoleWindow()->GetWindowHandle();
    }

    RECT GetViewport() override
    {
        const auto hwnd = GetHwnd();

        RECT rc;
        GetClientRect(hwnd, &rc);

        // https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getclientrect
        // > The left and top members are zero. The right and bottom members contain the width and height of the window.
        // --> We can turn the client rect into a screen-relative rect by adding the left/top position.
        ClientToScreen(hwnd, reinterpret_cast<POINT*>(&rc));
        rc.right += rc.left;
        rc.bottom += rc.top;

        return rc;
    }

    RECT GetCursorPosition() override
    {
        const auto& gci = Microsoft::Console::Interactivity::ServiceLocator::LocateGlobals().getConsoleInformation();
        const auto& screenBuffer = gci.GetActiveOutputBuffer();

        // Map the absolute cursor position to a viewport-relative one.
        const auto viewport = screenBuffer.GetViewport().ToExclusive();
        auto coordCursor = screenBuffer.GetTextBuffer().GetCursor().GetPosition();
        coordCursor.x -= viewport.left;
        coordCursor.y -= viewport.top;

        coordCursor.x = std::clamp(coordCursor.x, 0, viewport.width() - 1);
        coordCursor.y = std::clamp(coordCursor.y, 0, viewport.height() - 1);

        // Convert from columns/rows to pixels.
        const auto coordFont = screenBuffer.GetCurrentFont().GetSize();
        POINT ptSuggestion{
            .x = coordCursor.x * coordFont.width,
            .y = coordCursor.y * coordFont.height,
        };

        ClientToScreen(GetHwnd(), &ptSuggestion);

        return {
            .left = ptSuggestion.x,
            .top = ptSuggestion.y,
            .right = ptSuggestion.x + coordFont.width,
            .bottom = ptSuggestion.y + coordFont.height,
        };
    }

    void HandleOutput(std::wstring_view text) override
    {
        auto& gci = Microsoft::Console::Interactivity::ServiceLocator::LocateGlobals().getConsoleInformation();
        gci.LockConsole();
        const auto unlock = wil::scope_exit([&] { gci.UnlockConsole(); });
        gci.GetActiveInputBuffer()->WriteString(text);
    }

    Microsoft::Console::Render::Renderer* GetRenderer() override
    {
        auto& g = Microsoft::Console::Interactivity::ServiceLocator::LocateGlobals();
        return g.pRender;
    }
};

static constexpr TsfDataProvider s_tsfDataProvider;

// The static and specific window procedures for this class are contained here
#pragma region Window Procedure

[[nodiscard]] LRESULT CALLBACK Window::s_ConsoleWindowProc(_In_ HWND hWnd, _In_ UINT Message, _In_ WPARAM wParam, _In_ LPARAM lParam)
{
    // Save the pointer here to the specific window instance when one is created
    if (Message == WM_CREATE)
    {
        const CREATESTRUCT* const pCreateStruct = reinterpret_cast<CREATESTRUCT*>(lParam);

        const auto pWindow = reinterpret_cast<Window*>(pCreateStruct->lpCreateParams);
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pWindow));
    }

    // Dispatch the message to the specific class instance
    const auto pWindow = reinterpret_cast<Window*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
    if (pWindow != nullptr)
    {
        return pWindow->ConsoleWindowProc(hWnd, Message, wParam, lParam);
    }

    // If we get this far, call the default window proc
    return DefWindowProcW(hWnd, Message, wParam, lParam);
}

[[nodiscard]] LRESULT CALLBACK Window::ConsoleWindowProc(_In_ HWND hWnd, _In_ UINT Message, _In_ WPARAM wParam, _In_ LPARAM lParam)
{
    auto& g = ServiceLocator::LocateGlobals();
    auto& gci = g.getConsoleInformation();
    LRESULT Status = 0;
    auto Unlock = TRUE;

    LockConsole();

    auto& ScreenInfo = GetScreenInfo();
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

        // NOTE: GetDpiForWindow can be *WRONG* at this point in time depending on monitor configuration.
        //       They won't be correct until the window is actually shown. So instead of using those APIs, figure out the DPI
        //       based on the rectangle that is about to be shown using the nearest monitor.

        // Get proposed window rect from create structure
        auto pcs = (CREATESTRUCTW*)lParam;
        RECT rc;
        rc.left = pcs->x;
        rc.top = pcs->y;
        rc.right = rc.left + pcs->cx;
        rc.bottom = rc.top + pcs->cy;

        // Find nearest monitor.
        auto hmon = MonitorFromRect(&rc, MONITOR_DEFAULTTONEAREST);

        // This API guarantees that dpix and dpiy will be equal, but neither is an optional parameter so give two UINTs.
        UINT dpix = USER_DEFAULT_SCREEN_DPI;
        UINT dpiy = USER_DEFAULT_SCREEN_DPI;
        GetDpiForMonitor(hmon, MDT_EFFECTIVE_DPI, &dpix, &dpiy); // If this fails, we'll use the default of 96.

        // Pick one and set it to the global DPI.
        ServiceLocator::LocateGlobals().dpi = (int)dpix;

        _UpdateSystemMetrics(); // scroll bars and cursors and such.
        s_ReinitializeFontsForDPIChange(); // font sizes.

        // Now re-propose the window size with the same origin.
        til::rect rectProposed = { rc.left, rc.top, 0, 0 };
        _CalculateWindowRect(_pSettings->GetWindowSize(), &rectProposed);

        SetWindowPos(hWnd, nullptr, rectProposed.left, rectProposed.top, rectProposed.width(), rectProposed.height(), SWP_NOACTIVATE | SWP_NOZORDER);

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
            UiaReturnRawElementProvider(hWnd, 0, 0, nullptr);
        }
        break;
    }

    case WM_SIZING:
    {
        goto CallDefWin;
        break;
    }

    case WM_GETDPISCALEDSIZE:
    {
        const auto result = _HandleGetDpiScaledSize(
            static_cast<WORD>(wParam), reinterpret_cast<SIZE*>(lParam));
        UnlockConsole();
        return result;
    }

    case WM_DPICHANGED:
    {
        _fInDPIChange = true;
        ServiceLocator::LocateGlobals().dpi = HIWORD(wParam);
        _UpdateSystemMetrics();
        s_ReinitializeFontsForDPIChange();

        // This is the RECT that the system suggests.
        const auto prcNewScale = (RECT*)lParam;
        SetWindowPos(hWnd,
                     HWND_TOP,
                     prcNewScale->left,
                     prcNewScale->top,
                     RECT_WIDTH(prcNewScale),
                     RECT_HEIGHT(prcNewScale),
                     SWP_NOZORDER | SWP_NOACTIVATE);

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

        if (const auto renderer = ServiceLocator::LocateGlobals().pRender)
        {
            renderer->AllowCursorVisibility(Render::InhibitionSource::Host, true);
        }

        if (!g.tsf)
        {
            g.tsf = TSF::Handle::Create();
            g.tsf.AssociateFocus(const_cast<TsfDataProvider*>(&s_tsfDataProvider));
        }

        // set the text area to have focus for accessibility consumers
        if (_pUiaProvider)
        {
            LOG_IF_FAILED(_pUiaProvider->SetTextAreaFocus());
        }

        HandleFocusEvent(TRUE);
        break;
    }

    case WM_KILLFOCUS:
    {
        gci.ProcessHandleList.ModifyConsoleProcessFocus(FALSE);
        gci.Flags &= ~CONSOLE_HAS_FOCUS;

        if (const auto renderer = ServiceLocator::LocateGlobals().pRender)
        {
            renderer->AllowCursorVisibility(Render::InhibitionSource::Host, false);
        }

        HandleFocusEvent(FALSE);
        break;
    }

    case WM_PAINT:
    {
        // Since we handle our own minimized window state, we need to
        // check if we're minimized (iconic) and set our internal state flags accordingly.
        // http://msdn.microsoft.com/en-us/library/windows/desktop/dd162483(v=vs.85).aspx
        // NOTE: We will not get called to paint ourselves when minimized because we set an icon when registering the window class.
        //       That means this CONSOLE_IS_ICONIC is unnecessary when/if we can decouple the drawing with D2D.
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
        _CloseWindow();
        break;
    }

    case WM_SETTINGCHANGE:
    {
        LOG_IF_FAILED(Microsoft::Console::Internal::Theming::TrySetDarkMode(hWnd));
        gci.renderData.UpdateSystemMetrics();
        __fallthrough;
    }

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
        auto lpwpos = (LPWINDOWPOS)lParam;

        // We only need to apply restrictions if the size is changing.
        if (!WI_IsFlagSet(lpwpos->flags, SWP_NOSIZE))
        {
            // Figure out the suggested dimensions
            til::rect rcSuggested;
            rcSuggested.left = lpwpos->x;
            rcSuggested.top = lpwpos->y;
            rcSuggested.right = rcSuggested.left + lpwpos->cx;
            rcSuggested.bottom = rcSuggested.top + lpwpos->cy;
            til::size szSuggested;
            szSuggested.width = rcSuggested.width();
            szSuggested.height = rcSuggested.height();

            // Figure out the current dimensions for comparison.
            auto rcCurrent = GetWindowRect();

            // Determine whether we're being resized by someone dragging the edge or completely moved around.
            auto fIsEdgeResize = false;
            {
                // We can only be edge resizing if our existing rectangle wasn't empty. If it was empty, we're doing the initial create.
                if (rcCurrent)
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
                til::rect rcMaximum;

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
                    ((szSuggested.width > rcMaximum.width()) || (szSuggested.height > rcMaximum.height())))
                {
                    lpwpos->cx = std::min(rcMaximum.width(), szSuggested.width);
                    lpwpos->cy = std::min(rcMaximum.height(), szSuggested.height);

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
        const auto dpi = ServiceLocator::LocateHighDpiApi<WindowDpiApi>()->GetDpiForWindow(hWnd);
        if (dpi == ServiceLocator::LocateGlobals().dpi)
        {
            _HandleWindowPosChanged(lParam);
        }
        break;
    }

    case WM_CONTEXTMENU:
    {
        if (DefWindowProcW(hWnd, WM_NCHITTEST, 0, lParam) == HTCLIENT)
        {
            auto hHeirMenu = Menu::s_GetHeirMenuHandle();

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
        else if (wParam == SC_RESTORE && _fIsInFullscreen)
        {
            SetIsFullscreen(false);
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

        auto isMouseWheel = Message == WM_MOUSEWHEEL;
        auto isMouseHWheel = Message == WM_MOUSEHWHEEL;

        if (isMouseWheel || isMouseHWheel)
        {
            auto wheelDelta = (short)HIWORD(wParam);
            auto hasShift = (wParam & MK_SHIFT) ? true : false;

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
        const auto state = ScreenInfo.FetchScrollBarState();

        // EnableScrollbar() and especially SetScrollInfo() are prohibitively expensive functions nowadays.
        // Unlocking early here improves throughput of good old `type` in cmd.exe by ~10x.
        UnlockConsole();
        Unlock = FALSE;

        _resizingWindow++;
        UpdateScrollBars(state);
        _resizingWindow--;
        break;
    }

    case CM_UPDATE_TITLE:
    {
        // SetWindowTextW needs null terminated string so assign view to string.
        const std::wstring titleAndPrefix{ gci.GetTitleAndPrefix() };

        SetWindowTextW(hWnd, titleAndPrefix.c_str());
        break;
    }

    case CM_UPDATE_EDITKEYS:
    {
        // Re-read the edit key settings from registry.
        Registry reg(&gci);
        reg.GetEditKeys(nullptr);
        break;
    }

#ifdef DBG
    case CM_SET_KEY_STATE:
    {
        const auto keyboardInputTableStateSize = 256;
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
            const auto wstr = fmt::format(FMT_COMPILE(L"{:08d}"), wParam);
            LoadKeyboardLayout(wstr.c_str(), KLF_ACTIVATE);
        }
        catch (...)
        {
            LOG_HR_MSG(wil::ResultFromCaughtException(), "CM_SET_KEYBOARD_LAYOUT exception");
        }
        break;
    }
#endif // DBG

    case CM_UPDATE_CLIPBOARD:
    {
        if (const auto clipboardText = gci.UsePendingClipboardText())
        {
            Clipboard::Instance().CopyText(clipboardText.value());
        }
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
    auto hWnd = GetWindowHandle();
    auto& ScreenInfo = GetScreenInfo();

    const auto lpWindowPos = (LPWINDOWPOS)lParam;

    // If the frame changed, update the system metrics.
    if (WI_IsFlagSet(lpWindowPos->flags, SWP_FRAMECHANGED))
    {
        _UpdateSystemMetrics();
    }

    // This message is sent as the result of someone calling SetWindowPos(). We use it here to set/clear the
    // CONSOLE_IS_ICONIC bit appropriately. doing so in the WM_SIZE handler is incorrect because the WM_SIZE
    // comes after the WM_ERASEBKGND during SetWindowPos() processing, and the WM_ERASEBKGND needs to know if
    // the console window is iconic or not.
    if (!_resizingWindow && (lpWindowPos->cx || lpWindowPos->cy) && !IsIconic(hWnd))
    {
        // calculate the dimensions for the newly proposed window rectangle
        til::rect rcNew;
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

// WM_GETDPISCALEDSIZE is sent prior to the window changing DPI, allowing us to
// choose the size at the new DPI (overriding the default, linearly scaled).
//
// This is used to keep the rows and columns from changing when the DPI changes.
LRESULT Window::_HandleGetDpiScaledSize(UINT dpiNew, _Inout_ SIZE* pSizeNew) const
{
    // Get the current DPI and font size.
    HWND hwnd = GetWindowHandle();
    UINT dpiCurrent = ServiceLocator::LocateHighDpiApi<WindowDpiApi>()->GetDpiForWindow(hwnd);
    const auto& fontInfoCurrent = GetScreenInfo().GetCurrentFont();
    til::size fontSizeCurrent = fontInfoCurrent.GetSize();

    // Scale the current font to the new DPI and get the new font size.
    FontInfoDesired fontInfoDesired(fontInfoCurrent);
    FontInfo fontInfoNew(L"", 0, 0, { 0, 0 }, 0);
    if (!SUCCEEDED(ServiceLocator::LocateGlobals().pRender->GetProposedFont(
            dpiNew, fontInfoDesired, fontInfoNew)))
    {
        // On failure, return FALSE, which scales the window linearly for DPI.
        return FALSE;
    }
    til::size fontSizeNew = fontInfoNew.GetSize();

    // The provided size is the window rect, which includes non-client area
    // (caption bars, resize borders, scroll bars, etc). We want to scale the
    // client area separately from the non-client area. The client area will be
    // scaled using the new/old font sizes, so that the size of the grid (rows/
    // columns) does not change.

    // Subtract the size of the window's current non-client area from the
    // provided size. This gives us the new client area size at the previous DPI.
    til::rect rc;
    s_ExpandRectByNonClientSize(hwnd, dpiCurrent, &rc);
    pSizeNew->cx -= rc.width();
    pSizeNew->cy -= rc.height();

    // Scale the size of the client rect by the new/old font sizes.
    pSizeNew->cx = MulDiv(pSizeNew->cx, fontSizeNew.width, fontSizeCurrent.width);
    pSizeNew->cy = MulDiv(pSizeNew->cy, fontSizeNew.height, fontSizeCurrent.height);

    // Add the size of the non-client area at the new DPI to the final size,
    // getting the new window rect (the output of this function).
    rc = { 0, 0, pSizeNew->cx, pSizeNew->cy };
    s_ExpandRectByNonClientSize(hwnd, dpiNew, &rc);

    // Write the final size to the out parameter.
    // If not Maximized/Arranged (snapped), this will determine the size of the
    // rect in the WM_DPICHANGED message. Otherwise, the provided size is the
    // normal position (restored, last non-Maximized/Arranged).
    pSizeNew->cx = rc.width();
    pSizeNew->cy = rc.height();

    // Return true. The next WM_DPICHANGED (if at this DPI) should contain a
    // rect with the size we picked here. (If we change to another DPI than this
    // one we'll get another WM_GETDPISCALEDSIZE before changing DPI).
    return TRUE;
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
    const auto hwnd = GetWindowHandle();
    RETURN_HR_IF_NULL(HRESULT_FROM_WIN32(ERROR_INVALID_HANDLE), hwnd);

    // We have to call BeginPaint to retrieve the invalid rectangle state
    // BeginPaint/EndPaint does a bunch of other magic in the system level
    // that we can't sufficiently replicate with GetInvalidRect/ValidateRect.
    // ---
    // We've tried in the past to not call BeginPaint/EndPaint
    // and under certain circumstances (windows with SW_HIDE, SKUs without DWM, etc.)
    // the system either sends WM_PAINT messages ad nauseum or fails to redraw everything correctly.
    PAINTSTRUCT ps;
    const auto hdc = BeginPaint(hwnd, &ps);
    RETURN_HR_IF_NULL(E_FAIL, hdc);

    if (ServiceLocator::LocateGlobals().pRender != nullptr)
    {
        // In lieu of actually painting right now, we're just going to aggregate this information in the renderer
        // and let it paint whenever it feels appropriate.
        const auto rcUpdate = til::rect{ ps.rcPaint };
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
    const auto drop = reinterpret_cast<HDROP>(wParam);
    Clipboard::Instance().PasteDrop(drop);
    DragFinish(drop);
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
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    const auto& ScreenInfo = GetScreenInfo();

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
