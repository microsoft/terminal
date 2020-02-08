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
// - string - The string to be printed.
// Return Value:
// - <none>
void WriteBuffer::PrintString(const std::wstring_view string)
{
    _DefaultStringCase(string);
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
    _DefaultStringCase({ &wch, 1 });
}

// Routine Description:
// - Default text editing/printing handler for all characters that were not routed elsewhere by other state machine intercepts.
// Arguments:
// - string - The string to be processed by our default text editing/printing mechanisms.
// Return Value:
// - <none>
void WriteBuffer::_DefaultStringCase(const std::wstring_view string)
{
    size_t dwNumBytes = string.size() * sizeof(wchar_t);

    _io.GetActiveOutputBuffer().GetTextBuffer().GetCursor().SetIsOn(true);

    _ntstatus = WriteCharsLegacy(_io.GetActiveOutputBuffer(),
                                 string.data(),
                                 string.data(),
                                 string.data(),
                                 &dwNumBytes,
                                 nullptr,
                                 _io.GetActiveOutputBuffer().GetTextBuffer().GetCursor().GetPosition().X,
                                 WC_LIMIT_BACKSPACE | WC_DELAY_EOL_WRAP,
                                 nullptr);
}

ConhostInternalGetSet::ConhostInternalGetSet(_In_ IIoProvider& io) :
    _io{ io }
{
}

// Routine Description:
// - Connects the GetConsoleScreenBufferInfoEx API call directly into our Driver Message servicing call inside Conhost.exe
// Arguments:
// - screenBufferInfo - Structure to hold screen buffer information like the public API call.
// Return Value:
// - true if successful (see DoSrvGetConsoleScreenBufferInfo). false otherwise.
bool ConhostInternalGetSet::GetConsoleScreenBufferInfoEx(CONSOLE_SCREEN_BUFFER_INFOEX& screenBufferInfo) const
{
    ServiceLocator::LocateGlobals().api.GetConsoleScreenBufferInfoExImpl(_io.GetActiveOutputBuffer(), screenBufferInfo);
    return true;
}

// Routine Description:
// - Connects the SetConsoleScreenBufferInfoEx API call directly into our Driver Message servicing call inside Conhost.exe
// Arguments:
// - screenBufferInfo - Structure containing screen buffer information like the public API call.
// Return Value:
// - true if successful (see DoSrvSetConsoleScreenBufferInfo). false otherwise.
bool ConhostInternalGetSet::SetConsoleScreenBufferInfoEx(const CONSOLE_SCREEN_BUFFER_INFOEX& screenBufferInfo)
{
    return SUCCEEDED(ServiceLocator::LocateGlobals().api.SetConsoleScreenBufferInfoExImpl(_io.GetActiveOutputBuffer(), screenBufferInfo));
}

// Routine Description:
// - Connects the SetConsoleCursorPosition API call directly into our Driver Message servicing call inside Conhost.exe
// Arguments:
// - position - new cursor position to set like the public API call.
// Return Value:
// - true if successful (see DoSrvSetConsoleCursorPosition). false otherwise.
bool ConhostInternalGetSet::SetConsoleCursorPosition(const COORD position)
{
    return SUCCEEDED(ServiceLocator::LocateGlobals().api.SetConsoleCursorPositionImpl(_io.GetActiveOutputBuffer(), position));
}

// Routine Description:
// - Connects the GetConsoleCursorInfo API call directly into our Driver Message servicing call inside Conhost.exe
// Arguments:
// - cursorInfo - Structure to receive console cursor rendering info
// Return Value:
// - true if successful (see DoSrvGetConsoleCursorInfo). false otherwise.
bool ConhostInternalGetSet::GetConsoleCursorInfo(CONSOLE_CURSOR_INFO& cursorInfo) const
{
    bool visible;
    DWORD size;

    ServiceLocator::LocateGlobals().api.GetConsoleCursorInfoImpl(_io.GetActiveOutputBuffer(), size, visible);
    cursorInfo.bVisible = visible;
    cursorInfo.dwSize = size;
    return true;
}

// Routine Description:
// - Connects the SetConsoleCursorInfo API call directly into our Driver Message servicing call inside Conhost.exe
// Arguments:
// - cursorInfo - Updated size/visibility information to modify the cursor rendering behavior.
// Return Value:
// - true if successful (see DoSrvSetConsoleCursorInfo). false otherwise.
bool ConhostInternalGetSet::SetConsoleCursorInfo(const CONSOLE_CURSOR_INFO& cursorInfo)
{
    const bool visible = !!cursorInfo.bVisible;
    return SUCCEEDED(ServiceLocator::LocateGlobals().api.SetConsoleCursorInfoImpl(_io.GetActiveOutputBuffer(), cursorInfo.dwSize, visible));
}

