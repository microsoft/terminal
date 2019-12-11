// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Terminal.hpp"
#include "unicode.hpp"

using namespace Microsoft::Terminal::Core;

// Method Description:
// - Helper to determine the selected region of the buffer. Used for rendering.
// Return Value:
// - A vector of rectangles representing the regions to select, line by line. They are absolute coordinates relative to the buffer origin.
std::vector<SMALL_RECT> Terminal::_GetSelectionRects() const noexcept
{
    std::vector<SMALL_RECT> result;

    if (!IsSelectionActive())
    {
        return result;
    }

    try
    {
        // NOTE: (0,0) is the top-left of the screen
        // the physically "higher" coordinate is closer to the top-left
        // the physically "lower" coordinate is closer to the bottom-right
        const auto [higherCoord, lowerCoord] = _PreprocessSelectionCoords();

        SHORT selectionRectSize;
        THROW_IF_FAILED(ShortSub(lowerCoord.Y, higherCoord.Y, &selectionRectSize));
        THROW_IF_FAILED(ShortAdd(selectionRectSize, 1, &selectionRectSize));

        std::vector<SMALL_RECT> selectionArea;
        selectionArea.reserve(selectionRectSize);
        for (auto row = higherCoord.Y; row <= lowerCoord.Y; row++)
        {
            SMALL_RECT selectionRow = _GetSelectionRow(row, higherCoord, lowerCoord);
            _ExpandSelectionRow(selectionRow);
            selectionArea.emplace_back(selectionRow);
        }
        result.swap(selectionArea);
    }
    CATCH_LOG();
    return result;
}

// Method Description:
// - convert selection anchors to proper coordinates for rendering
// NOTE: (0,0) is top-left so vertical comparison is inverted
// Arguments:
// - None
// Return Value:
// - tuple.first: the physically "higher" coordinate (closer to the top-left)
// - tuple.second: the physically "lower" coordinate (closer to the bottom-right)
std::tuple<COORD, COORD> Terminal::_PreprocessSelectionCoords() const
{
    // create these new anchors for comparison and rendering
    COORD selectionAnchorWithOffset{ _selectionAnchor };
    COORD endSelectionPositionWithOffset{ _endSelectionPosition };

    // Add anchor offset here to update properly on new buffer output
    THROW_IF_FAILED(ShortAdd(selectionAnchorWithOffset.Y, _selectionVerticalOffset, &selectionAnchorWithOffset.Y));
    THROW_IF_FAILED(ShortAdd(endSelectionPositionWithOffset.Y, _selectionVerticalOffset, &endSelectionPositionWithOffset.Y));

    // clamp anchors to be within buffer bounds
    const auto bufferSize = _buffer->GetSize();
    bufferSize.Clamp(selectionAnchorWithOffset);
    bufferSize.Clamp(endSelectionPositionWithOffset);

    // NOTE: (0,0) is top-left so vertical comparison is inverted
    // CompareInBounds returns whether A is to the left of (rv<0), equal to (rv==0), or to the right of (rv>0) B.
    // Here, we want the "left"most coordinate to be the one "higher" on the screen. The other gets the dubious honor of
    // being the "lower."
    return bufferSize.CompareInBounds(selectionAnchorWithOffset, endSelectionPositionWithOffset) <= 0 ?
               std::make_tuple(selectionAnchorWithOffset, endSelectionPositionWithOffset) :
               std::make_tuple(endSelectionPositionWithOffset, selectionAnchorWithOffset);
}

// Method Description:
// - constructs the selection row at the given row
// NOTE: (0,0) is top-left so vertical comparison is inverted
// Arguments:
// - row: the buffer y-value under observation
// - higherCoord: the physically "higher" coordinate (closer to the top-left)
// - lowerCoord: the physically "lower" coordinate (closer to the bottom-right)
// Return Value:
// - the selection row needed for rendering
SMALL_RECT Terminal::_GetSelectionRow(const SHORT row, const COORD higherCoord, const COORD lowerCoord) const
{
    SMALL_RECT selectionRow;

    selectionRow.Top = row;
    selectionRow.Bottom = row;

    if (_boxSelection || higherCoord.Y == lowerCoord.Y)
    {
        selectionRow.Left = std::min(higherCoord.X, lowerCoord.X);
        selectionRow.Right = std::max(higherCoord.X, lowerCoord.X);
    }
    else
    {
        selectionRow.Left = (row == higherCoord.Y) ? higherCoord.X : _buffer->GetSize().Left();
        selectionRow.Right = (row == lowerCoord.Y) ? lowerCoord.X : _buffer->GetSize().RightInclusive();
    }

    return selectionRow;
}

