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
// - area - The character dimensions of the current dirty area of the frame.
//          This is an Inclusive rect.
// Return Value:
// - S_OK.
[[nodiscard]] HRESULT VtEngine::GetDirtyArea(std::span<const til::rect>& area) noexcept
{
    area = _invalidMap.runs();
    return S_OK;
}

// Routine Description:
// - Uses the currently selected font to determine how wide the given character will be when rendered.
// - NOTE: Only supports determining half-width/full-width status for CJK-type languages (e.g. is it 1 character wide or 2. a.k.a. is it a rectangle or square.)
// Arguments:
// - glyph - utf16 encoded codepoint to check
// - pResult - receives return value, True if it is full-width (2 wide). False if it is half-width (1 wide).
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
void VtEngine::_OrRect(_Inout_ til::inclusive_rect* const pRectExisting, const til::inclusive_rect* const pRectToOr) const
{
    pRectExisting->left = std::min(pRectExisting->left, pRectToOr->left);
    pRectExisting->top = std::min(pRectExisting->top, pRectToOr->top);
    pRectExisting->right = std::max(pRectExisting->right, pRectToOr->right);
    pRectExisting->bottom = std::max(pRectExisting->bottom, pRectToOr->bottom);
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
    // If there is no scroll delta, return false.
    if (til::point{ 0, 0 } != _scrollDelta)
    {
        return false;
    }

    // If there is more than one invalid char, return false.
    if (!_invalidMap.one())
    {
        return false;
    }

    // Get the single point at which things are invalid.
    const auto invalidPoint = _invalidMap.runs().front().origin();

    // Either the next character to the right or the immediately previous
    //      character should follow this code path
    //      (The immediate previous character would suggest a backspace)
    auto invalidIsNext = invalidPoint == _lastText;
    auto invalidIsLast = invalidPoint == til::point{ _lastText.x - 1, _lastText.y };

    return invalidIsNext || invalidIsLast;
}
