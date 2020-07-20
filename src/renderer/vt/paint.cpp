// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "vtrenderer.hpp"
#include "../../inc/conattrs.hpp"
#include "../../types/inc/convert.hpp"

#pragma hdrstop
using namespace Microsoft::Console::Render;
using namespace Microsoft::Console::Types;

// Routine Description:
// - Prepares internal structures for a painting operation.
// Arguments:
// - <none>
// Return Value:
// - S_OK if we started to paint. S_FALSE if we didn't need to paint.
//      HRESULT error code if painting didn't start successfully.
[[nodiscard]] HRESULT VtEngine::StartPaint() noexcept
{
    if (_pipeBroken)
    {
        return S_FALSE;
    }

    // If there's nothing to do, quick return
    bool somethingToDo = _invalidMap.any() ||
                         _scrollDelta != til::point{ 0, 0 } ||
                         _cursorMoved ||
                         _titleChanged;

    _quickReturn = !somethingToDo;
    _trace.TraceStartPaint(_quickReturn,
                           _invalidMap,
                           _lastViewport.ToInclusive(),
                           _scrollDelta,
                           _cursorMoved,
                           _wrappedRow);

    return _quickReturn ? S_FALSE : S_OK;
}

// Routine Description:
// - EndPaint helper to perform the final cleanup after painting. If we
//      returned S_FALSE from StartPaint, there's no guarantee this was called.
//      That's okay however, EndPaint only zeros structs that would be zero if
//      StartPaint returns S_FALSE.
// Arguments:
// - <none>
// Return Value:
// - S_OK, else an appropriate HRESULT for failing to allocate or write.
[[nodiscard]] HRESULT VtEngine::EndPaint() noexcept
{
    _trace.TraceEndPaint();

    _invalidMap.reset_all();

    _scrollDelta = { 0, 0 };
    _clearedAllThisFrame = false;
    _cursorMoved = false;
    _firstPaint = false;
    _skipCursor = false;
    _resized = false;
    // If we've circled the buffer this frame, move our virtual top upwards.
    // We do this at the END of the frame, so that during the paint, we still
    //      use the original virtual top.
    if (_circled)
    {
        if (_virtualTop > 0)
        {
            _virtualTop--;
        }
    }
    _circled = false;

    // If we deferred a cursor movement during the frame, make sure we put the
    //      cursor in the right place before we end the frame.
    if (_deferredCursorPos != INVALID_COORDS)
    {
        RETURN_IF_FAILED(_MoveCursor(_deferredCursorPos));
    }

    RETURN_IF_FAILED(_Flush());

    return S_OK;
}

// Routine Description:
// - Used to perform longer running presentation steps outside the lock so the
//      other threads can continue.
// - Not currently used by VtEngine.
// Arguments:
// - <none>
// Return Value:
// - S_FALSE since we do nothing.
[[nodiscard]] HRESULT VtEngine::Present() noexcept
{
    return S_FALSE;
}

// Routine Description:
// - Paints the background of the invalid area of the frame.
// Arguments:
// - <none>
// Return Value:
// - S_OK
[[nodiscard]] HRESULT VtEngine::PaintBackground() noexcept
{
    return S_OK;
}

// Routine Description:
// - Draws one line of the buffer to the screen. Writes the characters to the
//      pipe. If the characters are outside the ASCII range (0-0x7f), then
//      instead writes a '?'
// Arguments:
// - clusters - text and column count data to be written
// - trimLeft - This specifies whether to trim one character width off the left
//      side of the output. Used for drawing the right-half only of a
//      double-wide character.
// - lineWrapped: true if this run we're painting is the end of a line that
//   wrapped. If we're not painting the last column of a wrapped line, then this
//   will be false.
// Return Value:
// - S_OK or suitable HRESULT error from writing pipe.
[[nodiscard]] HRESULT VtEngine::PaintBufferLine(gsl::span<const Cluster> const clusters,
                                                const COORD coord,
                                                const bool /*trimLeft*/,
                                                const bool /*lineWrapped*/) noexcept
{
    return VtEngine::_PaintAsciiBufferLine(clusters, coord);
}

// Method Description:
// - Draws up to one line worth of grid lines on top of characters.
// Arguments:
// - lines - Enum defining which edges of the rectangle to draw
// - color - The color to use for drawing the edges.
// - cchLine - How many characters we should draw the grid lines along (left to right in a row)
// - coordTarget - The starting X/Y position of the first character to draw on.
// Return Value:
// - S_OK
[[nodiscard]] HRESULT VtEngine::PaintBufferGridLines(const GridLines /*lines*/,
                                                     const COLORREF /*color*/,
                                                     const size_t /*cchLine*/,
                                                     const COORD /*coordTarget*/) noexcept
{
    return S_OK;
}

