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

#include "cursor.h"
#include "Row.hpp"
#include "TextAttribute.hpp"
#include "UnicodeStorage.hpp"
#include "../types/inc/Viewport.hpp"

#include "../buffer/out/textBufferCellIterator.hpp"
#include "../buffer/out/textBufferTextIterator.hpp"

#include "../renderer/inc/IRenderTarget.hpp"

class TextBuffer final
{
public:
    TextBuffer(const COORD screenBufferSize,
               const TextAttribute defaultAttributes,
               const UINT cursorSize,
               Microsoft::Console::Render::IRenderTarget& renderTarget);
    TextBuffer(const TextBuffer& a) = delete;

    // Used for duplicating properties to another text buffer
    void CopyProperties(const TextBuffer& OtherBuffer) noexcept;

    // row manipulation
    const ROW& GetRowByOffset(const size_t index) const;
    ROW& GetRowByOffset(const size_t index);

    TextBufferCellIterator GetCellDataAt(const COORD at) const;
    TextBufferCellIterator GetCellLineDataAt(const COORD at) const;
    TextBufferCellIterator GetCellDataAt(const COORD at, const Microsoft::Console::Types::Viewport limit) const;
    TextBufferTextIterator GetTextDataAt(const COORD at) const;
    TextBufferTextIterator GetTextLineDataAt(const COORD at) const;
    TextBufferTextIterator GetTextDataAt(const COORD at, const Microsoft::Console::Types::Viewport limit) const;

    // Text insertion functions
    OutputCellIterator Write(const OutputCellIterator givenIt);

    OutputCellIterator Write(const OutputCellIterator givenIt,
                             const COORD target,
                             const std::optional<bool> wrap = true);

    OutputCellIterator WriteLine(const OutputCellIterator givenIt,
                                 const COORD target,
                                 const std::optional<bool> setWrap = std::nullopt,
                                 const std::optional<size_t> limitRight = std::nullopt);

    bool InsertCharacter(const wchar_t wch, const DbcsAttribute dbcsAttribute, const TextAttribute attr);
    bool InsertCharacter(const std::wstring_view chars, const DbcsAttribute dbcsAttribute, const TextAttribute attr);
    bool IncrementCursor();
    bool NewlineCursor();

    // Scroll needs access to this to quickly rotate around the buffer.
    bool IncrementCircularBuffer();

    COORD GetLastNonSpaceCharacter() const;
    COORD GetLastNonSpaceCharacter(const Microsoft::Console::Types::Viewport viewport) const;

    Cursor& GetCursor() noexcept;
    const Cursor& GetCursor() const noexcept;

    const SHORT GetFirstRowIndex() const noexcept;

    const Microsoft::Console::Types::Viewport GetSize() const;

    void ScrollRows(const SHORT firstRow, const SHORT size, const SHORT delta);

    UINT TotalRowCount() const noexcept;

    [[nodiscard]] TextAttribute GetCurrentAttributes() const noexcept;

    void SetCurrentAttributes(const TextAttribute currentAttributes) noexcept;

    void Reset();

    [[nodiscard]] HRESULT ResizeTraditional(const COORD newSize);

    const UnicodeStorage& GetUnicodeStorage() const noexcept;
    UnicodeStorage& GetUnicodeStorage() noexcept;

    Microsoft::Console::Render::IRenderTarget& GetRenderTarget() noexcept;

    class TextAndColor
    {
    public:
        std::vector<std::wstring> text;
        std::vector<std::vector<COLORREF>> FgAttr;
        std::vector<std::vector<COLORREF>> BkAttr;
    };

    const TextAndColor GetTextForClipboard(const bool lineSelection,
                                           const bool trimTrailingWhitespace,
                                           const std::vector<SMALL_RECT>& selectionRects,
                                           std::function<COLORREF(TextAttribute&)> GetForegroundColor,
                                           std::function<COLORREF(TextAttribute&)> GetBackgroundColor) const;

    static std::string GenHTML(const TextAndColor& rows,
                               const int fontHeightPoints,
                               const std::wstring_view fontFaceName,
                               const COLORREF backgroundColor,
                               const std::string& htmlTitle);

    static std::string GenRTF(const TextAndColor& rows,
                              const int fontHeightPoints,
                              const std::wstring_view fontFaceName,
                              const COLORREF backgroundColor);

private:
    std::deque<ROW> _storage;
    Cursor _cursor;

    SHORT _firstRow; // indexes top row (not necessarily 0)

    TextAttribute _currentAttributes;

    // storage location for glyphs that can't fit into the buffer normally
    UnicodeStorage _unicodeStorage;

    void _RefreshRowIDs(std::optional<SHORT> newRowWidth);

    Microsoft::Console::Render::IRenderTarget& _renderTarget;

    void _SetFirstRowIndex(const SHORT FirstRowIndex) noexcept;

    COORD _GetPreviousFromCursor() const;

    void _SetWrapOnCurrentRow();
    void _AdjustWrapOnCurrentRow(const bool fSet);

    void _NotifyPaint(const Microsoft::Console::Types::Viewport& viewport) const;

    // Assist with maintaining proper buffer state for Double Byte character sequences
    bool _PrepareForDoubleByteSequence(const DbcsAttribute dbcsAttribute);
    bool _AssertValidDoubleByteSequence(const DbcsAttribute dbcsAttribute);

    ROW& _GetFirstRow();
    ROW& _GetPrevRowNoWrap(const ROW& row);

#ifdef UNIT_TESTING
    friend class TextBufferTests;
    friend class UiaTextRangeTests;
#endif
};
