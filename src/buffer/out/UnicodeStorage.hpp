/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- UnicodeStorage.hpp

Abstract:
- dynamic storage location for glyphs that can't normally fit in the output buffer

Author(s):
- Austin Diviness (AustDi) 02-May-2018
--*/

#pragma once

#include <vector>
#include <unordered_map>
#include <climits>

// std::unordered_map needs help to know how to hash a COORD
namespace std
{
    template<>
    struct hash<COORD>
    {
        // Routine Description:
        // - hashes a coord. coord will be hashed by storing the x and y values consecutively in the lower
        // bits of a size_t.
        // Arguments:
        // - coord - the coord to hash
        // Return Value:
        // - the hashed coord
        constexpr size_t operator()(const COORD& coord) const noexcept
        {
            size_t retVal = coord.Y;
            const size_t xCoord = coord.X;
            retVal |= xCoord << (sizeof(coord.Y) * CHAR_BIT);
            return retVal;
        }
    };
}

class UnicodeStorage final
{
public:
    using key_type = typename COORD;
    using mapped_type = typename std::vector<wchar_t>;

    UnicodeStorage() noexcept;

    const mapped_type& GetText(const key_type key) const;

    void StoreGlyph(const key_type key, const mapped_type& glyph);

    void Erase(const key_type key);

    void Remap(const std::map<SHORT, SHORT>& rowMap, const std::optional<SHORT> width);

private:
    std::unordered_map<key_type, mapped_type> _map;

#ifdef UNIT_TESTING
    friend class UnicodeStorageTests;
    friend class TextBufferTests;
#endif
};
