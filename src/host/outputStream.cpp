// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "outputStream.hpp"

#include "_stream.h"
#include "getset.h"
#include "directio.h"

#include "../interactivity/inc/ServiceLocator.hpp"

#pragma hdrstop

using namespace Microsoft::Console;
using Microsoft::Console::Interactivity::ServiceLocator;

WriteBuffer::WriteBuffer(_In_ Microsoft::Console::IIoProvider& io) :
    _io{ io },
    _ntstatus{ STATUS_INVALID_DEVICE_STATE }
{
}

// Routine Description:
// - Handles the print action from the state machine
// Arguments:
// - wch - The character to be printed.
// Return Value:
// - <none>
void WriteBuffer::Print(const wchar_t wch)
{
    _DefaultCase(wch);
}

// Routine Description:
// - Handles the print action from the state machine
// Arguments:
// - wch - The character to be printed.
// Return Value:
// - <none>
void WriteBuffer::PrintString(const wchar_t* const rgwch, const size_t cch)
{
    _DefaultStringCase(rgwch, cch);
}

// Routine Description:
// - Handles the execute action from the state machine
// Arguments:
// - wch - The C0 control character to be executed.
// Return Value:
// - <none>
void WriteBuffer::Execute(const wchar_t wch)
{
    _DefaultCase(wch);
}

// Routine Description:
// - Default text editing/printing handler for all characters that were not routed elsewhere by other state machine intercepts.
// Arguments:
// - wch - The character to be processed by our default text editing/printing mechanisms.
// Return Value:
// - <none>
void WriteBuffer::_DefaultCase(const wchar_t wch)
{
    _DefaultStringCase(const_cast<wchar_t*>(&wch), 1); // WriteCharsLegacy wants mutable chars, so we'll givve it mutable chars.
}

// Routine Description:
// - Default text editing/printing handler for all characters that were not routed elsewhere by other state machine intercepts.
// Arguments:
// - wch - The character to be processed by our default text editing/printing mechanisms.
// Return Value:
// - <none>
void WriteBuffer::_DefaultStringCase(_In_reads_(cch) const wchar_t* const rgwch, const size_t cch)
{
    size_t dwNumBytes = cch * sizeof(wchar_t);

    _io.GetActiveOutputBuffer().GetTextBuffer().GetCursor().SetIsOn(true);

    _ntstatus = WriteCharsLegacy(_io.GetActiveOutputBuffer(),
                                 rgwch,
                                 rgwch,
                                 rgwch,
                                 &dwNumBytes,
                                 nullptr,
                                 _io.GetActiveOutputBuffer().GetTextBuffer().GetCursor().GetPosition().X,
                                 WC_LIMIT_BACKSPACE | WC_NONDESTRUCTIVE_TAB | WC_DELAY_EOL_WRAP,
                                 nullptr);
}

ConhostInternalGetSet::ConhostInternalGetSet(_In_ IIoProvider& io) :
    _io{ io }
{
}

// Routine Description:
// - Connects the GetConsoleScreenBufferInfoEx API call directly into our Driver Message servicing call inside Conhost.exe
// Arguments:
// - pConsoleScreenBufferInfoEx - Pointer to structure to hold screen buffer information like the public API call.
// Return Value:
// - TRUE if successful (see DoSrvGetConsoleScreenBufferInfo). FALSE otherwise.
BOOL ConhostInternalGetSet::GetConsoleScreenBufferInfoEx(_Out_ CONSOLE_SCREEN_BUFFER_INFOEX* const pConsoleScreenBufferInfoEx) const
{
    ServiceLocator::LocateGlobals().api.GetConsoleScreenBufferInfoExImpl(_io.GetActiveOutputBuffer(), *pConsoleScreenBufferInfoEx);
    return TRUE;
}

// Routine Description:
// - Connects the SetConsoleScreenBufferInfoEx API call directly into our Driver Message servicing call inside Conhost.exe
// Arguments:
// - pConsoleScreenBufferInfoEx - Pointer to structure containing screen buffer information like the public API call.
// Return Value:
// - TRUE if successful (see DoSrvSetConsoleScreenBufferInfo). FALSE otherwise.
BOOL ConhostInternalGetSet::SetConsoleScreenBufferInfoEx(const CONSOLE_SCREEN_BUFFER_INFOEX* const pConsoleScreenBufferInfoEx)
{
    return SUCCEEDED(ServiceLocator::LocateGlobals().api.SetConsoleScreenBufferInfoExImpl(_io.GetActiveOutputBuffer(), *pConsoleScreenBufferInfoEx));
}

// Routine Description:
// - Connects the SetConsoleCursorPosition API call directly into our Driver Message servicing call inside Conhost.exe
// Arguments:
// - coordCursorPosition - new cursor position to set like the public API call.
// Return Value:
// - TRUE if successful (see DoSrvSetConsoleCursorPosition). FALSE otherwise.
BOOL ConhostInternalGetSet::SetConsoleCursorPosition(const COORD coordCursorPosition)
{
    return SUCCEEDED(ServiceLocator::LocateGlobals().api.SetConsoleCursorPositionImpl(_io.GetActiveOutputBuffer(), coordCursorPosition));
}

