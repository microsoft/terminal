/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- textBuffer.hpp

Abstract:
- This module contains structures and functions for manipulating a text
  based buffer within the console host window.

Author(s):
- Michael Niksa (miniksa) 10-Apr-2014
- Paul Campbell (paulcam) 10-Apr-2014

Revision History:
- From components of output.h/.c
  by Therese Stowell (ThereseS) 1990-1991

Notes:
ScreenBuffer data structure overview:

each screen buffer has an array of ROW structures.  each ROW structure
contains the data for one row of text.  the data stored for one row of
text is a character array and an attribute array.  the character array
is allocated the full length of the row from the heap, regardless of the
non-space length. we also maintain the non-space length.  the character
array is initialized to spaces.  the attribute
array is run length encoded (i.e 5 BLUE, 3 RED). if there is only one
attribute for the whole row (the normal case), it is stored in the ATTR_ROW
structure.  otherwise the attr string is allocated from the heap.

ROW - CHAR_ROW - CHAR string
\          \ length of char string
\
ATTR_ROW - ATTR_PAIR string
\ length of attr pair string
ROW
ROW
ROW

ScreenInfo->Rows points to the ROW array. ScreenInfo->Rows[0] is not
necessarily the top row. ScreenInfo->BufferInfo.TextInfo->FirstRow contains the index of
the top row.  That means scrolling (if scrolling entire screen)
merely involves changing the FirstRow index,
filling in the last row, and updating the screen.

--*/

#pragma once

#include <vector>

#include "cursor.h"
#include "Row.hpp"
#include "TextAttribute.hpp"
#include "../types/inc/Viewport.hpp"

#include "../buffer/out/textBufferCellIterator.hpp"
#include "../buffer/out/textBufferTextIterator.hpp"

namespace Microsoft::Console::Render
{
    class Renderer;
}

enum class MarkCategory
{
    Prompt = 0,
    Error = 1,
    Warning = 2,
    Success = 3,
    Info = 4
};
struct ScrollMark
{
    std::optional<til::color> color;
    til::point start;
    til::point end; // exclusive
    std::optional<til::point> commandEnd;
    std::optional<til::point> outputEnd;

    MarkCategory category{ MarkCategory::Info };
    // Other things we may want to think about in the future are listed in
    // GH#11000

    bool HasCommand() const noexcept
    {
        return commandEnd.has_value() && *commandEnd != end;
    }
    bool HasOutput() const noexcept
    {
        return outputEnd.has_value() && *outputEnd != *commandEnd;
    }
    std::pair<til::point, til::point> GetExtent() const
    {
        til::point realEnd{ til::coalesce_value(outputEnd, commandEnd, end) };
        return std::make_pair(til::point{ start }, realEnd);
    }
};

class TextBuffer final
{
public:
    TextBuffer(const til::size screenBufferSize,
               const TextAttribute defaultAttributes,
               const UINT cursorSize,
               const bool isActiveBuffer,
               Microsoft::Console::Render::Renderer& renderer);

    TextBuffer(const TextBuffer&) = delete;
    TextBuffer(TextBuffer&&) = delete;
    TextBuffer& operator=(const TextBuffer&) = delete;
    TextBuffer& operator=(TextBuffer&&) = delete;

    ~TextBuffer();

    // Used for duplicating properties to another text buffer
    void CopyProperties(const TextBuffer& OtherBuffer) noexcept;

    // row manipulation
    ROW& GetScratchpadRow();
    ROW& GetScratchpadRow(const TextAttribute& attributes);
    const ROW& GetRowByOffset(til::CoordType index) const;
    ROW& GetRowByOffset(til::CoordType index);

    TextBufferCellIterator GetCellDataAt(const til::point at) const;
    TextBufferCellIterator GetCellLineDataAt(const til::point at) const;
    TextBufferCellIterator GetCellDataAt(const til::point at, const Microsoft::Console::Types::Viewport limit) const;
    TextBufferTextIterator GetTextDataAt(const til::point at) const;
    TextBufferTextIterator GetTextLineDataAt(const til::point at) const;
    TextBufferTextIterator GetTextDataAt(const til::point at, const Microsoft::Console::Types::Viewport limit) const;

    size_t GetCellDistance(const til::point from, const til::point to) const;

    static size_t GraphemeNext(const std::wstring_view& chars, size_t position) noexcept;
    static size_t GraphemePrev(const std::wstring_view& chars, size_t position) noexcept;