// Routine Description:
// - Draws the cursor on the screen
// Arguments:
// - options - Options that affect the presentation of the cursor
// Return Value:
// - S_OK or suitable HRESULT error from writing pipe.
[[nodiscard]] HRESULT VtEngine::PaintCursor(const CursorOptions& options) noexcept
{
    _trace.TracePaintCursor(options.coordCursor);

    // MSFT:15933349 - Send the terminal the updated cursor information, if it's changed.
    LOG_IF_FAILED(_MoveCursor(options.coordCursor));

    return S_OK;
}

// Routine Description:
//  - Inverts the selected region on the current screen buffer.
//  - Reads the selected area, selection mode, and active screen buffer
//    from the global properties and dispatches a GDI invert on the selected text area.
//  Because the selection is the responsibility of the terminal, and not the
//      host, render nothing.
// Arguments:
//  - rect - Rectangle to invert or highlight to make the selection area
// Return Value:
// - S_OK
[[nodiscard]] HRESULT VtEngine::PaintSelection(const SMALL_RECT /*rect*/) noexcept
{
    return S_OK;
}

// Routine Description:
// - Write a VT sequence to change the current colors of text. Writes true RGB
//      color sequences.
// Arguments:
// - textAttributes: Text attributes to use for the colors.
// Return Value:
// - S_OK if we succeeded, else an appropriate HRESULT for failing to allocate or write.
[[nodiscard]] HRESULT VtEngine::_RgbUpdateDrawingBrushes(const TextAttribute& textAttributes) noexcept
{
    const auto fg = textAttributes.GetForeground();
    const auto bg = textAttributes.GetBackground();
    auto lastFg = _lastTextAttributes.GetForeground();
    auto lastBg = _lastTextAttributes.GetBackground();

    // If both the FG and BG should be the defaults, emit a SGR reset.
    if (fg.IsDefault() && bg.IsDefault() && !(lastFg.IsDefault() && lastBg.IsDefault()))
    {
        // SGR Reset will clear all attributes.
        RETURN_IF_FAILED(_SetGraphicsDefault());
        _lastTextAttributes = {};
        lastFg = {};
        lastBg = {};
    }

    if (fg != lastFg)
    {
        if (fg.IsDefault())
        {
            RETURN_IF_FAILED(_SetGraphicsRenditionDefaultColor(true));
        }
        else if (fg.IsIndex16())
        {
            RETURN_IF_FAILED(_SetGraphicsRendition16Color(fg.GetIndex(), true));
        }
        else if (fg.IsIndex256())
        {
            RETURN_IF_FAILED(_SetGraphicsRendition256Color(fg.GetIndex(), true));
        }
        else if (fg.IsRgb())
        {
            RETURN_IF_FAILED(_SetGraphicsRenditionRGBColor(fg.GetRGB(), true));
        }
        _lastTextAttributes.SetForeground(fg);
    }

    if (bg != lastBg)
    {
        if (bg.IsDefault())
        {
            RETURN_IF_FAILED(_SetGraphicsRenditionDefaultColor(false));
        }
        else if (bg.IsIndex16())
        {
            RETURN_IF_FAILED(_SetGraphicsRendition16Color(bg.GetIndex(), false));
        }
        else if (bg.IsIndex256())
        {
            RETURN_IF_FAILED(_SetGraphicsRendition256Color(bg.GetIndex(), false));
        }
        else if (bg.IsRgb())
        {
            RETURN_IF_FAILED(_SetGraphicsRenditionRGBColor(bg.GetRGB(), false));
        }
        _lastTextAttributes.SetBackground(bg);
    }

    return S_OK;
}

