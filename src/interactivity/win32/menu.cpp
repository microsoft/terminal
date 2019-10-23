// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include <cpl_core.h>

#include "menu.hpp"
#include "icon.hpp"
#include "window.hpp"

#include "..\..\host\dbcs.h"
#include "..\..\host\getset.h"
#include "..\..\host\handle.h"
#include "..\..\host\misc.h"
#include "..\..\host\server.h"
#include "..\..\host\scrolling.hpp"
#include "..\..\host\telemetry.hpp"

#include "..\inc\ServiceLocator.hpp"

using namespace Microsoft::Console::Interactivity::Win32;

CONST WCHAR gwszPropertiesDll[] = L"\\console.dll";
CONST WCHAR gwszRelativePropertiesDll[] = L".\\console.dll";

#pragma hdrstop

// Initialize static Menu variables.
Menu* Menu::s_Instance = nullptr;

#pragma region Public Methods

Menu::Menu(HMENU hMenu, HMENU hHeirMenu) :
    _hMenu(hMenu),
    _hHeirMenu(hHeirMenu)
{
}

// Routine Description:
// - This routine allocates and initializes the system menu for the console
// Arguments:
// - hWnd - The handle to the console's window
// Return Value:
// - STATUS_SUCCESS or suitable NT error code
[[nodiscard]] NTSTATUS Menu::CreateInstance(HWND hWnd)
{
    NTSTATUS status = STATUS_SUCCESS;
    HMENU hMenu = nullptr;
    HMENU hHeirMenu = nullptr;

    int ItemLength;
    WCHAR ItemString[32];

    // This gets the title bar menu.
    hMenu = GetSystemMenu(hWnd, FALSE);
    hHeirMenu = LoadMenuW(ServiceLocator::LocateGlobals().hInstance,
                          MAKEINTRESOURCE(ID_CONSOLE_SYSTEMMENU));

    Menu* pNewMenu = new (std::nothrow) Menu(hMenu, hHeirMenu);
    status = NT_TESTNULL(pNewMenu);

    if (NT_SUCCESS(status))
    {
        // Load the submenu to the system menu.
        if (hHeirMenu)
        {
            ItemLength = LoadStringW(ServiceLocator::LocateGlobals().hInstance,
                                     ID_CONSOLE_EDIT,
                                     ItemString,
                                     ARRAYSIZE(ItemString));
            if (ItemLength)
            {
                // Append the clipboard menu to system menu.
                AppendMenuW(hMenu,
                            MF_POPUP | MF_STRING,
                            (UINT_PTR)hHeirMenu,
                            ItemString);
            }
        }

        // Edit the accelerators off of the standard items.
        // - Edits the indicated control to one word.
        // - Trim the Accelerator key text off of the end of the standard menu
        //   items because we don't support the accelerators.
        ItemLength = LoadStringW(ServiceLocator::LocateGlobals().hInstance,
                                 SC_CLOSE,
                                 ItemString,
                                 ARRAYSIZE(ItemString));
        if (ItemLength != 0)
        {
            MENUITEMINFO mii = { 0 };
            mii.cbSize = sizeof(mii);
            mii.fMask = MIIM_STRING | MIIM_BITMAP;
            mii.dwTypeData = ItemString;
            mii.hbmpItem = HBMMENU_POPUP_CLOSE;

            SetMenuItemInfoW(hMenu, SC_CLOSE, FALSE, &mii);
        }

        // Add other items to the system menu.
        ItemLength = LoadStringW(ServiceLocator::LocateGlobals().hInstance,
                                 ID_CONSOLE_DEFAULTS,
                                 ItemString,
                                 ARRAYSIZE(ItemString));
        if (ItemLength)
        {
            AppendMenuW(hMenu,
                        MF_STRING,
                        ID_CONSOLE_DEFAULTS,
                        ItemString);
        }

        ItemLength = LoadStringW(ServiceLocator::LocateGlobals().hInstance,
                                 ID_CONSOLE_CONTROL,
                                 ItemString,
                                 ARRAYSIZE(ItemString));
        if (ItemLength)
        {
            AppendMenuW(hMenu,
                        MF_STRING,
                        ID_CONSOLE_CONTROL,
                        ItemString);
        }

        // Set the singleton instance.
        Menu::s_Instance = pNewMenu;
    }

    return status;
}