    // Text insertion functions
    void Write(til::CoordType row, const TextAttribute& attributes, RowWriteState& state);
    void FillRect(const til::rect& rect, const std::wstring_view& fill, const TextAttribute& attributes);

    OutputCellIterator Write(const OutputCellIterator givenIt);

    OutputCellIterator Write(const OutputCellIterator givenIt,
                             const til::point target,
                             const std::optional<bool> wrap = true);

    OutputCellIterator WriteLine(const OutputCellIterator givenIt,
                                 const til::point target,
                                 const std::optional<bool> setWrap = std::nullopt,
                                 const std::optional<til::CoordType> limitRight = std::nullopt);

    void InsertCharacter(const wchar_t wch, const DbcsAttribute dbcsAttribute, const TextAttribute attr);
    void InsertCharacter(const std::wstring_view chars, const DbcsAttribute dbcsAttribute, const TextAttribute attr);
    void IncrementCursor();
    void NewlineCursor();

    // Scroll needs access to this to quickly rotate around the buffer.
    void IncrementCircularBuffer(const TextAttribute& fillAttributes = {});

    til::point GetLastNonSpaceCharacter(std::optional<const Microsoft::Console::Types::Viewport> viewOptional = std::nullopt) const;

    Cursor& GetCursor() noexcept;
    const Cursor& GetCursor() const noexcept;

    const til::CoordType GetFirstRowIndex() const noexcept;

    const Microsoft::Console::Types::Viewport GetSize() const noexcept;

    void ScrollRows(const til::CoordType firstRow, const til::CoordType size, const til::CoordType delta);

    til::CoordType TotalRowCount() const noexcept;

    const TextAttribute& GetCurrentAttributes() const noexcept;

    void SetCurrentAttributes(const TextAttribute& currentAttributes) noexcept;

    void SetWrapForced(til::CoordType y, bool wrap);
    void SetCurrentLineRendition(const LineRendition lineRendition, const TextAttribute& fillAttributes);
    void ResetLineRenditionRange(const til::CoordType startRow, const til::CoordType endRow);
    LineRendition GetLineRendition(const til::CoordType row) const;
    bool IsDoubleWidthLine(const til::CoordType row) const;

    til::CoordType GetLineWidth(const til::CoordType row) const;
    til::point ClampPositionWithinLine(const til::point position) const;
    til::point ScreenToBufferPosition(const til::point position) const;
    til::point BufferToScreenPosition(const til::point position) const;

    void Reset() noexcept;

    [[nodiscard]] HRESULT ResizeTraditional(const til::size newSize) noexcept;

    void SetAsActiveBuffer(const bool isActiveBuffer) noexcept;
    bool IsActiveBuffer() const noexcept;

    Microsoft::Console::Render::Renderer& GetRenderer() noexcept;

    void TriggerRedraw(const Microsoft::Console::Types::Viewport& viewport);
    void TriggerRedrawCursor(const til::point position);
    void TriggerRedrawAll();
    void TriggerScroll();
    void TriggerScroll(const til::point delta);
    void TriggerNewTextNotification(const std::wstring_view newText);

    til::point GetWordStart(const til::point target, const std::wstring_view wordDelimiters, bool accessibilityMode = false, std::optional<til::point> limitOptional = std::nullopt) const;
    til::point GetWordEnd(const til::point target, const std::wstring_view wordDelimiters, bool accessibilityMode = false, std::optional<til::point> limitOptional = std::nullopt) const;
    bool MoveToNextWord(til::point& pos, const std::wstring_view wordDelimiters, std::optional<til::point> limitOptional = std::nullopt) const;
    bool MoveToPreviousWord(til::point& pos, const std::wstring_view wordDelimiters) const;

    til::point GetGlyphStart(const til::point pos, std::optional<til::point> limitOptional = std::nullopt) const;
    til::point GetGlyphEnd(const til::point pos, bool accessibilityMode = false, std::optional<til::point> limitOptional = std::nullopt) const;
    bool MoveToNextGlyph(til::point& pos, bool allowBottomExclusive = false, std::optional<til::point> limitOptional = std::nullopt) const;
    bool MoveToPreviousGlyph(til::point& pos, std::optional<til::point> limitOptional = std::nullopt) const;

    const std::vector<til::inclusive_rect> GetTextRects(til::point start, til::point end, bool blockSelection, bool bufferCoordinates) const;
    std::vector<til::point_span> GetTextSpans(til::point start, til::point end, bool blockSelection, bool bufferCoordinates) const;

