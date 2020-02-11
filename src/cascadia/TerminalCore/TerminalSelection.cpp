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
        COORD startPos = { _selectionAnchor };
        COORD endPos = { _endSelectionPosition };

        // Add anchor offset here to update properly on new buffer output
        startPos.Y = base::ClampAdd(startPos.Y, _selectionVerticalOffset);
        endPos.Y = base::ClampAdd(endPos.Y, _selectionVerticalOffset);

        // clamp anchors to be within buffer bounds
        const auto bufferSize = _buffer->GetSize();
        bufferSize.Clamp(startPos);
        bufferSize.Clamp(endPos);

        return _buffer->GetTextRects(startPos, endPos, _boxSelection);
    }
    CATCH_LOG();
    return result;
}

// Method Description:
// - Get the current anchor position relative to the whole text buffer
// Arguments:
// - None
// Return Value:
// - None
const COORD Terminal::GetSelectionAnchor() const
{
    COORD selectionAnchorPos{ _selectionAnchor };
    THROW_IF_FAILED(ShortAdd(selectionAnchorPos.Y, _selectionVerticalOffset, &selectionAnchorPos.Y));

    return selectionAnchorPos;
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
#pragma warning(suppress : 26496) // cpp core checks wants this const but .Clamp() can write it.
    COORD positionWithOffsets = _ConvertToBufferCell(position);

    // scan leftwards until delimiter is found and
    // set selection anchor to one right of that spot
    _selectionAnchor = _ExpandDoubleClickSelectionLeft(positionWithOffsets);
    THROW_IF_FAILED(ShortSub(_selectionAnchor.Y, gsl::narrow<SHORT>(ViewStartIndex()), &_selectionAnchor.Y));
    _selectionVerticalOffset = gsl::narrow<SHORT>(ViewStartIndex());

    // scan rightwards until delimiter is found and
    // set endSelectionPosition to one left of that spot
    _endSelectionPosition = _ExpandDoubleClickSelectionRight(positionWithOffsets);
    THROW_IF_FAILED(ShortSub(_endSelectionPosition.Y, gsl::narrow<SHORT>(ViewStartIndex()), &_endSelectionPosition.Y));

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
    _selectionVerticalOffset = gsl::narrow<SHORT>(ViewStartIndex());

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
    _selectionVerticalOffset = gsl::narrow<SHORT>(ViewStartIndex());

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
    // force position to be within bounds
#pragma warning(suppress : 26496) // cpp core checks wants this const but .Clamp() can write it.
    COORD positionWithOffsets = position;
    _buffer->GetSize().Clamp(positionWithOffsets);

    return _buffer->GetWordStart(positionWithOffsets, _wordDelimiters);
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
    // force position to be within bounds
#pragma warning(suppress : 26496) // cpp core checks wants this const but .Clamp() can write it.
    COORD positionWithOffsets = position;
    _buffer->GetSize().Clamp(positionWithOffsets);

    return _buffer->GetWordEnd(positionWithOffsets, _wordDelimiters);
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
    THROW_IF_FAILED(ShortAdd(positionWithOffsets.Y, gsl::narrow<SHORT>(ViewStartIndex()), &positionWithOffsets.Y));
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