Menu* Menu::Instance()
{
    return Menu::s_Instance;
}

Menu::~Menu()
{
}

// Routine Description:
// - this initializes the system menu when a WM_INITMENU message is read.
void Menu::Initialize()
{
    const CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    HMENU const hMenu = _hMenu;
    HMENU const hHeirMenu = _hHeirMenu;

    // If the console is iconic, disable Mark and Scroll.
    if (gci.Flags & CONSOLE_IS_ICONIC)
    {
        EnableMenuItem(hHeirMenu, ID_CONSOLE_MARK, MF_GRAYED);
        EnableMenuItem(hHeirMenu, ID_CONSOLE_SCROLL, MF_GRAYED);
    }
    else
    {
        // if the console is not iconic
        //   if there are no scroll bars
        //       or we're in mark mode
        //       disable scroll
        //   else
        //       enable scroll
        //
        //   if we're in scroll mode
        //       disable mark
        //   else
        //       enable mark

        if (gci.GetActiveOutputBuffer().IsMaximizedBoth() || gci.Flags & CONSOLE_SELECTING)
        {
            EnableMenuItem(hHeirMenu, ID_CONSOLE_SCROLL, MF_GRAYED);
        }
        else
        {
            EnableMenuItem(hHeirMenu, ID_CONSOLE_SCROLL, MF_ENABLED);
        }

        if (Scrolling::s_IsInScrollMode())
        {
            EnableMenuItem(hHeirMenu, ID_CONSOLE_SCROLL, MF_GRAYED);
        }
        else
        {
            EnableMenuItem(hHeirMenu, ID_CONSOLE_SCROLL, MF_ENABLED);
        }
    }

    // If we're selecting or scrolling, disable paste. Otherwise, enable it.
    if (gci.Flags & (CONSOLE_SELECTING | CONSOLE_SCROLLING))
    {
        EnableMenuItem(hHeirMenu, ID_CONSOLE_PASTE, MF_GRAYED);
    }
    else
    {
        EnableMenuItem(hHeirMenu, ID_CONSOLE_PASTE, MF_ENABLED);
    }

    // If app has active selection, enable copy. Otherwise, disable it.
    if (gci.Flags & CONSOLE_SELECTING && Selection::Instance().IsAreaSelected())
    {
        EnableMenuItem(hHeirMenu, ID_CONSOLE_COPY, MF_ENABLED);
    }
    else
    {
        EnableMenuItem(hHeirMenu, ID_CONSOLE_COPY, MF_GRAYED);
    }

    // Enable move if not iconic.
    if (gci.Flags & CONSOLE_IS_ICONIC)
    {
        EnableMenuItem(hMenu, SC_MOVE, MF_GRAYED);
    }
    else
    {
        EnableMenuItem(hMenu, SC_MOVE, MF_ENABLED);
    }

    // Enable settings.
    EnableMenuItem(hMenu, ID_CONSOLE_CONTROL, MF_ENABLED);
}

#pragma endregion

#pragma region Public Static Methods