    void AddHyperlinkToMap(std::wstring_view uri, uint16_t id);
    std::wstring GetHyperlinkUriFromId(uint16_t id) const;
    uint16_t GetHyperlinkId(std::wstring_view uri, std::wstring_view id);
    void RemoveHyperlinkFromMap(uint16_t id) noexcept;
    std::wstring GetCustomIdFromId(uint16_t id) const;
    void CopyHyperlinkMaps(const TextBuffer& OtherBuffer);

    class TextAndColor
    {
    public:
        std::vector<std::wstring> text;
        std::vector<std::vector<COLORREF>> FgAttr;
        std::vector<std::vector<COLORREF>> BkAttr;
    };

    size_t SpanLength(const til::point coordStart, const til::point coordEnd) const;

    const TextAndColor GetText(const bool includeCRLF,
                               const bool trimTrailingWhitespace,
                               const std::vector<til::inclusive_rect>& textRects,
                               std::function<std::pair<COLORREF, COLORREF>(const TextAttribute&)> GetAttributeColors = nullptr,
                               const bool formatWrappedRows = false) const;

    std::wstring GetPlainText(const til::point& start, const til::point& end) const;

    static std::string GenHTML(const TextAndColor& rows,
                               const int fontHeightPoints,
                               const std::wstring_view fontFaceName,
                               const COLORREF backgroundColor);

    static std::string GenRTF(const TextAndColor& rows,
                              const int fontHeightPoints,
                              const std::wstring_view fontFaceName,
                              const COLORREF backgroundColor);

    struct PositionInformation
    {
        til::CoordType mutableViewportTop{ 0 };
        til::CoordType visibleViewportTop{ 0 };
    };

    static HRESULT Reflow(TextBuffer& oldBuffer,
                          TextBuffer& newBuffer,
                          const std::optional<Microsoft::Console::Types::Viewport> lastCharacterViewport,
                          std::optional<std::reference_wrapper<PositionInformation>> positionInfo);

    const size_t AddPatternRecognizer(const std::wstring_view regexString);
    void ClearPatternRecognizers() noexcept;
    void CopyPatterns(const TextBuffer& OtherBuffer);
    interval_tree::IntervalTree<til::point, size_t> GetPatterns(const til::CoordType firstRow, const til::CoordType lastRow) const;

    const std::vector<ScrollMark>& GetMarks() const noexcept;
    void ClearMarksInRange(const til::point start, const til::point end);
    void ClearAllMarks() noexcept;
    void ScrollMarks(const int delta);
    void StartPromptMark(const ScrollMark& m);
    void AddMark(const ScrollMark& m);
    void SetCurrentPromptEnd(const til::point pos) noexcept;
    void SetCurrentCommandEnd(const til::point pos) noexcept;
    void SetCurrentOutputEnd(const til::point pos, ::MarkCategory category) noexcept;

private:
    void _reserve(til::size screenBufferSize, const TextAttribute& defaultAttributes);
    void _commit(const std::byte* row);
    void _decommit() noexcept;
    void _construct(const std::byte* until) noexcept;
    void _destroy() const noexcept;
    ROW& _getRowByOffsetDirect(size_t offset);
    til::CoordType _estimateOffsetOfLastCommittedRow() const noexcept;

    void _SetFirstRowIndex(const til::CoordType FirstRowIndex) noexcept;
    til::point _GetPreviousFromCursor() const;
    void _SetWrapOnCurrentRow();
    void _AdjustWrapOnCurrentRow(const bool fSet);
    // Assist with maintaining proper buffer state for Double Byte character sequences
    void _PrepareForDoubleByteSequence(const DbcsAttribute dbcsAttribute);
    bool _AssertValidDoubleByteSequence(const DbcsAttribute dbcsAttribute);
    void _ExpandTextRow(til::inclusive_rect& selectionRow) const;
    DelimiterClass _GetDelimiterClassAt(const til::point pos, const std::wstring_view wordDelimiters) const;
    til::point _GetWordStartForAccessibility(const til::point target, const std::wstring_view wordDelimiters) const;
    til::point _GetWordStartForSelection(const til::point target, const std::wstring_view wordDelimiters) const;
    til::point _GetWordEndForAccessibility(const til::point target, const std::wstring_view wordDelimiters, const til::point limit) const;
    til::point _GetWordEndForSelection(const til::point target, const std::wstring_view wordDelimiters) const;
    void _PruneHyperlinks();
    void _trimMarksOutsideBuffer();

