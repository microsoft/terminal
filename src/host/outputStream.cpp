// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "outputStream.hpp"

#include "_stream.h"
#include "getset.h"
#include "directio.h"
#include "output.h"

#include "../interactivity/inc/ServiceLocator.hpp"

#pragma hdrstop

using namespace Microsoft::Console;
using Microsoft::Console::Interactivity::ServiceLocator;
using Microsoft::Console::VirtualTerminal::StateMachine;

ConhostInternalGetSet::ConhostInternalGetSet(_In_ IIoProvider& io) :
    _io{ io }
{
}

// Routine Description:
// - Handles the print action from the state machine
// Arguments:
// - string - The string to be printed.
// Return Value:
// - <none>
void ConhostInternalGetSet::PrintString(const std::wstring_view string)
{
    size_t dwNumBytes = string.size() * sizeof(wchar_t);

    Cursor& cursor = _io.GetActiveOutputBuffer().GetTextBuffer().GetCursor();
    if (!cursor.IsOn())
    {
        cursor.SetIsOn(true);
    }

    // Defer the cursor drawing while we are iterating the string, for a better performance.
    // We can not waste time displaying a cursor event when we know more text is coming right behind it.
    cursor.StartDeferDrawing();
    const auto ntstatus = WriteCharsLegacy(_io.GetActiveOutputBuffer(),
                                           string.data(),
                                           string.data(),
                                           string.data(),
                                           &dwNumBytes,
                                           nullptr,
                                           _io.GetActiveOutputBuffer().GetTextBuffer().GetCursor().GetPosition().X,
                                           WC_LIMIT_BACKSPACE | WC_DELAY_EOL_WRAP,
                                           nullptr);
    cursor.EndDeferDrawing();

    THROW_IF_NTSTATUS_FAILED(ntstatus);
}

// Routine Description:
// - Retrieves the state machine for the active output buffer.
// Arguments:
// - <none>
// Return Value:
// - a reference to the StateMachine instance.
StateMachine& ConhostInternalGetSet::GetStateMachine()
{
    return _io.GetActiveOutputBuffer().GetStateMachine();
}

// Routine Description:
// - Retrieves the text buffer for the active output buffer.
// Arguments:
// - <none>
// Return Value:
// - a reference to the TextBuffer instance.
TextBuffer& ConhostInternalGetSet::GetTextBuffer()
{
    return _io.GetActiveOutputBuffer().GetTextBuffer();
}

// Routine Description:
// - Retrieves the virtual viewport of the active output buffer.
// Arguments:
// - <none>
// Return Value:
// - the exclusive coordinates of the viewport.
SMALL_RECT ConhostInternalGetSet::GetViewport() const
{
    return _io.GetActiveOutputBuffer().GetVirtualViewport().ToExclusive();
}

// Routine Description:
// - Sets the position of the window viewport.
// Arguments:
// - position - the new position of the viewport.
// Return Value:
// - <none>
void ConhostInternalGetSet::SetViewportPosition(const COORD position)
{
    auto& info = _io.GetActiveOutputBuffer();
    const auto dimensions = info.GetViewport().Dimensions();
    const auto windowRect = Viewport::FromDimensions(position, dimensions).ToInclusive();
    THROW_IF_FAILED(ServiceLocator::LocateGlobals().api->SetConsoleWindowInfoImpl(info, true, windowRect));
}

// Routine Description:
// - Connects the SetCursorPosition API call directly into our Driver Message servicing call inside Conhost.exe
// Arguments:
// - position - new cursor position to set like the public API call.
// Return Value:
// - <none>
void ConhostInternalGetSet::SetCursorPosition(const COORD position)
{
    auto& info = _io.GetActiveOutputBuffer();
    const auto clampedPosition = info.GetTextBuffer().ClampPositionWithinLine(position);
    THROW_IF_FAILED(ServiceLocator::LocateGlobals().api->SetConsoleCursorPositionImpl(info, clampedPosition));
}

