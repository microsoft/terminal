// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "XtermEngine.hpp"
#include "../../types/inc/convert.hpp"
#pragma hdrstop
using namespace Microsoft::Console;
using namespace Microsoft::Console::Render;
using namespace Microsoft::Console::Types;

XtermEngine::XtermEngine(_In_ wil::unique_hfile hPipe,
                         const IDefaultColorProvider& colorProvider,
                         const Viewport initialViewport,
                         _In_reads_(cColorTable) const COLORREF* const ColorTable,
                         const WORD cColorTable,
                         const bool fUseAsciiOnly) :
    VtEngine(std::move(hPipe), colorProvider, initialViewport),
    _ColorTable(ColorTable),
    _cColorTable(cColorTable),
    _fUseAsciiOnly(fUseAsciiOnly),
    _usingUnderLine(false),
    _needToDisableCursor(false),
    _lastCursorIsVisible(false),
    _nextCursorIsVisible(true)
{
    // Set out initial cursor position to -1, -1. This will force our initial
    //      paint to manually move the cursor to 0, 0, not just ignore it.
    _lastText = VtEngine::INVALID_COORDS;
}

// Method Description:
// - Prepares internal structures for a painting operation. Turns the cursor
//      off, so we don't see it flashing all over the client's screen as we
//      paint the new contents.
// Arguments:
// - <none>
// Return Value:
// - S_OK if we started to paint. S_FALSE if we didn't need to paint. HRESULT
//      error code if painting didn't start successfully, or we failed to write
//      the pipe.
[[nodiscard]] HRESULT XtermEngine::StartPaint() noexcept
{
    RETURN_IF_FAILED(VtEngine::StartPaint());

    _trace.TraceLastText(_lastText);

    // Prep us to think that the cursor is not visible this frame. If it _is_
    // visible, then PaintCursor will be called, and we'll set this to true
    // during the frame.
    _nextCursorIsVisible = false;

    if (_firstPaint)
    {
        // MSFT:17815688
        // If the caller requested to inherit the cursor, we shouldn't
        //      clear the screen on the first paint. Otherwise, we'll clear
        //      the screen on the first paint, just to make sure that the
        //      terminal's state is consistent with what we'll be rendering.
        RETURN_IF_FAILED(_ClearScreen());
        _clearedAllThisFrame = true;
        _firstPaint = false;
    }
    else
    {
        const auto dirty = GetDirtyArea();

        // If we have 0 or 1 dirty pieces in the area, set as appropriate.
        Viewport dirtyView = dirty.empty() ? Viewport::Empty() : Viewport::FromInclusive(til::at(dirty, 0));

        // If there's more than 1, union them all up with the 1 we already have.
        for (size_t i = 1; i < dirty.size(); ++i)
        {
            dirtyView = Viewport::Union(dirtyView, Viewport::FromInclusive(til::at(dirty, i)));
        }
    }

    if (!_quickReturn)
    {
        if (_WillWriteSingleChar())
        {
            // Don't re-enable the cursor.
            _quickReturn = true;
        }
    }

    return S_OK;
}

// Routine Description:
// - EndPaint helper to perform the final rendering steps. Turn the cursor back
//      on.
// Arguments:
// - <none>
// Return Value:
// - S_OK if we succeeded, else an appropriate HRESULT for failing to allocate or write.
[[nodiscard]] HRESULT XtermEngine::EndPaint() noexcept
{
    // If during the frame we determined that the cursor needed to be disabled,
    //      then insert a cursor off at the start of the buffer, and re-enable
    //      the cursor here.
    if (_needToDisableCursor)
    {
        // If the cursor was previously visible, let's hide it for this frame,
        // by prepending a cursor off.
        if (_lastCursorIsVisible)
        {
            _buffer.insert(0, "\x1b[?25l");
            _lastCursorIsVisible = false;
        }
        // If the cursor was NOT previously visible, then that's fine! we don't
        // need to worry, it's already off.
    }

    // If the cursor is moving from off -> on (including cases where we just
    // disabled if for this frame), show the cursor at the end of the frame
    if (_nextCursorIsVisible && !_lastCursorIsVisible)
    {
        RETURN_IF_FAILED(_ShowCursor());
    }
    // Otherwise, if the cursor previously was visible, and it should be hidden
    // (on -> off), hide it at the end of the frame.
    else if (!_nextCursorIsVisible && _lastCursorIsVisible)
    {
        RETURN_IF_FAILED(_HideCursor());
    }

    // Update our tracker of what we thought the last cursor state of the
    // terminal was.
    _lastCursorIsVisible = _nextCursorIsVisible;

    RETURN_IF_FAILED(VtEngine::EndPaint());

    _needToDisableCursor = false;

    return S_OK;
}

