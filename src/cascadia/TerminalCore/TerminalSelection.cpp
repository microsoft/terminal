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
        return _buffer->GetTextRects(_selectionStart, _selectionEnd, _blockSelection);
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
const COORD Terminal::GetSelectionAnchor() const noexcept
{
    return _selectionStart;
}

// Method Description:
// - Checks if selection is on a single cell
// Return Value:
// - bool representing if selection is only a single cell. Used for copyOnSelect
const bool Terminal::_IsSingleCellSelection() const noexcept
{
    return (_selectionStart == _selectionEnd);
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
// - Perform a multi-click selection at viewportPos expanding according to the expansionMode
// Arguments:
// - viewportPos: the (x,y) coordinate on the visible viewport
// - expansionMode: the SelectionExpansionMode to dictate the boundaries of the selection anchors
void Terminal::MultiClickSelection(const COORD viewportPos, SelectionExpansionMode expansionMode)
{
    // set the selection pivot to expand the selection using SetSelectionEnd()
    _selectionPivot = _ConvertToBufferCell(viewportPos);
    _buffer->GetSize().Clamp(_selectionPivot);

    _multiClickSelectionMode = expansionMode;
    SetSelectionEnd(viewportPos);

    // we need to set the _selectionPivot again
    // for future shift+clicks
    _selectionPivot = _selectionStart;
}

// Method Description:
// - Record the position of the beginning of a selection
// Arguments:
// - position: the (x,y) coordinate on the visible viewport
void Terminal::SetSelectionAnchor(const COORD viewportPos)
{
    _selectionPivot = _ConvertToBufferCell(viewportPos);
    _buffer->GetSize().Clamp(_selectionPivot);

    _multiClickSelectionMode = SelectionExpansionMode::Cell;

    // Unlike MultiClickSelection(), we need to set
    // these vars BEFORE SetSelectionEnd()
    _allowSingleCharSelection = (_copyOnSelect) ? false : true;
    _selectionStart = _selectionPivot;

    SetSelectionEnd(viewportPos);
}

// Method Description:
// - Update selection anchors when dragging to a position
// - based on the selection expansion mode
// Arguments:
// - viewportPos: the (x,y) coordinate on the visible viewport
// - expansionModeOverwrite: overwrites the _multiClickSelectionMode for this function call. Used for ShiftClick
void Terminal::SetSelectionEnd(const COORD viewportPos, std::optional<SelectionExpansionMode> expansionModeOverwrite)
{
    auto textBufferPos = _ConvertToBufferCell(viewportPos);
    _buffer->GetSize().Clamp(textBufferPos);

    // if this is a shiftClick action, we need to overwrite the _multiClickSelectionMode value (even if it's the same)
    // Otherwise, we may accidentally expand during other selection-based actions
    const auto shiftClick = expansionModeOverwrite.has_value();
    _multiClickSelectionMode = expansionModeOverwrite.has_value() ? *expansionModeOverwrite : _multiClickSelectionMode;

    switch (_multiClickSelectionMode)
    {
    case SelectionExpansionMode::Line:
        _ChunkSelectionByLine(textBufferPos, shiftClick);
        break;
    case SelectionExpansionMode::Word:
        _ChunkSelectionByWord(textBufferPos, shiftClick);
        break;
    case SelectionExpansionMode::Cell:
    default:
        _ChunkSelectionByCell(textBufferPos, shiftClick);

        if (_copyOnSelect && !_IsSingleCellSelection())
        {
            _allowSingleCharSelection = true;
        }
        break;
    }

    _selectionActive = true;
}

// Method Description:
// - Update the selection anchors when dragging to a position
// - Used for SelectionExpansionMode::Word
// Arguments:
// - targetPos: the (x,y) coordinate we are dragging to on the text buffer
// - shiftClick: when enabled, only update the _endSelectionPosition
void Terminal::_ChunkSelectionByCell(const COORD targetPos, bool /*shiftClick*/)
{
    // TODO GH #4557: Shift+Multi-Click Expansion
    //   shiftClick is reserved for that implementation
    if (_buffer->GetSize().CompareInBounds(targetPos, _selectionPivot) <= 0)
    {
        // target is before pivot
        _selectionStart = targetPos;
        _selectionEnd = _selectionPivot;
    }
    else
    {
        // target is after pivot
        _selectionStart = _selectionPivot;
        _selectionEnd = targetPos;
    }
}

// Method Description:
// - Update the selection anchors when dragging to a position
// - Used for SelectionExpansionMode::Word
// Arguments:
// - targetPos: the (x,y) coordinate we are dragging to on the text buffer
// - shiftClick: when enabled, only update the _endSelectionPosition
void Terminal::_ChunkSelectionByWord(const COORD targetPos, bool /*shiftClick*/)
{
    // TODO GH #4557: Shift+Multi-Click Expansion
    //   shiftClick is reserved for that implementation
    if (_buffer->GetSize().CompareInBounds(targetPos, _selectionPivot) <= 0)
    {
        // target is before pivot
        //  - start --> ExpandLeft(target)
        //  - end --> ExpandRight(pivot)

        _selectionStart = _ExpandDoubleClickSelectionLeft(targetPos);
        _selectionEnd = _ExpandDoubleClickSelectionRight(_selectionPivot);
    }
    else
    {
        // target is after pivot
        //  - start --> ExpandLeft(pivot)
        //  - end --> ExpandRight(target)

        _selectionStart = _ExpandDoubleClickSelectionLeft(_selectionPivot);
        _selectionEnd = _ExpandDoubleClickSelectionRight(targetPos);
    }
}

// Method Description:
// - Update the selection anchors when dragging to a position
// - Used for SelectionExpansionMode::Line
// Arguments:
// - targetPos: the (x,y) coordinate we are dragging to on the text buffer
// - shiftClick: when enabled, only update the _endSelectionPosition
void Terminal::_ChunkSelectionByLine(const COORD targetPos, bool /*shiftClick*/)
{
    // TODO GH #4557: Shift+Multi-Click Expansion
    //   shiftClick is reserved for that implementation
    const auto bufferSize = _buffer->GetSize();
    if (bufferSize.CompareInBounds(targetPos, _selectionPivot) <= 0)
    {
        // target is before pivot
        //  - start --> ExpandLeft(target)
        //  - end --> ExpandRight(pivot)

        _selectionStart = { bufferSize.Left(), targetPos.Y };
        _selectionEnd = { bufferSize.RightInclusive(), _selectionPivot.Y };
    }
    else
    {
        // target is after pivot
        //  - start --> ExpandLeft(pivot)
        //  - end --> ExpandRight(target)

        _selectionStart = { bufferSize.Left(), _selectionPivot.Y };
        _selectionEnd = { bufferSize.RightInclusive(), targetPos.Y };
    }
}

// Method Description:
// - enable/disable block selection (ALT + selection)
// Arguments:
// - isEnabled: new value for _blockSelection
void Terminal::SetBlockSelection(const bool isEnabled) noexcept
{
    _blockSelection = isEnabled;
}

// Method Description:
// - clear selection data and disable rendering it
void Terminal::ClearSelection()
{
    _selectionActive = false;
    _allowSingleCharSelection = false;
    _selectionStart = { 0, 0 };
    _selectionEnd = { 0, 0 };

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

    return _buffer->GetTextForClipboard(!_blockSelection,
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
COORD Terminal::_ConvertToBufferCell(const COORD viewportPos) const noexcept
{
    const auto yPos = base::ClampedNumeric<short>(_VisibleStartIndex()) + viewportPos.Y;
    return COORD{ viewportPos.X, yPos };
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