// Method Description:
// - Sets the current TextAttribute of the active screen buffer. Text
//   written to this buffer will be written with these attributes.
// Arguments:
// - attrs: The new TextAttribute to use
// Return Value:
// - <none>
void ConhostInternalGetSet::SetTextAttributes(const TextAttribute& attrs)
{
    _io.GetActiveOutputBuffer().SetAttributes(attrs);
}

// Routine Description:
// - Writes events to the input buffer already formed into IInputEvents
// Arguments:
// - events - the input events to be copied into the head of the input
//            buffer for the underlying attached process
// - eventsWritten - on output, the number of events written
// Return Value:
// - <none>
void ConhostInternalGetSet::WriteInput(std::deque<std::unique_ptr<IInputEvent>>& events, size_t& eventsWritten)
{
    eventsWritten = _io.GetActiveInputBuffer()->Write(events);
}

// Routine Description:
// - Sets the various render modes.
// Arguments:
// - mode - the render mode to change.
// - enabled - set to true to enable the mode, false to disable it.
// Return Value:
// - <none>
void ConhostInternalGetSet::SetRenderMode(const RenderSettings::Mode mode, const bool enabled)
{
    auto& g = ServiceLocator::LocateGlobals();
    auto& renderSettings = g.getConsoleInformation().GetRenderSettings();
    renderSettings.SetRenderMode(mode, enabled);

    if (g.pRender)
    {
        g.pRender->TriggerRedrawAll();
    }
}

// Routine Description:
// - Sets the ENABLE_WRAP_AT_EOL_OUTPUT mode. This controls whether the cursor moves
//     to the beginning of the next row when it reaches the end of the current row.
// Arguments:
// - wrapAtEOL - set to true to wrap, false to overwrite the last character.
// Return Value:
// - <none>
void ConhostInternalGetSet::SetAutoWrapMode(const bool wrapAtEOL)
{
    auto& outputMode = _io.GetActiveOutputBuffer().OutputMode;
    WI_UpdateFlag(outputMode, ENABLE_WRAP_AT_EOL_OUTPUT, wrapAtEOL);
}

// Routine Description:
// - Sets the top and bottom scrolling margins for the current page. This creates
//     a subsection of the screen that scrolls when input reaches the end of the
//     region, leaving the rest of the screen untouched.
// Arguments:
// - scrollMargins - A rect who's Top and Bottom members will be used to set
//     the new values of the top and bottom margins. If (0,0), then the margins
//     will be disabled. NOTE: This is a rect in the case that we'll need the
//     left and right margins in the future.
// Return Value:
// - <none>
void ConhostInternalGetSet::SetScrollingRegion(const SMALL_RECT& scrollMargins)
{
    auto& screenInfo = _io.GetActiveOutputBuffer();
    auto srScrollMargins = screenInfo.GetRelativeScrollMargins().ToInclusive();
    srScrollMargins.Top = scrollMargins.Top;
    srScrollMargins.Bottom = scrollMargins.Bottom;
    screenInfo.SetScrollMargins(Viewport::FromInclusive(srScrollMargins));
}

// Method Description:
// - Retrieves the current Line Feed/New Line (LNM) mode.
// Arguments:
// - None
// Return Value:
// - true if a line feed also produces a carriage return. false otherwise.
bool ConhostInternalGetSet::GetLineFeedMode() const
{
    const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    return gci.IsReturnOnNewlineAutomatic();
}

// Routine Description:
// - Performs a line feed, possibly preceded by carriage return.
// Arguments:
// - withReturn - Set to true if a carriage return should be performed as well.
// Return Value:
// - <none>
void ConhostInternalGetSet::LineFeed(const bool withReturn)
{
    auto& screenInfo = _io.GetActiveOutputBuffer();
    auto& textBuffer = screenInfo.GetTextBuffer();
    auto cursorPosition = textBuffer.GetCursor().GetPosition();

    // We turn the cursor on before an operation that might scroll the viewport, otherwise
    // that can result in an old copy of the cursor being left behind on the screen.
    textBuffer.GetCursor().SetIsOn(true);

    // Since we are explicitly moving down a row, clear the wrap status on the row we're leaving
    textBuffer.GetRowByOffset(cursorPosition.Y).SetWrapForced(false);

    cursorPosition.Y += 1;
    if (withReturn)
    {
        cursorPosition.X = 0;
    }
    else
    {
        cursorPosition = textBuffer.ClampPositionWithinLine(cursorPosition);
    }

    THROW_IF_NTSTATUS_FAILED(AdjustCursorPosition(screenInfo, cursorPosition, FALSE, nullptr));
}

