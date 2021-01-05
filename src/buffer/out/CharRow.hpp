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
#include "CharRowCell.hpp"
#include "UnicodeStorage.hpp"

class ROW;

enum class DelimiterClass
{
    ControlChar,
    DelimiterChar,
    RegularChar
};

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
    using iterator = typename boost::container::small_vector_base<value_type>::iterator;
    using const_iterator = typename boost::container::small_vector_base<value_type>::const_iterator;
    using const_reverse_iterator = typename boost::container::small_vector_base<value_type>::const_reverse_iterator;

    CharRow(size_t rowWidth, ROW* const pParent) noexcept;

    size_t size() const noexcept;
    [[nodiscard]] HRESULT Resize(const size_t newSize) noexcept;
    size_t MeasureLeft() const noexcept;
    size_t MeasureRight() const;
    bool ContainsText() const noexcept;
    const DbcsAttribute& DbcsAttrAt(const size_t column) const;
    DbcsAttribute& DbcsAttrAt(const size_t column);
    void ClearGlyph(const size_t column);

    const DelimiterClass DelimiterClassAt(const size_t column, const std::wstring_view wordDelimiters) const;

    // working with glyphs
    const std::wstring_view GlyphDataAt(const size_t column) const;

    // iterators
    iterator begin() noexcept;
    const_iterator cbegin() const noexcept;
    const_iterator begin() const noexcept { return cbegin(); }

    iterator end() noexcept;
    const_iterator cend() const noexcept;
    const_iterator end() const noexcept { return cend(); }

    UnicodeStorage& GetUnicodeStorage() noexcept;
    const UnicodeStorage& GetUnicodeStorage() const noexcept;
    COORD GetStorageKey(const size_t column) const noexcept;

    void UpdateParent(ROW* const pParent);

    friend class ROW;

private:
    void Reset() noexcept;
    void ClearCell(const size_t column);
    std::wstring GetText() const;
    void WriteCharsIntoColumn(size_t column, const std::wstring_view chars);

protected:
    // storage for glyph data and dbcs attributes
    boost::container::small_vector<value_type, 120> _data;

    // ROW that this CharRow belongs to
    ROW* _pParent;
};

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