// Routine Description:
// - Write a VT sequence to either start or stop underlining text.
// Arguments:
// - legacyColorAttribute: A console attributes bit field containing information
//      about the underlining state of the text.
// Return Value:
// - S_OK if we succeeded, else an appropriate HRESULT for failing to allocate or write.
[[nodiscard]] HRESULT XtermEngine::_UpdateUnderline(const WORD legacyColorAttribute) noexcept
{
    bool textUnderlined = WI_IsFlagSet(legacyColorAttribute, COMMON_LVB_UNDERSCORE);
    if (textUnderlined != _usingUnderLine)
    {
        if (textUnderlined)
        {
            RETURN_IF_FAILED(_BeginUnderline());
        }
        else
        {
            RETURN_IF_FAILED(_EndUnderline());
        }
        _usingUnderLine = textUnderlined;
    }
    return S_OK;
}

// Routine Description:
// - Write a VT sequence to change the current colors of text. Only writes
//      16-color attributes.
// Arguments:
// - colorForeground: The RGB Color to use to paint the foreground text.
// - colorBackground: The RGB Color to use to paint the background of the text.
// - legacyColorAttribute: A console attributes bit field specifying the brush
//      colors we should use.
// - extendedAttrs - extended text attributes (italic, underline, etc.) to use.
// - isSettingDefaultBrushes: indicates if we should change the background color of
//      the window. Unused for VT
// Return Value:
// - S_OK if we succeeded, else an appropriate HRESULT for failing to allocate or write.
[[nodiscard]] HRESULT XtermEngine::UpdateDrawingBrushes(const COLORREF colorForeground,
                                                        const COLORREF colorBackground,
                                                        const WORD legacyColorAttribute,
                                                        const ExtendedAttributes extendedAttrs,
                                                        const bool /*isSettingDefaultBrushes*/) noexcept
{
    //When we update the brushes, check the wAttrs to see if the LVB_UNDERSCORE
    //      flag is there. If the state of that flag is different then our
    //      current state, change the underlining state.
    // We have to do this here, instead of in PaintBufferGridLines, because
    //      we'll have already painted the text by the time PaintBufferGridLines
    //      is called.
    // TODO:GH#2915 Treat underline separately from LVB_UNDERSCORE
    RETURN_IF_FAILED(_UpdateUnderline(legacyColorAttribute));
    // The base xterm mode only knows about 16 colors
    return VtEngine::_16ColorUpdateDrawingBrushes(colorForeground,
                                                  colorBackground,
                                                  WI_IsFlagSet(extendedAttrs, ExtendedAttributes::Bold),
                                                  _ColorTable,
                                                  _cColorTable);
}

// Routine Description:
// - Draws the cursor on the screen
// Arguments:
// - options - Options that affect the presentation of the cursor
// Return Value:
// - S_OK or suitable HRESULT error from writing pipe.
[[nodiscard]] HRESULT XtermEngine::PaintCursor(const IRenderEngine::CursorOptions& options) noexcept
{
    // PaintCursor is only called when the cursor is in fact visible in a single
    // frame. When this is called, mark _nextCursorIsVisible as true. At the end
    // of the frame, we'll decide to either turn the cursor on or not, based
    // upon the previous state.

    // When this method is not called during a frame, it's because the cursor
    // was not visible. In that case, at the end of the frame,
    // _nextCursorIsVisible will still be false (from when we set it during
    // StartPaint)
    _nextCursorIsVisible = true;

    // If we did a delayed EOL wrap because we actually wrapped the line here,
    // then don't PaintCursor. When we're at the EOL because we've wrapped, our
    // internal _lastText thinks the cursor is on the cell just past the right
    // of the viewport (ex { 120, 0 }). However, conhost thinks the cursor is
    // actually on the last cell of the row. So it'll tell us to paint the
    // cursor at { 119, 0 }. If we do that movement, then we'll break line
    // wrapping.
    // See GH#5113, GH#1245, GH#357
    const auto nextCursorPosition = options.coordCursor;
    // Only skip this paint when we think the cursor is in the cell
    // immediately off the edge of the terminal, and the actual cursor is in
    // the last cell of the row. We're in a deferred wrap, but the host
    // thinks the cursor is actually in-frame.
    // See ConptyRoundtripTests::DontWrapMoveCursorInSingleFrame
    const bool cursorIsInDeferredWrap = (nextCursorPosition.X == _lastText.X - 1) && (nextCursorPosition.Y == _lastText.Y);
    // If all three of these conditions are true, then:
    //   * cursorIsInDeferredWrap: The cursor is in a position where the line
    //     filled the last cell of the row, but the host tried to paint it in
    //     the last cell anyways
    //   * _delayedEolWrap && _wrappedRow.has_value(): We think we've deferred
    //     the wrap of a line.
    // If they're all true, DON'T manually paint the cursor this frame.
    if (!(cursorIsInDeferredWrap && _delayedEolWrap && _wrappedRow.has_value()))
    {
        return VtEngine::PaintCursor(options);
    }

    return S_OK;
}

