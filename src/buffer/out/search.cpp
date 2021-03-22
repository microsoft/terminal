// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "search.h"

#include "textBuffer.hpp"
#include "../types/inc/Utf16Parser.hpp"
#include "../types/inc/GlyphWidth.hpp"

using namespace Microsoft::Console::Types;

// Routine Description:
// - Constructs a Search object.
// - Make a Search object then call .FindNext() to locate items.
// - Once you've found something, you can perform actions like .Select() or .Color()
// Arguments:
// - textBuffer - The screen text buffer to search through (the "haystack")
// - uiaData - The IUiaData type reference, it is for providing selection methods
// - str - The search term you want to find (the "needle")
// - direction - The direction to search (upward or downward)
// - sensitivity - Whether or not you care about case
Search::Search(IUiaData& uiaData,
               const std::wstring& str,
               const Direction direction,
               const Sensitivity sensitivity) :
    _direction(direction),
    _sensitivity(sensitivity),
    _needle(s_CreateNeedleFromString(str)),
    _uiaData(uiaData),
    _coordAnchor(s_GetInitialAnchor(uiaData, direction))
{
    _coordNext = _coordAnchor;
}

// Routine Description:
// - Constructs a Search object.
// - Make a Search object then call .FindNext() to locate items.
// - Once you've found something, you can perform actions like .Select() or .Color()
// Arguments:
// - textBuffer - The screen text buffer to search through (the "haystack")
// - uiaData - The IUiaData type reference, it is for providing selection methods
// - str - The search term you want to find (the "needle")
// - direction - The direction to search (upward or downward)
// - sensitivity - Whether or not you care about case
// - anchor - starting search location in screenInfo
Search::Search(IUiaData& uiaData,
               const std::wstring& str,
               const Direction direction,
               const Sensitivity sensitivity,
               const COORD anchor) :
    _direction(direction),
    _sensitivity(sensitivity),
    _needle(s_CreateNeedleFromString(str)),
    _coordAnchor(anchor),
    _uiaData(uiaData)
{
    _coordNext = _coordAnchor;
}

// Routine Description
// - Locates the next instance of the search term within the screen buffer.
// Arguments:
// - <none> - Uses internal state from constructor
// Return Value:
// - True if we found another item. False if we've reached the end of the buffer.
// - NOTE: You can FindNext() again after False to go around the buffer again.
bool Search::FindNext()
{
    if (_reachedEnd)
    {
        _reachedEnd = false;
        return false;
    }

    do
    {
        if (_FindNeedleInHaystackAt(_coordNext, _coordSelStart, _coordSelEnd))
        {
            _UpdateNextPosition();
            _reachedEnd = _coordNext == _coordAnchor;
            return true;
        }
        else
        {
            _UpdateNextPosition();
        }

    } while (_coordNext != _coordAnchor);

    return false;
}

// Routine Description:
// - Takes the found word and selects it in the screen buffer
void Search::Select() const
{
    // Convert buffer selection offsets into the equivalent screen coordinates
    // required by SelectNewRegion, taking line renditions into account.
    const auto& textBuffer = _uiaData.GetTextBuffer();
    const auto selStart = textBuffer.BufferToScreenPosition(_coordSelStart);
    const auto selEnd = textBuffer.BufferToScreenPosition(_coordSelEnd);
    _uiaData.SelectNewRegion(selStart, selEnd);
}

// Routine Description:
// - In console host, we take the found word and apply the given color to it in the screen buffer
// - In Windows Terminal, we just select the found word, but we do not modify the buffer
// Arguments:
// - ulAttr - The legacy color attribute to apply to the word
void Search::Color(const TextAttribute attr) const
{
    // Only select if we've found something.
    if (_coordSelStart != _coordSelEnd)
    {
        _uiaData.ColorSelection(_coordSelStart, _coordSelEnd, attr);
    }
}

// Routine Description:
// - gets start and end position of text sound by search. only guaranteed to have valid data if FindNext has
// been called and returned true.
// Return Value:
// - pair containing [start, end] coord positions of text found by search
std::pair<COORD, COORD> Search::GetFoundLocation() const noexcept
{
    return { _coordSelStart, _coordSelEnd };
}

// Routine Description:
// - Finds the anchor position where we will start searches from.
// - This position will represent the "wrap around" point in the buffer or where
//   we reach the end of our search.
// - If the screen buffer given already has a selection in it, it will be used to determine the anchor.
// - Otherwise, we will choose one of the ends of the screen buffer depending on direction.
// Arguments:
// - uiaData - The reference to the IUiaData interface type object
// - direction - The intended direction of the search
// Return Value:
// - Coordinate to start the search from.
COORD Search::s_GetInitialAnchor(IUiaData& uiaData, const Direction direction)
{
    const auto& textBuffer = uiaData.GetTextBuffer();
    const COORD textBufferEndPosition = uiaData.GetTextBufferEndPosition();
    if (uiaData.IsSelectionActive())
    {
        // Convert the screen position of the selection anchor into an equivalent
        // buffer position to start searching, taking line rendition into account.
        auto anchor = textBuffer.ScreenToBufferPosition(uiaData.GetSelectionAnchor());

        if (direction == Direction::Forward)
        {
            textBuffer.GetSize().IncrementInBoundsCircular(anchor);
        }
        else
        {
            textBuffer.GetSize().DecrementInBoundsCircular(anchor);
            // If the selection starts at (0, 0), we need to make sure
            // it does not exceed the text buffer end position
            anchor.X = std::min(textBufferEndPosition.X, anchor.X);
            anchor.Y = std::min(textBufferEndPosition.Y, anchor.Y);
        }
        return anchor;
    }
    else
    {
        if (direction == Direction::Forward)
        {
            return { 0, 0 };
        }
        else
        {
            return textBufferEndPosition;
        }
    }
}