// Routine Description:
// - Connects the SetConsoleTextAttribute API call directly into our Driver Message servicing call inside Conhost.exe
//     Sets BOTH the FG and the BG component of the attributes.
// Arguments:
// - attr - new color/graphical attributes to apply as default within the console text buffer
// Return Value:
// - true if successful (see DoSrvSetConsoleTextAttribute). false otherwise.
bool ConhostInternalGetSet::SetConsoleTextAttribute(const WORD attr)
{
    return SUCCEEDED(ServiceLocator::LocateGlobals().api.SetConsoleTextAttributeImpl(_io.GetActiveOutputBuffer(), attr));
}

// Routine Description:
// - Connects the PrivateSetDefaultAttributes API call directly into our Driver Message servicing call inside Conhost.exe
//     Sets the FG and/or BG to the Default attributes values.
// Arguments:
// - foreground - Set the foreground to the default attributes
// - background - Set the background to the default attributes
// Return Value:
// - true if successful (see DoSrvPrivateSetDefaultAttributes). false otherwise.
bool ConhostInternalGetSet::PrivateSetDefaultAttributes(const bool foreground,
                                                        const bool background)
{
    DoSrvPrivateSetDefaultAttributes(_io.GetActiveOutputBuffer(), foreground, background);
    return true;
}

// Routine Description:
// - Connects the PrivateSetLegacyAttributes API call directly into our Driver Message servicing call inside Conhost.exe
//     Sets only the components of the attributes requested with the foreground, background, and meta flags.
// Arguments:
// - attr - new color/graphical attributes to apply as default within the console text buffer
// - foreground - The new attributes contain an update to the foreground attributes
// - background - The new attributes contain an update to the background attributes
// - meta - The new attributes contain an update to the meta attributes
// Return Value:
// - true if successful (see DoSrvVtSetLegacyAttributes). false otherwise.
bool ConhostInternalGetSet::PrivateSetLegacyAttributes(const WORD attr,
                                                       const bool foreground,
                                                       const bool background,
                                                       const bool meta)
{
    DoSrvPrivateSetLegacyAttributes(_io.GetActiveOutputBuffer(), attr, foreground, background, meta);
    return true;
}

// Routine Description:
// - Sets the current attributes of the screen buffer to use the color table entry specified by
//     the xtermTableEntry. Sets either the FG or the BG component of the attributes.
// Arguments:
// - xtermTableEntry - The entry of the xterm table to use.
// - isForeground - Whether or not the color applies to the foreground.
// Return Value:
// - true if successful (see DoSrvPrivateSetConsoleXtermTextAttribute). false otherwise.
bool ConhostInternalGetSet::SetConsoleXtermTextAttribute(const int xtermTableEntry, const bool isForeground)
{
    DoSrvPrivateSetConsoleXtermTextAttribute(_io.GetActiveOutputBuffer(), xtermTableEntry, isForeground);
    return true;
}

// Routine Description:
// - Sets the current attributes of the screen buffer to use the given rgb color.
//     Sets either the FG or the BG component of the attributes.
// Arguments:
// - rgbColor - The rgb color to use.
// - isForeground - Whether or not the color applies to the foreground.
// Return Value:
// - true if successful (see DoSrvPrivateSetConsoleRGBTextAttribute). false otherwise.
bool ConhostInternalGetSet::SetConsoleRGBTextAttribute(const COLORREF rgbColor, const bool isForeground)
{
    DoSrvPrivateSetConsoleRGBTextAttribute(_io.GetActiveOutputBuffer(), rgbColor, isForeground);
    return true;
}

bool ConhostInternalGetSet::PrivateBoldText(const bool bolded)
{
    DoSrvPrivateBoldText(_io.GetActiveOutputBuffer(), bolded);
    return true;
}

// Method Description:
// - Retrieves the currently active ExtendedAttributes. See also
//   DoSrvPrivateGetExtendedTextAttributes
// Arguments:
// - attrs: Recieves the ExtendedAttributes value.
// Return Value:
// - true if successful (see DoSrvPrivateGetExtendedTextAttributes). false otherwise.
bool ConhostInternalGetSet::PrivateGetExtendedTextAttributes(ExtendedAttributes& attrs)
{
    attrs = DoSrvPrivateGetExtendedTextAttributes(_io.GetActiveOutputBuffer());
    return true;
}