// Routine Description:
// - Write a VT sequence to move the cursor to the specified coordinates. We
//      also store the last place we left the cursor for future optimizations.
//  If the cursor only needs to go to the origin, only write the home sequence.
//  If the new cursor is only down one line from the current, only write a newline
//  If the new cursor is only down one line and at the start of the line, write
//      a carriage return.
//  Otherwise just write the whole sequence for moving it.
// Arguments:
// - coord: location to move the cursor to.
// Return Value:
// - S_OK if we succeeded, else an appropriate HRESULT for failing to allocate or write.
[[nodiscard]] HRESULT XtermEngine::_MoveCursor(COORD const coord) noexcept
{
    HRESULT hr = S_OK;
    const auto originalPos = _lastText;
    _trace.TraceMoveCursor(_lastText, coord);
    bool performedSoftWrap = false;
    if (coord.X != _lastText.X || coord.Y != _lastText.Y)
    {
        if (coord.X == 0 && coord.Y == 0)
        {
            _needToDisableCursor = true;
            hr = _CursorHome();
        }
        else if (_resized && _resizeQuirk)
        {
            hr = _CursorPosition(coord);
        }
        else if (coord.X == 0 && coord.Y == (_lastText.Y + 1))
        {
            // Down one line, at the start of the line.

            // If the previous line wrapped, then the cursor is already at this
            //      position, we just don't know it yet. Don't emit anything.
            bool previousLineWrapped = false;
            if (_wrappedRow.has_value())
            {
                previousLineWrapped = coord.Y == _wrappedRow.value() + 1;
            }

            if (previousLineWrapped)
            {
                performedSoftWrap = true;
                _trace.TraceWrapped();
                hr = S_OK;
            }
            else
            {
                std::string seq = "\r\n";
                hr = _Write(seq);
            }
        }
        else if (_delayedEolWrap)
        {
            // GH#1245, GH#357 - If we were in the delayed EOL wrap state, make
            // sure to _manually_ position the cursor now, with a full CUP
            // sequence, don't try and be clever with \b or \r or other control
            // sequences. Different terminals (conhost, gnome-terminal, wt) all
            // behave differently with how the cursor behaves at an end of line.
            // This is the only solution that works in all of them, and also
            // works wrapped lines emitted by conpty.
            //
            // Make sure to do this _after_ the possible \r\n branch above,
            // otherwise we might accidentally break wrapped lines (GH#405)
            hr = _CursorPosition(coord);
        }
        else if (coord.X == 0 && coord.Y == _lastText.Y)
        {
            // Start of this line
            std::string seq = "\r";
            hr = _Write(seq);
        }
        else if (coord.X == _lastText.X && coord.Y == (_lastText.Y + 1))
        {
            // Down one line, same X position
            std::string seq = "\n";
            hr = _Write(seq);
        }
        else if (coord.X == (_lastText.X - 1) && coord.Y == (_lastText.Y))
        {
            // Back one char, same Y position
            std::string seq = "\b";
            hr = _Write(seq);
        }
        else if (coord.Y == _lastText.Y && coord.X > _lastText.X)
        {
            // Same line, forward some distance
            short distance = coord.X - _lastText.X;
            hr = _CursorForward(distance);
        }
        else
        {
            _needToDisableCursor = true;
            hr = _CursorPosition(coord);
        }

        if (SUCCEEDED(hr))
        {
            _lastText = coord;
        }
    }

    _deferredCursorPos = INVALID_COORDS;

    _wrappedRow = std::nullopt;

    _delayedEolWrap = false;

    return hr;
}

