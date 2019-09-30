/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- CharRow.hpp

Abstract:
- contains data structure for UCS2 encoded character data of a row

Author(s):
- Michael Niksa (miniksa) 10-Apr-2014
- Paul Campbell (paulcam) 10-Apr-2014

Revision History:
- From components of output.h/.c
  by Therese Stowell (ThereseS) 1990-1991
- Pulled into its own file from textBuffer.hpp/cpp (AustDi, 2017)
--*/

#pragma once

#include "DbcsAttribute.hpp"
#include "CharRowCellReference.hpp"
#include "CharRowCell.hpp"
#include "UnicodeStorage.hpp"

class ROW;

// the characters of one row of screen buffer
// we keep the following values so that we don't write
// more pixels to the screen than we have to:
// left is initialized to screenbuffer width.  right is
// initialized to zero.
//
//      [     foo.bar    12-12-61                       ]
//       ^    ^                  ^                     ^
//       |    |                  |                     |
//     Chars Left               Right                end of Chars buffer
class CharRow final
{
public:
    using glyph_type = typename wchar_t;
    using value_type = typename CharRowCell;
    using iterator = typename std::vector<value_type>::iterator;
    using const_iterator = typename std::vector<value_type>::const_iterator;
    using reference = typename CharRowCellReference;

    CharRow(size_t rowWidth, ROW* const pParent);

    void SetWrapForced(const bool wrap) noexcept;
    bool WasWrapForced() const noexcept;
    void SetDoubleBytePadded(const bool doubleBytePadded) noexcept;
    bool WasDoubleBytePadded() const noexcept;
    size_t size() const noexcept;
    void Reset() noexcept;
    [[nodiscard]] HRESULT Resize(const size_t newSize) noexcept;
    size_t MeasureLeft() const;
    size_t MeasureRight() const noexcept;
    void ClearCell(const size_t column);
    bool ContainsText() const noexcept;
    const DbcsAttribute& DbcsAttrAt(const size_t column) const;
    DbcsAttribute& DbcsAttrAt(const size_t column);
    void ClearGlyph(const size_t column);
    std::wstring GetText() const;

    // working with glyphs
    const reference GlyphAt(const size_t column) const;
    reference GlyphAt(const size_t column);

    // iterators
    iterator begin() noexcept;
    const_iterator cbegin() const noexcept;

    iterator end() noexcept;
    const_iterator cend() const noexcept;

    UnicodeStorage& GetUnicodeStorage() noexcept;
    const UnicodeStorage& GetUnicodeStorage() const noexcept;
    COORD GetStorageKey(const size_t column) const noexcept;

    void UpdateParent(ROW* const pParent) noexcept;

    friend CharRowCellReference;
    friend constexpr bool operator==(const CharRow& a, const CharRow& b) noexcept;

protected:
    // Occurs when the user runs out of text in a given row and we're forced to wrap the cursor to the next line
    bool _wrapForced;

    // Occurs when the user runs out of text to support a double byte character and we're forced to the next line
    bool _doubleBytePadded;

    // storage for glyph data and dbcs attributes
    std::vector<value_type> _data;

    // ROW that this CharRow belongs to
    ROW* _pParent;
};

constexpr bool operator==(const CharRow& a, const CharRow& b) noexcept
{
    return (a._wrapForced == b._wrapForced &&
            a._doubleBytePadded == b._doubleBytePadded &&
            a._data == b._data);
}

template<typename InputIt1, typename InputIt2>
void OverwriteColumns(InputIt1 startChars, InputIt1 endChars, InputIt2 startAttrs, CharRow::iterator outIt)
{
    std::transform(startChars,
                   endChars,
                   startAttrs,
                   outIt,
                   [](const wchar_t wch, const DbcsAttribute attr) {
                       return CharRow::value_type{ wch, attr };
                   });
}