// Routine Description:
// - Connects the GetConsoleCursorInfo API call directly into our Driver Message servicing call inside Conhost.exe
// Arguments:
// - pConsoleCursorInfo - Pointer to structure to receive console cursor rendering info
// Return Value:
// - TRUE if successful (see DoSrvGetConsoleCursorInfo). FALSE otherwise.
BOOL ConhostInternalGetSet::GetConsoleCursorInfo(_In_ CONSOLE_CURSOR_INFO* const pConsoleCursorInfo) const
{
    bool bVisible;
    DWORD dwSize;

    ServiceLocator::LocateGlobals().api.GetConsoleCursorInfoImpl(_io.GetActiveOutputBuffer(), dwSize, bVisible);
    pConsoleCursorInfo->bVisible = bVisible;
    pConsoleCursorInfo->dwSize = dwSize;
    return TRUE;
}

// Routine Description:
// - Connects the SetConsoleCursorInfo API call directly into our Driver Message servicing call inside Conhost.exe
// Arguments:
// - pConsoleCursorInfo - Updated size/visibility information to modify the cursor rendering behavior.
// Return Value:
// - TRUE if successful (see DoSrvSetConsoleCursorInfo). FALSE otherwise.
BOOL ConhostInternalGetSet::SetConsoleCursorInfo(const CONSOLE_CURSOR_INFO* const pConsoleCursorInfo)
{
    const bool visible = !!pConsoleCursorInfo->bVisible;
    return SUCCEEDED(ServiceLocator::LocateGlobals().api.SetConsoleCursorInfoImpl(_io.GetActiveOutputBuffer(), pConsoleCursorInfo->dwSize, visible));
}

// Routine Description:
// - Connects the FillConsoleOutputCharacter API call directly into our Driver Message servicing call inside Conhost.exe
// Arguments:
// - wch - Character to use for filling the buffer
// - nLength - The length of the fill run in characters (depending on mode, will wrap at the window edge so multiple lines are the sum of the total length)
// - dwWriteCoord - The first fill character's coordinate position in the buffer (writes continue rightward and possibly down from there)
// - numberOfCharsWritten - Pointer to memory location to hold the total number of characters written into the buffer
// Return Value:
// - TRUE if successful (see FillConsoleOutputCharacterWImpl). FALSE otherwise.
BOOL ConhostInternalGetSet::FillConsoleOutputCharacterW(const WCHAR wch, const DWORD nLength, const COORD dwWriteCoord, size_t& numberOfCharsWritten) noexcept
{
    return SUCCEEDED(ServiceLocator::LocateGlobals().api.FillConsoleOutputCharacterWImpl(_io.GetActiveOutputBuffer(),
                                                                                         wch,
                                                                                         nLength,
                                                                                         dwWriteCoord,
                                                                                         numberOfCharsWritten));
}

// Routine Description:
// - Connects the FillConsoleOutputAttribute API call directly into our Driver Message servicing call inside Conhost.exe
// Arguments:
// - wAttribute - Text attribute (colors/font style) for filling the buffer
// - nLength - The length of the fill run in characters (depending on mode, will wrap at the window edge so multiple lines are the sum of the total length)
// - dwWriteCoord - The first fill character's coordinate position in the buffer (writes continue rightward and possibly down from there)
// - numberOfCharsWritten - Pointer to memory location to hold the total number of text attributes written into the buffer
// Return Value:
// - TRUE if successful (see FillConsoleOutputAttributeImpl). FALSE otherwise.
BOOL ConhostInternalGetSet::FillConsoleOutputAttribute(const WORD wAttribute, const DWORD nLength, const COORD dwWriteCoord, size_t& numberOfAttrsWritten) noexcept
{
    return SUCCEEDED(ServiceLocator::LocateGlobals().api.FillConsoleOutputAttributeImpl(_io.GetActiveOutputBuffer(),
                                                                                        wAttribute,
                                                                                        nLength,
                                                                                        dwWriteCoord,
                                                                                        numberOfAttrsWritten));
}

// Routine Description:
// - Connects the SetConsoleTextAttribute API call directly into our Driver Message servicing call inside Conhost.exe
//     Sets BOTH the FG and the BG component of the attributes.
// Arguments:
// - wAttr - new color/graphical attributes to apply as default within the console text buffer
// Return Value:
// - TRUE if successful (see DoSrvSetConsoleTextAttribute). FALSE otherwise.
BOOL ConhostInternalGetSet::SetConsoleTextAttribute(const WORD wAttr)
{
    return SUCCEEDED(ServiceLocator::LocateGlobals().api.SetConsoleTextAttributeImpl(_io.GetActiveOutputBuffer(), wAttr));
}