    static void _AppendRTFText(std::ostringstream& contentBuilder, const std::wstring_view& text);

    Microsoft::Console::Render::Renderer& _renderer;

    std::unordered_map<uint16_t, std::wstring> _hyperlinkMap;
    std::unordered_map<std::wstring, uint16_t> _hyperlinkCustomIdMap;
    uint16_t _currentHyperlinkId = 1;

    std::unordered_map<size_t, std::wstring> _idsAndPatterns;
    size_t _currentPatternId = 0;

    // This block describes the state of the underlying virtual memory buffer that holds all ROWs, text and attributes.
    // Initially memory is only allocated with MEM_RESERVE to reduce the private working set of conhost.
    // ROWs are laid out like this in memory:
    //   ROW                <-- sizeof(ROW), stores
    //   (padding)
    //   ROW::_charsBuffer  <-- _width * sizeof(wchar_t)
    //   (padding)
    //   ROW::_charOffsets  <-- (_width + 1) * sizeof(uint16_t)
    //   (padding)
    //   ...
    // Padding may exist for alignment purposes.
    //
    // The base (start) address of the memory arena.
    wil::unique_virtualalloc_ptr<std::byte> _buffer;
    // The past-the-end pointer of the memory arena.
    std::byte* _bufferEnd = nullptr;
    // The range between _buffer (inclusive) and _commitWatermark (exclusive) is the range of
    // memory that has already been committed via MEM_COMMIT and contains ready-to-use ROWs.
    //
    // The problem is that calling VirtualAlloc(MEM_COMMIT) on each ROW one by one is extremely expensive, which forces
    // us to commit ROWs in batches and avoid calling it on already committed ROWs. Let's say we commit memory in
    // batches of 128 ROWs. One option to know whether a ROW has already been committed is to allocate a vector<uint8_t>
    // of size `(height + 127) / 128` and mark the corresponding slot as 1 if that 128-sized batch has been committed.
    // That way we know not to commit it again. But ROWs aren't accessed randomly. Instead, they're usually accessed
    // fairly linearly from row 1 to N. As such we can just commit ROWs up to the point of the highest accessed ROW
    // plus some read-ahead of 128 ROWs. This is exactly what _commitWatermark stores: The highest accessed ROW plus
    // some read-ahead. It's the amount of memory that has been committed and is ready to use.
    //
    // _commitWatermark will always be a multiple of _bufferRowStride away from _buffer.
    // In other words, _commitWatermark itself will either point exactly onto the next ROW
    // that should be committed or be equal to _bufferEnd when all ROWs are committed.
    std::byte* _commitWatermark = nullptr;
    // This will MEM_COMMIT 128 rows more than we need, to avoid us from having to call VirtualAlloc too often.
    // This equates to roughly the following commit chunk sizes at these column counts:
    // *  80 columns (the usual minimum) =  60KB chunks,  4.1MB buffer at 9001 rows
    // * 120 columns (the most common)   =  80KB chunks,  5.6MB buffer at 9001 rows
    // * 400 columns (the usual maximum) = 220KB chunks, 15.5MB buffer at 9001 rows
    // There's probably a better metric than this. (This comment was written when ROW had both,
    // a _chars array containing text and a _charOffsets array contain column-to-text indices.)
    static constexpr size_t _commitReadAheadRowCount = 128;
    // Before TextBuffer was made to use virtual memory it initialized the entire memory arena with the initial
    // attributes right away. To ensure it continues to work the way it used to, this stores these initial attributes.
    TextAttribute _initialAttributes;
    // ROW ---------------+--+--+
    // (padding)          |  |  v _bufferOffsetChars
    // ROW::_charsBuffer  |  |
    // (padding)          |  v _bufferOffsetCharOffsets
    // ROW::_charOffsets  |
    // (padding)          v _bufferRowStride
    size_t _bufferRowStride = 0;
    size_t _bufferOffsetChars = 0;
    size_t _bufferOffsetCharOffsets = 0;
    // The width of the buffer in columns.
    uint16_t _width = 0;
    // The height of the buffer in rows, excluding the scratchpad row.
    uint16_t _height = 0;

    TextAttribute _currentAttributes;
    til::CoordType _firstRow = 0; // indexes top row (not necessarily 0)

    Cursor _cursor;

    bool _isActiveBuffer = false;

    std::vector<ScrollMark> _marks;

#ifdef UNIT_TESTING
    friend class TextBufferTests;
    friend class UiaTextRangeTests;
#endif
};
