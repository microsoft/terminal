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
#include "UnicodeStorage.hpp"
#include "../types/inc/Viewport.hpp"

#include "../buffer/out/textBufferCellIterator.hpp"
#include "../buffer/out/textBufferTextIterator.hpp"

namespace Microsoft::Console::Render
{
    class Renderer;
}

class TextBuffer final
{
public:
    TextBuffer(const til::size screenBufferSize,
               const TextAttribute defaultAttributes,
               const UINT cursorSize,
               const bool isActiveBuffer,
               Microsoft::Console::Render::Renderer& renderer);
    TextBuffer(const TextBuffer& a) = delete;

    // Used for duplicating properties to another text buffer
    void CopyProperties(const TextBuffer& OtherBuffer) noexcept;

    // row manipulation
    const ROW& GetRowByOffset(const til::CoordType index) const noexcept;
    ROW& GetRowByOffset(const til::CoordType index) noexcept;

    TextBufferCellIterator GetCellDataAt(const til::point at) const;
    TextBufferCellIterator GetCellLineDataAt(const til::point at) const;
    TextBufferCellIterator GetCellDataAt(const til::point at, const Microsoft::Console::Types::Viewport limit) const;
    TextBufferTextIterator GetTextDataAt(const til::point at) const;
    TextBufferTextIterator GetTextLineDataAt(const til::point at) const;
    TextBufferTextIterator GetTextDataAt(const til::point at, const Microsoft::Console::Types::Viewport limit) const;

    // Text insertion functions
    OutputCellIterator Write(const OutputCellIterator givenIt);

    OutputCellIterator Write(const OutputCellIterator givenIt,
                             const til::point target,
                             const std::optional<bool> wrap = true);

    OutputCellIterator WriteLine(const OutputCellIterator givenIt,
                                 const til::point target,
                                 const std::optional<bool> setWrap = std::nullopt,
                                 const std::optional<til::CoordType> limitRight = std::nullopt);

    bool InsertCharacter(const wchar_t wch, const DbcsAttribute dbcsAttribute, const TextAttribute attr);
    bool InsertCharacter(const std::wstring_view chars, const DbcsAttribute dbcsAttribute, const TextAttribute attr);
    bool IncrementCursor();
    bool NewlineCursor();

    // Scroll needs access to this to quickly rotate around the buffer.
    bool IncrementCircularBuffer(const bool inVtMode = false);

    til::point GetLastNonSpaceCharacter(std::optional<const Microsoft::Console::Types::Viewport> viewOptional = std::nullopt) const;

    Cursor& GetCursor() noexcept;
    const Cursor& GetCursor() const noexcept;

    const til::CoordType GetFirstRowIndex() const noexcept;

    const Microsoft::Console::Types::Viewport GetSize() const noexcept;

    void ScrollRows(const til::CoordType firstRow, const til::CoordType size, const til::CoordType delta);

    til::CoordType TotalRowCount() const noexcept;

    [[nodiscard]] TextAttribute GetCurrentAttributes() const noexcept;

    void SetCurrentAttributes(const TextAttribute& currentAttributes) noexcept;

    void SetCurrentLineRendition(const LineRendition lineRendition);
    void ResetLineRenditionRange(const til::CoordType startRow, const til::CoordType endRow) noexcept;
    LineRendition GetLineRendition(const til::CoordType row) const noexcept;
    bool IsDoubleWidthLine(const til::CoordType row) const noexcept;

    til::CoordType GetLineWidth(const til::CoordType row) const noexcept;
    til::point ClampPositionWithinLine(const til::point position) const noexcept;
    til::point ScreenToBufferPosition(const til::point position) const noexcept;
    til::point BufferToScreenPosition(const til::point position) const noexcept;

    void Reset();

    [[nodiscard]] HRESULT ResizeTraditional(const til::size newSize) noexcept;

    const UnicodeStorage& GetUnicodeStorage() const noexcept;
    UnicodeStorage& GetUnicodeStorage() noexcept;

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
    const std::vector<std::tuple<til::point, til::point>> GetTextSpans(til::point start, til::point end, bool blockSelection, bool bufferCoordinates) const;

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

    const std::wstring GetPlainText(const bool trimTrailingWhitespace,
                                    const til::point& start,
                                    const til::point& end) const;

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

private:
    void _UpdateSize();
    Microsoft::Console::Types::Viewport _size;
    std::vector<ROW> _storage;
    Cursor _cursor;

    til::CoordType _firstRow; // indexes top row (not necessarily 0)

    TextAttribute _currentAttributes;

    // storage location for glyphs that can't fit into the buffer normally
    UnicodeStorage _unicodeStorage;

    bool _isActiveBuffer;
    Microsoft::Console::Render::Renderer& _renderer;

    std::unordered_map<uint16_t, std::wstring> _hyperlinkMap;
    std::unordered_map<std::wstring, uint16_t> _hyperlinkCustomIdMap;
    uint16_t _currentHyperlinkId;

    void _RefreshRowIDs(std::optional<til::CoordType> newRowWidth);

    void _SetFirstRowIndex(const til::CoordType FirstRowIndex) noexcept;

    til::point _GetPreviousFromCursor() const noexcept;

    void _SetWrapOnCurrentRow() noexcept;
    void _AdjustWrapOnCurrentRow(const bool fSet) noexcept;

    // Assist with maintaining proper buffer state for Double Byte character sequences
    bool _PrepareForDoubleByteSequence(const DbcsAttribute dbcsAttribute);
    bool _AssertValidDoubleByteSequence(const DbcsAttribute dbcsAttribute);

    ROW& _GetFirstRow() noexcept;
    ROW& _GetPrevRowNoWrap(const ROW& row);

    void _ExpandTextRow(til::inclusive_rect& selectionRow) const;

    DelimiterClass _GetDelimiterClassAt(const til::point pos, const std::wstring_view wordDelimiters) const;
    til::point _GetWordStartForAccessibility(const til::point target, const std::wstring_view wordDelimiters) const;
    til::point _GetWordStartForSelection(const til::point target, const std::wstring_view wordDelimiters) const;
    til::point _GetWordEndForAccessibility(const til::point target, const std::wstring_view wordDelimiters, const til::point limit) const;
    til::point _GetWordEndForSelection(const til::point target, const std::wstring_view wordDelimiters) const;

    void _PruneHyperlinks();

    static void _AppendRTFText(std::ostringstream& contentBuilder, const std::wstring_view& text);

    std::unordered_map<size_t, std::wstring> _idsAndPatterns;
    size_t _currentPatternId;

#ifdef UNIT_TESTING
    friend class TextBufferTests;
    friend class UiaTextRangeTests;
#endif
};
