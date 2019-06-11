// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "vtrenderer.hpp"

#pragma hdrstop

using namespace Microsoft::Console::Render;
using namespace Microsoft::Console::Types;

// Routine Description:
// - Gets the size in characters of the current dirty portion of the frame.
// Arguments:
// - <none>
// Return Value:
// - The character dimensions of the current dirty area of the frame.
//      This is an Inclusive rect.
SMALL_RECT VtEngine::GetDirtyRectInChars()
{
    SMALL_RECT dirty = _invalidRect.ToInclusive();
    if (dirty.Top < _virtualTop)
    {
        dirty.Top = _virtualTop;
    }
    return dirty;
}

// Routine Description:
// - Uses the currently selected font to determine how wide the given character will be when renderered.
// - NOTE: Only supports determining half-width/full-width status for CJK-type languages (e.g. is it 1 character wide or 2. a.k.a. is it a rectangle or square.)
// Arguments:
// - glyph - utf16 encoded codepoint to check
// - pResult - recieves return value, True if it is full-width (2 wide). False if it is half-width (1 wide).
// Return Value:
// - S_FALSE: This is unsupported by the VT Renderer and should use another engine's value.
[[nodiscard]] HRESULT VtEngine::IsGlyphWideByFont(const std::wstring_view /*glyph*/, _Out_ bool* const pResult) noexcept
{
    *pResult = false;
    return S_FALSE;
}

// Routine Description:
// - Performs a "CombineRect" with the "OR" operation.
// - Basically extends the existing rect outward to also encompass the passed-in region.
// Arguments:
// - pRectExisting - Expand this rectangle to encompass the add rect.
// - pRectToOr - Add this rectangle to the existing one.
// Return Value:
// - <none>
void VtEngine::_OrRect(_Inout_ SMALL_RECT* const pRectExisting, const SMALL_RECT* const pRectToOr) const
{
    pRectExisting->Left = std::min(pRectExisting->Left, pRectToOr->Left);
    pRectExisting->Top = std::min(pRectExisting->Top, pRectToOr->Top);
    pRectExisting->Right = std::max(pRectExisting->Right, pRectToOr->Right);
    pRectExisting->Bottom = std::max(pRectExisting->Bottom, pRectToOr->Bottom);
}

// Method Description:
// - Returns true if the invalidated region indicates that we only need to
//      simply print text from the current cursor position. This will prevent us
//      from sending extra VT set-up/tear down sequences (?12h/l) when all we
//      need to do is print more text at the current cursor position.
// Arguments:
// - <none>
// Return Value:
// - true iff only the next character is invalid
bool VtEngine::_WillWriteSingleChar() const
{
    COORD currentCursor = _lastText;
    SMALL_RECT _srcInvalid = _invalidRect.ToExclusive();
    bool noScrollDelta = (_scrollDelta.X == 0 && _scrollDelta.Y == 0);

    bool invalidIsOneChar = (_invalidRect.Width() == 1) &&
                            (_invalidRect.Height() == 1);
    // Either the next character to the right or the immediately previous
    //      character should follow this code path
    //      (The immediate previous character would suggest a backspace)
    bool invalidIsNext = (_srcInvalid.Top == _lastText.Y) && (_srcInvalid.Left == _lastText.X);
    bool invalidIsLast = (_srcInvalid.Top == _lastText.Y) && (_srcInvalid.Left == (_lastText.X - 1));

    return noScrollDelta && invalidIsOneChar && (invalidIsNext || invalidIsLast);
}