// Method Description:
// - Sets the active ExtendedAttributes of the active screen buffer. Text
//   written to this buffer will be written with these attributes.
// Arguments:
// - extendedAttrs: The new ExtendedAttributes to use
// Return Value:
// - true if successful (see DoSrvPrivateSetExtendedTextAttributes). false otherwise.
bool ConhostInternalGetSet::PrivateSetExtendedTextAttributes(const ExtendedAttributes attrs)
{
    DoSrvPrivateSetExtendedTextAttributes(_io.GetActiveOutputBuffer(), attrs);
    return true;
}

// Method Description:
// - Retrieves the current TextAttribute of the active screen buffer.
// Arguments:
// - attrs: Receives the TextAttribute value.
// Return Value:
// - true if successful. false otherwise.
bool ConhostInternalGetSet::PrivateGetTextAttributes(TextAttribute& attrs) const
{
    attrs = _io.GetActiveOutputBuffer().GetAttributes();
    return true;
}

// Method Description:
// - Sets the current TextAttribute of the active screen buffer. Text
//   written to this buffer will be written with these attributes.
// Arguments:
// - attrs: The new TextAttribute to use
// Return Value:
// - true if successful. false otherwise.
bool ConhostInternalGetSet::PrivateSetTextAttributes(const TextAttribute& attrs)
{
    _io.GetActiveOutputBuffer().SetAttributes(attrs);
    return true;
}

// Routine Description:
// - Connects the WriteConsoleInput API call directly into our Driver Message servicing call inside Conhost.exe
// Arguments:
// - events - the input events to be copied into the head of the input
//            buffer for the underlying attached process
// - eventsWritten - on output, the number of events written
// Return Value:
// - true if successful (see DoSrvWriteConsoleInput). false otherwise.
bool ConhostInternalGetSet::PrivateWriteConsoleInputW(std::deque<std::unique_ptr<IInputEvent>>& events,
                                                      size_t& eventsWritten)
{
    eventsWritten = 0;

    return SUCCEEDED(DoSrvPrivateWriteConsoleInputW(_io.GetActiveInputBuffer(),
                                                    events,
                                                    eventsWritten,
                                                    true)); // append
}

// Routine Description:
// - Connects the SetConsoleWindowInfo API call directly into our Driver Message servicing call inside Conhost.exe
// Arguments:
// - absolute - Should the window be moved to an absolute position? If false, the movement is relative to the current pos.
// - window - Info about how to move the viewport
// Return Value:
// - true if successful (see DoSrvSetConsoleWindowInfo). false otherwise.
bool ConhostInternalGetSet::SetConsoleWindowInfo(const bool absolute, const SMALL_RECT& window)
{
    return SUCCEEDED(ServiceLocator::LocateGlobals().api.SetConsoleWindowInfoImpl(_io.GetActiveOutputBuffer(), absolute, window));
}

// Routine Description:
// - Connects the PrivateSetCursorKeysMode call directly into our Driver Message servicing call inside Conhost.exe
//   PrivateSetCursorKeysMode is an internal-only "API" call that the vt commands can execute,
//     but it is not represented as a function call on out public API surface.
// Arguments:
// - fApplicationMode - set to true to enable Application Mode Input, false for Normal Mode.
// Return Value:
// - true if successful (see DoSrvPrivateSetCursorKeysMode). false otherwise.
bool ConhostInternalGetSet::PrivateSetCursorKeysMode(const bool fApplicationMode)
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
// - true if successful (see DoSrvPrivateSetKeypadMode). false otherwise.
bool ConhostInternalGetSet::PrivateSetKeypadMode(const bool fApplicationMode)
{
    return NT_SUCCESS(DoSrvPrivateSetKeypadMode(fApplicationMode));
}

// Routine Description:
// - Connects the PrivateSetScreenMode call directly into our Driver Message servicing call inside Conhost.exe
//   PrivateSetScreenMode is an internal-only "API" call that the vt commands can execute,
//     but it is not represented as a function call on our public API surface.
// Arguments:
// - reverseMode - set to true to enable reverse screen mode, false for normal mode.
// Return Value:
// - true if successful (see DoSrvPrivateSetScreenMode). false otherwise.
bool ConhostInternalGetSet::PrivateSetScreenMode(const bool reverseMode)
{
    return NT_SUCCESS(DoSrvPrivateSetScreenMode(reverseMode));
}

