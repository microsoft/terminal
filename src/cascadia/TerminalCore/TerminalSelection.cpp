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
std::vector<til::inclusive_rect> Terminal::_GetSelectionRects() const noexcept
{
    std::vector<til::inclusive_rect> result;

    if (!IsSelectionActive())
    {
        return result;
    }

    try
    {
        return _activeBuffer().GetTextRects(_selection->start, _selection->end, _blockSelection, false);
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
const til::point Terminal::GetSelectionAnchor() const noexcept
{
    return _selection->start;
}

// Method Description:
// - Get the current end anchor position relative to the whole text buffer
// Arguments:
// - None
// Return Value:
// - None
const til::point Terminal::GetSelectionEnd() const noexcept
{
    return _selection->end;
}

til::point Terminal::SelectionStartForRendering() const
{
    auto pos{ _selection->start };
    const auto bufferSize{ GetTextBuffer().GetSize() };
    bufferSize.DecrementInBounds(pos);
    pos.Y = base::ClampSub(pos.Y, _VisibleStartIndex());
    return til::point{ pos };
}

til::point Terminal::SelectionEndForRendering() const
{
    auto pos{ _selection->end };
    const auto bufferSize{ GetTextBuffer().GetSize() };
    bufferSize.IncrementInBounds(pos);
    pos.Y = base::ClampSub(pos.Y, _VisibleStartIndex());
    return til::point{ pos };
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
// - expansionMode: the SelectionExpansion to dictate the boundaries of the selection anchors
void Terminal::MultiClickSelection(const til::point viewportPos, SelectionExpansion expansionMode)
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
void Terminal::SetSelectionAnchor(const til::point viewportPos)
{
    _selection = SelectionAnchors{};
    _selection->pivot = _ConvertToBufferCell(viewportPos);

    _multiClickSelectionMode = SelectionExpansion::Char;
    SetSelectionEnd(viewportPos);

    _selection->start = _selection->pivot;
}

// Method Description:
// - Update selection anchors when dragging to a position
// - based on the selection expansion mode
// Arguments:
// - viewportPos: the (x,y) coordinate on the visible viewport
// - newExpansionMode: overwrites the _multiClickSelectionMode for this function call. Used for ShiftClick
void Terminal::SetSelectionEnd(const til::point viewportPos, std::optional<SelectionExpansion> newExpansionMode)
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

    auto targetStart = false;
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
std::pair<til::point, til::point> Terminal::_PivotSelection(const til::point targetPos, bool& targetStart) const
{
    if (targetStart = _activeBuffer().GetSize().CompareInBounds(targetPos, _selection->pivot) <= 0)
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
std::pair<til::point, til::point> Terminal::_ExpandSelectionAnchors(std::pair<til::point, til::point> anchors) const
{
    auto start = anchors.first;
    auto end = anchors.second;

    const auto bufferSize = _activeBuffer().GetSize();
    switch (_multiClickSelectionMode)
    {
    case SelectionExpansion::Line:
        start = { bufferSize.Left(), start.Y };
        end = { bufferSize.RightInclusive(), end.Y };
        break;
    case SelectionExpansion::Word:
        start = _activeBuffer().GetWordStart(start, _wordDelimiters);
        end = _activeBuffer().GetWordEnd(end, _wordDelimiters);
        break;
    case SelectionExpansion::Char:
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

bool Terminal::IsInMarkMode() const
{
    return _markMode;
}

void Terminal::ToggleMarkMode()
{
    if (_markMode)
    {
        // Exit Mark Mode
        ClearSelection();
    }
    else
    {
        // Enter Mark Mode
        // NOTE: directly set cursor state. We already should have locked before calling this function.
        _activeBuffer().GetCursor().SetIsOn(false);
        const auto cursorPos{ _activeBuffer().GetCursor().GetPosition() };
        _selection = SelectionAnchors{};
        _selection->start = cursorPos;
        _selection->end = cursorPos;
        _selection->pivot = cursorPos;
        _markMode = true;
        _blockSelection = false;
    }
}

Terminal::UpdateSelectionParams Terminal::ConvertKeyEventToUpdateSelectionParams(const ControlKeyStates mods, const WORD vkey) const
{
    if ((_markMode || mods.IsShiftPressed()) && !mods.IsAltPressed())
    {
        if (mods.IsCtrlPressed())
        {
            // Ctrl + Shift + _
            // (Mark Mode) Ctrl + _
            switch (vkey)
            {
            case VK_LEFT:
                return UpdateSelectionParams{ std::in_place, SelectionDirection::Left, SelectionExpansion::Word };
            case VK_RIGHT:
                return UpdateSelectionParams{ std::in_place, SelectionDirection::Right, SelectionExpansion::Word };
            case VK_HOME:
                return UpdateSelectionParams{ std::in_place, SelectionDirection::Left, SelectionExpansion::Buffer };
            case VK_END:
                return UpdateSelectionParams{ std::in_place, SelectionDirection::Right, SelectionExpansion::Buffer };
            }
        }
        else
        {
            // Shift + _
            // (Mark Mode) Just the vkeys
            switch (vkey)
            {
            case VK_HOME:
                return UpdateSelectionParams{ std::in_place, SelectionDirection::Left, SelectionExpansion::Viewport };
            case VK_END:
                return UpdateSelectionParams{ std::in_place, SelectionDirection::Right, SelectionExpansion::Viewport };
            case VK_PRIOR:
                return UpdateSelectionParams{ std::in_place, SelectionDirection::Up, SelectionExpansion::Viewport };
            case VK_NEXT:
                return UpdateSelectionParams{ std::in_place, SelectionDirection::Down, SelectionExpansion::Viewport };
            case VK_LEFT:
                return UpdateSelectionParams{ std::in_place, SelectionDirection::Left, SelectionExpansion::Char };
            case VK_RIGHT:
                return UpdateSelectionParams{ std::in_place, SelectionDirection::Right, SelectionExpansion::Char };
            case VK_UP:
                return UpdateSelectionParams{ std::in_place, SelectionDirection::Up, SelectionExpansion::Char };
            case VK_DOWN:
                return UpdateSelectionParams{ std::in_place, SelectionDirection::Down, SelectionExpansion::Char };
            }
        }
    }
    return std::nullopt;
}

bool Terminal::MovingEnd() const noexcept
{
    // true --> we're moving end endpoint ("lower")
    // false --> we're moving start endpoint ("higher")
    return _selection->start == _selection->pivot;
}

bool Terminal::MovingCursor() const noexcept
{
    // true --> we're moving end endpoint ("lower")
    // false --> we're moving start endpoint ("higher")
    return _selection->start == _selection->pivot && _selection->pivot == _selection->end;
}

// Method Description:
// - updates the selection endpoints based on a direction and expansion mode. Primarily used for keyboard selection.
// Arguments:
// - direction: the direction to move the selection endpoint in
// - mode: the type of movement to be performed (i.e. move by word)
// - mods: the key modifiers pressed when performing this update
void Terminal::UpdateSelection(SelectionDirection direction, SelectionExpansion mode, ControlKeyStates mods)
{
    // 1. Figure out which endpoint to update
    // One of the endpoints is the pivot,
    // signifying that the other endpoint is the one we want to move.
    auto targetPos{ MovingEnd() ? _selection->end : _selection->start };

    // 2. Perform the movement
    switch (mode)
    {
    case SelectionExpansion::Char:
        _MoveByChar(direction, targetPos);
        break;
    case SelectionExpansion::Word:
        _MoveByWord(direction, targetPos);
        break;
    case SelectionExpansion::Viewport:
        _MoveByViewport(direction, targetPos);
        break;
    case SelectionExpansion::Buffer:
        _MoveByBuffer(direction, targetPos);
        break;
    }

    // 3. Actually modify the selection
    if (_markMode && !mods.IsShiftPressed())
    {
        // [Mark Mode] + shift unpressed --> move all three (i.e. just use arrow keys)
        _selection->start = targetPos;
        _selection->end = targetPos;
        _selection->pivot = targetPos;
    }
    else
    {
        // [Mark Mode] + shift --> updating a standard selection
        // NOTE: targetStart doesn't matter here
        auto targetStart = false;
        std::tie(_selection->start, _selection->end) = _PivotSelection(targetPos, targetStart);
    }

    // 4. Scroll (if necessary)
    if (const auto viewport = _GetVisibleViewport(); !viewport.IsInBounds(targetPos))
    {
        if (const auto amtAboveView = viewport.Top() - targetPos.Y; amtAboveView > 0)
        {
            // anchor is above visible viewport, scroll by that amount
            _scrollOffset += amtAboveView;
        }
        else
        {
            // anchor is below visible viewport, scroll by that amount
            const auto amtBelowView = targetPos.Y - viewport.BottomInclusive();
            _scrollOffset -= amtBelowView;
        }
        _NotifyScrollEvent();
        _activeBuffer().TriggerScroll();
    }
}

void Terminal::SelectAll()
{
    const auto bufferSize{ _activeBuffer().GetSize() };
    _selection = SelectionAnchors{};
    _selection->start = bufferSize.Origin();
    _selection->end = { bufferSize.RightInclusive(), _GetMutableViewport().BottomInclusive() };
    _selection->pivot = _selection->end;
}

void Terminal::_MoveByChar(SelectionDirection direction, til::point& pos)
{
    switch (direction)
    {
    case SelectionDirection::Left:
        _activeBuffer().GetSize().DecrementInBounds(pos);
        pos = _activeBuffer().GetGlyphStart(pos);
        break;
    case SelectionDirection::Right:
        _activeBuffer().GetSize().IncrementInBounds(pos);
        pos = _activeBuffer().GetGlyphEnd(pos);
        break;
    case SelectionDirection::Up:
    {
        const auto bufferSize{ _activeBuffer().GetSize() };
        pos = { pos.X, std::clamp(pos.Y - 1, bufferSize.Top(), bufferSize.BottomInclusive()) };
        break;
    }
    case SelectionDirection::Down:
    {
        const auto bufferSize{ _activeBuffer().GetSize() };
        pos = { pos.X, std::clamp(pos.Y + 1, bufferSize.Top(), bufferSize.BottomInclusive()) };
        break;
    }
    }
}

void Terminal::_MoveByWord(SelectionDirection direction, til::point& pos)
{
    switch (direction)
    {
    case SelectionDirection::Left:
    {
        const auto wordStartPos{ _activeBuffer().GetWordStart(pos, _wordDelimiters) };
        if (_activeBuffer().GetSize().CompareInBounds(_selection->pivot, pos) < 0)
        {
            // If we're moving towards the pivot, move one more cell
            pos = wordStartPos;
            _activeBuffer().GetSize().DecrementInBounds(pos);
        }
        else if (wordStartPos == pos)
        {
            // already at the beginning of the current word,
            // move to the beginning of the previous word
            _activeBuffer().GetSize().DecrementInBounds(pos);
            pos = _activeBuffer().GetWordStart(pos, _wordDelimiters);
        }
        else
        {
            // move to the beginning of the current word
            pos = wordStartPos;
        }
        break;
    }
    case SelectionDirection::Right:
    {
        const auto wordEndPos{ _activeBuffer().GetWordEnd(pos, _wordDelimiters) };
        if (_activeBuffer().GetSize().CompareInBounds(pos, _selection->pivot) < 0)
        {
            // If we're moving towards the pivot, move one more cell
            pos = _activeBuffer().GetWordEnd(pos, _wordDelimiters);
            _activeBuffer().GetSize().IncrementInBounds(pos);
        }
        else if (wordEndPos == pos)
        {
            // already at the end of the current word,
            // move to the end of the next word
            _activeBuffer().GetSize().IncrementInBounds(pos);
            pos = _activeBuffer().GetWordEnd(pos, _wordDelimiters);
        }
        else
        {
            // move to the end of the current word
            pos = wordEndPos;
        }
        break;
    }
    case SelectionDirection::Up:
        _MoveByChar(direction, pos);
        pos = _activeBuffer().GetWordStart(pos, _wordDelimiters);
        break;
    case SelectionDirection::Down:
        _MoveByChar(direction, pos);
        pos = _activeBuffer().GetWordEnd(pos, _wordDelimiters);
        break;
    }
}

void Terminal::_MoveByViewport(SelectionDirection direction, til::point& pos)
{
    const auto bufferSize{ _activeBuffer().GetSize() };
    switch (direction)
    {
    case SelectionDirection::Left:
        pos = { bufferSize.Left(), pos.Y };
        break;
    case SelectionDirection::Right:
        pos = { bufferSize.RightInclusive(), pos.Y };
        break;
    case SelectionDirection::Up:
    {
        const auto viewportHeight{ _GetMutableViewport().Height() };
        const auto newY{ pos.Y - viewportHeight };
        pos = newY < bufferSize.Top() ? bufferSize.Origin() : til::point{ pos.X, newY };
        break;
    }
    case SelectionDirection::Down:
    {
        const auto viewportHeight{ _GetMutableViewport().Height() };
        const auto mutableBottom{ _GetMutableViewport().BottomInclusive() };
        const auto newY{ pos.Y + viewportHeight };
        pos = newY > mutableBottom ? til::point{ bufferSize.RightInclusive(), mutableBottom } : til::point{ pos.X, newY };
        break;
    }
    }
}

void Terminal::_MoveByBuffer(SelectionDirection direction, til::point& pos)
{
    const auto bufferSize{ _activeBuffer().GetSize() };
    switch (direction)
    {
    case SelectionDirection::Left:
    case SelectionDirection::Up:
        pos = bufferSize.Origin();
        break;
    case SelectionDirection::Right:
    case SelectionDirection::Down:
        pos = { bufferSize.RightInclusive(), _GetMutableViewport().BottomInclusive() };
        break;
    }
}

// Method Description:
// - clear selection data and disable rendering it
#pragma warning(disable : 26440) // changing this to noexcept would require a change to ConHost's selection model
void Terminal::ClearSelection()
{
    _selection = std::nullopt;
    _markMode = false;
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

    const auto GetAttributeColors = [&](const auto& attr) {
        return _renderSettings.GetAttributeColors(attr);
    };

    // GH#6740: Block selection should preserve the visual structure:
    // - CRLFs need to be added - so the lines structure is preserved
    // - We should apply formatting above to wrapped rows as well (newline should be added).
    // GH#9706: Trimming of trailing white-spaces in block selection is configurable.
    const auto includeCRLF = !singleLine || _blockSelection;
    const auto trimTrailingWhitespace = !singleLine && (!_blockSelection || _trimBlockSelection);
    const auto formatWrappedRows = _blockSelection;
    return _activeBuffer().GetText(includeCRLF, trimTrailingWhitespace, selectionRects, GetAttributeColors, formatWrappedRows);
}

// Method Description:
// - convert viewport position to the corresponding location on the buffer
// Arguments:
// - viewportPos: a coordinate on the viewport
// Return Value:
// - the corresponding location on the buffer
til::point Terminal::_ConvertToBufferCell(const til::point viewportPos) const
{
    const auto yPos = _VisibleStartIndex() + viewportPos.Y;
    til::point bufferPos = { viewportPos.X, yPos };
    _activeBuffer().GetSize().Clamp(bufferPos);
    return bufferPos;
}

// Method Description:
// - This method won't be used. We just throw and do nothing. For now we
//   need this method to implement UiaData interface
// Arguments:
// - coordSelectionStart - Not used
// - coordSelectionEnd - Not used
// - attr - Not used.
void Terminal::ColorSelection(const til::point, const til::point, const TextAttribute)
{
    THROW_HR(E_NOTIMPL);
}
