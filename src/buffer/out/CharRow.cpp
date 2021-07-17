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
#pragma warning(push)
#pragma warning(disable : 26447) // small_vector's constructor says it can throw but it should not given how we use it.  This suppresses this error for the AuditMode build.
CharRow::CharRow(CharRowCell* buffer, size_t rowWidth, ROW* const pParent) noexcept :
    _data(buffer, rowWidth),
    _pParent{ FAIL_FAST_IF_NULL(pParent) }
{
}
#pragma warning(pop)

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
}

// Routine Description:
// - resizes the width of the CharRowBase
// Arguments:
// - newSize - the new width of the character and attributes rows
void CharRow::Resize(CharRowCell* buffer, const size_t newSize) noexcept
{
    _data = {buffer, newSize};
}

// Routine Description:
// - Inspects the current internal string to find the left edge of it
// Arguments:
// - <none>
// Return Value:
// - The calculated left boundary of the internal string.
size_t CharRow::MeasureLeft() const noexcept
{
    const auto beg = _data.begin();
    const auto end = _data.end();

    auto it = beg;
    while (it != end && it->IsSpace())
    {
        ++it;
    }

    return it - beg;
}

// Routine Description:
// - Inspects the current internal string to find the right edge of it
// Arguments:
// - <none>
// Return Value:
// - The calculated right boundary of the internal string.
size_t CharRow::MeasureRight() const
{
    const auto beg = _data.rbegin();
    const auto end = _data.rend();

    auto it = beg;
    while (it != end && it->IsSpace())
    {
        ++it;
    }

    return end - it;
}

void CharRow::ClearCell(const size_t column)
{
    _data[column].Reset();
}

// Routine Description:
// - Tells you whether or not this row contains any valid text.
// Arguments:
// - <none>
// Return Value:
// - True if there is valid text in this row. False otherwise.
bool CharRow::ContainsText() const noexcept
{
    for (const auto& cell : _data)
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
    return _data[column].DbcsAttr();
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
    return _data[column].DbcsAttr();
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
    _data[column].EraseChars();
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

// Method Description:
// - get delimiter class for a position in the char row
// - used for double click selection and uia word navigation
// Arguments:
// - column: column to get text data for
// - wordDelimiters: the delimiters defined as a part of the DelimiterClass::DelimiterChar
// Return Value:
// - the delimiter class for the given char
const DelimiterClass CharRow::DelimiterClassAt(const size_t column, const std::wstring_view wordDelimiters) const
{
    THROW_HR_IF(E_INVALIDARG, column >= _data.size());

    const auto glyph = *GlyphAt(column).begin();
    if (glyph <= UNICODE_SPACE)
    {
        return DelimiterClass::ControlChar;
    }
    else if (wordDelimiters.find(glyph) != std::wstring_view::npos)
    {
        return DelimiterClass::DelimiterChar;
    }
    else
    {
        return DelimiterClass::RegularChar;
    }
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
void CharRow::UpdateParent(ROW* const pParent)
{
    _pParent = FAIL_FAST_IF_NULL(pParent);
}
