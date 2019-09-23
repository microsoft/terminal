// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "OutputCellView.hpp"

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
[[gsl::suppress(26445)]] const std::wstring_view& OutputCellView::Chars() const noexcept
{
    return _view;
}

// Routine Description:
// - Reports how many columns we expect the Chars() text data to consume
// Return Value:
// - Count of column cells on the screen
size_t OutputCellView::Columns() const noexcept
{
    if (DbcsAttr().IsSingle())
    {
        return 1;
    }
    else if (DbcsAttr().IsLeading())
    {
        return 2;
    }
    else if (DbcsAttr().IsTrailing())
    {
        return 1;
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
