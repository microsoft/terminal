/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- OutputCell.hpp

Abstract:
- Representation of all data stored in a cell of the output buffer.
- RGB color supported.

Author:
- Austin Diviness (AustDi) 20-Mar-2018

--*/

#pragma once

#include "DbcsAttribute.hpp"
#include "TextAttribute.hpp"
#include "OutputCellView.hpp"

#include <exception>
#include <variant>

class InvalidCharInfoConversionException : public std::exception
{
    const char* what() const noexcept override
    {
        return "Cannot convert to CHAR_INFO without explicit TextAttribute";
    }
};

class OutputCell final
{
public:
    OutputCell() noexcept;

    OutputCell(const std::wstring_view charData,
               const DbcsAttribute dbcsAttribute,
               const TextAttributeBehavior behavior);

    OutputCell(const std::wstring_view charData,
               const DbcsAttribute dbcsAttribute,
               const TextAttribute textAttribute);

    OutputCell(const CHAR_INFO& charInfo);
    OutputCell(const OutputCellView& view);

    OutputCell(const OutputCell&) = default;
    OutputCell(OutputCell&&) = default;

    OutputCell& operator=(const OutputCell&) = default;
    OutputCell& operator=(OutputCell&&) = default;

    ~OutputCell() = default;

    const std::wstring_view Chars() const noexcept;
    void SetChars(const std::wstring_view chars);

    DbcsAttribute& DbcsAttr() noexcept;
    TextAttribute& TextAttr();

    constexpr const DbcsAttribute& DbcsAttr() const
    {
        return _dbcsAttribute;
    }

    const TextAttribute& TextAttr() const
    {
        THROW_HR_IF(E_INVALIDARG, _behavior == TextAttributeBehavior::Current);
        return _textAttribute;
    }

    constexpr TextAttributeBehavior TextAttrBehavior() const
    {
        return _behavior;
    }

private:
    // basic_string contains a small storage internally so we don't need
    // to worry about heap allocation for short strings.
    std::wstring _text;
    DbcsAttribute _dbcsAttribute;
    TextAttribute _textAttribute;
    TextAttributeBehavior _behavior;

    void _setFromBehavior(const TextAttributeBehavior behavior);
    void _setFromCharInfo(const CHAR_INFO& charInfo);
    void _setFromStringView(const std::wstring_view view);
    void _setFromOutputCellView(const OutputCellView& cell);
};