// Routine Description:
// - Sends a notify message to play the "SystemHand" sound event.
// Return Value:
// - <none>
void ConhostInternalGetSet::WarningBell()
{
    _io.GetActiveOutputBuffer().SendNotifyBeep();
}

// Routine Description:
// - Sets the title of the console window.
// Arguments:
// - title - The null-terminated string to set as the window title
// Return Value:
// - <none>
void ConhostInternalGetSet::SetWindowTitle(std::wstring_view title)
{
    ServiceLocator::LocateGlobals().getConsoleInformation().SetTitle(title);
}

// Routine Description:
// - Swaps to the alternate screen buffer. In virtual terminals, there exists both a "main"
//     screen buffer and an alternate. This creates a new alternate, and switches to it.
//     If there is an already existing alternate, it is discarded.
// Return Value:
// - <none>
void ConhostInternalGetSet::UseAlternateScreenBuffer()
{
    THROW_IF_NTSTATUS_FAILED(_io.GetActiveOutputBuffer().UseAlternateScreenBuffer());
}

// Routine Description:
// - Swaps to the main screen buffer. From the alternate buffer, returns to the main screen
//     buffer. From the main screen buffer, does nothing. The alternate is discarded.
// Return Value:
// - <none>
void ConhostInternalGetSet::UseMainScreenBuffer()
{
    _io.GetActiveOutputBuffer().UseMainScreenBuffer();
}

// Method Description:
// - Retrieves the current user default cursor style.
// Arguments:
// - <none>
// Return Value:
// - the default cursor style.
CursorType ConhostInternalGetSet::GetUserDefaultCursorStyle() const
{
    const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    return gci.GetCursorType();
}

// Routine Description:
// - Forces the renderer to repaint the screen. If the input screen buffer is
//      not the active one, then just do nothing. We only want to redraw the
//      screen buffer that requested the repaint, and switching screen buffers
//      will already force a repaint.
// Arguments:
// - <none>
// Return Value:
// - <none>
void ConhostInternalGetSet::RefreshWindow()
{
    auto& g = ServiceLocator::LocateGlobals();
    if (&_io.GetActiveOutputBuffer() == &g.getConsoleInformation().GetActiveOutputBuffer())
    {
        g.pRender->TriggerRedrawAll();
    }
}

// Routine Description:
// - Connects the SetConsoleOutputCP API call directly into our Driver Message servicing call inside Conhost.exe
// Arguments:
// - codepage - the new output codepage of the console.
// Return Value:
// - <none>
void ConhostInternalGetSet::SetConsoleOutputCP(const unsigned int codepage)
{
    THROW_IF_FAILED(DoSrvSetConsoleOutputCodePage(codepage));
}

// Routine Description:
// - Gets the codepage used for translating text when calling A versions of functions affecting the output buffer.
// Arguments:
// - <none>
// Return Value:
// - the outputCP of the console.
unsigned int ConhostInternalGetSet::GetConsoleOutputCP() const
{
    return ServiceLocator::LocateGlobals().getConsoleInformation().OutputCP;
}

