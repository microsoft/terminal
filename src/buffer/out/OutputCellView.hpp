/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- OutputCellView.hpp

Abstract:
- Read view into a single cell of data that someone is attempting to write into the output buffer.
- This is done for performance reasons (avoid heap allocs and copies).

Author:
- Michael Niksa (MiNiksa) 06-Oct-2018

Revision History:
- Based on work from OutputCell.hpp/cpp by Austin Diviness (AustDi)

--*/

#pragma once

#include "DbcsAttribute.hpp"
#include "TextAttribute.hpp"

class OutputCellView
{
public:
    OutputCellView(const std::wstring_view view,
                   const DbcsAttribute dbcsAttr,
                   const TextAttribute textAttr,
                   const TextAttributeBehavior behavior) noexcept;

    const std::wstring_view& Chars() const noexcept;
    til::CoordType Columns() const noexcept;
    DbcsAttribute DbcsAttr() const noexcept;
    TextAttribute TextAttr() const noexcept;
    TextAttributeBehavior TextAttrBehavior() const noexcept;

    void UpdateText(const std::wstring_view& view) noexcept
    {
        _view = view;
    };

    void UpdateDbcsAttribute(const DbcsAttribute& dbcsAttr) noexcept
    {
        _dbcsAttr = dbcsAttr;
    }

    void UpdateTextAttribute(const TextAttribute& textAttr) noexcept
    {
        _textAttr = textAttr;
    }

    bool operator==(const OutputCellView& view) const noexcept;
    bool operator!=(const OutputCellView& view) const noexcept;

private:
    std::wstring_view _view;
    DbcsAttribute _dbcsAttr;
    TextAttribute _textAttr;
    TextAttributeBehavior _behavior;
};
