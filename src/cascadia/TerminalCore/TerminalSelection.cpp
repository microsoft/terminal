// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Terminal.hpp"
#include "unicode.hpp"

using namespace Microsoft::Terminal::Core;
using namespace Microsoft::Console::Types;

DEFINE_ENUM_FLAG_OPERATORS(Terminal::SelectionEndpoint);

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
// - Identical to GetTextRects if it's a block selection, else returns a single span for the whole selection.
// Return Value:
// - A vector of one or more spans representing the selection. They are absolute coordinates relative to the buffer origin.
std::vector<til::point_span> Terminal::_GetSelectionSpans() const noexcept
{
    std::vector<til::point_span> result;

    if (!IsSelectionActive())
    {
        return result;
    }

    try
    {
        return _activeBuffer().GetTextSpans(_selection->start, _selection->end, _selection->blockSelection, false);
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
til::point Terminal::GetSelectionAnchor() const noexcept
{
    _assertLocked();
    return _selection->start;
}

// Method Description:
// - Get the current end anchor position relative to the whole text buffer
// Arguments:
// - None
// Return Value:
// - None
til::point Terminal::GetSelectionEnd() const noexcept
{
    _assertLocked();
    return _selection->end;
}

// Method Description:
// - Gets the viewport-relative position of where to draw the marker
//    for the selection's start endpoint
til::point Terminal::SelectionStartForRendering() const
{
    auto pos{ _selection->start };
    const auto& buffer = GetTextBuffer();
    const auto bufferSize{ buffer.GetSize() };
    if (bufferSize.IsInBounds(pos) && buffer.GetCellDataAt(pos)->DbcsAttr() == DbcsAttribute::Trailing)
    {
        // if we're on a trailing byte, move off of it to include it
        bufferSize.DecrementInExclusiveBounds(pos);
    }

    if (pos.x != bufferSize.Left())
    {
        // In general, we need to draw the marker one before the
        // beginning of the selection.
        // When we're at the left boundary, we want to
        // flip the marker, so we skip this step.
        bufferSize.DecrementInExclusiveBounds(pos);
    }
    pos.y = base::ClampSub(pos.y, _VisibleStartIndex());
    return til::point{ pos };
}

// Method Description:
// - Gets the viewport-relative position of where to draw the marker
//    for the selection's end endpoint
til::point Terminal::SelectionEndForRendering() const
{
    auto pos{ _selection->end };
    const auto& buffer = GetTextBuffer();
    const auto bufferSize{ buffer.GetSize() };
    if (bufferSize.IsInBounds(pos) && buffer.GetCellDataAt(pos)->DbcsAttr() == DbcsAttribute::Trailing)
    {
        // if we're on a trailing byte, move off of it to include it
        bufferSize.IncrementInExclusiveBounds(pos);
    }

    if (pos.x == bufferSize.RightExclusive())
    {
        // sln->end is exclusive
        // In general, we need to draw the marker on the same cell.
        // When we're at the right boundary, we want to
        // flip the marker, so we move one cell to the left.
        bufferSize.DecrementInExclusiveBounds(pos);
    }
    pos.y = base::ClampSub(pos.y, _VisibleStartIndex());
    return til::point{ pos };
}

Terminal::SelectionEndpoint Terminal::SelectionEndpointTarget() const noexcept
{
    return _selectionEndpoint;
}

// Method Description:
// - Checks if selection is active
// Return Value:
// - bool representing if selection is active. Used to decide copy/paste on right click
bool Terminal::IsSelectionActive() const noexcept
{
    _assertLocked();
    return _selection->active;
}

bool Terminal::IsBlockSelection() const noexcept
{
    _assertLocked();
    return _selection->blockSelection;
}

// Method Description:
// - Perform a multi-click selection at viewportPos expanding according to the expansionMode
// Arguments:
// - viewportPos: the (x,y) coordinate on the visible viewport
// - expansionMode: the SelectionExpansion to dictate the boundaries of the selection anchors
void Terminal::MultiClickSelection(const til::point viewportPos, SelectionExpansion expansionMode)
{
    // set the selection pivot to expand the selection using SetSelectionEnd()
    auto selection{ _selection.write() };
    wil::hide_name _selection;

    selection->pivot = _ConvertToBufferCell(viewportPos, true);
    selection->active = true;

    _multiClickSelectionMode = expansionMode;
    SetSelectionEnd(viewportPos);

    // we need to set the _selectionPivot again
    // for future shift+clicks
    selection->pivot = selection->start;
}

// Method Description:
// - Record the position of the beginning of a selection
// Arguments:
// - position: the (x,y) coordinate on the visible viewport
void Terminal::SetSelectionAnchor(const til::point viewportPos)
{
    _assertLocked();

    auto selection{ _selection.write() };
    wil::hide_name _selection;

    selection->pivot = _ConvertToBufferCell(viewportPos, true);
    selection->active = true;

    _multiClickSelectionMode = SelectionExpansion::Char;
    SetSelectionEnd(viewportPos);

    selection->start = selection->pivot;
}

void Terminal::SetSelectionEnd(const til::point viewportPos, std::optional<SelectionExpansion> newExpansionMode)
{
    _SetSelectionEnd(_selection.write(), viewportPos, newExpansionMode);
}

// Method Description:
// - Update selection anchors when dragging to a position
// - based on the selection expansion mode
// Arguments:
// - viewportPos: the (x,y) coordinate on the visible viewport
// - newExpansionMode: overwrites the _multiClickSelectionMode for this function call. Used for Shift+Click
void Terminal::_SetSelectionEnd(SelectionInfo* selection, const til::point viewportPos, std::optional<SelectionExpansion> newExpansionMode)
{
    wil::hide_name _selection;
    if (!selection->active)
    {
        // capture a log for spurious endpoint sets without an active selection
        LOG_HR(E_ILLEGAL_STATE_CHANGE);
        return;
    }

    auto textBufferPos = _ConvertToBufferCell(viewportPos, true);
    if (newExpansionMode && *newExpansionMode == SelectionExpansion::Char && textBufferPos >= selection->pivot)
    {
        // Shift+Click forwards should highlight the clicked space
        _activeBuffer().GetSize().IncrementInExclusiveBounds(textBufferPos);
    }

    // if this is a shiftClick action, we need to overwrite the _multiClickSelectionMode value (even if it's the same)
    // Otherwise, we may accidentally expand during other selection-based actions
    _multiClickSelectionMode = newExpansionMode.has_value() ? *newExpansionMode : _multiClickSelectionMode;

    auto targetStart = false;
    const auto anchors = _PivotSelection(textBufferPos, targetStart);
    const auto expandedAnchors = _ExpandSelectionAnchors(anchors);

    if (newExpansionMode.has_value())
    {
        // shift-click operations only expand the target side
        auto& anchorToExpand = targetStart ? selection->start : selection->end;
        anchorToExpand = targetStart ? expandedAnchors.first : expandedAnchors.second;

        // the other anchor should then become the pivot (we don't expand it)
        auto& anchorToPivot = targetStart ? selection->end : selection->start;
        anchorToPivot = selection->pivot;
    }
    else
    {
        // expand both anchors
        std::tie(selection->start, selection->end) = expandedAnchors;
    }
    _selectionMode = SelectionInteractionMode::Mouse;
    _selectionIsTargetingUrl = false;
}

// Method Description:
// - returns a new pair of selection anchors for selecting around the pivot
// - This ensures start < end when compared
// Arguments:
// - targetPos: the (x,y) coordinate we are moving to on the text buffer
// - targetStart: if true, target will be the new start. Otherwise, target will be the new end.
// Return Value:
// - the new start/end for a selection
std::pair<til::point, til::point> Terminal::_PivotSelection(const til::point targetPos, bool& targetStart) const noexcept
{
    if (targetStart = targetPos <= _selection->pivot)
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

    const auto& buffer = _activeBuffer();
    const auto bufferSize = buffer.GetSize();
    const auto height = buffer.GetSize().Height();

    switch (_multiClickSelectionMode)
    {
    case SelectionExpansion::Line:
        // climb up to the first row that is wrapped
        while (start.y > 0 && buffer.GetRowByOffset(start.y - 1).WasWrapForced())
        {
            --start.y;
        }
        // climb down to the last row that is wrapped
        while (end.y + 1 < height && buffer.GetRowByOffset(end.y).WasWrapForced())
        {
            ++end.y;
        }
        start = { bufferSize.Left(), start.y };
        end = { bufferSize.RightExclusive(), end.y };
        break;
    case SelectionExpansion::Word:
    {
        start = buffer.GetWordStart2(start, _wordDelimiters, false);

        // GH#5099: We round to the nearest cell boundary,
        //   so we would normally prematurely expand to the next word
        //   as we approach it during a 2x-click+drag.
        //   To remedy this, decrement the end's position by 1.
        //   However, only do this when expanding right (it's correct
        //   as is when expanding left).
        if (end > _selection->pivot)
        {
            bufferSize.DecrementInExclusiveBounds(end);
        }
        end = buffer.GetWordEnd2(end, _wordDelimiters, false);
        break;
    }
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
    _selection.write()->blockSelection = isEnabled;
}

Terminal::SelectionInteractionMode Terminal::SelectionMode() const noexcept
{
    return _selectionMode;
}

void Terminal::ToggleMarkMode()
{
    if (_selectionMode == SelectionInteractionMode::Mark)
    {
        // Exit Mark Mode
        ClearSelection();
    }
    else
    {
        // Enter Mark Mode
        // NOTE: directly set cursor state. We already should have locked before calling this function.
        if (!IsSelectionActive())
        {
            // If we're scrolled up, use the viewport origin as the selection start.
            // Otherwise, use the cursor position.
            const auto cursorPos = _scrollOffset != 0 ? _GetVisibleViewport().Origin() :
                                                        _activeBuffer().GetCursor().GetPosition();
            *_selection.write() = SelectionInfo{
                .start = cursorPos,
                .end = cursorPos,
                .pivot = cursorPos,
                .blockSelection = false,
                .active = true,
            };
            WI_SetAllFlags(_selectionEndpoint, SelectionEndpoint::Start | SelectionEndpoint::End);
        }
        else if (WI_AreAllFlagsClear(_selectionEndpoint, SelectionEndpoint::Start | SelectionEndpoint::End))
        {
            // Selection already existed
            WI_SetFlag(_selectionEndpoint, SelectionEndpoint::End);
        }
        _ScrollToPoint(_selection->start);
        _selectionMode = SelectionInteractionMode::Mark;
        _selectionIsTargetingUrl = false;
    }
}

// Method Description:
// - switch the targeted selection endpoint with the other one (i.e. start <--> end)
void Terminal::SwitchSelectionEndpoint() noexcept
{
    if (IsSelectionActive())
    {
        if (WI_IsFlagSet(_selectionEndpoint, SelectionEndpoint::Start) && WI_IsFlagSet(_selectionEndpoint, SelectionEndpoint::End))
        {
            // moving cursor --> anchor start, move end
            _selectionEndpoint = SelectionEndpoint::End;
            _anchorInactiveSelectionEndpoint = true;
        }
        else if (WI_IsFlagSet(_selectionEndpoint, SelectionEndpoint::End))
        {
            // moving end --> now we're moving start
            _selectionEndpoint = SelectionEndpoint::Start;
            _selection.write()->pivot = _selection->end;
        }
        else if (WI_IsFlagSet(_selectionEndpoint, SelectionEndpoint::Start))
        {
            // moving start --> now we're moving end
            _selectionEndpoint = SelectionEndpoint::End;
            _selection.write()->pivot = _selection->start;
        }
    }
}

void Terminal::ExpandSelectionToWord()
{
    if (IsSelectionActive())
    {
        const auto& buffer = _activeBuffer();
        auto selection{ _selection.write() };
        wil::hide_name _selection;
        selection->start = buffer.GetWordStart2(selection->start, _wordDelimiters, false);
        selection->pivot = selection->start;
        selection->end = buffer.GetWordEnd2(selection->end, _wordDelimiters, false);

        // if we're targeting both endpoints, instead just target "end"
        if (WI_IsFlagSet(_selectionEndpoint, SelectionEndpoint::Start) && WI_IsFlagSet(_selectionEndpoint, SelectionEndpoint::End))
        {
            _selectionEndpoint = SelectionEndpoint::End;
        }
    }
}

// Method Description:
// - selects the next/previous hyperlink, if one is available
// Arguments:
// - dir: the direction we're scanning the buffer in to find the hyperlink of interest
// Return Value:
// - true if we found a hyperlink to select (and selected it); otherwise, false.
void Terminal::SelectHyperlink(const SearchDirection dir)
{
    if (_selectionMode != SelectionInteractionMode::Mark)
    {
        // This feature only works in mark mode
        _selectionIsTargetingUrl = false;
        return;
    }

    // 0. Useful tools/vars
    const auto& buffer = _activeBuffer();
    const auto bufferSize = buffer.GetSize();
    const auto viewportHeight = _GetMutableViewport().Height();

    // The patterns are stored relative to the "search area". Initially, this search area will be the viewport,
    // but as we progressively search through more of the buffer, this will change.
    // Keep track of the search area here, and use the lambda below to convert points to the search area coordinate space.
    auto searchArea = _GetVisibleViewport();
    auto convertToSearchArea = [&searchArea](const til::point pt) noexcept {
        auto copy = pt;
        searchArea.ConvertToOrigin(&copy);
        return copy;
    };

    // extracts the next/previous hyperlink from the list of hyperlink ranges provided
    auto extractResultFromList = [&](std::vector<interval_tree::Interval<til::point, size_t>>& list) noexcept {
        const auto selectionStartInSearchArea = convertToSearchArea(_selection->start);

        std::optional<std::pair<til::point, til::point>> resultFromList;
        if (!list.empty())
        {
            if (dir == SearchDirection::Forward)
            {
                // pattern tree includes the currently selected range when going forward,
                // so we need to check if we're pointing to that one before returning it.
                auto range = list.front();
                if (_selectionIsTargetingUrl && range.start == selectionStartInSearchArea)
                {
                    if (list.size() > 1)
                    {
                        // if we're pointing to the currently selected URL,
                        // pick the next one.
                        range = til::at(list, 1);
                        resultFromList = { range.start, range.stop };
                    }
                    else
                    {
                        // LOAD-BEARING: the only range here is the one that's currently selected.
                        // Make sure this is set to nullopt so that we keep searching through the buffer.
                        resultFromList = std::nullopt;
                    }
                }
                else
                {
                    // not on currently selected range, return the first one
                    resultFromList = { range.start, range.stop };
                }
            }
            else if (dir == SearchDirection::Backward)
            {
                // moving backwards excludes the currently selected range,
                // simply return the last one in the list as it's ordered
                const auto& range = list.back();
                resultFromList = { range.start, range.stop };
            }
        }

        // pattern tree stores everything as viewport coords,
        // so we need to convert them on the way out
        if (resultFromList)
        {
            searchArea.ConvertFromOrigin(&resultFromList->first);
            searchArea.ConvertFromOrigin(&resultFromList->second);
        }
        return resultFromList;
    };

    // 1. Look for the hyperlink
    til::point searchStart;
    til::point searchEnd;
    if (dir == SearchDirection::Forward)
    {
        searchStart = _selection->start;
        searchEnd = til::point{ bufferSize.RightInclusive(), _VisibleEndIndex() };
    }
    else
    {
        searchStart = til::point{ bufferSize.Left(), _VisibleStartIndex() };
        searchEnd = _selection->start;
    }

    // 1.A) Try searching the current viewport (no scrolling required)
    auto resultList = _patternIntervalTree.findContained(convertToSearchArea(searchStart), convertToSearchArea(searchEnd));
    std::optional<std::pair<til::point, til::point>> result = extractResultFromList(resultList);
    if (!result)
    {
        // 1.B) Incrementally search through more of the space
        if (dir == SearchDirection::Forward)
        {
            searchStart = { bufferSize.Left(), searchEnd.y + 1 };
            searchEnd = { bufferSize.RightInclusive(), std::min(searchStart.y + viewportHeight, ViewEndIndex()) };
        }
        else
        {
            searchEnd = { bufferSize.RightInclusive(), searchStart.y - 1 };
            searchStart = { bufferSize.Left(), std::max(searchStart.y - viewportHeight, bufferSize.Top()) };
        }
        searchArea = Viewport::FromDimensions(searchStart, { searchEnd.x + 1, searchEnd.y + 1 });

        const til::point bufferStart{ bufferSize.Origin() };
        const til::point bufferEnd{ bufferSize.RightInclusive(), ViewEndIndex() };
        while (!result && bufferSize.IsInBounds(searchStart) && bufferSize.IsInBounds(searchEnd) && searchStart <= searchEnd && bufferStart <= searchStart && searchEnd <= bufferEnd)
        {
            auto patterns = _getPatterns(searchStart.y, searchEnd.y);
            resultList = patterns.findContained(convertToSearchArea(searchStart), convertToSearchArea(searchEnd));
            result = extractResultFromList(resultList);
            if (!result)
            {
                if (dir == SearchDirection::Forward)
                {
                    searchStart.y += 1;
                    searchEnd.y = std::min(searchStart.y + viewportHeight, ViewEndIndex());
                }
                else
                {
                    searchEnd.y -= 1;
                    searchStart.y = std::max(searchEnd.y - viewportHeight, bufferSize.Top());
                }
                searchArea = Viewport::FromDimensions(searchStart, { searchEnd.x + 1, searchEnd.y + 1 });
            }
        }
    }

    // 2. We found a hyperlink from the pattern tree. Look for embedded hyperlinks too!
    // Use the result (if one was found) to narrow down the search.
    if (dir == SearchDirection::Forward)
    {
        searchStart = _selection->start;
        searchEnd = (result ? result->first : buffer.GetLastNonSpaceCharacter());
    }
    else
    {
        searchStart = (result ? result->second : bufferSize.Origin());
        searchEnd = _selection->start;
    }

    // Careful! Selection can point to RightExclusive(), which doesn't contain data!
    // Clamp to be safe.
    auto initialPos = dir == SearchDirection::Forward ? searchStart : searchEnd;
    bufferSize.Clamp(initialPos);
    auto iter = buffer.GetCellDataAt(initialPos);
    while (dir == SearchDirection::Forward ? iter.Pos() < searchEnd : iter.Pos() > searchStart)
    {
        // Don't let us select the same hyperlink again
        if (iter.Pos() < _selection->start || iter.Pos() > _selection->end)
        {
            if (auto attr = iter->TextAttr(); attr.IsHyperlink())
            {
                // Found an embedded hyperlink!
                const auto hyperlinkId = attr.GetHyperlinkId();

                // Expand the start to include the entire hyperlink
                TextBufferCellIterator hyperlinkStartIter{ buffer, iter.Pos() };
                while (hyperlinkStartIter.Pos() > searchStart && attr.IsHyperlink() && attr.GetHyperlinkId() == hyperlinkId)
                {
                    --hyperlinkStartIter;
                    attr = hyperlinkStartIter->TextAttr();
                }
                if (hyperlinkStartIter.Pos() != bufferSize.Origin())
                {
                    // undo a move to be inclusive
                    ++hyperlinkStartIter;
                }

                // Expand the end to include the entire hyperlink
                // No need to undo a move! We'll decrement in the next step anyways.
                TextBufferCellIterator hyperlinkEndIter{ buffer, iter.Pos() };
                attr = hyperlinkEndIter->TextAttr();
                while (hyperlinkEndIter.Pos() < searchEnd && attr.IsHyperlink() && attr.GetHyperlinkId() == hyperlinkId)
                {
                    ++hyperlinkEndIter;
                    attr = hyperlinkEndIter->TextAttr();
                }

                result = { hyperlinkStartIter.Pos(), hyperlinkEndIter.Pos() };
                break;
            }
        }
        iter += dir == SearchDirection::Forward ? 1 : -1;
    }

    // 3. Select the hyperlink, if one exists
    if (!result.has_value())
    {
        return;
    }
    auto selection{ _selection.write() };
    wil::hide_name _selection;
    selection->start = result->first;
    selection->pivot = result->first;
    selection->end = result->second;
    _selectionIsTargetingUrl = true;
    _selectionEndpoint = SelectionEndpoint::End;

    // 4. Scroll to the selected area (if necessary)
    _ScrollToPoint(selection->end);
}

Terminal::UpdateSelectionParams Terminal::ConvertKeyEventToUpdateSelectionParams(const ControlKeyStates mods, const WORD vkey) const noexcept
{
    if ((_selectionMode == SelectionInteractionMode::Mark || mods.IsShiftPressed()) && !mods.IsAltPressed())
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
            default:
                break;
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
            default:
                break;
            }
        }
    }
    return std::nullopt;
}