// Routine Description:
// - Connects the PrivateSetDefaultAttributes API call directly into our Driver Message servicing call inside Conhost.exe
//     Sets the FG and/or BG to the Default attributes values.
// Arguments:
// - fForeground - Set the foreground to the default attributes
// - fBackground - Set the background to the default attributes
// Return Value:
// - TRUE if successful (see DoSrvPrivateSetDefaultAttributes). FALSE otherwise.
BOOL ConhostInternalGetSet::PrivateSetDefaultAttributes(const bool fForeground,
                                                        const bool fBackground)
{
    DoSrvPrivateSetDefaultAttributes(_io.GetActiveOutputBuffer(), fForeground, fBackground);
    return TRUE;
}

// Routine Description:
// - Connects the PrivateSetLegacyAttributes API call directly into our Driver Message servicing call inside Conhost.exe
//     Sets only the components of the attributes requested with the fForeground, fBackground, and fMeta flags.
// Arguments:
// - wAttr - new color/graphical attributes to apply as default within the console text buffer
// - fForeground - The new attributes contain an update to the foreground attributes
// - fBackground - The new attributes contain an update to the background attributes
// - fMeta - The new attributes contain an update to the meta attributes
// Return Value:
// - TRUE if successful (see DoSrvVtSetLegacyAttributes). FALSE otherwise.
BOOL ConhostInternalGetSet::PrivateSetLegacyAttributes(const WORD wAttr,
                                                       const bool fForeground,
                                                       const bool fBackground,
                                                       const bool fMeta)
{
    DoSrvPrivateSetLegacyAttributes(_io.GetActiveOutputBuffer(), wAttr, fForeground, fBackground, fMeta);
    return TRUE;
}

// Routine Description:
// - Sets the current attributes of the screen buffer to use the color table entry specified by
//     the iXtermTableEntry. Sets either the FG or the BG component of the attributes.
// Arguments:
// - iXtermTableEntry - The entry of the xterm table to use.
// - fIsForeground - Whether or not the color applies to the foreground.
// Return Value:
// - TRUE if successful (see DoSrvPrivateSetConsoleXtermTextAttribute). FALSE otherwise.
BOOL ConhostInternalGetSet::SetConsoleXtermTextAttribute(const int iXtermTableEntry, const bool fIsForeground)
{
    DoSrvPrivateSetConsoleXtermTextAttribute(_io.GetActiveOutputBuffer(), iXtermTableEntry, fIsForeground);
    return TRUE;
}

// Routine Description:
// - Sets the current attributes of the screen buffer to use the given rgb color.
//     Sets either the FG or the BG component of the attributes.
// Arguments:
// - rgbColor - The rgb color to use.
// - fIsForeground - Whether or not the color applies to the foreground.
// Return Value:
// - TRUE if successful (see DoSrvPrivateSetConsoleRGBTextAttribute). FALSE otherwise.
BOOL ConhostInternalGetSet::SetConsoleRGBTextAttribute(const COLORREF rgbColor, const bool fIsForeground)
{
    DoSrvPrivateSetConsoleRGBTextAttribute(_io.GetActiveOutputBuffer(), rgbColor, fIsForeground);
    return TRUE;
}

BOOL ConhostInternalGetSet::PrivateBoldText(const bool bolded)
{
    DoSrvPrivateBoldText(_io.GetActiveOutputBuffer(), bolded);
    return TRUE;
}

// Method Description:
// - Retrieves the currently active ExtendedAttributes. See also
//   DoSrvPrivateGetExtendedTextAttributes
// Arguments:
// - pAttrs: Recieves the ExtendedAttributes value.
// Return Value:
// - TRUE if successful (see DoSrvPrivateGetExtendedTextAttributes). FALSE otherwise.
BOOL ConhostInternalGetSet::PrivateGetExtendedTextAttributes(ExtendedAttributes* const pAttrs)
{
    *pAttrs = DoSrvPrivateGetExtendedTextAttributes(_io.GetActiveOutputBuffer());
    return TRUE;
}

// Method Description:
// - Sets the active ExtendedAttributes of the active screen buffer. Text
//   written to this buffer will be written with these attributes.
// Arguments:
// - extendedAttrs: The new ExtendedAttributes to use
// Return Value:
// - TRUE if successful (see DoSrvPrivateSetExtendedTextAttributes). FALSE otherwise.
BOOL ConhostInternalGetSet::PrivateSetExtendedTextAttributes(const ExtendedAttributes attrs)
{
    DoSrvPrivateSetExtendedTextAttributes(_io.GetActiveOutputBuffer(), attrs);
    return TRUE;
}

// Method Description:
// - Retrieves the current TextAttribute of the active screen buffer.
// Arguments:
// - pAttrs: Receives the TextAttribute value.
// Return Value:
// - TRUE if successful. FALSE otherwise.
BOOL ConhostInternalGetSet::PrivateGetTextAttributes(TextAttribute* const pAttrs) const
{
    *pAttrs = _io.GetActiveOutputBuffer().GetAttributes();
    return TRUE;
}

