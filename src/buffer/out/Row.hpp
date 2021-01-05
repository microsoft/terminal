/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- Row.hpp

Abstract:
- data structure for information associated with one row of screen buffer

Author(s):
- Michael Niksa (miniksa) 10-Apr-2014
- Paul Campbell (paulcam) 10-Apr-2014

Revision History:
- From components of output.h/.c
  by Therese Stowell (ThereseS) 1990-1991
- Pulled into its own file from textBuffer.hpp/cpp (AustDi, 2017)
--*/

#pragma once

#include "AttrRow.hpp"
#include "OutputCell.hpp"
#include "OutputCellIterator.hpp"
#include "CharRow.hpp"
#include "RowCellIterator.hpp"
#include "UnicodeStorage.hpp"

class TextBuffer;

class ROW final
{
public:
    ROW(const SHORT rowId, const unsigned short rowWidth, const TextAttribute fillAttribute, TextBuffer* const pParent)
    noexcept;

    size_t size() const noexcept { return _rowWidth; }

    const CharRow& GetCharRow() const noexcept { return _charRow; }
    CharRow& GetCharRow() noexcept { return _charRow; }

    const ATTR_ROW& GetAttrRow() const noexcept { return _attrRow; }
    ATTR_ROW& GetAttrRow() noexcept { return _attrRow; }

    SHORT GetId() const noexcept { return _id; }
    void SetId(const SHORT id) noexcept { _id = id; }

    bool Reset(const TextAttribute Attr);
    [[nodiscard]] HRESULT Resize(const unsigned short width);

    void ClearColumn(const size_t column);
    std::wstring GetText() const { return _charRow.GetText(); }

    RowCellIterator AsCellIter(const size_t startIndex) const { return AsCellIter(startIndex, size() - startIndex); }
    RowCellIterator AsCellIter(const size_t startIndex, const size_t count) const { return RowCellIterator(*this, startIndex, count); }

    UnicodeStorage& GetUnicodeStorage() noexcept;
    const UnicodeStorage& GetUnicodeStorage() const noexcept;

    OutputCellIterator WriteCells(OutputCellIterator it, const size_t index, const std::optional<bool> wrap = std::nullopt, std::optional<size_t> limitRight = std::nullopt);

    friend bool operator==(const ROW& a, const ROW& b) noexcept;

#ifdef UNIT_TESTING
    friend class RowTests;
#endif

private:
    CharRow _charRow;
    ATTR_ROW _attrRow;
    SHORT _id;
    unsigned short _rowWidth;
    TextBuffer* _pParent; // non ownership pointer
};

inline bool operator==(const ROW& a, const ROW& b) noexcept
{
    return (a._charRow == b._charRow &&
            a._attrRow == b._attrRow &&
            a._rowWidth == b._rowWidth &&
            a._pParent == b._pParent &&
            a._id == b._id);
}