// Method Description:
// - updates the selection endpoints based on a direction and expansion mode. Primarily used for keyboard selection.
// Arguments:
// - direction: the direction to move the selection endpoint in
// - mode: the type of movement to be performed (i.e. move by word)
// - mods: the key modifiers pressed when performing this update
void Terminal::UpdateSelection(SelectionDirection direction, SelectionExpansion mode, ControlKeyStates mods)
{
    // This is a special variable used to track if we should move the cursor when in mark mode.
    //   We have special functionality where if you use the "switchSelectionEndpoint" action
    //   when in mark mode (moving the cursor), we anchor an endpoint and you can use the
    //   plain arrow keys to move the endpoint. This way, you don't have to hold shift anymore!
    const bool shouldMoveBothEndpoints = _selectionMode == SelectionInteractionMode::Mark && !_anchorInactiveSelectionEndpoint && !mods.IsShiftPressed();

    // 1. Figure out which endpoint to update
    // [Mark Mode]
    // - shift pressed --> only move "end" (or "start" if "pivot" == "end")
    // - otherwise --> move both "start" and "end" (moving cursor)
    // [Quick Edit]
    // - just move "end" (or "start" if "pivot" == "end")
    _selectionEndpoint = static_cast<SelectionEndpoint>(0);
    if (shouldMoveBothEndpoints)
    {
        WI_SetAllFlags(_selectionEndpoint, SelectionEndpoint::Start | SelectionEndpoint::End);
    }
    else if (_selection->start == _selection->pivot)
    {
        WI_SetFlag(_selectionEndpoint, SelectionEndpoint::End);
    }
    else if (_selection->end == _selection->pivot)
    {
        WI_SetFlag(_selectionEndpoint, SelectionEndpoint::Start);
    }
    auto targetPos{ WI_IsFlagSet(_selectionEndpoint, SelectionEndpoint::Start) ? _selection->start : _selection->end };

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

    // 3. Actually modify the selection state
    _selectionIsTargetingUrl = false;
    _selectionMode = std::max(_selectionMode, SelectionInteractionMode::Keyboard);

    auto selection{ _selection.write() };
    wil::hide_name _selection;

    if (shouldMoveBothEndpoints)
    {
        // [Mark Mode] + shift unpressed --> move all three (i.e. just use arrow keys)
        selection->start = targetPos;
        selection->end = targetPos;
        selection->pivot = targetPos;
    }
    else
    {
        // [Mark Mode] + shift --> updating a standard selection
        // This is also standard quick-edit modification
        bool targetStart = false;
        std::tie(selection->start, selection->end) = _PivotSelection(targetPos, targetStart);

        // IMPORTANT! Pivoting the selection here might have changed which endpoint we're targeting.
        // So let's set the targeted endpoint again.
        WI_UpdateFlag(_selectionEndpoint, SelectionEndpoint::Start, targetStart);
        WI_UpdateFlag(_selectionEndpoint, SelectionEndpoint::End, !targetStart);
    }

    // 4. Scroll (if necessary)
    _ScrollToPoint(targetPos);
}