// Method Description:
// - Get the current anchor position
// Arguments:
// - None
// Return Value:
// - None
const COORD Terminal::GetSelectionAnchor() const
{
    return _selectionAnchor;
}

// Method Description:
// - Expand the selection row according to selection mode and wide glyphs
// - this is particularly useful for box selections (ALT + selection)
// Arguments:
// - selectionRow: the selection row to be expanded
// Return Value:
// - modifies selectionRow's Left and Right values to expand properly
void Terminal::_ExpandSelectionRow(SMALL_RECT& selectionRow) const
{
    const auto row = selectionRow.Top;

    // expand selection for Double/Triple Click
    if (_multiClickSelectionMode == SelectionExpansionMode::Word)
    {
        selectionRow.Left = _ExpandDoubleClickSelectionLeft({ selectionRow.Left, row }).X;
        selectionRow.Right = _ExpandDoubleClickSelectionRight({ selectionRow.Right, row }).X;
    }
    else if (_multiClickSelectionMode == SelectionExpansionMode::Line)
    {
        selectionRow.Left = _buffer->GetSize().Left();
        selectionRow.Right = _buffer->GetSize().RightInclusive();
    }

    // expand selection for Wide Glyphs
    selectionRow.Left = _ExpandWideGlyphSelectionLeft(selectionRow.Left, row);
    selectionRow.Right = _ExpandWideGlyphSelectionRight(selectionRow.Right, row);
}

// Method Description:
// - Expands the selection left-wards to cover a wide glyph, if necessary
// Arguments:
// - position: the (x,y) coordinate on the visible viewport
// Return Value:
// - updated x position to encapsulate the wide glyph
SHORT Terminal::_ExpandWideGlyphSelectionLeft(const SHORT xPos, const SHORT yPos) const
{
    // don't change the value if at/outside the boundary
    const auto bufferSize = _buffer->GetSize();
    if (xPos <= bufferSize.Left() || xPos > bufferSize.RightInclusive())
    {
        return xPos;
    }

    COORD position{ xPos, yPos };
    const auto attr = _buffer->GetCellDataAt(position)->DbcsAttr();
    if (attr.IsTrailing())
    {
        // move off by highlighting the lead half too.
        // alters position.X
        bufferSize.DecrementInBounds(position);
    }
    return position.X;
}

// Method Description:
// - Expands the selection right-wards to cover a wide glyph, if necessary
// Arguments:
// - position: the (x,y) coordinate on the visible viewport
// Return Value:
// - updated x position to encapsulate the wide glyph
SHORT Terminal::_ExpandWideGlyphSelectionRight(const SHORT xPos, const SHORT yPos) const
{
    // don't change the value if at/outside the boundary
    const auto bufferSize = _buffer->GetSize();
    if (xPos < bufferSize.Left() || xPos >= bufferSize.RightInclusive())
    {
        return xPos;
    }

    COORD position{ xPos, yPos };
    const auto attr = _buffer->GetCellDataAt(position)->DbcsAttr();
    if (attr.IsLeading())
    {
        // move off by highlighting the trailing half too.
        // alters position.X
        bufferSize.IncrementInBounds(position);
    }
    return position.X;
}

// Method Description:
// - Checks if selection is on a single cell
// Return Value:
// - bool representing if selection is only a single cell. Used for copyOnSelect
const bool Terminal::_IsSingleCellSelection() const noexcept
{
    return (_selectionAnchor == _endSelectionPosition);
}

// Method Description:
// - Checks if selection is active
// Return Value:
// - bool representing if selection is active. Used to decide copy/paste on right click
const bool Terminal::IsSelectionActive() const noexcept
{
    // A single cell selection is not considered an active selection,
    // if it's not allowed
    if (!_allowSingleCharSelection && _IsSingleCellSelection())
    {
        return false;
    }
    return _selectionActive;
}

// Method Description:
// - Checks if the CopyOnSelect setting is active
// Return Value:
// - true if feature is active, false otherwise.
const bool Terminal::IsCopyOnSelectActive() const noexcept
{
    return _copyOnSelect;
}