// Routine Description:
// - Connects the PrivateSetAutoWrapMode call directly into our Driver Message servicing call inside Conhost.exe
//   PrivateSetAutoWrapMode is an internal-only "API" call that the vt commands can execute,
//     but it is not represented as a function call on out public API surface.
// Arguments:
// - wrapAtEOL - set to true to wrap, false to overwrite the last character.
// Return Value:
// - true if successful (see DoSrvPrivateSetAutoWrapMode). false otherwise.
bool ConhostInternalGetSet::PrivateSetAutoWrapMode(const bool wrapAtEOL)
{
    return NT_SUCCESS(DoSrvPrivateSetAutoWrapMode(wrapAtEOL));
}

// Routine Description:
// - Connects the PrivateShowCursor call directly into our Driver Message servicing call inside Conhost.exe
//   PrivateShowCursor is an internal-only "API" call that the vt commands can execute,
//     but it is not represented as a function call on out public API surface.
// Arguments:
// - show - set to true to make the cursor visible, false to hide.
// Return Value:
// - true if successful (see DoSrvPrivateShowCursor). false otherwise.
bool ConhostInternalGetSet::PrivateShowCursor(const bool show) noexcept
{
    DoSrvPrivateShowCursor(_io.GetActiveOutputBuffer(), show);
    return true;
}

// Routine Description:
// - Connects the PrivateAllowCursorBlinking call directly into our Driver Message servicing call inside Conhost.exe
//   PrivateAllowCursorBlinking is an internal-only "API" call that the vt commands can execute,
//     but it is not represented as a function call on out public API surface.
// Arguments:
// - fEnable - set to true to enable blinking, false to disable
// Return Value:
// - true if successful (see DoSrvPrivateAllowCursorBlinking). false otherwise.
bool ConhostInternalGetSet::PrivateAllowCursorBlinking(const bool fEnable)
{
    DoSrvPrivateAllowCursorBlinking(_io.GetActiveOutputBuffer(), fEnable);
    return true;
}

// Routine Description:
// - Connects the PrivateSetScrollingRegion call directly into our Driver Message servicing call inside Conhost.exe
//   PrivateSetScrollingRegion is an internal-only "API" call that the vt commands can execute,
//     but it is not represented as a function call on out public API surface.
// Arguments:
// - scrollMargins - The bounds of the region to be the scrolling region of the viewport.
// Return Value:
// - true if successful (see DoSrvPrivateSetScrollingRegion). false otherwise.
bool ConhostInternalGetSet::PrivateSetScrollingRegion(const SMALL_RECT& scrollMargins)
{
    return NT_SUCCESS(DoSrvPrivateSetScrollingRegion(_io.GetActiveOutputBuffer(), scrollMargins));
}

// Method Description:
// - Retrieves the current Line Feed/New Line (LNM) mode.
// Arguments:
// - None
// Return Value:
// - true if a line feed also produces a carriage return. false otherwise.
bool ConhostInternalGetSet::PrivateGetLineFeedMode() const
{
    const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    return gci.IsReturnOnNewlineAutomatic();
}

// Routine Description:
// - Connects the PrivateLineFeed call directly into our Driver Message servicing call inside Conhost.exe
//   PrivateLineFeed is an internal-only "API" call that the vt commands can execute,
//     but it is not represented as a function call on our public API surface.
// Arguments:
// - withReturn - Set to true if a carriage return should be performed as well.
// Return Value:
// - true if successful (see DoSrvPrivateLineFeed). false otherwise.
bool ConhostInternalGetSet::PrivateLineFeed(const bool withReturn)
{
    return NT_SUCCESS(DoSrvPrivateLineFeed(_io.GetActiveOutputBuffer(), withReturn));
}

// Routine Description:
// - Sends a notify message to play the "SystemHand" sound event.
// Return Value:
// - true if successful. false otherwise.
bool ConhostInternalGetSet::PrivateWarningBell()
{
    return _io.GetActiveOutputBuffer().SendNotifyBeep();
}