void Terminal::SelectAll()
{
    const auto bufferSize{ _activeBuffer().GetSize() };
    const til::point end{ bufferSize.RightInclusive(), _GetMutableViewport().BottomInclusive() };
    *_selection.write() = SelectionInfo{
        .start = bufferSize.Origin(),
        .end = end,
        .pivot = end,
        .blockSelection = false,
        .active = true,
    };

    _selectionMode = SelectionInteractionMode::Keyboard;
    _selectionIsTargetingUrl = false;
    _ScrollToPoint(_selection->start);
}

void Terminal::_MoveByChar(SelectionDirection direction, til::point& pos)
{
    switch (direction)
    {
    case SelectionDirection::Left:
        _activeBuffer().MoveToPreviousGlyph2(pos);
        break;
    case SelectionDirection::Right:
        // We need the limit to be the mutable viewport here,
        // otherwise we're allowed to navigate by character past the mutable bottom
        _activeBuffer().MoveToNextGlyph2(pos, _GetMutableViewport().BottomInclusiveRightExclusive());
        break;
    case SelectionDirection::Up:
    {
        const auto bufferSize{ _activeBuffer().GetSize() };
        const auto newY{ pos.y - 1 };
        pos = newY < bufferSize.Top() ? bufferSize.Origin() : til::point{ pos.x, newY };
        break;
    }
    case SelectionDirection::Down:
    {
        const auto bufferSize{ _activeBuffer().GetSize() };
        const auto mutableBottom{ _GetMutableViewport().BottomInclusive() };
        const auto newY{ pos.y + 1 };
        pos = newY > mutableBottom ? til::point{ bufferSize.RightExclusive(), mutableBottom } : til::point{ pos.x, newY };
        break;
    }
    }
}