// Method Description:
// - Sets the current TextAttribute of the active screen buffer. Text
//   written to this buffer will be written with these attributes.
// Arguments:
// - attrs: The new TextAttribute to use
// Return Value:
// - TRUE if successful. FALSE otherwise.
BOOL ConhostInternalGetSet::PrivateSetTextAttributes(const TextAttribute& attrs)
{
    _io.GetActiveOutputBuffer().SetAttributes(attrs);
    return TRUE;
}

// Routine Description:
// - Connects the WriteConsoleInput API call directly into our Driver Message servicing call inside Conhost.exe
// Arguments:
// - events - the input events to be copied into the head of the input
// buffer for the underlying attached process
// - eventsWritten - on output, the number of events written
// Return Value:
// - TRUE if successful (see DoSrvWriteConsoleInput). FALSE otherwise.
BOOL ConhostInternalGetSet::PrivateWriteConsoleInputW(_Inout_ std::deque<std::unique_ptr<IInputEvent>>& events,
                                                      _Out_ size_t& eventsWritten)
{
    eventsWritten = 0;

    return SUCCEEDED(DoSrvPrivateWriteConsoleInputW(_io.GetActiveInputBuffer(),
                                                    events,
                                                    eventsWritten,
                                                    true)); // append
}

// Routine Description:
// - Connects the ScrollConsoleScreenBuffer API call directly into our Driver Message servicing call inside Conhost.exe
// Arguments:
// - pScrollRectangle - The region to "cut" from the existing buffer text
// - pClipRectangle - The bounding rectangle within which all modifications should happen. Any modification outside this RECT should be clipped.
// - coordDestinationOrigin - The top left corner of the "paste" from pScrollREctangle
// - pFill - The text/attribute pair to fill all remaining space behind after the "cut" operation (bounded by clip, of course.)
// Return Value:
// - TRUE if successful (see DoSrvScrollConsoleScreenBuffer). FALSE otherwise.
BOOL ConhostInternalGetSet::ScrollConsoleScreenBufferW(const SMALL_RECT* pScrollRectangle,
                                                       _In_opt_ const SMALL_RECT* pClipRectangle,
                                                       _In_ COORD coordDestinationOrigin,
                                                       const CHAR_INFO* pFill)
{
    return SUCCEEDED(ServiceLocator::LocateGlobals().api.ScrollConsoleScreenBufferWImpl(_io.GetActiveOutputBuffer(),
                                                                                        *pScrollRectangle,
                                                                                        coordDestinationOrigin,
                                                                                        pClipRectangle != nullptr ? std::optional<SMALL_RECT>(*pClipRectangle) : std::nullopt,
                                                                                        pFill->Char.UnicodeChar,
                                                                                        pFill->Attributes));
}

// Routine Description:
// - Connects the SetConsoleWindowInfo API call directly into our Driver Message servicing call inside Conhost.exe
// Arguments:
// - bAbsolute - Should the window be moved to an absolute position? If false, the movement is relative to the current pos.
// - lpConsoleWindow - Info about how to move the viewport
// Return Value:
// - TRUE if successful (see DoSrvSetConsoleWindowInfo). FALSE otherwise.
BOOL ConhostInternalGetSet::SetConsoleWindowInfo(const BOOL bAbsolute, const SMALL_RECT* const lpConsoleWindow)
{
    return SUCCEEDED(ServiceLocator::LocateGlobals().api.SetConsoleWindowInfoImpl(_io.GetActiveOutputBuffer(), !!bAbsolute, *lpConsoleWindow));
}

// Routine Description:
// - Connects the PrivateSetCursorKeysMode call directly into our Driver Message servicing call inside Conhost.exe
//   PrivateSetCursorKeysMode is an internal-only "API" call that the vt commands can execute,
//     but it is not represented as a function call on out public API surface.
// Arguments:
// - fApplicationMode - set to true to enable Application Mode Input, false for Normal Mode.
// Return Value:
// - TRUE if successful (see DoSrvPrivateSetCursorKeysMode). FALSE otherwise.
BOOL ConhostInternalGetSet::PrivateSetCursorKeysMode(const bool fApplicationMode)
{
    return NT_SUCCESS(DoSrvPrivateSetCursorKeysMode(fApplicationMode));
}

// Routine Description:
// - Connects the PrivateSetKeypadMode call directly into our Driver Message servicing call inside Conhost.exe
//   PrivateSetKeypadMode is an internal-only "API" call that the vt commands can execute,
//     but it is not represented as a function call on out public API surface.
// Arguments:
// - fApplicationMode - set to true to enable Application Mode Input, false for Numeric Mode.
// Return Value:
// - TRUE if successful (see DoSrvPrivateSetKeypadMode). FALSE otherwise.
BOOL ConhostInternalGetSet::PrivateSetKeypadMode(const bool fApplicationMode)
{
    return NT_SUCCESS(DoSrvPrivateSetKeypadMode(fApplicationMode));
}

