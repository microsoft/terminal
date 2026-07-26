// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "OutputCellView.hpp"

// BODGY: Misdiagnosis in MSVC 17.11: Referencing global constants in the member
// initializer list leads to this warning. Can probably be removed in the future.
#pragma warning(disable : 26493) // Don't use C-style casts (type.4).)

// Routine Description:
// - Constructs a read-only view of data formatted as a single output buffer cell
// Arguments:
// - view - String data for the text displayed on screen
// - dbcsAttr - Describes column width information (double byte character data)
// - textAttr - Describes color and formatting data
// - behavior - Describes where to retrieve color/format data. From this view? From defaults? etc.
OutputCellView::OutputCellView(const std::wstring_view view,
                               const DbcsAttribute dbcsAttr,
                               const TextAttribute textAttr,
                               const TextAttributeBehavior behavior) noexcept :
    _view(view),
    _dbcsAttr(dbcsAttr),
    _textAttr(textAttr),
    _behavior(behavior)
{
}

// Routine Description:
// - Returns reference to view over text data
// Return Value:
// - Reference to UTF-16 character data
// C26445 - suppressed to enable the `TextBufferTextIterator::operator->` method which needs a non-temporary memory location holding the wstring_view.
// TODO: GH 2681 - remove this suppression by reconciling the probably bad design of the iterators that leads to this being required.
GSL_SUPPRESS(26445)
const std::wstring_view& OutputCellView::Chars() const noexcept
{
    return _view;
}

// Routine Description:
// - Reports how many columns we expect the Chars() text data to consume
// Return Value:
// - Count of column cells on the screen (0 for combining marks, 1 for normal, 2 for wide)
til::CoordType OutputCellView::Columns() const noexcept
{
    if (DbcsAttr() == DbcsAttribute::Leading)
        return 2;

    // Combining marks (Unicode General Category Mn, Mc, Me) should not advance the
    // column. They combine with the preceding base character and have zero visual width.
    // This fixes rendering of Lao, Thai, Devanagari, Arabic, Hebrew, and other complex
    // scripts where vowels and tone marks were incorrectly occupying their own cells.
    if (_view.size() == 1)
    {
        const auto ch = _view[0];
        if ((ch >= 0x0300 && ch <= 0x036F) ||   // Combining Diacritical Marks
            (ch >= 0x0591 && ch <= 0x05C7) ||   // Hebrew
            (ch >= 0x0610 && ch <= 0x065F) ||   // Arabic
            (ch == 0x0670) ||
            (ch >= 0x06D6 && ch <= 0x06ED) ||   // Arabic marks
            (ch >= 0x0900 && ch <= 0x0903) ||   // Devanagari
            (ch >= 0x093A && ch <= 0x094F) ||   // Devanagari
            (ch >= 0x0951 && ch <= 0x0957) ||
            (ch >= 0x0962 && ch <= 0x0963) ||
            (ch == 0x0981) || (ch == 0x0982) || (ch == 0x0983) || // Bengali
            (ch >= 0x09BE && ch <= 0x09CD) ||
            (ch >= 0x0A01 && ch <= 0x0A03) ||   // Gurmukhi
            (ch >= 0x0A3C && ch <= 0x0A4D) ||
            (ch >= 0x0A81 && ch <= 0x0A83) ||   // Gujarati
            (ch >= 0x0ABC && ch <= 0x0ACD) ||
            (ch >= 0x0B01 && ch <= 0x0B03) ||   // Oriya
            (ch >= 0x0B3E && ch <= 0x0B4D) ||
            (ch >= 0x0B82 && ch <= 0x0B83) ||   // Tamil
            (ch >= 0x0BBE && ch <= 0x0BCD) ||
            (ch >= 0x0C00 && ch <= 0x0C04) ||   // Telugu
            (ch >= 0x0C3E && ch <= 0x0C56) ||
            (ch >= 0x0C81 && ch <= 0x0C83) ||   // Kannada
            (ch >= 0x0CBC && ch <= 0x0CCD) ||
            (ch >= 0x0D01 && ch <= 0x0D03) ||   // Malayalam
            (ch >= 0x0D3E && ch <= 0x0D4D) ||
            (ch == 0x0E31) ||                   // Thai Mai Han Akat
            (ch >= 0x0E34 && ch <= 0x0E3A) ||   // Thai vowels
            (ch >= 0x0E47 && ch <= 0x0E4E) ||   // Thai tones
            (ch == 0x0EB1) ||                   // Lao Mai Kan
            (ch >= 0x0EB4 && ch <= 0x0EB9) ||   // Lao vowels I/U/Y
            (ch == 0x0EBB) ||                   // Lao Mai Kon
            (ch >= 0x0EC8 && ch <= 0x0ECD) ||   // Lao tones
            (ch >= 0x102B && ch <= 0x103E) ||   // Myanmar
            (ch >= 0x1056 && ch <= 0x1059) ||
            (ch >= 0x17B4 && ch <= 0x17D3) ||   // Khmer
            (ch >= 0x200B && ch <= 0x200F) ||   // ZW spaces/marks
            (ch >= 0x2028 && ch <= 0x202E) ||
            (ch >= 0x2060 && ch <= 0x2069) ||
            (ch >= 0x20D0 && ch <= 0x20F0) ||   // Combining marks for symbols
            (ch >= 0xFE00 && ch <= 0xFE0F) ||   // Variation Selectors
            (ch >= 0xFE20 && ch <= 0xFE2F) ||   // Combining Half Marks
            (ch >= 0xFF9E && ch <= 0xFF9F))     // Halfwidth marks
        {
            return 0;
        }
    }

    return 1;
}

// Routine Description:
// - Retrieves character cell width data
// Return Value:
// - DbcsAttribute data
DbcsAttribute OutputCellView::DbcsAttr() const noexcept
{
    return _dbcsAttr;
}

// Routine Description:
// - Retrieves text color/formatting information
// Return Value:
// - TextAttribute with encoded formatting data
TextAttribute OutputCellView::TextAttr() const noexcept
{
    return _textAttr;
}

// Routine Description:
// - Retrieves behavior for inserting this cell into the buffer. See enum for details.
// Return Value:
// - TextAttributeBehavior enum value
TextAttributeBehavior OutputCellView::TextAttrBehavior() const noexcept
{
    return _behavior;
}

// Routine Description:
// - Compares two views
// Arguments:
// - it - Other view to compare to this one
// Return Value:
// - True if all contents/references are equal. False otherwise.
bool OutputCellView::operator==(const OutputCellView& it) const noexcept
{
    return _view == it._view &&
           _dbcsAttr == it._dbcsAttr &&
           _textAttr == it._textAttr &&
           _behavior == it._behavior;
}

// Routine Description:
// - Compares two views for inequality
// Arguments:
// - it - Other view to compare tot his one.
// Return Value:
// - True if any contents or references are inequal. False if they're all equal.
bool OutputCellView::operator!=(const OutputCellView& it) const noexcept
{
    return !(*this == it);
}