void Terminal::_MoveByWord(SelectionDirection direction, til::point& pos)
{
    const auto& buffer = _activeBuffer();
    switch (direction)
    {
    case SelectionDirection::Left:
    {
        auto nextPos = pos;
        nextPos = buffer.GetWordStart2(nextPos, _wordDelimiters, true);
        if (nextPos == pos)
        {
            // didn't move because we're already at the beginning of a word,
            // so move to the beginning of the previous word
            buffer.GetSize().DecrementInExclusiveBounds(nextPos);
            nextPos = buffer.GetWordStart2(nextPos, _wordDelimiters, true);
        }
        pos = nextPos;
        break;
    }
    case SelectionDirection::Right:
    {
        const auto mutableViewportEndExclusive = _GetMutableViewport().BottomInclusiveRightExclusive();
        auto nextPos = pos;
        nextPos = buffer.GetWordEnd2(nextPos, _wordDelimiters, true, mutableViewportEndExclusive);
        if (nextPos == pos)
        {
            // didn't move because we're already at the end of a word,
            // so move to the end of the next word
            buffer.GetSize().IncrementInExclusiveBounds(nextPos);
            nextPos = buffer.GetWordEnd2(nextPos, _wordDelimiters, true, mutableViewportEndExclusive);
        }
        pos = nextPos;
        break;
    }
    case SelectionDirection::Up:
        _MoveByChar(direction, pos);
        pos = buffer.GetWordStart2(pos, _wordDelimiters, true);
        break;
    case SelectionDirection::Down:
        _MoveByChar(direction, pos);
        pos = buffer.GetWordEnd2(pos, _wordDelimiters, true);
        break;
    }
}

