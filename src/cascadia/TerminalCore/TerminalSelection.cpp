// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Terminal.hpp"

using namespace Microsoft::Terminal::Core;

// Method Description:
// - Helper to determine the selected region of the buffer. Used for rendering.
// Return Value:
// - A vector of rectangles representing the regions to select, line by line. They are absolute coordinates relative to the buffer origin.
std::vector<SMALL_RECT> Terminal::_GetSelectionRects() const
{
    std::vector<SMALL_RECT> selectionArea;

    if (!_selectionActive)
    {
        return selectionArea;
    }

    // create these new anchors for comparison and rendering
    COORD selectionAnchorWithOffset{ _selectionAnchor };
    COORD endSelectionPositionWithOffset{ _endSelectionPosition };

    // Add anchor offset here to update properly on new buffer output
    THROW_IF_FAILED(ShortAdd(selectionAnchorWithOffset.Y, _selectionAnchor_YOffset, &selectionAnchorWithOffset.Y));
    THROW_IF_FAILED(ShortAdd(endSelectionPositionWithOffset.Y, _endSelectionPosition_YOffset, &endSelectionPositionWithOffset.Y));

    // clamp Y values to be within mutable viewport bounds
    selectionAnchorWithOffset.Y = std::clamp(selectionAnchorWithOffset.Y, static_cast<SHORT>(0), _mutableViewport.BottomInclusive());
    endSelectionPositionWithOffset.Y = std::clamp(endSelectionPositionWithOffset.Y, static_cast<SHORT>(0), _mutableViewport.BottomInclusive());

    // clamp X values to be within buffer bounds
    const auto bufferSize = _buffer->GetSize();
    selectionAnchorWithOffset.X = std::clamp(_selectionAnchor.X, bufferSize.Left(), bufferSize.RightInclusive());
    endSelectionPositionWithOffset.X = std::clamp(_endSelectionPosition.X, bufferSize.Left(), bufferSize.RightInclusive());

    // NOTE: (0,0) is top-left so vertical comparison is inverted
    const COORD& higherCoord = (selectionAnchorWithOffset.Y <= endSelectionPositionWithOffset.Y) ?
                                   selectionAnchorWithOffset :
                                   endSelectionPositionWithOffset;
    const COORD& lowerCoord = (selectionAnchorWithOffset.Y > endSelectionPositionWithOffset.Y) ?
                                  selectionAnchorWithOffset :
                                  endSelectionPositionWithOffset;

    selectionArea.reserve(lowerCoord.Y - higherCoord.Y + 1);
    for (auto row = higherCoord.Y; row <= lowerCoord.Y; row++)
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
            selectionRow.Left = (row == higherCoord.Y) ? higherCoord.X : 0;
            selectionRow.Right = (row == lowerCoord.Y) ? lowerCoord.X : bufferSize.RightInclusive();
        }

        selectionRow.Left = _ExpandWideGlyphSelectionLeft(selectionRow.Left, row);
        selectionRow.Right = _ExpandWideGlyphSelectionRight(selectionRow.Right, row);

        selectionArea.emplace_back(selectionRow);
    }
    return selectionArea;
}

// Method Description:
// - Expands the selection left-wards to cover a wide glyph, if necessary
// Arguments:
// - position: the (x,y) coordinate on the visible viewport
// Return Value:
// - updated x position to encapsulate the wide glyph
const SHORT Terminal::_ExpandWideGlyphSelectionLeft(const SHORT xPos, const SHORT yPos) const
{
    // don't change the value if at/outside the boundary
    if (xPos <= 0 || xPos > _buffer->GetSize().RightInclusive())
    {
        return xPos;
    }

    COORD position{ xPos, yPos };
    const auto attr = _buffer->GetCellDataAt(position)->DbcsAttr();
    if (attr.IsTrailing())
    {
        // move off by highlighting the lead half too.
        // alters position.X
        _mutableViewport.DecrementInBounds(position);
    }
    return position.X;
}

// Method Description:
// - Expands the selection right-wards to cover a wide glyph, if necessary
// Arguments:
// - position: the (x,y) coordinate on the visible viewport
// Return Value:
// - updated x position to encapsulate the wide glyph
const SHORT Terminal::_ExpandWideGlyphSelectionRight(const SHORT xPos, const SHORT yPos) const
{
    // don't change the value if at/outside the boundary
    if (xPos < 0 || xPos >= _buffer->GetSize().RightInclusive())
    {
        return xPos;
    }

    COORD position{ xPos, yPos };
    const auto attr = _buffer->GetCellDataAt(position)->DbcsAttr();
    if (attr.IsLeading())
    {
        // move off by highlighting the trailing half too.
        // alters position.X
        _mutableViewport.IncrementInBounds(position);
    }
    return position.X;
}

// Method Description:
// - Checks if selection is active
// Return Value:
// - bool representing if selection is active. Used to decide copy/paste on right click
const bool Terminal::IsSelectionActive() const noexcept
{
    return _selectionActive;
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
    _selectionAnchor_YOffset = gsl::narrow<SHORT>(_ViewStartIndex());

    _selectionActive = true;
    SetEndSelectionPosition(position);
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
    _endSelectionPosition_YOffset = gsl::narrow<SHORT>(_ViewStartIndex());
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
void Terminal::ClearSelection() noexcept
{
    _selectionActive = false;
    _selectionAnchor = { 0, 0 };
    _endSelectionPosition = { 0, 0 };
    _selectionAnchor_YOffset = 0;
    _endSelectionPosition_YOffset = 0;

    _buffer->GetRenderTarget().TriggerSelection();
}

// Method Description:
// - get wstring text from highlighted portion of text buffer
// Arguments:
// - trimTrailingWhitespace: enable removing any whitespace from copied selection
//    and get text to appear on separate lines.
// Return Value:
// - wstring text from buffer. If extended to multiple lines, each line is separated by \r\n
const std::wstring Terminal::RetrieveSelectedTextFromBuffer(bool trimTrailingWhitespace) const
{
    std::function<COLORREF(TextAttribute&)> GetForegroundColor = std::bind(&Terminal::GetForegroundColor, this, std::placeholders::_1);
    std::function<COLORREF(TextAttribute&)> GetBackgroundColor = std::bind(&Terminal::GetBackgroundColor, this, std::placeholders::_1);

    auto data = _buffer->GetTextForClipboard(!_boxSelection,
                                             trimTrailingWhitespace,
                                             _GetSelectionRects(),
                                             GetForegroundColor,
                                             GetBackgroundColor);

    std::wstring result;
    for (const auto& text : data.text)
    {
        result += text;
    }

    return result;
}
