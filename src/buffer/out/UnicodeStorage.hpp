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

#include <unordered_map>
#include <vector>

#include <til/bit.h>
#include <til/hash.h>

// std::unordered_map needs help to know how to hash a til::point
namespace std
{
    template<>
    struct hash<til::point>
    {
        // Routine Description:
        // - hashes a coord. coord will be hashed by storing the x and y values consecutively in the lower
        // bits of a size_t.
        // Arguments:
        // - coord - the coord to hash
        // Return Value:
        // - the hashed coord
        constexpr size_t operator()(const til::point coord) const noexcept
        {
            return til::hash(til::bit_cast<uint64_t>(coord));
        }
    };
}

class UnicodeStorage final
{
public:
    using key_type = typename til::point;
    using mapped_type = typename std::vector<wchar_t>;

    UnicodeStorage() noexcept;

    const mapped_type& GetText(const key_type key) const;

    void StoreGlyph(const key_type key, const mapped_type& glyph);

    void Erase(const key_type key) noexcept;

    void Remap(const std::unordered_map<til::CoordType, til::CoordType>& rowMap, const std::optional<til::CoordType> width);

private:
    std::unordered_map<key_type, mapped_type> _map;

#ifdef UNIT_TESTING
    friend class UnicodeStorageTests;
    friend class TextBufferTests;
#endif
};