// Displays the properties dialog and updates the window state as necessary.
void Menu::s_ShowPropertiesDialog(HWND const hwnd, BOOL const Defaults)
{
    CONSOLE_STATE_INFO StateInfo = { 0 };
    if (!Defaults)
    {
        THROW_IF_FAILED(Menu::s_GetConsoleState(&StateInfo));
        StateInfo.UpdateValues = FALSE;
    }

    // The Property sheet is going to copy the data from the values passed in
    //      to it, and potentially overwrite StateInfo.*Title.
    // However, we just allocated wchar_t[]'s for these values.
    // Stash the pointers to the arrays we just allocated, so we can free those
    //       arrays correctly.
    const wchar_t* const allocatedOriginalTitle = StateInfo.OriginalTitle;
    const wchar_t* const allocatedLinkTitle = StateInfo.LinkTitle;

    StateInfo.hWnd = hwnd;
    StateInfo.Defaults = Defaults;
    StateInfo.fIsV2Console = TRUE;

    UnlockConsole();

    // First try to find the console.dll next to the launched exe, else default to /windows/system32/console.dll
    HANDLE hLibrary = LoadLibraryExW(gwszRelativePropertiesDll, 0, 0);
    bool fLoadedDll = hLibrary != nullptr;
    if (!fLoadedDll)
    {
        WCHAR wszFilePath[MAX_PATH + 1] = { 0 };
        UINT const len = GetSystemDirectoryW(wszFilePath, ARRAYSIZE(wszFilePath));
        if (len < ARRAYSIZE(wszFilePath))
        {
            if (SUCCEEDED(StringCchCatW(wszFilePath, ARRAYSIZE(wszFilePath) - len, gwszPropertiesDll)))
            {
                wszFilePath[ARRAYSIZE(wszFilePath) - 1] = UNICODE_NULL;

                hLibrary = LoadLibraryExW(wszFilePath, 0, LOAD_WITH_ALTERED_SEARCH_PATH);
                fLoadedDll = hLibrary != nullptr;
            }
        }
    }

    if (fLoadedDll)
    {
        APPLET_PROC const pfnCplApplet = (APPLET_PROC)GetProcAddress((HMODULE)hLibrary, "CPlApplet");
        if (pfnCplApplet != nullptr)
        {
            (*pfnCplApplet)(hwnd, CPL_INIT, 0, 0);
            (*pfnCplApplet)(hwnd, CPL_DBLCLK, (LPARAM)&StateInfo, 0);
            (*pfnCplApplet)(hwnd, CPL_EXIT, 0, 0);
        }

        LOG_IF_WIN32_BOOL_FALSE(FreeLibrary((HMODULE)hLibrary));
    }

    LockConsole();

    if (!Defaults && StateInfo.UpdateValues)
    {
        Menu::s_PropertiesUpdate(&StateInfo);
    }

    // s_GetConsoleState may have created new wchar_t[]s for the title and link title.
    //  delete them before they're leaked.
    if (allocatedOriginalTitle != nullptr)
    {
        delete[] allocatedOriginalTitle;
    }
    if (allocatedLinkTitle != nullptr)
    {
        delete[] allocatedLinkTitle;
    }
}

