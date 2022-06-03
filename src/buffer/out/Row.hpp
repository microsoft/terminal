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
#include "LineRendition.hpp"
#include "OutputCell.hpp"
#include "OutputCellIterator.hpp"
#include "CharRow.hpp"
#include "UnicodeStorage.hpp"

class TextBuffer;

class ROW final
{
public:
    ROW(const til::CoordType rowId, const til::CoordType rowWidth, const TextAttribute fillAttribute, TextBuffer* const pParent);

    til::CoordType size() const noexcept { return _rowWidth; }

    void SetWrapForced(const bool wrap) noexcept { _wrapForced = wrap; }
    bool WasWrapForced() const noexcept { return _wrapForced; }

    void SetDoubleBytePadded(const bool doubleBytePadded) noexcept { _doubleBytePadded = doubleBytePadded; }
    bool WasDoubleBytePadded() const noexcept { return _doubleBytePadded; }

    const CharRow& GetCharRow() const noexcept { return _charRow; }
    CharRow& GetCharRow() noexcept { return _charRow; }

    const ATTR_ROW& GetAttrRow() const noexcept { return _attrRow; }
    ATTR_ROW& GetAttrRow() noexcept { return _attrRow; }

    LineRendition GetLineRendition() const noexcept { return _lineRendition; }
    void SetLineRendition(const LineRendition lineRendition) noexcept { _lineRendition = lineRendition; }

    til::CoordType GetId() const noexcept { return _id; }
    void SetId(const til::CoordType id) noexcept { _id = id; }

    bool Reset(const TextAttribute Attr);
    [[nodiscard]] HRESULT Resize(const til::CoordType width);

    void ClearColumn(const til::CoordType column);
    std::wstring GetText() const { return _charRow.GetText(); }

    UnicodeStorage& GetUnicodeStorage() noexcept;
    const UnicodeStorage& GetUnicodeStorage() const noexcept;

    OutputCellIterator WriteCells(OutputCellIterator it, const til::CoordType index, const std::optional<bool> wrap = std::nullopt, std::optional<til::CoordType> limitRight = std::nullopt);

#ifdef UNIT_TESTING
    friend constexpr bool operator==(const ROW& a, const ROW& b) noexcept;
    friend class RowTests;
#endif

private:
    CharRow _charRow;
    ATTR_ROW _attrRow;
    LineRendition _lineRendition;
    til::CoordType _id;
    til::CoordType _rowWidth;
    // Occurs when the user runs out of text in a given row and we're forced to wrap the cursor to the next line
    bool _wrapForced;
    // Occurs when the user runs out of text to support a double byte character and we're forced to the next line
    bool _doubleBytePadded;
    TextBuffer* _pParent; // non ownership pointer
};

#ifdef UNIT_TESTING
constexpr bool operator==(const ROW& a, const ROW& b) noexcept
{
    // comparison is only used in the tests; this should suffice.
    return (a._pParent == b._pParent &&
            a._id == b._id);
}
#endif
