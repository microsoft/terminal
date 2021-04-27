// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Terminal.hpp"
#include "unicode.hpp"

using namespace Microsoft::Terminal::Core;

/* Selection Pivot Description:
 *   The pivot helps properly update the selection when a user moves a selection over itself
 *   See SelectionTest::DoubleClickDrag_Left for an example of the functionality mentioned here
 *   As an example, consider the following scenario...
 *     1. Perform a word selection (double-click) on a word
 *
 *                  |-position where we double-clicked
 *                 _|_
 *               |word|
 *                |--|
 *  start & pivot-|  |-end
 *
 *     2. Drag your mouse down a line
 *
 *
 *  start & pivot-|__________
 *             __|word_______|
 *            |______|
 *                  |
 *                  |-end & mouse position
 *
 *     3. Drag your mouse up two lines
 *
 *                  |-start & mouse position
 *                  |________
 *             ____|   ______|
 *            |___w|ord
 *                |-end & pivot
 *
 *    The pivot never moves until a new selection is created. It ensures that that cell will always be selected.
 */

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
        return _buffer->GetTextRects(_selection->start, _selection->end, _blockSelection, false);
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
    return _selection->start;
}

// Method Description:
// - Get the current end anchor position relative to the whole text buffer
// Arguments:
// - None
// Return Value:
// - None
const COORD Terminal::GetSelectionEnd() const noexcept
{
    return _selection->end;
}

// Method Description:
// - Checks if selection is active
// Return Value:
// - bool representing if selection is active. Used to decide copy/paste on right click
const bool Terminal::IsSelectionActive() const noexcept
{
    return _selection.has_value();
}

const bool Terminal::IsBlockSelection() const noexcept
{
    return _blockSelection;
}

// Method Description:
// - Perform a multi-click selection at viewportPos expanding according to the expansionMode
// Arguments:
// - viewportPos: the (x,y) coordinate on the visible viewport
// - expansionMode: the SelectionExpansionMode to dictate the boundaries of the selection anchors
void Terminal::MultiClickSelection(const COORD viewportPos, SelectionExpansionMode expansionMode)
{
    // set the selection pivot to expand the selection using SetSelectionEnd()
    _selection = SelectionAnchors{};
    _selection->pivot = _ConvertToBufferCell(viewportPos);

    _multiClickSelectionMode = expansionMode;
    SetSelectionEnd(viewportPos);

    // we need to set the _selectionPivot again
    // for future shift+clicks
    _selection->pivot = _selection->start;
}

// Method Description:
// - Record the position of the beginning of a selection
// Arguments:
// - position: the (x,y) coordinate on the visible viewport
void Terminal::SetSelectionAnchor(const COORD viewportPos)
{
    _selection = SelectionAnchors{};
    _selection->pivot = _ConvertToBufferCell(viewportPos);

    _multiClickSelectionMode = SelectionExpansionMode::Cell;
    SetSelectionEnd(viewportPos);

    _selection->start = _selection->pivot;
}

// Method Description:
// - Update selection anchors when dragging to a position
// - based on the selection expansion mode
// Arguments:
// - viewportPos: the (x,y) coordinate on the visible viewport
// - newExpansionMode: overwrites the _multiClickSelectionMode for this function call. Used for ShiftClick
void Terminal::SetSelectionEnd(const COORD viewportPos, std::optional<SelectionExpansionMode> newExpansionMode)
{
    if (!_selection.has_value())
    {
        // capture a log for spurious endpoint sets without an active selection
        LOG_HR(E_ILLEGAL_STATE_CHANGE);
        return;
    }

    const auto textBufferPos = _ConvertToBufferCell(viewportPos);

    // if this is a shiftClick action, we need to overwrite the _multiClickSelectionMode value (even if it's the same)
    // Otherwise, we may accidentally expand during other selection-based actions
    _multiClickSelectionMode = newExpansionMode.has_value() ? *newExpansionMode : _multiClickSelectionMode;

    bool targetStart = false;
    const auto anchors = _PivotSelection(textBufferPos, targetStart);
    const auto expandedAnchors = _ExpandSelectionAnchors(anchors);

    if (newExpansionMode.has_value())
    {
        // shift-click operations only expand the target side
        auto& anchorToExpand = targetStart ? _selection->start : _selection->end;
        anchorToExpand = targetStart ? expandedAnchors.first : expandedAnchors.second;

        // the other anchor should then become the pivot (we don't expand it)
        auto& anchorToPivot = targetStart ? _selection->end : _selection->start;
        anchorToPivot = _selection->pivot;
    }
    else
    {
        // expand both anchors
        std::tie(_selection->start, _selection->end) = expandedAnchors;
    }
}

