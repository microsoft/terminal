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
