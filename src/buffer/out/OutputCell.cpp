// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "OutputCell.hpp"

#include "../../types/inc/GlyphWidth.hpp"
#include "../../types/inc/convert.hpp"
#include "../../inc/conattrs.hpp"

// BODGY: Misdiagnosis in MSVC 17.11: Referencing global constants in the member
// initializer list leads to this warning. Can probably be removed in the future.
#pragma warning(disable : 26493) // Don't use C-style casts (type.4).)

static constexpr TextAttribute InvalidTextAttribute{ INVALID_COLOR, INVALID_COLOR };

OutputCell::OutputCell() noexcept :
    _text{},
    _dbcsAttribute{},
    _textAttribute{ InvalidTextAttribute },
    _behavior{ TextAttributeBehavior::Stored }
{
}

OutputCell::OutputCell(const std::wstring_view charData,
                       const DbcsAttribute dbcsAttribute,
                       const TextAttributeBehavior behavior) :
    _text{ UNICODE_INVALID },
    _dbcsAttribute{ dbcsAttribute },
    _textAttribute{ InvalidTextAttribute },
    _behavior{ behavior }
{
    THROW_HR_IF(E_INVALIDARG, charData.empty());
    _setFromStringView(charData);
    _setFromBehavior(behavior);
}

OutputCell::OutputCell(const std::wstring_view charData,
                       const DbcsAttribute dbcsAttribute,
                       const TextAttribute textAttribute) :
    _text{ UNICODE_INVALID },
    _dbcsAttribute{ dbcsAttribute },
    _textAttribute{ textAttribute },
    _behavior{ TextAttributeBehavior::Stored }
{
    THROW_HR_IF(E_INVALIDARG, charData.empty());
    _setFromStringView(charData);
}

OutputCell::OutputCell(const OutputCellView& cell)
{
    _setFromOutputCellView(cell);
}

const std::wstring_view OutputCell::Chars() const noexcept
{
    return _text;
}

void OutputCell::SetChars(const std::wstring_view chars)
{
    _setFromStringView(chars);
}

DbcsAttribute& OutputCell::DbcsAttr() noexcept
{
    return _dbcsAttribute;
}

TextAttribute& OutputCell::TextAttr()
{
    THROW_HR_IF(E_INVALIDARG, _behavior == TextAttributeBehavior::Current);
    return _textAttribute;
}

void OutputCell::_setFromBehavior(const TextAttributeBehavior behavior)
{
    THROW_HR_IF(E_INVALIDARG, behavior == TextAttributeBehavior::Stored);
}

void OutputCell::_setFromStringView(const std::wstring_view view)
{
    _text = view;
}

void OutputCell::_setFromOutputCellView(const OutputCellView& cell)
{
    _dbcsAttribute = cell.DbcsAttr();
    _textAttribute = cell.TextAttr();
    _behavior = cell.TextAttrBehavior();
    _text = cell.Chars();
}