void Terminal::_MoveByViewport(SelectionDirection direction, til::point& pos) noexcept
{
    const auto bufferSize{ _activeBuffer().GetSize() };
    switch (direction)
    {
    case SelectionDirection::Left:
        pos = { bufferSize.Left(), pos.y };
        break;
    case SelectionDirection::Right:
        pos = { bufferSize.RightExclusive(), pos.y };
        break;
    case SelectionDirection::Up:
    {
        const auto viewportHeight{ _GetMutableViewport().Height() };
        const auto newY{ pos.y - viewportHeight };
        pos = newY < bufferSize.Top() ? bufferSize.Origin() : til::point{ pos.x, newY };
        break;
    }
    case SelectionDirection::Down:
    {
        const auto viewportHeight{ _GetMutableViewport().Height() };
        const auto mutableBottom{ _GetMutableViewport().BottomInclusive() };
        const auto newY{ pos.y + viewportHeight };
        pos = newY > mutableBottom ? til::point{ bufferSize.RightExclusive(), mutableBottom } : til::point{ pos.x, newY };
        break;
    }
    }
}

void Terminal::_MoveByBuffer(SelectionDirection direction, til::point& pos) noexcept
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
        pos = { bufferSize.RightExclusive(), _GetMutableViewport().BottomInclusive() };
        break;
    }
}