// Routine Description:
// - Scrolls the existing data on the in-memory frame by the scroll region
//      deltas we have collectively received through the Invalidate methods
//      since the last time this was called.
//  Move the cursor to the origin, and insert or delete rows as appropriate.
//      The inserted rows will be blank, but marked invalid by InvalidateScroll,
//      so they will later be written by PaintBufferLine.
// Arguments:
// - <none>
// Return Value:
// - S_OK if we succeeded, else an appropriate HRESULT for failing to allocate or write.
[[nodiscard]] HRESULT XtermEngine::ScrollFrame() noexcept
try
{
    _trace.TraceScrollFrame(_scrollDelta);

    if (_scrollDelta.x() != 0)
    {
        // No easy way to shift left-right. Everything needs repainting.
        return InvalidateAll();
    }
    if (_scrollDelta.y() == 0)
    {
        // There's nothing to do here. Do nothing.
        return S_OK;
    }

    const short dy = _scrollDelta.y<short>();
    const short absDy = static_cast<short>(abs(dy));

    // Save the old wrap state here. We're going to clear it so that
    // _MoveCursor will definitely move us to the right position. We'll
    // restore the state afterwards.
    const auto oldWrappedRow = _wrappedRow;
    const auto oldDelayedEolWrap = _delayedEolWrap;
    _delayedEolWrap = false;
    _wrappedRow = std::nullopt;

    if (dy < 0)
    {
        // TODO GH#5228 - We could optimize this by only doing this newline work
        // when there's more invalid than just the bottom line. If only the
        // bottom line is invalid, then the next thing the Renderer is going to
        // tell us to do is print the new line at the bottom of the viewport,
        // and _MoveCursor will automatically give us the newline we want.
        // When that's implemented, we'll probably want to make sure to add a
        //   _lastText.Y += dy;
        // statement here.

        // Move the cursor to the bottom of the current viewport
        const short bottom = _lastViewport.BottomInclusive();
        RETURN_IF_FAILED(_MoveCursor({ 0, bottom }));
        // Emit some number of newlines to create space in the buffer.
        RETURN_IF_FAILED(_Write(std::string(absDy, '\n')));
    }
    else if (dy > 0)
    {
        // If we've scrolled _down_, then move the cursor to the top of the
        // buffer, and insert some newlines using the InsertLines VT sequence
        RETURN_IF_FAILED(_MoveCursor({ 0, 0 }));
        RETURN_IF_FAILED(_InsertLine(absDy));
    }

    // Restore our wrap state.
    _wrappedRow = oldWrappedRow;
    _delayedEolWrap = oldDelayedEolWrap;

    // Shift our internal tracker of the last text position according to how
    // much we've scrolled. If we manually scroll the buffer right now, by
    // moving the cursor to the bottom row of the viewport and emitting a
    // newline, we'll cause any wrapped lines to get broken.
    //
    // Instead, we'll just update our internal tracker of where the buffer
    // contents are. On this frame, we'll then still move the cursor correctly
    // relative to the new frame contents. To do this, we'll shift our
    // coordinates we're tracking, like the row that we wrapped on and the
    // position we think we left the cursor.
    //
    // See GH#5113
    _trace.TraceLastText(_lastText);
    if (_wrappedRow.has_value())
    {
        _wrappedRow.value() += dy;
        _trace.TraceSetWrapped(_wrappedRow.value());
    }

    if (_delayedEolWrap && _wrappedRow.has_value())
    {
        // If we wrapped the last line, and we're in the middle of painting it,
        // then the newline we did above just manually broke the line. What
        // we're doing here is a hack: we're going to manually re-invalidate the
        // last character of the wrapped row. When the PaintBufferLine calls
        // come back through, we'll paint this last character again, causing us
        // to get into the wrapped state once again. This is the only way to
        // ensure that if a line was wrapped, and we painted the first line in
        // one frame, and the second line in another frame that included other
        // changes _above_ the wrapped line, that we maintain the wrap state in
        // the Terminal.
        const til::rectangle lastCellOfWrappedRow{
            til::point{ _lastViewport.RightInclusive(), _wrappedRow.value() },
            til::size{ 1, 1 }
        };
        _trace.TraceInvalidate(lastCellOfWrappedRow);
        _invalidMap.set(lastCellOfWrappedRow);
    }

    // If the entire viewport was invalidated this frame, don't mark the bottom
    // line as new. There are cases where this can cause visual artifacts - see
    // GH#5039 and ConptyRoundtripTests::ClearHostTrickeryTest
    const bool allInvalidated = _invalidMap.all();
    _newBottomLine = !allInvalidated;

    return S_OK;
}
CATCH_RETURN();

