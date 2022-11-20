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

#include <til/rle.h>

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
    void SetLineRendition(const LineRendition lineRendition) noexcept;
    LineRendition GetLineRendition() const noexcept;

    void Reset(const TextAttribute& attr);
    void Resize(wchar_t* charsBuffer, uint16_t* charOffsetsBuffer, uint16_t rowWidth, const TextAttribute& fillAttribute);
    void TransferAttributes(const til::small_rle<TextAttribute, uint16_t, 1>& attr, til::CoordType newWidth);

    void ClearCell(til::CoordType column);
    OutputCellIterator WriteCells(OutputCellIterator it, til::CoordType columnBegin, std::optional<bool> wrap = std::nullopt, std::optional<til::CoordType> limitRight = std::nullopt);
    bool SetAttrToEnd(til::CoordType columnBegin, TextAttribute attr);
    void ReplaceAttributes(til::CoordType beginIndex, til::CoordType endIndex, const TextAttribute& newAttr);
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
    // To simplify the detection of wide glyphs, we don't just store the simple character offset as described
    // for _charOffsets. Instead we use the most significant bit to indicate whether any column is the
    // trailing half of a wide glyph. This simplifies many implementation details via _uncheckedIsTrailer.
    static constexpr uint16_t CharOffsetsTrailer = 0x8000;
    static constexpr uint16_t CharOffsetsMask = 0x7fff;

    template<typename T>
    static constexpr uint16_t _clampedUint16(T v) noexcept;
    template<typename T>
    constexpr uint16_t _clampedColumn(T v) const noexcept;
    template<typename T>
    constexpr uint16_t _clampedColumnInclusive(T v) const noexcept;

    wchar_t _uncheckedChar(size_t off) const noexcept;
    uint16_t _charSize() const noexcept;
    uint16_t _uncheckedCharOffset(size_t col) const noexcept;
    bool _uncheckedIsTrailer(size_t col) const noexcept;

    void _init() noexcept;
    void _resizeChars(uint16_t colExtEnd, uint16_t chExtBeg, uint16_t chExtEnd, size_t chExtEndNew);

    // These fields are a bit "wasteful", but it makes all this a bit more robust against
    // programming errors during initial development (which is when this comment was written).
    // * _chars and _charsHeap are redundant
    //   If _charsHeap is stored in _chars, we can still infer that
    //   _chars was allocated on the heap if _chars != _charsBuffer.
    // * _chars doesn't need a size_t size()
    //   The size may never exceed an uint16_t anyways.
    // * _charOffsets doesn't need a size() at all
    //   The length is already stored in _columns.

    // Most text uses only a single wchar_t per codepoint / grapheme cluster.
    // That's why TextBuffer allocates a large blob of virtual memory which we can use as
    // a simplified chars buffer, without having to allocate any additional heap memory.
    // _charsBuffer fits _columnCount characters at most.
    wchar_t* _charsBuffer = nullptr;
    // ...but if this ROW needs to store more than _columnCount characters
    // then it will allocate a larger string on the heap and store it here.
    // The capacity of this string on the heap is stored in _chars.size().
    std::unique_ptr<wchar_t[]> _charsHeap;
    // _chars either refers to our _charsBuffer or _charsHeap, defaulting to the former.
    // _chars.size() is NOT the length of the string, but rather its capacity.
    // _charOffsets[_columnCount] stores the length.
    std::span<wchar_t> _chars;
    // _charOffsets accelerates indexing into the above _chars string given a terminal column,
    // by storing the character index/offset at which a column's text in _chars starts.
    // It stores 1 more item than this row is wide, allowing it to store the
    // past-the-end offset, which is thus equal to the length of the string.
    //
    // For instance given a 4 column ROW containing "abcd" it would store 01234,
    // because each of "abcd" are 1 column wide and 1 wchar_t per column.
    // Given "a\u732Bd" it would store 01123, because "\u732B" is a wide glyph
    // and "11" indicates that both column 1 and 2 start at &_chars[1] (= wide glyph).
    // The fact that the next offset is 2 tells us that the glyph is 1 wchar_t long.
    // Given "a\uD83D\uDE00d" ("\uD83D\uDE00" is an Emoji) it would store 01134,
    // because while it's 2 cells wide as indicated by 2 offsets that are identical (11),
    // the next offset is 3, which indicates that the glyph is 3-1 = 2 wchar_t long.
    //
    // In other words, _charOffsets tells us both the width in chars and width in columns.
    // See CharOffsetsTrailer for more information.
    std::span<uint16_t> _charOffsets;
    // _attr is a run-length-encoded vector of TextAttribute with a decompressed
    // length equal to _columnCount (= 1 TextAttribute per column).
    til::small_rle<TextAttribute, uint16_t, 1> _attr;
    // The width of the row in visual columns.
    uint16_t _columnCount = 0;
    // Stores double-width/height (DECSWL/DECDWL/DECDHL) attributes.
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