// Method Description:
// - clear selection data and disable rendering it
#pragma warning(disable : 26440) // changing this to noexcept would require a change to ConHost's selection model
void Terminal::ClearSelection()
{
    _assertLocked();
    _selection.write()->active = false;
    _selectionMode = SelectionInteractionMode::None;
    _selectionIsTargetingUrl = false;
    _selectionEndpoint = static_cast<SelectionEndpoint>(0);
    _anchorInactiveSelectionEndpoint = false;
}

// Method Description:
// - Get text from highlighted portion of text buffer
// - Optionally, get the highlighted text in HTML and RTF formats
// Arguments:
// - singleLine: collapse all of the text to one line. (Turns off trailing whitespace trimming)
// - withControlSequences: if enabled, the copied plain text contains color/style ANSI escape codes from the selection
// - html: also get text in HTML format
// - rtf: also get text in RTF format
// Return Value:
// - Plain and formatted selected text from buffer. Empty string represents no data for that format.
// - If extended to multiple lines, each line is separated by \r\n
Terminal::TextCopyData Terminal::RetrieveSelectedTextFromBuffer(const bool singleLine, const bool withControlSequences, const bool html, const bool rtf) const
{
    TextCopyData data;

    if (!IsSelectionActive())
    {
        return data;
    }

    const auto GetAttributeColors = [&](const auto& attr) {
        const auto [fg, bg] = _renderSettings.GetAttributeColors(attr);
        const auto ul = _renderSettings.GetAttributeUnderlineColor(attr);
        return std::tuple{ fg, bg, ul };
    };

    const auto& textBuffer = _activeBuffer();

    const auto req = TextBuffer::CopyRequest::FromConfig(textBuffer, _selection->start, _selection->end, singleLine, _selection->blockSelection, _trimBlockSelection);
    if (withControlSequences)
    {
        data.plainText = textBuffer.GetWithControlSequences(req);
    }
    else
    {
        data.plainText = textBuffer.GetPlainText(req);
    }

    if (html || rtf)
    {
        const auto bgColor = _renderSettings.GetAttributeColors({}).second;
        const auto isIntenseBold = _renderSettings.GetRenderMode(::Microsoft::Console::Render::RenderSettings::Mode::IntenseIsBold);
        const auto fontSizePt = _fontInfo.GetUnscaledSize().height; // already in points
        const auto& fontName = _fontInfo.GetFaceName();

        if (html)
        {
            data.html = textBuffer.GenHTML(req, fontSizePt, fontName, bgColor, isIntenseBold, GetAttributeColors);
        }
        if (rtf)
        {
            data.rtf = textBuffer.GenRTF(req, fontSizePt, fontName, bgColor, isIntenseBold, GetAttributeColors);
        }
    }

    return data;
}

