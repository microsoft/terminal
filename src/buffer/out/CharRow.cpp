// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "CharRow.hpp"
#include "unicode.hpp"
#include "Row.hpp"

// Routine Description:
// - constructor
// Arguments:
// - rowWidth - the size (in wchar_t) of the char and attribute rows
// - pParent - the parent ROW
// Return Value:
// - instantiated object
// Note: will through if unable to allocate char/attribute buffers
CharRow::CharRow(size_t rowWidth, ROW* const pParent) :
    _wrapForced{ false },
    _doubleBytePadded{ false },
    _data(rowWidth, value_type()),
    _pParent{ FAIL_FAST_IF_NULL(pParent) }
{
}

// Routine Description:
// - Sets the wrap status for the current row
// Arguments:
// - wrapForced - True if the row ran out of space and we forced to wrap to the next row. False otherwise.
// Return Value:
// - <none>
void CharRow::SetWrapForced(const bool wrapForced) noexcept
{
    _wrapForced = wrapForced;
}

// Routine Description:
// - Gets the wrap status for the current row
// Arguments:
// - <none>
// Return Value:
// - True if the row ran out of space and we were forced to wrap to the next row. False otherwise.
bool CharRow::WasWrapForced() const noexcept
{
    return _wrapForced;
}

// Routine Description:
// - Sets the double byte padding for the current row
// Arguments:
// - fWrapWasForced - True if the row ran out of space for a double byte character and we padded out the row. False otherwise.
// Return Value:
// - <none>
void CharRow::SetDoubleBytePadded(const bool doubleBytePadded) noexcept
{
    _doubleBytePadded = doubleBytePadded;
}

// Routine Description:
// - Gets the double byte padding status for the current row.
// Arguments:
// - <none>
// Return Value:
// - True if the row didn't have space for a double byte character and we were padded out the row. False otherwise.
bool CharRow::WasDoubleBytePadded() const noexcept
{
    return _doubleBytePadded;
}

// Routine Description:
// - gets the size of the row, in glyph cells
// Arguments:
// - <none>
// Return Value:
// - the size of the row
size_t CharRow::size() const noexcept
{
    return _data.size();
}

// Routine Description:
// - Sets all properties of the CharRowBase to default values
// Arguments:
// - sRowWidth - The width of the row.
// Return Value:
// - <none>
void CharRow::Reset() noexcept
{
    for (auto& cell : _data)
    {
        cell.Reset();
    }

    _wrapForced = false;
    _doubleBytePadded = false;
}

// Routine Description:
// - resizes the width of the CharRowBase
// Arguments:
// - newSize - the new width of the character and attributes rows
// Return Value:
// - S_OK on success, otherwise relevant error code
[[nodiscard]] HRESULT CharRow::Resize(const size_t newSize) noexcept
{
    try
    {
        const value_type insertVals;
        _data.resize(newSize, insertVals);
    }
    CATCH_RETURN();

    return S_OK;
}

typename CharRow::iterator CharRow::begin() noexcept
{
    return _data.begin();
}

typename CharRow::const_iterator CharRow::cbegin() const noexcept
{
    return _data.cbegin();
}

typename CharRow::iterator CharRow::end() noexcept
{
    return _data.end();
}

typename CharRow::const_iterator CharRow::cend() const noexcept
{
    return _data.cend();
}

// Routine Description:
// - Inspects the current internal string to find the left edge of it
// Arguments:
// - <none>
// Return Value:
// - The calculated left boundary of the internal string.
size_t CharRow::MeasureLeft() const
{
    std::vector<value_type>::const_iterator it = _data.cbegin();
    while (it != _data.cend() && it->IsSpace())
    {
        ++it;
    }
    return it - _data.cbegin();
}

// Routine Description:
// - Inspects the current internal string to find the right edge of it
// Arguments:
// - <none>
// Return Value:
// - The calculated right boundary of the internal string.
size_t CharRow::MeasureRight() const noexcept
{
    std::vector<value_type>::const_reverse_iterator it = _data.crbegin();
    while (it != _data.crend() && it->IsSpace())
    {
        ++it;
    }
    return _data.crend() - it;
}

void CharRow::ClearCell(const size_t column)
{
    _data.at(column).Reset();
}

// Routine Description:
// - Tells you whether or not this row contains any valid text.
// Arguments:
// - <none>
// Return Value:
// - True if there is valid text in this row. False otherwise.
bool CharRow::ContainsText() const noexcept
{
    for (const value_type& cell : _data)
    {
        if (!cell.IsSpace())
        {
            return true;
        }
    }
    return false;
}

// Routine Description:
// - gets the attribute at the specified column
// Arguments:
// - column - the column to get the attribute for
// Return Value:
// - the attribute
// Note: will throw exception if column is out of bounds
const DbcsAttribute& CharRow::DbcsAttrAt(const size_t column) const
{
    return _data.at(column).DbcsAttr();
}

// Routine Description:
// - gets the attribute at the specified column
// Arguments:
// - column - the column to get the attribute for
// Return Value:
// - the attribute
// Note: will throw exception if column is out of bounds
DbcsAttribute& CharRow::DbcsAttrAt(const size_t column)
{
    return _data.at(column).DbcsAttr();
}

// Routine Description:
// - resets text data at column
// Arguments:
// - column - column index to clear text data from
// Return Value:
// - <none>
// Note: will throw exception if column is out of bounds
void CharRow::ClearGlyph(const size_t column)
{
    _data.at(column).EraseChars();
}

// Routine Description:
// - returns text data at column as a const reference.
// Arguments:
// - column - column to get text data for
// Return Value:
// - text data at column
// - Note: will throw exception if column is out of bounds
const CharRow::reference CharRow::GlyphAt(const size_t column) const
{
    THROW_HR_IF(E_INVALIDARG, column >= _data.size());
    return { const_cast<CharRow&>(*this), column };
}

// Routine Description:
// - returns text data at column as a reference.
// Arguments:
// - column - column to get text data for
// Return Value:
// - text data at column
// - Note: will throw exception if column is out of bounds
CharRow::reference CharRow::GlyphAt(const size_t column)
{
    THROW_HR_IF(E_INVALIDARG, column >= _data.size());
    return { *this, column };
}

std::wstring CharRow::GetText() const
{
    std::wstring wstr;
    wstr.reserve(_data.size());

    for (size_t i = 0; i < _data.size(); ++i)
    {
        const auto glyph = GlyphAt(i);
        if (!DbcsAttrAt(i).IsTrailing())
        {
            for (const auto wch : glyph)
            {
                wstr.push_back(wch);
            }
        }
    }
    return wstr;
}

UnicodeStorage& CharRow::GetUnicodeStorage() noexcept
{
    return _pParent->GetUnicodeStorage();
}

const UnicodeStorage& CharRow::GetUnicodeStorage() const noexcept
{
    return _pParent->GetUnicodeStorage();
}

// Routine Description:
// - calculates the key used by the given column of the char row to store glyph data in UnicodeStorage
// Arguments:
// - column - the column to generate the key for
// Return Value:
// - the COORD key for data access from UnicodeStorage for the column
COORD CharRow::GetStorageKey(const size_t column) const noexcept
{
    return { gsl::narrow<SHORT>(column), _pParent->GetId() };
}

// Routine Description:
// - Updates the pointer to the parent row (which might change if we shuffle the rows around)
// Arguments:
// - pParent - Pointer to the parent row
void CharRow::UpdateParent(ROW* const pParent) noexcept
{
    _pParent = FAIL_FAST_IF_NULL(pParent);
}
