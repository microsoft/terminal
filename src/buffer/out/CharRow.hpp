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
    using reference = typename CharRowCellReference;

    CharRow(til::CoordType rowWidth, ROW* const pParent) noexcept;

    til::CoordType size() const noexcept;
    [[nodiscard]] HRESULT Resize(const til::CoordType newSize) noexcept;
    til::CoordType MeasureLeft() const noexcept;
    til::CoordType MeasureRight() const;
    bool ContainsText() const noexcept;
    const DbcsAttribute& DbcsAttrAt(const til::CoordType column) const;
    DbcsAttribute& DbcsAttrAt(const til::CoordType column);
    void ClearGlyph(const til::CoordType column);

    const DelimiterClass DelimiterClassAt(const til::CoordType column, const std::wstring_view wordDelimiters) const;

    // working with glyphs
    const reference GlyphAt(const til::CoordType column) const;
    reference GlyphAt(const til::CoordType column);

    // iterators
    iterator begin() noexcept;
    const_iterator cbegin() const noexcept;
    const_iterator begin() const noexcept { return cbegin(); }

    iterator end() noexcept;
    const_iterator cend() const noexcept;
    const_iterator end() const noexcept { return cend(); }

    UnicodeStorage& GetUnicodeStorage() noexcept;
    const UnicodeStorage& GetUnicodeStorage() const noexcept;
    til::point GetStorageKey(const til::CoordType column) const noexcept;

    void UpdateParent(ROW* const pParent);

    friend CharRowCellReference;
    friend class ROW;

private:
    void Reset() noexcept;
    void ClearCell(const til::CoordType column);
    std::wstring GetText() const;

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