// Routine Description:
// - Connects the PrivateReverseLineFeed call directly into our Driver Message servicing call inside Conhost.exe
//   PrivateReverseLineFeed is an internal-only "API" call that the vt commands can execute,
//     but it is not represented as a function call on out public API surface.
// Return Value:
// - true if successful (see DoSrvPrivateReverseLineFeed). false otherwise.
bool ConhostInternalGetSet::PrivateReverseLineFeed()
{
    return NT_SUCCESS(DoSrvPrivateReverseLineFeed(_io.GetActiveOutputBuffer()));
}

// Routine Description:
// - Connects the SetConsoleTitleW API call directly into our Driver Message servicing call inside Conhost.exe
// Arguments:
// - title - The null-terminated string to set as the window title
// Return Value:
// - true if successful (see DoSrvSetConsoleTitle). false otherwise.
bool ConhostInternalGetSet::SetConsoleTitleW(std::wstring_view title)
{
    return SUCCEEDED(DoSrvSetConsoleTitleW(title));
}

// Routine Description:
// - Connects the PrivateUseAlternateScreenBuffer call directly into our Driver Message servicing call inside Conhost.exe
//   PrivateUseAlternateScreenBuffer is an internal-only "API" call that the vt commands can execute,
//     but it is not represented as a function call on out public API surface.
// Return Value:
// - true if successful (see DoSrvPrivateUseAlternateScreenBuffer). false otherwise.
bool ConhostInternalGetSet::PrivateUseAlternateScreenBuffer()
{
    return NT_SUCCESS(DoSrvPrivateUseAlternateScreenBuffer(_io.GetActiveOutputBuffer()));
}

// Routine Description:
// - Connects the PrivateUseMainScreenBuffer call directly into our Driver Message servicing call inside Conhost.exe
//   PrivateUseMainScreenBuffer is an internal-only "API" call that the vt commands can execute,
//     but it is not represented as a function call on out public API surface.
// Return Value:
// - true if successful (see DoSrvPrivateUseMainScreenBuffer). false otherwise.
bool ConhostInternalGetSet::PrivateUseMainScreenBuffer()
{
    DoSrvPrivateUseMainScreenBuffer(_io.GetActiveOutputBuffer());
    return true;
}

// - Connects the PrivateHorizontalTabSet call directly into our Driver Message servicing call inside Conhost.exe
//   PrivateHorizontalTabSet is an internal-only "API" call that the vt commands can execute,
//     but it is not represented as a function call on out public API surface.
// Arguments:
// <none>
// Return Value:
// - true if successful (see PrivateHorizontalTabSet). false otherwise.
bool ConhostInternalGetSet::PrivateHorizontalTabSet()
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
// - true if successful (see PrivateForwardTab). false otherwise.
bool ConhostInternalGetSet::PrivateForwardTab(const size_t numTabs)
{
    SHORT tabs;
    if (FAILED(SizeTToShort(numTabs, &tabs)))
    {
        return false;
    }

    return NT_SUCCESS(DoSrvPrivateForwardTab(tabs));
}

// Routine Description:
// - Connects the PrivateBackwardsTab call directly into our Driver Message servicing call inside Conhost.exe
//   PrivateBackwardsTab is an internal-only "API" call that the vt commands can execute,
//     but it is not represented as a function call on out public API surface.
// Arguments:
// - numTabs - the number of tabs to execute
// Return Value:
// - true if successful (see PrivateBackwardsTab). false otherwise.
bool ConhostInternalGetSet::PrivateBackwardsTab(const size_t numTabs)
{
    SHORT tabs;
    if (FAILED(SizeTToShort(numTabs, &tabs)))
    {
        return false;
    }

    return NT_SUCCESS(DoSrvPrivateBackwardsTab(tabs));
}

// Routine Description:
// - Connects the PrivateTabClear call directly into our Driver Message servicing call inside Conhost.exe
//   PrivateTabClear is an internal-only "API" call that the vt commands can execute,
//     but it is not represented as a function call on out public API surface.
// Arguments:
// - clearAll - set to true to enable blinking, false to disable
// Return Value:
// - true if successful (see PrivateTabClear). false otherwise.
bool ConhostInternalGetSet::PrivateTabClear(const bool clearAll)
{
    DoSrvPrivateTabClear(clearAll);
    return true;
}

// Routine Description:
// - Connects the PrivateSetDefaultTabStops call directly into the private api point
// Return Value:
// - true
bool ConhostInternalGetSet::PrivateSetDefaultTabStops()
{
    DoSrvPrivateSetDefaultTabStops();
    return true;
}