// Routine Description:
// - Connects the PrivateShowCursor call directly into our Driver Message servicing call inside Conhost.exe
//   PrivateShowCursor is an internal-only "API" call that the vt commands can execute,
//     but it is not represented as a function call on out public API surface.
// Arguments:
// - show - set to true to make the cursor visible, false to hide.
// Return Value:
// - TRUE if successful (see DoSrvPrivateShowCursor). FALSE otherwise.
BOOL ConhostInternalGetSet::PrivateShowCursor(const bool show) noexcept
{
    DoSrvPrivateShowCursor(_io.GetActiveOutputBuffer(), show);
    return TRUE;
}

// Routine Description:
// - Connects the PrivateAllowCursorBlinking call directly into our Driver Message servicing call inside Conhost.exe
//   PrivateAllowCursorBlinking is an internal-only "API" call that the vt commands can execute,
//     but it is not represented as a function call on out public API surface.
// Arguments:
// - fEnable - set to true to enable blinking, false to disable
// Return Value:
// - TRUE if successful (see DoSrvPrivateAllowCursorBlinking). FALSE otherwise.
BOOL ConhostInternalGetSet::PrivateAllowCursorBlinking(const bool fEnable)
{
    DoSrvPrivateAllowCursorBlinking(_io.GetActiveOutputBuffer(), fEnable);
    return TRUE;
}

// Routine Description:
// - Connects the PrivateSetScrollingRegion call directly into our Driver Message servicing call inside Conhost.exe
//   PrivateSetScrollingRegion is an internal-only "API" call that the vt commands can execute,
//     but it is not represented as a function call on out public API surface.
// Arguments:
// - psrScrollMargins - The bounds of the region to be the scrolling region of the viewport.
// Return Value:
// - TRUE if successful (see DoSrvPrivateSetScrollingRegion). FALSE otherwise.
BOOL ConhostInternalGetSet::PrivateSetScrollingRegion(const SMALL_RECT* const psrScrollMargins)
{
    return NT_SUCCESS(DoSrvPrivateSetScrollingRegion(_io.GetActiveOutputBuffer(), psrScrollMargins));
}

// Routine Description:
// - Connects the PrivateReverseLineFeed call directly into our Driver Message servicing call inside Conhost.exe
//   PrivateReverseLineFeed is an internal-only "API" call that the vt commands can execute,
//     but it is not represented as a function call on out public API surface.
// Return Value:
// - TRUE if successful (see DoSrvPrivateReverseLineFeed). FALSE otherwise.
BOOL ConhostInternalGetSet::PrivateReverseLineFeed()
{
    return NT_SUCCESS(DoSrvPrivateReverseLineFeed(_io.GetActiveOutputBuffer()));
}

// Routine Description:
// - Connects the MoveCursorVertically call directly into our Driver Message servicing call inside Conhost.exe
//   MoveCursorVertically is an internal-only "API" call that the vt commands can execute,
//     but it is not represented as a function call on out public API surface.
// Return Value:
// - TRUE if successful (see DoSrvMoveCursorVertically). FALSE otherwise.
BOOL ConhostInternalGetSet::MoveCursorVertically(const short lines)
{
    return SUCCEEDED(DoSrvMoveCursorVertically(_io.GetActiveOutputBuffer(), lines));
}

// Routine Description:
// - Connects the SetConsoleTitleW API call directly into our Driver Message servicing call inside Conhost.exe
// Arguments:
// - title - The null-terminated string to set as the window title
// Return Value:
// - TRUE if successful (see DoSrvSetConsoleTitle). FALSE otherwise.
BOOL ConhostInternalGetSet::SetConsoleTitleW(std::wstring_view title)
{
    return SUCCEEDED(DoSrvSetConsoleTitleW(title));
}

// Routine Description:
// - Connects the PrivateUseAlternateScreenBuffer call directly into our Driver Message servicing call inside Conhost.exe
//   PrivateUseAlternateScreenBuffer is an internal-only "API" call that the vt commands can execute,
//     but it is not represented as a function call on out public API surface.
// Return Value:
// - TRUE if successful (see DoSrvPrivateUseAlternateScreenBuffer). FALSE otherwise.
BOOL ConhostInternalGetSet::PrivateUseAlternateScreenBuffer()
{
    return NT_SUCCESS(DoSrvPrivateUseAlternateScreenBuffer(_io.GetActiveOutputBuffer()));
}

// Routine Description:
// - Connects the PrivateUseMainScreenBuffer call directly into our Driver Message servicing call inside Conhost.exe
//   PrivateUseMainScreenBuffer is an internal-only "API" call that the vt commands can execute,
//     but it is not represented as a function call on out public API surface.
// Return Value:
// - TRUE if successful (see DoSrvPrivateUseMainScreenBuffer). FALSE otherwise.
BOOL ConhostInternalGetSet::PrivateUseMainScreenBuffer()
{
    DoSrvPrivateUseMainScreenBuffer(_io.GetActiveOutputBuffer());
    return TRUE;
}

// - Connects the PrivateHorizontalTabSet call directly into our Driver Message servicing call inside Conhost.exe
//   PrivateHorizontalTabSet is an internal-only "API" call that the vt commands can execute,
//     but it is not represented as a function call on out public API surface.
// Arguments:
// <none>
// Return Value:
// - TRUE if successful (see PrivateHorizontalTabSet). FALSE otherwise.
BOOL ConhostInternalGetSet::PrivateHorizontalTabSet()
{
    return NT_SUCCESS(DoSrvPrivateHorizontalTabSet());
}