// Routine Description:
// - Write a VT sequence to change the current colors of text. It will try to
//      find ANSI colors that are nearest to the input colors, and write those
//      indices to the pipe.
// Arguments:
// - textAttributes: Text attributes to use for the colors.
// Return Value:
// - S_OK if we succeeded, else an appropriate HRESULT for failing to allocate or write.
[[nodiscard]] HRESULT VtEngine::_16ColorUpdateDrawingBrushes(const TextAttribute& textAttributes) noexcept
{
    const auto fg = textAttributes.GetForeground();
    const auto bg = textAttributes.GetBackground();
    auto lastFg = _lastTextAttributes.GetForeground();
    auto lastBg = _lastTextAttributes.GetBackground();

    // If either FG or BG have changed to default, emit a SGR reset.
    // We can't reset FG and BG to default individually.
    if ((fg.IsDefault() && !lastFg.IsDefault()) || (bg.IsDefault() && !lastBg.IsDefault()))
    {
        // SGR Reset will clear all attributes.
        RETURN_IF_FAILED(_SetGraphicsDefault());
        _lastTextAttributes = {};
        lastFg = {};
        lastBg = {};
    }

    // We use the legacy color calculations to generate an approximation of the
    // colors in the 16-color table.
    auto fgIndex = fg.GetLegacyIndex(0);
    auto bgIndex = bg.GetLegacyIndex(0);

    // If the bold attribute is set, and the foreground can be brightened, then do so.
    const bool brighten = textAttributes.IsBold() && fg.CanBeBrightened();
    fgIndex |= (brighten ? FOREGROUND_INTENSITY : 0);

    // To actually render bright colors, though, we need to use SGR bold.
    const auto needBold = fgIndex > 7;
    if (needBold != _lastTextAttributes.IsBold())
    {
        RETURN_IF_FAILED(_SetBold(needBold));
        _lastTextAttributes.SetBold(needBold);
    }

    // After which we drop the high bits, since only colors 0 to 7 are supported.

    fgIndex &= 7;
    bgIndex &= 7;

    if (!fg.IsDefault() && (lastFg.IsDefault() || fgIndex != lastFg.GetIndex()))
    {
        RETURN_IF_FAILED(_SetGraphicsRendition16Color(fgIndex, true));
        _lastTextAttributes.SetIndexedForeground(fgIndex);
    }

    if (!bg.IsDefault() && (lastBg.IsDefault() || bgIndex != lastBg.GetIndex()))
    {
        RETURN_IF_FAILED(_SetGraphicsRendition16Color(bgIndex, false));
        _lastTextAttributes.SetIndexedBackground(bgIndex);
    }

    return S_OK;
}

// Routine Description:
// - Draws one line of the buffer to the screen. Writes the characters to the
//      pipe. If the characters are outside the ASCII range (0-0x7f), then
//      instead writes a '?'.
//   This is needed because the Windows internal telnet client implementation
//      doesn't know how to handle >ASCII characters. The old telnetd would
//      just replace them with '?' characters. If we render the >ASCII
//      characters to telnet, it will likely end up drawing them wrong, which
//      will make the client appear buggy and broken.
// Arguments:
// - clusters - text and column width data to be written
// - coord - character coordinate target to render within viewport
// Return Value:
// - S_OK or suitable HRESULT error from writing pipe.
[[nodiscard]] HRESULT VtEngine::_PaintAsciiBufferLine(gsl::span<const Cluster> const clusters,
                                                      const COORD coord) noexcept
{
    try
    {
        RETURN_IF_FAILED(_MoveCursor(coord));

        _bufferLine.clear();
        _bufferLine.reserve(clusters.size());

        short totalWidth = 0;
        for (const auto& cluster : clusters)
        {
            _bufferLine.append(cluster.GetText());
            RETURN_IF_FAILED(ShortAdd(totalWidth, gsl::narrow<short>(cluster.GetColumns()), &totalWidth));
        }

        RETURN_IF_FAILED(VtEngine::_WriteTerminalAscii(_bufferLine));

        // Update our internal tracker of the cursor's position
        _lastText.X += totalWidth;

        return S_OK;
    }
    CATCH_RETURN();
}