// Routine Description:
// - Connects the PrivateEnableVT200MouseMode call directly into our Driver Message servicing call inside Conhost.exe
//   PrivateEnableVT200MouseMode is an internal-only "API" call that the vt commands can execute,
//     but it is not represented as a function call on out public API surface.
// Arguments:
// - enabled - set to true to enable vt200 mouse mode, false to disable
// Return Value:
// - true if successful (see DoSrvPrivateEnableVT200MouseMode). false otherwise.
bool ConhostInternalGetSet::PrivateEnableVT200MouseMode(const bool enabled)
{
    DoSrvPrivateEnableVT200MouseMode(enabled);
    return true;
}

// Routine Description:
// - Connects the PrivateEnableUTF8ExtendedMouseMode call directly into our Driver Message servicing call inside Conhost.exe
//   PrivateEnableUTF8ExtendedMouseMode is an internal-only "API" call that the vt commands can execute,
//     but it is not represented as a function call on out public API surface.
// Arguments:
// - enabled - set to true to enable utf8 extended mouse mode, false to disable
// Return Value:
// - true if successful (see DoSrvPrivateEnableUTF8ExtendedMouseMode). false otherwise.
bool ConhostInternalGetSet::PrivateEnableUTF8ExtendedMouseMode(const bool enabled)
{
    DoSrvPrivateEnableUTF8ExtendedMouseMode(enabled);
    return true;
}

// Routine Description:
// - Connects the PrivateEnableSGRExtendedMouseMode call directly into our Driver Message servicing call inside Conhost.exe
//   PrivateEnableSGRExtendedMouseMode is an internal-only "API" call that the vt commands can execute,
//     but it is not represented as a function call on out public API surface.
// Arguments:
// - enabled - set to true to enable SGR extended mouse mode, false to disable
// Return Value:
// - true if successful (see DoSrvPrivateEnableSGRExtendedMouseMode). false otherwise.
bool ConhostInternalGetSet::PrivateEnableSGRExtendedMouseMode(const bool enabled)
{
    DoSrvPrivateEnableSGRExtendedMouseMode(enabled);
    return true;
}

// Routine Description:
// - Connects the PrivateEnableButtonEventMouseMode call directly into our Driver Message servicing call inside Conhost.exe
//   PrivateEnableButtonEventMouseMode is an internal-only "API" call that the vt commands can execute,
//     but it is not represented as a function call on out public API surface.
// Arguments:
// - enabled - set to true to enable button-event mouse mode, false to disable
// Return Value:
// - true if successful (see DoSrvPrivateEnableButtonEventMouseMode). false otherwise.
bool ConhostInternalGetSet::PrivateEnableButtonEventMouseMode(const bool enabled)
{
    DoSrvPrivateEnableButtonEventMouseMode(enabled);
    return true;
}

// Routine Description:
// - Connects the PrivateEnableAnyEventMouseMode call directly into our Driver Message servicing call inside Conhost.exe
//   PrivateEnableAnyEventMouseMode is an internal-only "API" call that the vt commands can execute,
//     but it is not represented as a function call on out public API surface.
// Arguments:
// - enabled - set to true to enable any-event mouse mode, false to disable
// Return Value:
// - true if successful (see DoSrvPrivateEnableAnyEventMouseMode). false otherwise.
bool ConhostInternalGetSet::PrivateEnableAnyEventMouseMode(const bool enabled)
{
    DoSrvPrivateEnableAnyEventMouseMode(enabled);
    return true;
}

// Routine Description:
// - Connects the PrivateEnableAlternateScroll call directly into our Driver Message servicing call inside Conhost.exe
//   PrivateEnableAlternateScroll is an internal-only "API" call that the vt commands can execute,
//     but it is not represented as a function call on out public API surface.
// Arguments:
// - enabled - set to true to enable alternate scroll mode, false to disable
// Return Value:
// - true if successful (see DoSrvPrivateEnableAnyEventMouseMode). false otherwise.
bool ConhostInternalGetSet::PrivateEnableAlternateScroll(const bool enabled)
{
    DoSrvPrivateEnableAlternateScroll(enabled);
    return true;
}

// Routine Description:
// - Connects the PrivateEraseAll call directly into our Driver Message servicing call inside Conhost.exe
//   PrivateEraseAll is an internal-only "API" call that the vt commands can execute,
//     but it is not represented as a function call on our public API surface.
// Arguments:
// <none>
// Return Value:
// - true if successful (see DoSrvPrivateEraseAll). false otherwise.
bool ConhostInternalGetSet::PrivateEraseAll()
{
    return NT_SUCCESS(DoSrvPrivateEraseAll(_io.GetActiveOutputBuffer()));
}

