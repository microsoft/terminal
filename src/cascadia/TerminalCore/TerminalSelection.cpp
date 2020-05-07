// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Terminal.hpp"
#include "unicode.hpp"

using namespace Microsoft::Terminal::Core;
using namespace winrt::Microsoft::Terminal::Settings;

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
        return _buffer->GetTextRects(_selection->start, _selection->end, _blockSelection);
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
    const auto textBufferPos = _ConvertToBufferCell(viewportPos);

    // if this is a shiftClick action, we need to overwrite the _multiClickSelectionMode value (even if it's the same)
    // Otherwise, we may accidentally expand during other selection-based actions
    _multiClickSelectionMode = newExpansionMode.has_value() ? *newExpansionMode : _multiClickSelectionMode;

    const auto anchors = _PivotSelection(textBufferPos);
    std::tie(_selection->start, _selection->end) = _ExpandSelectionAnchors(anchors);
}

// Method Description:
// - returns a new pair of selection anchors for selecting around the pivot
// - This ensures start < end when compared
// Arguments:
// - targetPos: the (x,y) coordinate we are moving to on the text buffer
// Return Value:
// - the new start/end for a selection
std::pair<COORD, COORD> Terminal::_PivotSelection(const COORD targetPos) const
{
    if (_buffer->GetSize().CompareInBounds(targetPos, _selection->pivot) <= 0)
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
const TextBuffer::TextAndColor Terminal::RetrieveSelectedTextFromBuffer(bool singleLine) const
{
    const auto selectionRects = _GetSelectionRects();

    std::function<COLORREF(TextAttribute&)> GetForegroundColor = std::bind(&Terminal::GetForegroundColor, this, std::placeholders::_1);
    std::function<COLORREF(TextAttribute&)> GetBackgroundColor = std::bind(&Terminal::GetBackgroundColor, this, std::placeholders::_1);

    return _buffer->GetText(!singleLine,
                            !singleLine,
                            selectionRects,
                            GetForegroundColor,
                            GetBackgroundColor);
}

// Method Description:
// - convert viewport position to the corresponding location on the buffer
// Arguments:
// - viewportPos: a coordinate relative to the visible viewport
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

// Method Description:
// - update the endSelectionPosition anchor according to the direction provided.
//    It is moved according the the expansion mode.
// Arguments:
// - dir: the direction that the selection anchor will attempt to move to
// - mode: the selection expansion mode that the selection anchor will adhere to
// Return Value:
// - <none>
void Terminal::MoveSelectionAnchor(Direction dir, SelectionExpansionMode mode, SelectionAnchorTarget target)
{
    // convert to buffer coordinates
    COORD endpoint = ((target == SelectionAnchorTarget::Start) ? _selection->start : _selection->end);

    switch (mode)
    {
    case SelectionExpansionMode::Cell:
        _UpdateAnchorByCell(dir, endpoint);
        break;
    case SelectionExpansionMode::Word:
        _UpdateAnchorByWord(dir, endpoint, target);
        break;
    case SelectionExpansionMode::Viewport:
        _UpdateAnchorByViewport(dir, endpoint);
        break;
    case SelectionExpansionMode::Buffer:
        _UpdateAnchorByBuffer(dir, endpoint);
        break;
    default:
        // the provided selection expansion mode is not supported
        THROW_HR(E_NOTIMPL);
        return;
    }

    // scroll if necessary
    bool notifyScroll = false;
    if (endpoint.Y < _VisibleStartIndex())
    {
        _scrollOffset = ViewStartIndex() - endpoint.Y;
        notifyScroll = true;
    }
    else if (endpoint.Y > _VisibleEndIndex())
    {
        _scrollOffset = std::max(0, ViewStartIndex() - endpoint.Y);
        notifyScroll = true;
    }

    if (notifyScroll)
    {
        _buffer->GetRenderTarget().TriggerRedrawAll();
        _NotifyScrollEvent();
    }

    // update internal state of selection anchor and vertical offset
    THROW_IF_FAILED(ShortSub(endpoint.Y, gsl::narrow<SHORT>(ViewStartIndex()), &endpoint.Y));
    if (target == SelectionAnchorTarget::Start)
    {
        _selection->start = endpoint;
    }
    else
    {
        _selection->end = endpoint;
    }
}

// Method Description:
// - update the endSelectionPosition anchor by one cell according to the direction provided.
// Arguments:
// - dir: the direction that the selection anchor will attempt to move to
// Return Value:
// - <none>
void Terminal::_UpdateAnchorByCell(Direction dir, COORD& anchor)
{
    auto bufferViewport = _buffer->GetSize();
    switch (dir)
    {
    case Direction::Left:
        bufferViewport.DecrementInBounds(anchor);
        break;
    case Direction::Right:
        bufferViewport.IncrementInBounds(anchor);
        break;
    case Direction::Up:
        THROW_IF_FAILED(ShortSub(anchor.Y, 1, &anchor.Y));
        break;
    case Direction::Down:
        THROW_IF_FAILED(ShortAdd(anchor.Y, 1, &anchor.Y));
        break;
    default:
        // direction is not supported.
        THROW_HR(E_NOTIMPL);
        return;
    }
}

// Method Description:
// - update the endSelectionPosition anchor by one word according to the direction provided.
// Arguments:
// - dir: the direction that the selection anchor will attempt to move to
// Return Value:
// - <none>
void Terminal::_UpdateAnchorByWord(Direction dir, COORD& anchor, SelectionAnchorTarget target)
{
    // we need this to be able to compare it later
    // NOTE: if target = START --> return END; else return START
    COORD otherSelectionAnchor = ((target == SelectionAnchorTarget::Start) ? _selection->end : _selection->start);

    const auto bufferViewport = _buffer->GetSize();
    const auto comparison = bufferViewport.CompareInBounds(anchor, otherSelectionAnchor);
    switch (dir)
    {
    case Direction::Up:
        THROW_IF_FAILED(ShortSub(anchor.Y, 1, &anchor.Y));
        bufferViewport.Clamp(anchor);
    case Direction::Left:
        if (comparison < 0)
        {
            // get on new run, before expanding left
            bufferViewport.DecrementInBounds(anchor);
        }
        anchor = _buffer->GetWordStart(anchor, _wordDelimiters);
        if (comparison > 0)
        {
            // get off of run, after expanding left
            bufferViewport.DecrementInBounds(anchor);
        }
        break;
    case Direction::Down:
        THROW_IF_FAILED(ShortAdd(anchor.Y, 1, &anchor.Y));
        bufferViewport.Clamp(anchor);
    case Direction::Right:
        if (comparison > 0)
        {
            // get on new run, before expanding left
            bufferViewport.IncrementInBounds(anchor);
        }
        anchor = _buffer->GetWordEnd(anchor, _wordDelimiters);
        if (comparison < 0)
        {
            // get off of run, after expanding left
            bufferViewport.IncrementInBounds(anchor);
        }
        break;
    default:
        // direction is not supported.
        THROW_HR(E_NOTIMPL);
        return;
    }
}

// Method Description:
// - update the endSelectionPosition anchor by one viewport according to the direction provided.
// Arguments:
// - dir: the direction that the selection anchor will attempt to move to
// Return Value:
// - <none>
void Terminal::_UpdateAnchorByViewport(Direction dir, COORD& anchor)
{
    const auto bufferViewport = _buffer->GetSize();
    switch (dir)
    {
    case Direction::Up:
        THROW_IF_FAILED(ShortSub(anchor.Y, _mutableViewport.Height(), &anchor.Y));
        if (!bufferViewport.IsInBounds(anchor))
        {
            anchor = bufferViewport.Origin();
        }
        break;
    case Direction::Down:
        THROW_IF_FAILED(ShortAdd(anchor.Y, _mutableViewport.Height(), &anchor.Y));
        if (!bufferViewport.IsInBounds(anchor))
        {
            anchor = { _mutableViewport.RightInclusive(), _mutableViewport.BottomInclusive() };
        }
        break;
    case Direction::Left:
        anchor.X = _mutableViewport.Left();
        break;
    case Direction::Right:
        anchor.X = _mutableViewport.RightInclusive();
        break;
    default:
        // direction is not supported.
        THROW_HR(E_NOTIMPL);
        return;
    }
}

// Method Description:
// - update the endSelectionPosition anchor by the size of the buffer according to the direction provided.
// Arguments:
// - dir: the direction that the selection anchor will attempt to move to
// Return Value:
// - <none>
void Terminal::_UpdateAnchorByBuffer(Direction dir, COORD& anchor)
{
    const auto bufferViewport = _buffer->GetSize();
    switch (dir)
    {
    case Direction::Up:
        anchor = bufferViewport.Origin();
        break;
    case Direction::Down:
        anchor = { bufferViewport.RightInclusive(), bufferViewport.BottomInclusive() };
        break;
    case Direction::Left:
    case Direction::Right:
    default:
        // direction is not supported.
        THROW_HR(E_NOTIMPL);
        return;
    }
}