// Method Description:
// - convert viewport position to the corresponding location on the buffer
// Arguments:
// - viewportPos: a coordinate on the viewport
// - allowRightExclusive: if true, clamp to the right exclusive boundary of the buffer.
//     Careful! This position doesn't point to any data in the buffer!
// Return Value:
// - the corresponding location on the buffer
til::point Terminal::_ConvertToBufferCell(const til::point viewportPos, bool allowRightExclusive) const
{
    const auto yPos = _VisibleStartIndex() + viewportPos.y;
    til::point bufferPos = { viewportPos.x, yPos };
    const auto bufferSize = _activeBuffer().GetSize();
    bufferPos.x = std::clamp(bufferPos.x, bufferSize.Left(), allowRightExclusive ? bufferSize.RightExclusive() : bufferSize.RightInclusive());
    bufferPos.y = std::clamp(bufferPos.y, bufferSize.Top(), bufferSize.BottomInclusive());
    return bufferPos;
}

// Method Description:
// - if necessary, scroll the viewport such that the given point is visible
// Arguments:
// - pos: a coordinate relative to the buffer (not viewport)
void Terminal::_ScrollToPoint(const til::point pos)
{
    if (const auto visibleViewport = _GetVisibleViewport(); !visibleViewport.IsInExclusiveBounds(pos))
    {
        if (const auto amtAboveView = visibleViewport.Top() - pos.y; amtAboveView > 0)
        {
            // anchor is above visible viewport, scroll by that amount
            _scrollOffset += amtAboveView;
        }
        else
        {
            // anchor is below visible viewport, scroll by that amount
            const auto amtBelowView = pos.y - visibleViewport.BottomInclusive();
            _scrollOffset -= amtBelowView;
        }
        _NotifyScrollEvent();
        _activeBuffer().TriggerScroll();
    }
}