// Method Description:
// - returns a new pair of selection anchors for selecting around the pivot
// - This ensures start < end when compared
// Arguments:
// - targetPos: the (x,y) coordinate we are moving to on the text buffer
// - targetStart: if true, target will be the new start. Otherwise, target will be the new end.
// Return Value:
// - the new start/end for a selection
std::pair<COORD, COORD> Terminal::_PivotSelection(const COORD targetPos, bool& targetStart) const
{
    if (targetStart = _buffer->GetSize().CompareInBounds(targetPos, _selection->pivot) <= 0)
    {
        // target is before pivot
        // treat target as start
        return std::make_pair(targetPos, _selection->pivot);
    }
    else
    {
        // target is after pivot
        // treat pivot as start
        return std::make_pair(_selection->pivot, targetPos);
    }
}

// Method Description:
// - Update the selection anchors to expand according to the expansion mode
// Arguments:
// - anchors: a pair of selection anchors representing a desired selection
// Return Value:
// - the new start/end for a selection
std::pair<COORD, COORD> Terminal::_ExpandSelectionAnchors(std::pair<COORD, COORD> anchors) const
{
    COORD start = anchors.first;
    COORD end = anchors.second;

    const auto bufferSize = _buffer->GetSize();
    switch (_multiClickSelectionMode)
    {
    case SelectionExpansionMode::Line:
        start = { bufferSize.Left(), start.Y };
        end = { bufferSize.RightInclusive(), end.Y };
        break;
    case SelectionExpansionMode::Word:
        start = _buffer->GetWordStart(start, _wordDelimiters);
        end = _buffer->GetWordEnd(end, _wordDelimiters);
        break;
    case SelectionExpansionMode::Cell:
    default:
        // no expansion is necessary
        break;
    }
    return std::make_pair(start, end);
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
#pragma warning(disable : 26440) // changing this to noexcept would require a change to ConHost's selection model
void Terminal::ClearSelection()
{
    _selection = std::nullopt;
}

// Method Description:
// - get wstring text from highlighted portion of text buffer
// Arguments:
// - singleLine: collapse all of the text to one line
// Return Value:
// - wstring text from buffer. If extended to multiple lines, each line is separated by \r\n
const TextBuffer::TextAndColor Terminal::RetrieveSelectedTextFromBuffer(bool singleLine)
{
    auto lock = LockForReading();

    const auto selectionRects = _GetSelectionRects();

    const auto GetAttributeColors = std::bind(&Terminal::GetAttributeColors, this, std::placeholders::_1);

    // GH#6740: Block selection should preserve the visual structure:
    // - CRLFs need to be added - so the lines structure is preserved
    // - We should apply formatting above to wrapped rows as well (newline should be added).
    // GH#9706: Trimming of trailing white-spaces in block selection is configurable.
    const auto includeCRLF = !singleLine || _blockSelection;
    const auto trimTrailingWhitespace = !singleLine && (!_blockSelection || _trimBlockSelection);
    const auto formatWrappedRows = _blockSelection;
    return _buffer->GetText(includeCRLF, trimTrailingWhitespace, selectionRects, GetAttributeColors, formatWrappedRows);
}

// Method Description:
// - convert viewport position to the corresponding location on the buffer
// Arguments:
// - viewportPos: a coordinate on the viewport
// Return Value:
// - the corresponding location on the buffer
COORD Terminal::_ConvertToBufferCell(const COORD viewportPos) const
{
    const auto yPos = base::ClampedNumeric<short>(_VisibleStartIndex()) + viewportPos.Y;
    COORD bufferPos = { viewportPos.X, yPos };
    _buffer->GetSize().Clamp(bufferPos);
    return bufferPos;
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