// Method Description:
// - Select the sequence between delimiters defined in Settings
// Arguments:
// - position: the (x,y) coordinate on the visible viewport
void Terminal::DoubleClickSelection(const COORD position)
{
    COORD positionWithOffsets = _ConvertToBufferCell(position);

    // scan leftwards until delimiter is found and
    // set selection anchor to one right of that spot
    _selectionAnchor = _ExpandDoubleClickSelectionLeft(positionWithOffsets);
    THROW_IF_FAILED(ShortSub(_selectionAnchor.Y, gsl::narrow<SHORT>(_ViewStartIndex()), &_selectionAnchor.Y));
    _selectionVerticalOffset = gsl::narrow<SHORT>(_ViewStartIndex());

    // scan rightwards until delimiter is found and
    // set endSelectionPosition to one left of that spot
    _endSelectionPosition = _ExpandDoubleClickSelectionRight(positionWithOffsets);
    THROW_IF_FAILED(ShortSub(_endSelectionPosition.Y, gsl::narrow<SHORT>(_ViewStartIndex()), &_endSelectionPosition.Y));

    _selectionActive = true;
    _multiClickSelectionMode = SelectionExpansionMode::Word;
}

// Method Description:
// - Select the entire row of the position clicked
// Arguments:
// - position: the (x,y) coordinate on the visible viewport
void Terminal::TripleClickSelection(const COORD position)
{
    SetSelectionAnchor({ 0, position.Y });
    SetEndSelectionPosition({ _buffer->GetSize().RightInclusive(), position.Y });

    _multiClickSelectionMode = SelectionExpansionMode::Line;
}

// Method Description:
// - Record the position of the beginning of a selection
// Arguments:
// - position: the (x,y) coordinate on the visible viewport
void Terminal::SetSelectionAnchor(const COORD position)
{
    _selectionAnchor = position;

    // include _scrollOffset here to ensure this maps to the right spot of the original viewport
    THROW_IF_FAILED(ShortSub(_selectionAnchor.Y, gsl::narrow<SHORT>(_scrollOffset), &_selectionAnchor.Y));

    // copy value of ViewStartIndex to support scrolling
    // and update on new buffer output (used in _GetSelectionRects())
    _selectionVerticalOffset = gsl::narrow<SHORT>(_ViewStartIndex());

    _selectionActive = true;
    _allowSingleCharSelection = (_copyOnSelect) ? false : true;

    SetEndSelectionPosition(position);

    _multiClickSelectionMode = SelectionExpansionMode::Cell;
}

// Method Description:
// - Record the position of the end of a selection
// Arguments:
// - position: the (x,y) coordinate on the visible viewport
void Terminal::SetEndSelectionPosition(const COORD position)
{
    _endSelectionPosition = position;

    // include _scrollOffset here to ensure this maps to the right spot of the original viewport
    THROW_IF_FAILED(ShortSub(_endSelectionPosition.Y, gsl::narrow<SHORT>(_scrollOffset), &_endSelectionPosition.Y));

    // copy value of ViewStartIndex to support scrolling
    // and update on new buffer output (used in _GetSelectionRects())
    _selectionVerticalOffset = gsl::narrow<SHORT>(_ViewStartIndex());

    if (_copyOnSelect && !_IsSingleCellSelection())
    {
        _allowSingleCharSelection = true;
    }
}

// Method Description:
// - enable/disable box selection (ALT + selection)
// Arguments:
// - isEnabled: new value for _boxSelection
void Terminal::SetBoxSelection(const bool isEnabled) noexcept
{
    _boxSelection = isEnabled;
}

// Method Description:
// - clear selection data and disable rendering it
void Terminal::ClearSelection()
{
    _selectionActive = false;
    _allowSingleCharSelection = false;
    _selectionAnchor = { 0, 0 };
    _endSelectionPosition = { 0, 0 };
    _selectionVerticalOffset = 0;

    _buffer->GetRenderTarget().TriggerSelection();
}

// Method Description:
// - get wstring text from highlighted portion of text buffer
// Arguments:
// - trimTrailingWhitespace: enable removing any whitespace from copied selection
//    and get text to appear on separate lines.
// Return Value:
// - wstring text from buffer. If extended to multiple lines, each line is separated by \r\n
const TextBuffer::TextAndColor Terminal::RetrieveSelectedTextFromBuffer(bool trimTrailingWhitespace) const
{
    std::function<COLORREF(TextAttribute&)> GetForegroundColor = std::bind(&Terminal::GetForegroundColor, this, std::placeholders::_1);
    std::function<COLORREF(TextAttribute&)> GetBackgroundColor = std::bind(&Terminal::GetBackgroundColor, this, std::placeholders::_1);

    return _buffer->GetTextForClipboard(!_boxSelection,
                                        trimTrailingWhitespace,
                                        _GetSelectionRects(),
                                        GetForegroundColor,
                                        GetBackgroundColor);
}