// Routine Description:
// - Connects the SetCursorStyle call directly into our Driver Message servicing call inside Conhost.exe
//   SetCursorStyle is an internal-only "API" call that the vt commands can execute,
//     but it is not represented as a function call on our public API surface.
// Arguments:
// - style: The style of cursor to change the cursor to.
// Return Value:
// - true if successful (see DoSrvSetCursorStyle). false otherwise.
bool ConhostInternalGetSet::SetCursorStyle(const CursorType style)
{
    DoSrvSetCursorStyle(_io.GetActiveOutputBuffer(), style);
    return true;
}

// Routine Description:
// - Retrieves the default color attributes information for the active screen buffer.
// - This function is used to optimize SGR calls in lieu of calling GetConsoleScreenBufferInfoEx.
// Arguments:
// - pwAttributes - Pointer to space to receive color attributes data
// Return Value:
// - true if successful. false otherwise.
bool ConhostInternalGetSet::PrivateGetConsoleScreenBufferAttributes(WORD& attributes)
{
    return NT_SUCCESS(DoSrvPrivateGetConsoleScreenBufferAttributes(_io.GetActiveOutputBuffer(), attributes));
}

// Routine Description:
// - Connects the PrivatePrependConsoleInput API call directly into our Driver Message servicing call inside Conhost.exe
// Arguments:
// - events - the input events to be copied into the head of the input
//            buffer for the underlying attached process
// - eventsWritten - on output, the number of events written
// Return Value:
// - true if successful (see DoSrvPrivatePrependConsoleInput). false otherwise.
bool ConhostInternalGetSet::PrivatePrependConsoleInput(std::deque<std::unique_ptr<IInputEvent>>& events,
                                                       size_t& eventsWritten)
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
// - true if successful (see DoSrvPrivateRefreshWindow). false otherwise.
bool ConhostInternalGetSet::PrivateRefreshWindow()
{
    DoSrvPrivateRefreshWindow(_io.GetActiveOutputBuffer());
    return true;
}

// Routine Description:
// - Connects the PrivateWriteConsoleControlInput API call directly into our Driver Message servicing call inside Conhost.exe
// Arguments:
// - key - a KeyEvent representing a special type of keypress, typically Ctrl-C
// Return Value:
// - true if successful (see DoSrvPrivateWriteConsoleControlInput). false otherwise.
bool ConhostInternalGetSet::PrivateWriteConsoleControlInput(const KeyEvent key)
{
    return SUCCEEDED(DoSrvPrivateWriteConsoleControlInput(_io.GetActiveInputBuffer(),
                                                          key));
}

// Routine Description:
// - Connects the GetConsoleOutputCP API call directly into our Driver Message servicing call inside Conhost.exe
// Arguments:
// - codepage - recieves the outputCP of the console.
// Return Value:
// - true if successful (see DoSrvPrivateWriteConsoleControlInput). false otherwise.
bool ConhostInternalGetSet::GetConsoleOutputCP(unsigned int& codepage)
{
    DoSrvGetConsoleOutputCodePage(codepage);
    return true;
}

// Routine Description:
// - Connects the PrivateSuppressResizeRepaint API call directly into our Driver
//      Message servicing call inside Conhost.exe
// Arguments:
// - <none>
// Return Value:
// - true if successful (see DoSrvPrivateSuppressResizeRepaint). false otherwise.
bool ConhostInternalGetSet::PrivateSuppressResizeRepaint()
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
// - true if successful (see DoSrvSetCursorStyle). false otherwise.
bool ConhostInternalGetSet::SetCursorColor(const COLORREF cursorColor)
{
    DoSrvSetCursorColor(_io.GetActiveOutputBuffer(), cursorColor);
    return true;
}

// Routine Description:
// - Connects the IsConsolePty call directly into our Driver Message servicing call inside Conhost.exe
// Arguments:
// - isPty: recieves the bool indicating whether or not we're in pty mode.
// Return Value:
// - true if successful (see DoSrvIsConsolePty). false otherwise.
bool ConhostInternalGetSet::IsConsolePty(bool& isPty) const
{
    DoSrvIsConsolePty(isPty);
    return true;
}

bool ConhostInternalGetSet::DeleteLines(const size_t count)
{
    DoSrvPrivateDeleteLines(count);
    return true;
}