// Routine Description:
// - Connects the PrivateForwardTab call directly into our Driver Message servicing call inside Conhost.exe
//   PrivateForwardTab is an internal-only "API" call that the vt commands can execute,
//     but it is not represented as a function call on out public API surface.
// Arguments:
// - sNumTabs - the number of tabs to execute
// Return Value:
// - TRUE if successful (see PrivateForwardTab). FALSE otherwise.
BOOL ConhostInternalGetSet::PrivateForwardTab(const SHORT sNumTabs)
{
    return NT_SUCCESS(DoSrvPrivateForwardTab(sNumTabs));
}

// Routine Description:
// - Connects the PrivateBackwardsTab call directly into our Driver Message servicing call inside Conhost.exe
//   PrivateBackwardsTab is an internal-only "API" call that the vt commands can execute,
//     but it is not represented as a function call on out public API surface.
// Arguments:
// - sNumTabs - the number of tabs to execute
// Return Value:
// - TRUE if successful (see PrivateBackwardsTab). FALSE otherwise.
BOOL ConhostInternalGetSet::PrivateBackwardsTab(const SHORT sNumTabs)
{
    return NT_SUCCESS(DoSrvPrivateBackwardsTab(sNumTabs));
}

// Routine Description:
// - Connects the PrivateTabClear call directly into our Driver Message servicing call inside Conhost.exe
//   PrivateTabClear is an internal-only "API" call that the vt commands can execute,
//     but it is not represented as a function call on out public API surface.
// Arguments:
// - fClearAll - set to true to enable blinking, false to disable
// Return Value:
// - TRUE if successful (see PrivateTabClear). FALSE otherwise.
BOOL ConhostInternalGetSet::PrivateTabClear(const bool fClearAll)
{
    DoSrvPrivateTabClear(fClearAll);
    return TRUE;
}

// Routine Description:
// - Connects the PrivateSetDefaultTabStops call directly into the private api point
// Return Value:
// - TRUE
BOOL ConhostInternalGetSet::PrivateSetDefaultTabStops()
{
    DoSrvPrivateSetDefaultTabStops();
    return TRUE;
}

// Routine Description:
// - Connects the PrivateEnableVT200MouseMode call directly into our Driver Message servicing call inside Conhost.exe
//   PrivateEnableVT200MouseMode is an internal-only "API" call that the vt commands can execute,
//     but it is not represented as a function call on out public API surface.
// Arguments:
// - fEnabled - set to true to enable vt200 mouse mode, false to disable
// Return Value:
// - TRUE if successful (see DoSrvPrivateEnableVT200MouseMode). FALSE otherwise.
BOOL ConhostInternalGetSet::PrivateEnableVT200MouseMode(const bool fEnabled)
{
    DoSrvPrivateEnableVT200MouseMode(fEnabled);
    return TRUE;
}

// Routine Description:
// - Connects the PrivateEnableUTF8ExtendedMouseMode call directly into our Driver Message servicing call inside Conhost.exe
//   PrivateEnableUTF8ExtendedMouseMode is an internal-only "API" call that the vt commands can execute,
//     but it is not represented as a function call on out public API surface.
// Arguments:
// - fEnabled - set to true to enable utf8 extended mouse mode, false to disable
// Return Value:
// - TRUE if successful (see DoSrvPrivateEnableUTF8ExtendedMouseMode). FALSE otherwise.
BOOL ConhostInternalGetSet::PrivateEnableUTF8ExtendedMouseMode(const bool fEnabled)
{
    DoSrvPrivateEnableUTF8ExtendedMouseMode(fEnabled);
    return TRUE;
}

// Routine Description:
// - Connects the PrivateEnableSGRExtendedMouseMode call directly into our Driver Message servicing call inside Conhost.exe
//   PrivateEnableSGRExtendedMouseMode is an internal-only "API" call that the vt commands can execute,
//     but it is not represented as a function call on out public API surface.
// Arguments:
// - fEnabled - set to true to enable SGR extended mouse mode, false to disable
// Return Value:
// - TRUE if successful (see DoSrvPrivateEnableSGRExtendedMouseMode). FALSE otherwise.
BOOL ConhostInternalGetSet::PrivateEnableSGRExtendedMouseMode(const bool fEnabled)
{
    DoSrvPrivateEnableSGRExtendedMouseMode(fEnabled);
    return TRUE;
}

// Routine Description:
// - Connects the PrivateEnableButtonEventMouseMode call directly into our Driver Message servicing call inside Conhost.exe
//   PrivateEnableButtonEventMouseMode is an internal-only "API" call that the vt commands can execute,
//     but it is not represented as a function call on out public API surface.
// Arguments:
// - fEnabled - set to true to enable button-event mouse mode, false to disable
// Return Value:
// - TRUE if successful (see DoSrvPrivateEnableButtonEventMouseMode). FALSE otherwise.
BOOL ConhostInternalGetSet::PrivateEnableButtonEventMouseMode(const bool fEnabled)
{
    DoSrvPrivateEnableButtonEventMouseMode(fEnabled);
    return TRUE;
}