// Routine Description:
// - Resizes the window to the specified dimensions, in characters.
// Arguments:
// - width: The new width of the window, in columns
// - height: The new height of the window, in rows
// Return Value:
// - True if handled successfully. False otherwise.
bool ConhostInternalGetSet::ResizeWindow(const size_t width, const size_t height)
{
    SHORT sColumns = 0;
    SHORT sRows = 0;

    THROW_IF_FAILED(SizeTToShort(width, &sColumns));
    THROW_IF_FAILED(SizeTToShort(height, &sRows));
    // We should do nothing if 0 is passed in for a size.
    RETURN_BOOL_IF_FALSE(width > 0 && height > 0);

    auto api = ServiceLocator::LocateGlobals().api;
    auto& screenInfo = _io.GetActiveOutputBuffer();

    CONSOLE_SCREEN_BUFFER_INFOEX csbiex = { 0 };
    csbiex.cbSize = sizeof(CONSOLE_SCREEN_BUFFER_INFOEX);
    api->GetConsoleScreenBufferInfoExImpl(screenInfo, csbiex);

    const Viewport oldViewport = Viewport::FromInclusive(csbiex.srWindow);
    const Viewport newViewport = Viewport::FromDimensions(oldViewport.Origin(), sColumns, sRows);
    // Always resize the width of the console
    csbiex.dwSize.X = sColumns;
    // Only set the screen buffer's height if it's currently less than
    //  what we're requesting.
    if (sRows > csbiex.dwSize.Y)
    {
        csbiex.dwSize.Y = sRows;
    }

    // SetWindowInfo expect inclusive rects
    const auto sri = newViewport.ToInclusive();

    // SetConsoleScreenBufferInfoEx however expects exclusive rects
    const auto sre = newViewport.ToExclusive();
    csbiex.srWindow = sre;

    THROW_IF_FAILED(api->SetConsoleScreenBufferInfoExImpl(screenInfo, csbiex));
    THROW_IF_FAILED(api->SetConsoleWindowInfoImpl(screenInfo, true, sri));
    return true;
}

// Routine Description:
// - Checks if the console host is acting as a pty.
// Arguments:
// - <none>
// Return Value:
// - true if we're in pty mode.
bool ConhostInternalGetSet::IsConsolePty() const
{
    return ServiceLocator::LocateGlobals().getConsoleInformation().IsInVtIoMode();
}

// Method Description:
// - Retrieves the value in the colortable at the specified index.
// Arguments:
// - tableIndex: the index of the color table to retrieve.
// Return Value:
// - the COLORREF value for the color at that index in the table.
COLORREF ConhostInternalGetSet::GetColorTableEntry(const size_t tableIndex) const
{
    auto& g = ServiceLocator::LocateGlobals();
    auto& gci = g.getConsoleInformation();
    return gci.GetColorTableEntry(tableIndex);
}

// Method Description:
// - Updates the value in the colortable at index tableIndex to the new color
//   color. color is a COLORREF, format 0x00BBGGRR.
// Arguments:
// - tableIndex: the index of the color table to update.
// - color: the new COLORREF to use as that color table value.
// Return Value:
// - true if successful. false otherwise.
bool ConhostInternalGetSet::SetColorTableEntry(const size_t tableIndex, const COLORREF color)
{
    auto& g = ServiceLocator::LocateGlobals();
    auto& gci = g.getConsoleInformation();
    gci.SetColorTableEntry(tableIndex, color);

    // Update the screen colors if we're not a pty
    // No need to force a redraw in pty mode.
    if (g.pRender && !gci.IsInVtIoMode())
    {
        g.pRender->TriggerRedrawAll();
    }

    // If we're a conpty, always return false, so that we send the updated color
    //      value to the terminal. Still handle the sequence so apps that use
    //      the API or VT to query the values of the color table still read the
    //      correct color.
    return !gci.IsInVtIoMode();
}

// Routine Description:
// - Sets the position in the color table for the given color alias.
// Arguments:
// - alias: the color alias to update.
// - tableIndex: the new position of the alias in the color table.
// Return Value:
// - <none>
void ConhostInternalGetSet::SetColorAliasIndex(const ColorAlias alias, const size_t tableIndex)
{
    auto& renderSettings = ServiceLocator::LocateGlobals().getConsoleInformation().GetRenderSettings();
    renderSettings.SetColorAliasIndex(alias, tableIndex);
}

