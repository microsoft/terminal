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
CharRow::CharRow(size_t rowWidth, ROW* const pParent) noexcept :
    _data(rowWidth, value_type()),
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
size_t CharRow::MeasureLeft() const noexcept
{
    const_iterator it = _data.cbegin();
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
size_t CharRow::MeasureRight() const
{
    const_reverse_iterator it = _data.crbegin();
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

std::wstring CharRow::GetText() const
{
    std::wstring wstr;
    wstr.reserve(_data.size());

    for (size_t i = 0; i < _data.size(); ++i)
    {
        if (!_data.at(i).DbcsAttr().IsTrailing())
        {
            wstr.append(GlyphDataAt(i));
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

    const auto glyph = *GlyphDataAt(column).begin();
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
// - assignment operator. will store extended glyph data in a separate storage location
// Arguments:
// - chars - the glyph data to store
void CharRow::WriteCharsIntoColumn(size_t column, const std::wstring_view chars)
{
    THROW_HR_IF(E_INVALIDARG, chars.empty());
    auto& position = _data.at(column);
    if (chars.size() == 1)
    {
        position.Char() = chars.front();
        position.DbcsAttr().SetGlyphStored(false);
    }
    else
    {
        auto& storage = _pParent->GetUnicodeStorage();
        const auto key = GetStorageKey(column);
        storage.StoreGlyph(key, { chars.cbegin(), chars.cend() });
        position.DbcsAttr().SetGlyphStored(true);
    }
}

const std::wstring_view CharRow::GlyphDataAt(const size_t column) const
{
    if (_data.at(column).DbcsAttr().IsGlyphStored())
    {
        const auto& text = GetUnicodeStorage().GetText(GetStorageKey(column));
        return { text.data(), text.size() };
    }
    else
    {
        return { &_data.at(column).Char(), 1 };
    }
}

// - Updates the pointer to the parent row (which might change if we shuffle the rows around)
// Arguments:
// - pParent - Pointer to the parent row
void CharRow::UpdateParent(ROW* const pParent)
{
    _pParent = FAIL_FAST_IF_NULL(pParent);
}