// Routine Description:
// - Notifies us that the console is attempting to scroll the existing screen
//      area. Add the top or bottom rows to the invalid region, and update the
//      total scroll delta accumulated this frame.
// Arguments:
// - pcoordDelta - Pointer to character dimension (COORD) of the distance the
//      console would like us to move while scrolling.
// Return Value:
// - S_OK if we succeeded, else an appropriate HRESULT for safemath failure
[[nodiscard]] HRESULT XtermEngine::InvalidateScroll(const COORD* const pcoordDelta) noexcept
try
{
    const til::point delta{ *pcoordDelta };

    if (delta != til::point{ 0, 0 })
    {
        _trace.TraceInvalidateScroll(delta);

        // Scroll the current offset and invalidate the revealed area
        _invalidMap.translate(delta, true);

        _scrollDelta += delta;
    }

    return S_OK;
}
CATCH_RETURN();

// Routine Description:
// - Draws one line of the buffer to the screen. Writes the characters to the
//      pipe, encoded in UTF-8 or ASCII only, depending on the VtIoMode.
//      (See descriptions of both implementations for details.)
// Arguments:
// - clusters - text and column counts for each piece of text.
// - coord - character coordinate target to render within viewport
// - trimLeft - This specifies whether to trim one character width off the left
//      side of the output. Used for drawing the right-half only of a
//      double-wide character.
// - lineWrapped: true if this run we're painting is the end of a line that
//   wrapped. If we're not painting the last column of a wrapped line, then this
//   will be false.
// Return Value:
// - S_OK or suitable HRESULT error from writing pipe.
[[nodiscard]] HRESULT XtermEngine::PaintBufferLine(std::basic_string_view<Cluster> const clusters,
                                                   const COORD coord,
                                                   const bool /*trimLeft*/,
                                                   const bool lineWrapped) noexcept
{
    return _fUseAsciiOnly ?
               VtEngine::_PaintAsciiBufferLine(clusters, coord) :
               VtEngine::_PaintUtf8BufferLine(clusters, coord, lineWrapped);
}

// Method Description:
// - Wrapper for ITerminalOutputConnection. Write either an ascii-only, or a
//      proper utf-8 string, depending on our mode.
// Arguments:
// - wstr - wstring of text to be written
// Return Value:
// - S_OK or suitable HRESULT error from either conversion or writing pipe.
[[nodiscard]] HRESULT XtermEngine::WriteTerminalW(const std::wstring_view wstr) noexcept
{
    RETURN_IF_FAILED(_fUseAsciiOnly ?
                         VtEngine::_WriteTerminalAscii(wstr) :
                         VtEngine::_WriteTerminalUtf8(wstr));
    // GH#4106, GH#2011 - WriteTerminalW is only ever called by the
    // StateMachine, when we've encountered a string we don't understand. When
    // this happens, we usually don't actually trigger another frame, but we
    // _do_ want this string to immediately be sent to the terminal. Since we
    // only flush our buffer on actual frames, this means that strings we've
    // decided to pass through would have gotten buffered here until the next
    // actual frame is triggered.
    //
    // To fix this, flush here, so this string is sent to the connected terminal
    // application.

    return _Flush();
}

// Method Description:
// - Updates the window's title string. Emits the VT sequence to SetWindowTitle.
// Arguments:
// - newTitle: the new string to use for the title of the window
// Return Value:
// - S_OK
[[nodiscard]] HRESULT XtermEngine::_DoUpdateTitle(const std::wstring& newTitle) noexcept
{
    // inbox telnet uses xterm-ascii as its mode. If we're in ascii mode, don't
    //      do anything, to maintain compatibility.
    if (_fUseAsciiOnly)
    {
        return S_OK;
    }

    try
    {
        const auto converted = ConvertToA(CP_UTF8, newTitle);
        return VtEngine::_ChangeTitle(converted);
    }
    CATCH_RETURN();
}