// Method Description:
// - expand the double click selection to the left
// - stopped by delimiter if started on delimiter
// Arguments:
// - position: buffer coordinate for selection
// Return Value:
// - updated copy of "position" to new expanded location (with vertical offset)
COORD Terminal::_ExpandDoubleClickSelectionLeft(const COORD position) const
{
    const auto bufferViewport = _buffer->GetSize();

    // force position to be within bounds
    COORD positionWithOffsets = position;
    bufferViewport.Clamp(positionWithOffsets);

    // can't expand left
    if (position.X == bufferViewport.Left())
    {
        return positionWithOffsets;
    }

    auto bufferIterator = _buffer->GetTextDataAt(positionWithOffsets);
    const auto startedOnDelimiter = _GetDelimiterClass(*bufferIterator);
    while (positionWithOffsets.X > bufferViewport.Left() && (_GetDelimiterClass(*bufferIterator) == startedOnDelimiter))
    {
        bufferViewport.DecrementInBounds(positionWithOffsets);
        bufferIterator--;
    }

    if (_GetDelimiterClass(*bufferIterator) != startedOnDelimiter)
    {
        // move off of delimiter to highlight properly
        bufferViewport.IncrementInBounds(positionWithOffsets);
    }

    return positionWithOffsets;
}

// Method Description:
// - expand the double click selection to the right
// - stopped by delimiter if started on delimiter
// Arguments:
// - position: buffer coordinate for selection
// Return Value:
// - updated copy of "position" to new expanded location (with vertical offset)
COORD Terminal::_ExpandDoubleClickSelectionRight(const COORD position) const
{
    const auto bufferViewport = _buffer->GetSize();

    // force position to be within bounds
    COORD positionWithOffsets = position;
    bufferViewport.Clamp(positionWithOffsets);

    // can't expand right
    if (position.X == bufferViewport.RightInclusive())
    {
        return positionWithOffsets;
    }

    auto bufferIterator = _buffer->GetTextDataAt(positionWithOffsets);
    const auto startedOnDelimiter = _GetDelimiterClass(*bufferIterator);
    while (positionWithOffsets.X < bufferViewport.RightInclusive() && (_GetDelimiterClass(*bufferIterator) == startedOnDelimiter))
    {
        bufferViewport.IncrementInBounds(positionWithOffsets);
        bufferIterator++;
    }

    if (_GetDelimiterClass(*bufferIterator) != startedOnDelimiter)
    {
        // move off of delimiter to highlight properly
        bufferViewport.DecrementInBounds(positionWithOffsets);
    }

    return positionWithOffsets;
}

// Method Description:
// - get delimiter class for buffer cell data
// - used for double click selection
// Arguments:
// - cellChar: the char saved to the buffer cell under observation
// Return Value:
// - the delimiter class for the given char
Terminal::DelimiterClass Terminal::_GetDelimiterClass(const std::wstring_view cellChar) const noexcept
{
    if (cellChar[0] <= UNICODE_SPACE)
    {
        return DelimiterClass::ControlChar;
    }
    else if (_wordDelimiters.find(cellChar) != std::wstring_view::npos)
    {
        return DelimiterClass::DelimiterChar;
    }
    else
    {
        return DelimiterClass::RegularChar;
    }
}

// Method Description:
// - convert viewport position to the corresponding location on the buffer
// Arguments:
// - viewportPos: a coordinate on the viewport
// Return Value:
// - the corresponding location on the buffer
COORD Terminal::_ConvertToBufferCell(const COORD viewportPos) const
{
    // Force position to be valid
    COORD positionWithOffsets = viewportPos;
    _buffer->GetSize().Clamp(positionWithOffsets);

    THROW_IF_FAILED(ShortSub(viewportPos.Y, gsl::narrow<SHORT>(_scrollOffset), &positionWithOffsets.Y));
    THROW_IF_FAILED(ShortAdd(positionWithOffsets.Y, gsl::narrow<SHORT>(_ViewStartIndex()), &positionWithOffsets.Y));
    return positionWithOffsets;
}

// Method Description:
// - This method won't be used. We just throw and do nothing. For now we
//   need this method to implement UiaData interface
// Arguments:
// - coordSelectionStart - Not used
// - coordSelectionEnd - Not used
// - attr - Not used.
void Terminal::ColorSelection(const COORD, const COORD, const TextAttribute)
{
    THROW_HR(E_NOTIMPL);
}