[[nodiscard]] HRESULT Menu::s_GetConsoleState(CONSOLE_STATE_INFO* const pStateInfo)
{
    const CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    const SCREEN_INFORMATION& ScreenInfo = gci.GetActiveOutputBuffer();
    pStateInfo->ScreenBufferSize = ScreenInfo.GetBufferSize().Dimensions();
    pStateInfo->WindowSize = ScreenInfo.GetViewport().Dimensions();

    const RECT rcWindow = ServiceLocator::LocateConsoleWindow<Window>()->GetWindowRect();
    pStateInfo->WindowPosX = rcWindow.left;
    pStateInfo->WindowPosY = rcWindow.top;

    const FontInfo& currentFont = ScreenInfo.GetCurrentFont();
    pStateInfo->FontFamily = currentFont.GetFamily();
    pStateInfo->FontSize = currentFont.GetUnscaledSize();
    pStateInfo->FontWeight = currentFont.GetWeight();
    LOG_IF_FAILED(StringCchCopyW(pStateInfo->FaceName, ARRAYSIZE(pStateInfo->FaceName), currentFont.GetFaceName().data()));

    const Cursor& cursor = ScreenInfo.GetTextBuffer().GetCursor();
    pStateInfo->CursorSize = cursor.GetSize();
    pStateInfo->CursorColor = cursor.GetColor();
    pStateInfo->CursorType = static_cast<unsigned int>(cursor.GetType());

    // Retrieve small icon for use in displaying the dialog
    LOG_IF_FAILED(Icon::Instance().GetIcons(nullptr, &pStateInfo->hIcon));

    pStateInfo->QuickEdit = !!(gci.Flags & CONSOLE_QUICK_EDIT_MODE);
    pStateInfo->AutoPosition = !!(gci.Flags & CONSOLE_AUTO_POSITION);
    pStateInfo->InsertMode = gci.GetInsertMode();
    pStateInfo->ScreenAttributes = gci.GetFillAttribute();
    pStateInfo->PopupAttributes = gci.GetPopupFillAttribute();

    // Ensure that attributes are only describing colors to the properties dialog
    WI_ClearAllFlags(pStateInfo->ScreenAttributes, ~(FG_ATTRS | BG_ATTRS));
    WI_ClearAllFlags(pStateInfo->PopupAttributes, ~(FG_ATTRS | BG_ATTRS));

    pStateInfo->HistoryBufferSize = gci.GetHistoryBufferSize();
    pStateInfo->NumberOfHistoryBuffers = gci.GetNumberOfHistoryBuffers();
    pStateInfo->HistoryNoDup = !!(gci.Flags & CONSOLE_HISTORY_NODUP);

    memmove(pStateInfo->ColorTable, gci.GetColorTable(), gci.GetColorTableSize() * sizeof(COLORREF));

    // Create mutable copies of the titles so the propsheet can do something with them.
    if (gci.GetOriginalTitle().length() > 0)
    {
        pStateInfo->OriginalTitle = new (std::nothrow) wchar_t[gci.GetOriginalTitle().length() + 1]{ UNICODE_NULL };
        RETURN_IF_NULL_ALLOC(pStateInfo->OriginalTitle);
        gci.GetOriginalTitle().copy(pStateInfo->OriginalTitle, gci.GetOriginalTitle().length());
    }
    else
    {
        pStateInfo->OriginalTitle = nullptr;
    }

    if (gci.GetLinkTitle().length() > 0)
    {
        pStateInfo->LinkTitle = new (std::nothrow) wchar_t[gci.GetLinkTitle().length() + 1]{ UNICODE_NULL };
        RETURN_IF_NULL_ALLOC(pStateInfo->LinkTitle);
        gci.GetLinkTitle().copy(pStateInfo->LinkTitle, gci.GetLinkTitle().length());
    }
    else
    {
        pStateInfo->LinkTitle = nullptr;
    }

    pStateInfo->CodePage = gci.OutputCP;

    // begin console v2 properties
    pStateInfo->fIsV2Console = TRUE;
    pStateInfo->fWrapText = gci.GetWrapText();
    pStateInfo->fFilterOnPaste = !!gci.GetFilterOnPaste();
    pStateInfo->fCtrlKeyShortcutsDisabled = gci.GetCtrlKeyShortcutsDisabled();
    pStateInfo->fLineSelection = gci.GetLineSelection();
    pStateInfo->bWindowTransparency = ServiceLocator::LocateConsoleWindow<Window>()->GetWindowOpacity();

    pStateInfo->InterceptCopyPaste = gci.GetInterceptCopyPaste();

    // Get the properties from the settings - CONSOLE_INFORMATION overloads
    //  these methods to implement IDefaultColorProvider
    pStateInfo->DefaultForeground = gci.GetDefaultForegroundColor();
    pStateInfo->DefaultBackground = gci.GetDefaultBackgroundColor();

    pStateInfo->TerminalScrolling = gci.IsTerminalScrolling();
    // end console v2 properties
    return S_OK;
}

HMENU Menu::s_GetMenuHandle()
{
    if (Menu::s_Instance)
    {
        return Menu::s_Instance->_hMenu;
    }
    else
    {
        return nullptr;
    }
}

HMENU Menu::s_GetHeirMenuHandle()
{
    if (Menu::s_Instance)
    {
        return Menu::s_Instance->_hHeirMenu;
    }
    else
    {
        return nullptr;
    }
}

#pragma endregion

#pragma region Private Methods