// Routine Description:
// - Attempts to compare the search term (the needle) to the screen buffer (the haystack)
//   at the given coordinate position of the screen buffer.
// - Performs one comparison. Call again with new positions to check other spots.
// Arguments:
// - pos - The position in the haystack (screen buffer) to compare
// - start - If we found it, this is filled with the coordinate of the first character of the needle.
// - end - If we found it, this is filled with the coordinate of the last character of the needle.
// Return Value:
// - True if we found it. False if not.
bool Search::_FindNeedleInHaystackAt(const COORD pos, COORD& start, COORD& end) const
{
    start = { 0 };
    end = { 0 };

    COORD bufferPos = pos;

    for (const auto& needleCell : _needle)
    {
        // Haystack is the buffer. Needle is the string we were given.
        const auto hayIter = _uiaData.GetTextBuffer().GetTextDataAt(bufferPos);
        const auto hayChars = *hayIter;
        const auto needleChars = std::wstring_view(needleCell.data(), needleCell.size());

        // If we didn't match at any point of the needle, return false.
        if (!_CompareChars(hayChars, needleChars))
        {
            return false;
        }

        _IncrementCoord(bufferPos);
    }

    _DecrementCoord(bufferPos);

    // If we made it the whole way through the needle, then it was in the haystack.
    // Fill out the span that we found the result at and return true.
    start = pos;
    end = bufferPos;

    return true;
}

// Routine Description:
// - Provides an abstraction for comparing two spans of text.
// - Internally handles case sensitivity based on object construction.
// Arguments:
// - one - String view representing the first string of text
// - two - String view representing the second string of text
// Return Value:
// - True if they are the same. False otherwise.
bool Search::_CompareChars(const std::wstring_view one, const std::wstring_view two) const noexcept
{
    if (one.size() != two.size())
    {
        return false;
    }

    for (size_t i = 0; i < one.size(); i++)
    {
        if (_ApplySensitivity(one.at(i)) != _ApplySensitivity(two.at(i)))
        {
            return false;
        }
    }

    return true;
}

// Routine Description:
// - Provides an abstraction for conditionally applying case sensitivity
//   based on object construction
// Arguments:
// - wch - Character to adjust if necessary
// Return Value:
// - Adjusted value (or not).
wchar_t Search::_ApplySensitivity(const wchar_t wch) const noexcept
{
    if (_sensitivity == Sensitivity::CaseInsensitive)
    {
        return ::towlower(wch);
    }
    else
    {
        return wch;
    }
}

// Routine Description:
// - Helper to increment a coordinate in respect to the associated screen buffer
// Arguments
// - coord - Updated by function to increment one position (will wrap X and Y direction)
void Search::_IncrementCoord(COORD& coord) const noexcept
{
    _uiaData.GetTextBuffer().GetSize().IncrementInBoundsCircular(coord);
}

// Routine Description:
// - Helper to decrement a coordinate in respect to the associated screen buffer
// Arguments
// - coord - Updated by function to decrement one position (will wrap X and Y direction)
void Search::_DecrementCoord(COORD& coord) const noexcept
{
    _uiaData.GetTextBuffer().GetSize().DecrementInBoundsCircular(coord);
}

// Routine Description:
// - Helper to update the coordinate position to the next point to be searched
// Return Value:
// - True if we haven't reached the end of the buffer. False otherwise.
void Search::_UpdateNextPosition()
{
    if (_direction == Direction::Forward)
    {
        _IncrementCoord(_coordNext);
    }
    else if (_direction == Direction::Backward)
    {
        _DecrementCoord(_coordNext);
    }
    else
    {
        THROW_HR(E_NOTIMPL);
    }

    // To reduce wrap-around time, if the next position is larger than
    // the end position of the written text
    // We put the next position to:
    // Forward: (0, 0)
    // Backward: the position of the end of the text buffer
    const COORD bufferEndPosition = _uiaData.GetTextBufferEndPosition();

    if (_coordNext.Y > bufferEndPosition.Y ||
        (_coordNext.Y == bufferEndPosition.Y && _coordNext.X > bufferEndPosition.X))
    {
        if (_direction == Direction::Forward)
        {
            _coordNext = { 0 };
        }
        else
        {
            _coordNext = bufferEndPosition;
        }
    }
}

// Routine Description:
// - Creates a "needle" of the correct format for comparison to the screen buffer text data
//   that we can use for our search
// Arguments:
// - wstr - String that will be our search term
// Return Value:
// - Structured text data for comparison to screen buffer text data.
std::vector<std::vector<wchar_t>> Search::s_CreateNeedleFromString(const std::wstring& wstr)
{
    const auto charData = Utf16Parser::Parse(wstr);
    std::vector<std::vector<wchar_t>> cells;
    for (const auto chars : charData)
    {
        if (IsGlyphFullWidth(std::wstring_view{ chars.data(), chars.size() }))
        {
            cells.emplace_back(chars);
        }
        cells.emplace_back(chars);
    }
    return cells;
}