// Routine Description:
// - Fills a region of the screen buffer.
// Arguments:
// - startPosition - The position to begin filling at.
// - fillLength - The number of characters to fill.
// - fillChar - Character to fill the target region with.
// - standardFillAttrs - If true, fill with the standard erase attributes.
//                       If false, fill with the default attributes.
// Return value:
// - <none>
void ConhostInternalGetSet::FillRegion(const COORD startPosition,
                                       const size_t fillLength,
                                       const wchar_t fillChar,
                                       const bool standardFillAttrs)
{
    auto& screenInfo = _io.GetActiveOutputBuffer();

    if (fillLength == 0)
    {
        return;
    }

    // For most VT erasing operations, the standard requires that the
    // erased area be filled with the current background color, but with
    // no additional meta attributes set. For all other cases, we just
    // fill with the default attributes.
    auto fillAttrs = TextAttribute{};
    if (standardFillAttrs)
    {
        fillAttrs = screenInfo.GetAttributes();
        fillAttrs.SetStandardErase();
    }

    const auto fillData = OutputCellIterator{ fillChar, fillAttrs, fillLength };
    screenInfo.Write(fillData, startPosition, false);

    // Notify accessibility
    if (screenInfo.HasAccessibilityEventing())
    {
        auto endPosition = startPosition;
        const auto bufferSize = screenInfo.GetBufferSize();
        bufferSize.MoveInBounds(fillLength - 1, endPosition);
        screenInfo.NotifyAccessibilityEventing(startPosition.X, startPosition.Y, endPosition.X, endPosition.Y);
    }
}

// Routine Description:
// - Moves a block of data in the screen buffer, optionally limiting the effects
//      of the move to a clipping rectangle.
// Arguments:
// - scrollRect - Region to copy/move (source and size).
// - clipRect - Optional clip region to contain buffer change effects.
// - destinationOrigin - Upper left corner of target region.
// - standardFillAttrs - If true, fill with the standard erase attributes.
//                       If false, fill with the default attributes.
// Return value:
// - <none>
void ConhostInternalGetSet::ScrollRegion(const SMALL_RECT scrollRect,
                                         const std::optional<SMALL_RECT> clipRect,
                                         const COORD destinationOrigin,
                                         const bool standardFillAttrs)
{
    auto& screenInfo = _io.GetActiveOutputBuffer();

    // For most VT scrolling operations, the standard requires that the
    // erased area be filled with the current background color, but with
    // no additional meta attributes set. For all other cases, we just
    // fill with the default attributes.
    auto fillAttrs = TextAttribute{};
    if (standardFillAttrs)
    {
        fillAttrs = screenInfo.GetAttributes();
        fillAttrs.SetStandardErase();
    }

    ::ScrollRegion(screenInfo, scrollRect, clipRect, destinationOrigin, UNICODE_SPACE, fillAttrs);
}

// Routine Description:
// - Checks if the InputBuffer is willing to accept VT Input directly
//   IsVtInputEnabled is an internal-only "API" call that the vt commands can execute,
//    but it is not represented as a function call on our public API surface.
// Arguments:
// - <none>
// Return value:
// - true if enabled (see IsInVirtualTerminalInputMode). false otherwise.
bool ConhostInternalGetSet::IsVtInputEnabled() const
{
    return _io.GetActiveInputBuffer()->IsInVirtualTerminalInputMode();
}

// Routine Description:
// - Replaces the active soft font with the given bit pattern.
// Arguments:
// - bitPattern - An array of scanlines representing all the glyphs in the font.
// - cellSize - The cell size for an individual glyph.
// - centeringHint - The horizontal extent that glyphs are offset from center.
// Return Value:
// - <none>
void ConhostInternalGetSet::UpdateSoftFont(const gsl::span<const uint16_t> bitPattern,
                                           const SIZE cellSize,
                                           const size_t centeringHint)
{
    auto* pRender = ServiceLocator::LocateGlobals().pRender;
    if (pRender)
    {
        pRender->UpdateSoftFont(bitPattern, cellSize, centeringHint);
    }
}