// Routine Description:
// - Connects the PrivateEnableAnyEventMouseMode call directly into our Driver Message servicing call inside Conhost.exe
//   PrivateEnableAnyEventMouseMode is an internal-only "API" call that the vt commands can execute,
//     but it is not represented as a function call on out public API surface.
// Arguments:
// - fEnabled - set to true to enable any-event mouse mode, false to disable
// Return Value:
// - TRUE if successful (see DoSrvPrivateEnableAnyEventMouseMode). FALSE otherwise.
BOOL ConhostInternalGetSet::PrivateEnableAnyEventMouseMode(const bool fEnabled)
{
    DoSrvPrivateEnableAnyEventMouseMode(fEnabled);
    return TRUE;
}

// Routine Description:
// - Connects the PrivateEnableAlternateScroll call directly into our Driver Message servicing call inside Conhost.exe
//   PrivateEnableAlternateScroll is an internal-only "API" call that the vt commands can execute,
//     but it is not represented as a function call on out public API surface.
// Arguments:
// - fEnabled - set to true to enable alternate scroll mode, false to disable
// Return Value:
// - TRUE if successful (see DoSrvPrivateEnableAnyEventMouseMode). FALSE otherwise.
BOOL ConhostInternalGetSet::PrivateEnableAlternateScroll(const bool fEnabled)
{
    DoSrvPrivateEnableAlternateScroll(fEnabled);
    return TRUE;
}

// Routine Description:
// - Connects the PrivateEraseAll call directly into our Driver Message servicing call inside Conhost.exe
//   PrivateEraseAll is an internal-only "API" call that the vt commands can execute,
//     but it is not represented as a function call on our public API surface.
// Arguments:
// <none>
// Return Value:
// - TRUE if successful (see DoSrvPrivateEraseAll). FALSE otherwise.
BOOL ConhostInternalGetSet::PrivateEraseAll()
{
    return NT_SUCCESS(DoSrvPrivateEraseAll(_io.GetActiveOutputBuffer()));
}

// Routine Description:
// - Connects the SetCursorStyle call directly into our Driver Message servicing call inside Conhost.exe
//   SetCursorStyle is an internal-only "API" call that the vt commands can execute,
//     but it is not represented as a function call on our public API surface.
// Arguments:
// - cursorType: The style of cursor to change the cursor to.
// Return Value:
// - TRUE if successful (see DoSrvSetCursorStyle). FALSE otherwise.
BOOL ConhostInternalGetSet::SetCursorStyle(const CursorType cursorType)
{
    DoSrvSetCursorStyle(_io.GetActiveOutputBuffer(), cursorType);
    return TRUE;
}

// Routine Description:
// - Retrieves the default color attributes information for the active screen buffer.
// - This function is used to optimize SGR calls in lieu of calling GetConsoleScreenBufferInfoEx.
// Arguments:
// - pwAttributes - Pointer to space to receive color attributes data
// Return Value:
// - TRUE if successful. FALSE otherwise.
BOOL ConhostInternalGetSet::PrivateGetConsoleScreenBufferAttributes(_Out_ WORD* const pwAttributes)
{
    return NT_SUCCESS(DoSrvPrivateGetConsoleScreenBufferAttributes(_io.GetActiveOutputBuffer(), pwAttributes));
}

// Routine Description:
// - Connects the PrivatePrependConsoleInput API call directly into our Driver Message servicing call inside Conhost.exe
// Arguments:
// - events - the input events to be copied into the head of the input
// buffer for the underlying attached process
// - eventsWritten - on output, the number of events written
// Return Value:
// - TRUE if successful (see DoSrvPrivatePrependConsoleInput). FALSE otherwise.
BOOL ConhostInternalGetSet::PrivatePrependConsoleInput(_Inout_ std::deque<std::unique_ptr<IInputEvent>>& events,
                                                       _Out_ size_t& eventsWritten)
{
    return SUCCEEDED(DoSrvPrivatePrependConsoleInput(_io.GetActiveInputBuffer(),
                                                     events,
                                                     eventsWritten));
}

// Routine Description:
// - Connects the PrivatePrependConsoleInput API call directly into our Driver Message servicing call inside Conhost.exe
// Arguments:
// - <none>
// Return Value:
// - TRUE if successful (see DoSrvPrivateRefreshWindow). FALSE otherwise.
BOOL ConhostInternalGetSet::PrivateRefreshWindow()
{
    DoSrvPrivateRefreshWindow(_io.GetActiveOutputBuffer());
    return TRUE;
}