bool ConhostInternalGetSet::InsertLines(const size_t count)
{
    DoSrvPrivateInsertLines(count);
    return true;
}

// Method Description:
// - Connects the MoveToBottom call directly into our Driver Message servicing
//      call inside Conhost.exe
// Arguments:
// <none>
// Return Value:
// - true if successful (see DoSrvPrivateMoveToBottom). false otherwise.
bool ConhostInternalGetSet::MoveToBottom() const
{
    DoSrvPrivateMoveToBottom(_io.GetActiveOutputBuffer());
    return true;
}

// Method Description:
// - Connects the PrivateSetColorTableEntry call directly into our Driver Message servicing
//      call inside Conhost.exe
// Arguments:
// - index: the index in the table to change.
// - value: the new RGB value to use for that index in the color table.
// Return Value:
// - true if successful (see DoSrvPrivateSetColorTableEntry). false otherwise.
bool ConhostInternalGetSet::PrivateSetColorTableEntry(const short index, const COLORREF value) const noexcept
{
    return SUCCEEDED(DoSrvPrivateSetColorTableEntry(index, value));
}

// Method Description:
// - Connects the PrivateSetDefaultForeground call directly into our Driver Message servicing
//      call inside Conhost.exe
// Arguments:
// - value: the new RGB value to use, as a COLORREF, format 0x00BBGGRR.
// Return Value:
// - true if successful (see DoSrvPrivateSetDefaultForegroundColor). false otherwise.
bool ConhostInternalGetSet::PrivateSetDefaultForeground(const COLORREF value) const noexcept
{
    return SUCCEEDED(DoSrvPrivateSetDefaultForegroundColor(value));
}

// Method Description:
// - Connects the PrivateSetDefaultBackground call directly into our Driver Message servicing
//      call inside Conhost.exe
// Arguments:
// - value: the new RGB value to use, as a COLORREF, format 0x00BBGGRR.
// Return Value:
// - true if successful (see DoSrvPrivateSetDefaultBackgroundColor). false otherwise.
bool ConhostInternalGetSet::PrivateSetDefaultBackground(const COLORREF value) const noexcept
{
    return SUCCEEDED(DoSrvPrivateSetDefaultBackgroundColor(value));
}

// Routine Description:
// - Connects the PrivateFillRegion call directly into our Driver Message servicing
//    call inside Conhost.exe
//   PrivateFillRegion is an internal-only "API" call that the vt commands can execute,
//    but it is not represented as a function call on our public API surface.
// Arguments:
// - screenInfo - Reference to screen buffer info.
// - startPosition - The position to begin filling at.
// - fillLength - The number of characters to fill.
// - fillChar - Character to fill the target region with.
// - standardFillAttrs - If true, fill with the standard erase attributes.
//                       If false, fill with the default attributes.
// Return value:
// - true if successful (see DoSrvPrivateScrollRegion). false otherwise.
bool ConhostInternalGetSet::PrivateFillRegion(const COORD startPosition,
                                              const size_t fillLength,
                                              const wchar_t fillChar,
                                              const bool standardFillAttrs) noexcept
{
    return SUCCEEDED(DoSrvPrivateFillRegion(_io.GetActiveOutputBuffer(),
                                            startPosition,
                                            fillLength,
                                            fillChar,
                                            standardFillAttrs));
}

// Routine Description:
// - Connects the PrivateScrollRegion call directly into our Driver Message servicing
//    call inside Conhost.exe
//   PrivateScrollRegion is an internal-only "API" call that the vt commands can execute,
//    but it is not represented as a function call on our public API surface.
// Arguments:
// - scrollRect - Region to copy/move (source and size).
// - clipRect - Optional clip region to contain buffer change effects.
// - destinationOrigin - Upper left corner of target region.
// - standardFillAttrs - If true, fill with the standard erase attributes.
//                       If false, fill with the default attributes.
// Return value:
// - true if successful (see DoSrvPrivateScrollRegion). false otherwise.
bool ConhostInternalGetSet::PrivateScrollRegion(const SMALL_RECT scrollRect,
                                                const std::optional<SMALL_RECT> clipRect,
                                                const COORD destinationOrigin,
                                                const bool standardFillAttrs) noexcept
{
    return SUCCEEDED(DoSrvPrivateScrollRegion(_io.GetActiveOutputBuffer(),
                                              scrollRect,
                                              clipRect,
                                              destinationOrigin,
                                              standardFillAttrs));
}
