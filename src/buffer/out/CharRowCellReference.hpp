/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- CharRowCellReference.hpp

Abstract:
- reference class for the glyph data of a char row cell

Author(s):
- Austin Diviness (AustDi) 02-May-2018
--*/

#pragma once

#include "DbcsAttribute.hpp"
#include "CharRowCell.hpp"
#include <utility>

class CharRow;

class CharRowCellReference final
{
public:
    using const_iterator = const wchar_t*;

    CharRowCellReference(CharRow& parent, const size_t index) noexcept :
        _parent{ parent },
        _index{ index }
    {
    }

    ~CharRowCellReference() = default;
    CharRowCellReference(const CharRowCellReference&) = default;
    CharRowCellReference(CharRowCellReference&&) = default;

    void operator=(const CharRowCellReference&) = delete;
    void operator=(CharRowCellReference&&) = delete;

    void operator=(const std::wstring_view chars);
    operator std::wstring_view() const;

    const_iterator begin() const;
    const_iterator end() const;

    friend bool operator==(const CharRowCellReference& ref, const std::vector<wchar_t>& glyph);
    friend bool operator==(const std::vector<wchar_t>& glyph, const CharRowCellReference& ref);

private:
    // what char row the object belongs to
    CharRow& _parent;
    // the index of the cell in the parent char row
    const size_t _index;

    CharRowCell& _cellData();
    const CharRowCell& _cellData() const;

    std::wstring_view _glyphData() const;
};

bool operator==(const CharRowCellReference& ref, const std::vector<wchar_t>& glyph);
bool operator==(const std::vector<wchar_t>& glyph, const CharRowCellReference& ref);