// Routine Description:
// - Connects the PrivateWriteConsoleControlInput API call directly into our Driver Message servicing call inside Conhost.exe
// Arguments:
// - key - a KeyEvent representing a special type of keypress, typically Ctrl-C
// Return Value:
// - TRUE if successful (see DoSrvPrivateWriteConsoleControlInput). FALSE otherwise.
BOOL ConhostInternalGetSet::PrivateWriteConsoleControlInput(_In_ KeyEvent key)
{
    return SUCCEEDED(DoSrvPrivateWriteConsoleControlInput(_io.GetActiveInputBuffer(),
                                                          key));
}

// Routine Description:
// - Connects the GetConsoleOutputCP API call directly into our Driver Message servicing call inside Conhost.exe
// Arguments:
// - puiOutputCP - recieves the outputCP of the console.
// Return Value:
// - TRUE if successful (see DoSrvPrivateWriteConsoleControlInput). FALSE otherwise.
BOOL ConhostInternalGetSet::GetConsoleOutputCP(_Out_ unsigned int* const puiOutputCP)
{
    DoSrvGetConsoleOutputCodePage(puiOutputCP);
    return TRUE;
}

// Routine Description:
// - Connects the PrivateSuppressResizeRepaint API call directly into our Driver
//      Message servicing call inside Conhost.exe
// Arguments:
// - <none>
// Return Value:
// - TRUE if successful (see DoSrvPrivateSuppressResizeRepaint). FALSE otherwise.
BOOL ConhostInternalGetSet::PrivateSuppressResizeRepaint()
{
    return SUCCEEDED(DoSrvPrivateSuppressResizeRepaint());
}

// Routine Description:
// - Connects the SetCursorStyle call directly into our Driver Message servicing call inside Conhost.exe
//   SetCursorStyle is an internal-only "API" call that the vt commands can execute,
//     but it is not represented as a function call on our public API surface.
// Arguments:
// - cursorColor: The color to change the cursor to. INVALID_COLOR will revert
//      it to the legacy inverting behavior.
// Return Value:
// - TRUE if successful (see DoSrvSetCursorStyle). FALSE otherwise.
BOOL ConhostInternalGetSet::SetCursorColor(const COLORREF cursorColor)
{
    DoSrvSetCursorColor(_io.GetActiveOutputBuffer(), cursorColor);
    return TRUE;
}

// Routine Description:
// - Connects the IsConsolePty call directly into our Driver Message servicing call inside Conhost.exe
// Arguments:
// - isPty: recieves the bool indicating whether or not we're in pty mode.
// Return Value:
// - TRUE if successful (see DoSrvIsConsolePty). FALSE otherwise.
BOOL ConhostInternalGetSet::IsConsolePty(_Out_ bool* const pIsPty) const
{
    DoSrvIsConsolePty(pIsPty);
    return TRUE;
}

BOOL ConhostInternalGetSet::DeleteLines(const unsigned int count)
{
    DoSrvPrivateDeleteLines(count);
    return TRUE;
}

BOOL ConhostInternalGetSet::InsertLines(const unsigned int count)
{
    DoSrvPrivateInsertLines(count);
    return TRUE;
}

// Method Description:
// - Connects the MoveToBottom call directly into our Driver Message servicing
//      call inside Conhost.exe
// Arguments:
// <none>
// Return Value:
// - TRUE if successful (see DoSrvPrivateMoveToBottom). FALSE otherwise.
BOOL ConhostInternalGetSet::MoveToBottom() const
{
    DoSrvPrivateMoveToBottom(_io.GetActiveOutputBuffer());
    return TRUE;
}

// Method Description:
// - Connects the PrivateSetColorTableEntry call directly into our Driver Message servicing
//      call inside Conhost.exe
// Arguments:
// - index: the index in the table to change.
// - value: the new RGB value to use for that index in the color table.
// Return Value:
// - TRUE if successful (see DoSrvPrivateSetColorTableEntry). FALSE otherwise.
BOOL ConhostInternalGetSet::PrivateSetColorTableEntry(const short index, const COLORREF value) const noexcept
{
    return SUCCEEDED(DoSrvPrivateSetColorTableEntry(index, value));
}

// Method Description:
// - Connects the PrivateSetDefaultForeground call directly into our Driver Message servicing
//      call inside Conhost.exe
// Arguments:
// - value: the new RGB value to use, as a COLORREF, format 0x00BBGGRR.
// Return Value:
// - TRUE if successful (see DoSrvPrivateSetDefaultForegroundColor). FALSE otherwise.
BOOL ConhostInternalGetSet::PrivateSetDefaultForeground(const COLORREF value) const noexcept
{
    return SUCCEEDED(DoSrvPrivateSetDefaultForegroundColor(value));
}

// Method Description:
// - Connects the PrivateSetDefaultBackground call directly into our Driver Message servicing
//      call inside Conhost.exe
// Arguments:
// - value: the new RGB value to use, as a COLORREF, format 0x00BBGGRR.
// Return Value:
// - TRUE if successful (see DoSrvPrivateSetDefaultBackgroundColor). FALSE otherwise.
BOOL ConhostInternalGetSet::PrivateSetDefaultBackground(const COLORREF value) const noexcept
{
    return SUCCEEDED(DoSrvPrivateSetDefaultBackgroundColor(value));
}
