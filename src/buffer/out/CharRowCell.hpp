/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- CharRowCell.hpp

Abstract:
- data structure for one cell of a char row. contains the char data for one
  coordinate position in the output buffer (leading/trailing information and
  the char itself.

Author(s):
- Austin Diviness (AustDi) 02-May-2018
--*/

#pragma once

#include "DbcsAttribute.hpp"

#if (defined(_M_IX86) || defined(_M_AMD64))
// currently CharRowCell's fields use 3 bytes of memory, leaving the 4th byte in unused. this leads
// to a rather large amount of useless memory allocated. so instead, pack CharRowCell by bytes instead of words.
#pragma pack(push, 1)
#endif

class CharRowCell final
{
public:
    CharRowCell() noexcept;
    CharRowCell(const wchar_t wch, const DbcsAttribute attr) noexcept;

    void EraseChars() noexcept;
    void Reset() noexcept;

    bool IsSpace() const noexcept;

    DbcsAttribute& DbcsAttr() noexcept;
    const DbcsAttribute& DbcsAttr() const noexcept;

    wchar_t& Char() noexcept;
    const wchar_t& Char() const noexcept;

    friend constexpr bool operator==(const CharRowCell& a, const CharRowCell& b) noexcept;

private:
    wchar_t _wch;
    DbcsAttribute _attr;
};

#if (defined(_M_IX86) || defined(_M_AMD64))
#pragma pack(pop)
#endif

constexpr bool operator==(const CharRowCell& a, const CharRowCell& b) noexcept
{
    return (a._wch == b._wch &&
            a._attr == b._attr);
}
