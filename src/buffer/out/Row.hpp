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

#include <span>

#include "til/rle.h"
#include "LineRendition.hpp"
#include "OutputCell.hpp"
#include "OutputCellIterator.hpp"

class TextBuffer;

enum class DelimiterClass
{
    ControlChar,
    DelimiterChar,
    RegularChar
};

class ROW final
{
public:
    ROW() = default;
    ROW(wchar_t* charsBuffer, uint16_t* charOffsetsBuffer, uint16_t rowWidth, const TextAttribute& fillAttribute);

    ROW(const ROW& other) = delete;
    ROW& operator=(const ROW& other) = delete;

    explicit ROW(ROW&& other) = default;
    ROW& operator=(ROW&& other) = default;

    friend void swap(ROW& lhs, ROW& rhs) noexcept;

    void SetWrapForced(const bool wrap) noexcept;
    bool WasWrapForced() const noexcept;
    void SetDoubleBytePadded(const bool doubleBytePadded) noexcept;
    bool WasDoubleBytePadded() const noexcept;
    LineRendition GetLineRendition() const noexcept;
    void SetLineRendition(const LineRendition lineRendition) noexcept;

    void Reset(const TextAttribute& attr);
    void Resize(wchar_t* charsBuffer, uint16_t* charOffsetsBuffer, uint16_t rowWidth, const TextAttribute& fillAttribute);
    void TransferAttributes(const til::small_rle<TextAttribute, uint16_t, 1>& attr, til::CoordType newWidth);

    void ClearCell(til::CoordType column);
    OutputCellIterator WriteCells(OutputCellIterator it, til::CoordType columnBegin, std::optional<bool> wrap = std::nullopt, std::optional<til::CoordType> limitRight = std::nullopt);
    bool SetAttrToEnd(til::CoordType columnBegin, TextAttribute attr);
    void ReplaceCharacters(til::CoordType columnBegin, til::CoordType width, const std::wstring_view& chars);

    const til::small_rle<TextAttribute, uint16_t, 1>& Attributes() const noexcept;
    TextAttribute GetAttrByColumn(til::CoordType column) const;
    std::vector<uint16_t> GetHyperlinks() const;
    uint16_t size() const noexcept;
    til::CoordType MeasureLeft() const noexcept;
    til::CoordType MeasureRight() const noexcept;
    bool ContainsText() const noexcept;
    std::wstring_view GlyphAt(til::CoordType column) const noexcept;
    DbcsAttribute DbcsAttrAt(til::CoordType column) const noexcept;
    std::wstring_view GetText() const noexcept;
    DelimiterClass DelimiterClassAt(til::CoordType column, const std::wstring_view& wordDelimiters) const noexcept;

    auto AttrBegin() const noexcept { return _attr.begin(); }
    auto AttrEnd() const noexcept { return _attr.end(); }

#ifdef UNIT_TESTING
    friend constexpr bool operator==(const ROW& a, const ROW& b) noexcept;
    friend class RowTests;
#endif

private:
    static constexpr uint16_t CharOffsetsTrailer = 0x8000;
    static constexpr uint16_t CharOffsetsMask = 0x7fff;

    template<typename T>
    static constexpr uint16_t clampedUint16(T v) noexcept;
    template<typename T>
    constexpr uint16_t clampedColumn(T v) const noexcept;
    template<typename T>
    constexpr uint16_t clampedColumnInclusive(T v) const noexcept;

    wchar_t _uncheckedChar(size_t off) const noexcept;
    uint16_t _charSize() const noexcept;
    uint16_t _uncheckedCharOffset(size_t col) const noexcept;
    bool _uncheckedIsTrailer(size_t col) const noexcept;

    void _init() noexcept;
    void _resizeChars(uint16_t colExtEnd, uint16_t chExtBeg, uint16_t chExtEnd, size_t chExtEndNew);

    // These fields are a bit "wasteful", but it makes all this a bit more robust against
    // programming errors during initial development (which is when this comment was written).
    // * _charsHeap as unique_ptr
    //   It can be stored in _chars and delete[] manually called if `_chars != _charsBuffer`
    // * _chars as std::span
    //   The size may never exceed an uint16_t anyways
    // * _charOffsets as std::span
    //   The length is already stored in _columns
    wchar_t* _charsBuffer = nullptr;
    std::unique_ptr<wchar_t[]> _charsHeap;
    std::span<wchar_t> _chars;
    std::span<uint16_t> _charOffsets;
    til::small_rle<TextAttribute, uint16_t, 1> _attr;
    uint16_t _columnCount = 0;
    LineRendition _lineRendition = LineRendition::SingleWidth;
    // Occurs when the user runs out of text in a given row and we're forced to wrap the cursor to the next line
    bool _wrapForced = false;
    // Occurs when the user runs out of text to support a double byte character and we're forced to the next line
    bool _doubleBytePadded = false;
};

#ifdef UNIT_TESTING
constexpr bool operator==(const ROW& a, const ROW& b) noexcept
{
    // comparison is only used in the tests; this should suffice.
    return a._charsBuffer == b._charsBuffer;
}
#endif
