// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
#include "precomp.h"

#include "CharRowCell.hpp"
#include "unicode.hpp"

// default glyph value, used for reseting the character data portion of a cell
static constexpr wchar_t DefaultValue = UNICODE_SPACE;

CharRowCell::CharRowCell() noexcept :
    _wch{ DefaultValue },
    _attr{}
{
}

CharRowCell::CharRowCell(const wchar_t wch, const DbcsAttribute attr) noexcept :
    _wch{ wch },
    _attr{ attr }
{
}

// Routine Description:
// - "erases" the glyph. really sets it back to the default "empty" value
void CharRowCell::EraseChars() noexcept
{
    if (_attr.IsGlyphStored())
    {
        _attr.SetGlyphStored(false);
    }
    _wch = DefaultValue;
}

// Routine Description:
// - resets this object back to the defaults it would have from the default constructor
void CharRowCell::Reset() noexcept
{
    _attr.Reset();
    _wch = DefaultValue;
}

// Routine Description:
// - checks if cell contains a space glyph
// Return Value:
// - true if cell contains a space glyph, false otherwise
bool CharRowCell::IsSpace() const noexcept
{
    return !_attr.IsGlyphStored() && _wch == UNICODE_SPACE;
}

// Routine Description:
// - Access the DbcsAttribute for the cell
// Return Value:
// - ref to the cells' DbcsAttribute
DbcsAttribute& CharRowCell::DbcsAttr() noexcept
{
    return _attr;
}

// Routine Description:
// - Access the DbcsAttribute for the cell
// Return Value:
// - ref to the cells' DbcsAttribute
const DbcsAttribute& CharRowCell::DbcsAttr() const noexcept
{
    return _attr;
}

// Routine Description:
// - Access the cell's wchar field. this does not access any char data through UnicodeStorage.
// Return Value:
// - the cell's wchar field
wchar_t& CharRowCell::Char() noexcept
{
    return _wch;
}

// Routine Description:
// - Access the cell's wchar field. this does not access any char data through UnicodeStorage.
// Return Value:
// - the cell's wchar field
const wchar_t& CharRowCell::Char() const noexcept
{
    return _wch;
}