// Updates the console state from information sent by the properties dialog box.
void Menu::s_PropertiesUpdate(PCONSOLE_STATE_INFO pStateInfo)
{
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    SCREEN_INFORMATION& ScreenInfo = gci.GetActiveOutputBuffer();

    if (gci.OutputCP != pStateInfo->CodePage)
    {
        gci.OutputCP = pStateInfo->CodePage;

        SetConsoleCPInfo(TRUE);
    }

    if (gci.CP != pStateInfo->CodePage)
    {
        gci.CP = pStateInfo->CodePage;

        SetConsoleCPInfo(FALSE);
    }

    // begin V2 console properties

    // NOTE: We must set the wrap state before further manipulating the buffer/window.
    // If we do not, the user will get a different result than the preview (e.g. we'll resize without scroll bars first then turn off wrapping.)
    gci.SetWrapText(!!pStateInfo->fWrapText);
    gci.SetFilterOnPaste(!!pStateInfo->fFilterOnPaste);
    gci.SetCtrlKeyShortcutsDisabled(!!pStateInfo->fCtrlKeyShortcutsDisabled);
    gci.SetLineSelection(!!pStateInfo->fLineSelection);
    Selection::Instance().SetLineSelection(!!gci.GetLineSelection());

    ServiceLocator::LocateConsoleWindow<Window>()->SetWindowOpacity(pStateInfo->bWindowTransparency);
    ServiceLocator::LocateConsoleWindow<Window>()->ApplyWindowOpacity();
    // end V2 console properties

    // Apply font information (must come before all character calculations for window/buffer size).
    FontInfo fiNewFont(pStateInfo->FaceName, gsl::narrow_cast<unsigned char>(pStateInfo->FontFamily), pStateInfo->FontWeight, pStateInfo->FontSize, pStateInfo->CodePage);

    ScreenInfo.UpdateFont(&fiNewFont);

    const FontInfo& fontApplied = ScreenInfo.GetCurrentFont();

    // Now make sure internal font state reflects the font chosen
    gci.SetFontFamily(fontApplied.GetFamily());
    gci.SetFontSize(fontApplied.GetUnscaledSize());
    gci.SetFontWeight(fontApplied.GetWeight());
    gci.SetFaceName(fontApplied.GetFaceName());

    // Set the cursor properties in the Settings
    const auto cursorType = static_cast<CursorType>(pStateInfo->CursorType);
    gci.SetCursorColor(pStateInfo->CursorColor);
    gci.SetCursorType(cursorType);

    // Then also apply them to the buffer's cursor
    ScreenInfo.SetCursorInformation(pStateInfo->CursorSize,
                                    ScreenInfo.GetTextBuffer().GetCursor().IsVisible());
    ScreenInfo.SetCursorColor(pStateInfo->CursorColor, true);
    ScreenInfo.SetCursorType(cursorType, true);

    gci.SetTerminalScrolling(pStateInfo->TerminalScrolling);

    {
        // Requested window in characters
        COORD coordWindow = pStateInfo->WindowSize;

        // Requested buffer in characters.
        COORD coordBuffer = pStateInfo->ScreenBufferSize;

        // First limit the window so it cannot be bigger than the monitor.
        // Maximum number of characters we could fit on the given monitor.
        COORD const coordLargest = ScreenInfo.GetLargestWindowSizeInCharacters();

        coordWindow.X = std::min(coordLargest.X, coordWindow.X);
        coordWindow.Y = std::min(coordLargest.Y, coordWindow.Y);

        if (gci.GetWrapText())
        {
            // Then if wrap text is on, the buffer width gets fixed to the window width value.
            coordBuffer.X = coordWindow.X;

            // However, we're not done. The "max window size" is if we had no scroll bar.
            // We need to adjust slightly more if there's space reserved for a vertical scroll bar
            // which happens when the buffer Y is taller than the window Y.
            if (coordBuffer.Y > coordWindow.Y)
            {
                // Since we need a scroll bar in the Y direction, clamp the buffer width to make sure that
                // it is leaving appropriate space for a scroll bar.
                COORD const coordScrollBars = ScreenInfo.GetScrollBarSizesInCharacters();
                SHORT const sMaxBufferWidthWithScroll = coordLargest.X - coordScrollBars.X;

                coordBuffer.X = std::min(coordBuffer.X, sMaxBufferWidthWithScroll);
            }
        }

        // Now adjust the buffer size first to whatever we want it to be if it's different than before.
        const COORD coordScreenBufferSize = ScreenInfo.GetBufferSize().Dimensions();
        if (coordBuffer.X != coordScreenBufferSize.X ||
            coordBuffer.Y != coordScreenBufferSize.Y)
        {
            CommandLine* const pCommandLine = &CommandLine::Instance();

            pCommandLine->Hide(FALSE);

            LOG_IF_FAILED(ScreenInfo.ResizeScreenBuffer(coordBuffer, TRUE));

            pCommandLine->Show();
        }

        // Finally, restrict window size to the maximum possible size for the given buffer now that it's processed.
        COORD const coordMaxForBuffer = ScreenInfo.GetMaxWindowSizeInCharacters();

        coordWindow.X = std::min(coordWindow.X, coordMaxForBuffer.X);
        coordWindow.Y = std::min(coordWindow.Y, coordMaxForBuffer.Y);

        // Then finish by updating the window. This will update the window size,
        //      as well as the screen buffer's viewport.
        ServiceLocator::LocateConsoleWindow<Window>()->UpdateWindowSize(coordWindow);
    }

    if (pStateInfo->QuickEdit)
    {
        gci.Flags |= CONSOLE_QUICK_EDIT_MODE;
    }
    else
    {
        gci.Flags &= ~CONSOLE_QUICK_EDIT_MODE;
    }

    if (pStateInfo->AutoPosition)
    {
        gci.Flags |= CONSOLE_AUTO_POSITION;
    }
    else
    {
        gci.Flags &= ~CONSOLE_AUTO_POSITION;

        POINT pt;
        pt.x = pStateInfo->WindowPosX;
        pt.y = pStateInfo->WindowPosY;

        ServiceLocator::LocateConsoleWindow<Window>()->UpdateWindowPosition(pt);
    }

    if (gci.GetInsertMode() != !!pStateInfo->InsertMode)
    {
        ScreenInfo.SetCursorDBMode(false);
        gci.SetInsertMode(pStateInfo->InsertMode != FALSE);
        if (gci.HasPendingCookedRead())
        {
            gci.CookedReadData().SetInsertMode(gci.GetInsertMode());
        }
    }

    gci.SetColorTable(pStateInfo->ColorTable, gci.GetColorTableSize());

    // Ensure that attributes only contain color specification.
    WI_ClearAllFlags(pStateInfo->ScreenAttributes, ~(FG_ATTRS | BG_ATTRS));
    WI_ClearAllFlags(pStateInfo->PopupAttributes, ~(FG_ATTRS | BG_ATTRS));

    // Place our new legacy fill attributes in gci
    //      (recall they are already persisted to the reg/link by the propsheet
    //      when it was closed)
    gci.SetFillAttribute(pStateInfo->ScreenAttributes);
    gci.SetPopupFillAttribute(pStateInfo->PopupAttributes);
    // Store our updated Default Color values
    gci.SetDefaultForegroundColor(pStateInfo->DefaultForeground);
    gci.SetDefaultBackgroundColor(pStateInfo->DefaultBackground);

    // Set the screen info's default text attributes to defaults -
    ScreenInfo.SetDefaultAttributes(gci.GetDefaultAttributes(), { gci.GetPopupFillAttribute() });

    CommandHistory::s_ResizeAll(pStateInfo->HistoryBufferSize);
    gci.SetNumberOfHistoryBuffers(pStateInfo->NumberOfHistoryBuffers);
    if (pStateInfo->HistoryNoDup)
    {
        gci.Flags |= CONSOLE_HISTORY_NODUP;
    }
    else
    {
        gci.Flags &= ~CONSOLE_HISTORY_NODUP;
    }

    // Since edit keys are global state only stored once in the registry, post the message to the queue to reload
    // those properties specifically from the registry in case they were changed.
    ServiceLocator::LocateConsoleWindow<Window>()->PostUpdateExtendedEditKeys();

    gci.ConsoleIme.RefreshAreaAttributes();

    gci.SetInterceptCopyPaste(!!pStateInfo->InterceptCopyPaste);
}

#pragma endregion