// Routine Description:
// - Draws one line of the buffer to the screen. Writes the characters to the
//      pipe, encoded in UTF-8.
// Arguments:
// - clusters - text and column widths to be written
// - coord - character coordinate target to render within viewport
// Return Value:
// - S_OK or suitable HRESULT error from writing pipe.
[[nodiscard]] HRESULT VtEngine::_PaintUtf8BufferLine(gsl::span<const Cluster> const clusters,
                                                     const COORD coord,
                                                     const bool lineWrapped) noexcept
{
    if (coord.Y < _virtualTop)
    {
        return S_OK;
    }

    _bufferLine.clear();
    _bufferLine.reserve(clusters.size());
    short totalWidth = 0;
    for (const auto& cluster : clusters)
    {
        _bufferLine.append(cluster.GetText());
        RETURN_IF_FAILED(ShortAdd(totalWidth, static_cast<short>(cluster.GetColumns()), &totalWidth));
    }
    const size_t cchLine = _bufferLine.size();

    bool foundNonspace = false;
    size_t lastNonSpace = 0;
    for (size_t i = 0; i < cchLine; i++)
    {
        if (_bufferLine.at(i) != L'\x20')
        {
            lastNonSpace = i;
            foundNonspace = true;
        }
    }
    // Examples:
    // - "  ":
    //      cch = 2, lastNonSpace = 0, foundNonSpace = false
    //      cch-lastNonSpace = 2 -> good
    //      cch-lastNonSpace-(0) = 2 -> good
    // - "A "
    //      cch = 2, lastNonSpace = 0, foundNonSpace = true
    //      cch-lastNonSpace = 2 -> bad
    //      cch-lastNonSpace-(1) = 1 -> good
    // - "AA"
    //      cch = 2, lastNonSpace = 1, foundNonSpace = true
    //      cch-lastNonSpace = 1 -> bad
    //      cch-lastNonSpace-(1) = 0 -> good
    const size_t numSpaces = cchLine - lastNonSpace - (foundNonspace ? 1 : 0);

    // Optimizations:
    // If there are lots of spaces at the end of the line, we can try to Erase
    //      Character that number of spaces, then move the cursor forward (to
    //      where it would be if we had written the spaces)
    // An erase character and move right sequence is 8 chars, and possibly 10
    //      (if there are at least 10 spaces, 2 digits to print)
    // ESC [ %d X ESC [ %d C
    // ESC [ %d %d X ESC [ %d %d C
    // So we need at least 9 spaces for the optimized sequence to make sense.
    // Also, if we already erased the entire display this frame, then
    //    don't do ANYTHING with erasing at all.

    // Note: We're only doing these optimizations along the UTF-8 path, because
    // the inbox telnet client doesn't understand the Erase Character sequence,
    // and it uses xterm-ascii. This ensures that xterm and -256color consumers
    // get the enhancements, and telnet isn't broken.
    const bool optimalToUseECH = numSpaces > ERASE_CHARACTER_STRING_LENGTH;
    const bool useEraseChar = (optimalToUseECH) &&
                              (!_newBottomLine) &&
                              (!_clearedAllThisFrame);
    const bool printingBottomLine = coord.Y == _lastViewport.BottomInclusive();

    // GH#5502 - If the background color of the "new bottom line" is different
    // than when we emitted the line, we can't optimize out the spaces from it.
    // We'll still need to emit those spaces, so that the connected terminal
    // will have the same background color on those blank cells.
    const bool bgMatched = _newBottomLineBG.has_value() ? (_newBottomLineBG.value() == _lastTextAttributes.GetBackground()) : true;

    // If we're not using erase char, but we did erase all at the start of the
    // frame, don't add spaces at the end.
    //
    // GH#5161: Only removeSpaces when we're in the _newBottomLine state and the
    // line we're trying to print right now _actually is the bottom line_
    //
    // GH#5291: DON'T remove spaces when the row wrapped. We might need those
    // spaces to preserve the wrap state of this line, or the cursor position.
    // For example, vim.exe uses "~    "... to clear the line, and then leaves
    // the lines _wrapped_. It doesn't care to manually break the lines, but if
    // we trimmed the spaces off here, we'd print all the "~"s one after another
    // on the same line.
    const bool removeSpaces = !lineWrapped && (useEraseChar ||
                                               _clearedAllThisFrame ||
                                               (_newBottomLine && printingBottomLine && bgMatched));
    const size_t cchActual = removeSpaces ?
                                 (cchLine - numSpaces) :
                                 cchLine;

    const size_t columnsActual = removeSpaces ?
                                     (totalWidth - numSpaces) :
                                     totalWidth;

    if (cchActual == 0)
    {
        // If the previous row wrapped, but this line is empty, then we actually
        // do want to move the cursor down. Otherwise, we'll possibly end up
        // accidentally erasing the last character from the previous line, as
        // the cursor is still waiting on that character for the next character
        // to follow it.
        //
        // GH#5839 - If we've emitted a wrapped row, because the cursor is
        // sitting just past the last cell of the previous row, if we execute a
        // EraseCharacter or EraseLine here, then the row won't actually get
        // cleared here. This logic is important to make sure that the cursor is
        // in the right position before we do that.

        _wrappedRow = std::nullopt;
        _trace.TraceClearWrapped();
    }

    // Move the cursor to the start of this run.
    RETURN_IF_FAILED(_MoveCursor(coord));

    // Write the actual text string
    RETURN_IF_FAILED(VtEngine::_WriteTerminalUtf8({ _bufferLine.data(), cchActual }));

    // GH#4415, GH#5181
    // If the renderer told us that this was a wrapped line, then mark
    // that we've wrapped this line. The next time we attempt to move the
    // cursor, if we're trying to move it to the start of the next line,
    // we'll remember that this line was wrapped, and not manually break the
    // line.
    if (lineWrapped)
    {
        _wrappedRow = coord.Y;
        _trace.TraceSetWrapped(coord.Y);
    }

    // Update our internal tracker of the cursor's position.
    // See MSFT:20266233 (which is also GH#357)
    // If the cursor is at the rightmost column of the terminal, and we write a
    //      space, the cursor won't actually move to the next cell (which would
    //      be {0, _lastText.Y++}). The cursor will stay visibly in that last
    //      cell until then next character is output.
    // If in that case, we increment the cursor position here (such that the X
    //      position would be one past the right of the terminal), when we come
    //      back through to MoveCursor in the last PaintCursor of the frame,
    //      we'll determine that we need to emit a \b to put the cursor in the
    //      right position. This is wrong, and will cause us to move the cursor
    //      back one character more than we wanted.
    //
    // GH#1245: This needs to be RightExclusive, _not_ inclusive. Otherwise, we
    // won't update our internal cursor position tracker correctly at the last
    // character of the row.
    if (_lastText.X < _lastViewport.RightExclusive())
    {
        _lastText.X += static_cast<short>(columnsActual);
    }
    // GH#1245: If we wrote the exactly last char of the row, then we're in the
    // "delayed EOL wrap" state. Different terminals (conhost, gnome-terminal,
    // wt) all behave differently with how the cursor behaves at an end of line.
    // Mark that we're in the delayed EOL wrap state - we don't want to be
    // clever about how we move the cursor in this state, since different
    // terminals will handle a backspace differently in this state.
    if (_lastText.X >= _lastViewport.RightInclusive())
    {
        _delayedEolWrap = true;
    }

    short sNumSpaces;
    try
    {
        sNumSpaces = gsl::narrow<short>(numSpaces);
    }
    CATCH_RETURN();

    if (useEraseChar)
    {
        // ECH doesn't actually move the cursor itself. However, we think that
        //   the cursor *should* be at the end of the area we just erased. Stash
        //   that position as our new deferred position. If we don't move the
        //   cursor somewhere else before the end of the frame, we'll move the
        //   cursor to the deferred position at the end of the frame, or right
        //   before we need to print new text.
        _deferredCursorPos = { _lastText.X + sNumSpaces, _lastText.Y };

        if (_deferredCursorPos.X <= _lastViewport.RightInclusive())
        {
            RETURN_IF_FAILED(_EraseCharacter(sNumSpaces));
        }
        else
        {
            RETURN_IF_FAILED(_EraseLine());
        }
    }
    else if (_newBottomLine && printingBottomLine)
    {
        // If we're on a new line, then we don't need to erase the line. The
        //      line is already empty.
        if (optimalToUseECH)
        {
            _deferredCursorPos = { _lastText.X + sNumSpaces, _lastText.Y };
        }
        else if (numSpaces > 0 && removeSpaces) // if we deleted the spaces... re-add them
        {
            // TODO GH#5430 - Determine why and when we would do this.
            std::wstring spaces = std::wstring(numSpaces, L' ');
            RETURN_IF_FAILED(VtEngine::_WriteTerminalUtf8(spaces));

            _lastText.X += static_cast<short>(numSpaces);
        }
    }

    // If we printed to the bottom line, and we previously thought that this was
    // a new bottom line, it certainly isn't new any longer.
    if (printingBottomLine)
    {
        _newBottomLine = false;
        _newBottomLineBG = std::nullopt;
    }

    return S_OK;
}

// Method Description:
// - Updates the window's title string. Emits the VT sequence to SetWindowTitle.
//      Because wintelnet does not understand these sequences by default, we
//      don't do anything by default. Other modes can implement if they support
//      the sequence.
// Arguments:
// - newTitle: the new string to use for the title of the window
// Return Value:
// - S_OK
[[nodiscard]] HRESULT VtEngine::_DoUpdateTitle(const std::wstring& /*newTitle*/) noexcept
{
    return S_OK;
}
