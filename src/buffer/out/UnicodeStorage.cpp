// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "UnicodeStorage.hpp"

UnicodeStorage::UnicodeStorage() noexcept :
    _map{}
{
}

// Routine Description:
// - fetches the text associated with key
// Arguments:
// - key - the key into the storage
// Return Value:
// - the glyph data associated with key
// Note: will throw exception if key is not stored yet
const UnicodeStorage::mapped_type& UnicodeStorage::GetText(const key_type key) const
{
    return _map.at(key);
}

// Routine Description:
// - stores glyph data associated with key.
// Arguments:
// - key - the key into the storage
// - glyph - the glyph data to store
void UnicodeStorage::StoreGlyph(const key_type key, const mapped_type& glyph)
{
    _map.insert_or_assign(key, glyph);
}

// Routine Description:
// - erases key and its associated data from the storage
// Arguments:
// - key - the key to remove
void UnicodeStorage::Erase(const key_type key)
{
    _map.erase(key);
}

// Routine Description:
// - Remaps all of the stored items to new coordinate positions
//   based on a bulk rearrangement of row IDs and potential row width resize.
// Arguments:
// - rowMap - A map of the old row IDs to the new row IDs.
// - width - The width of the new row. Remove any items that are beyond the row width.
//         - Use nullopt if we're not resizing the width of the row, just renumbering the rows.
void UnicodeStorage::Remap(const std::map<SHORT, SHORT>& rowMap, const std::optional<SHORT> width)
{
    // Make a temporary map to hold all the new row positioning
    std::unordered_map<key_type, mapped_type> newMap;

    // Walk through every stored item.
    for (const auto& pair : _map)
    {
        // Extract the old coordinate position
        const auto oldCoord = pair.first;

        // Only try to short-circuit based on width if we were told it changed
        // by being given a new width value.
        if (width.has_value())
        {
            // Get the column ID
            const auto oldColId = oldCoord.X;

            // If the column index is at/beyond the row width, don't bother copying it to the new map.
            if (oldColId >= width.value())
            {
                continue;
            }
        }

        // Get the row ID from the position as that's what we need to remap
        const auto oldRowId = oldCoord.Y;

        // Use the mapping given to convert the old row ID to the new row ID
        const auto mapIter = rowMap.find(oldRowId);

        // If there's no mapping to a new row, don't bother copying it to the new map. The row is gone.
        if (mapIter == rowMap.end())
        {
            continue;
        }

        const auto newRowId = mapIter->second;

        // Generate a new coordinate with the same X as the old one, but a new Y value.
        const auto newCoord = COORD{ oldCoord.X, newRowId };

        // Put the adjusted coordinate into the map with the original value.
        newMap.emplace(newCoord, pair.second);
    }

    // Swap into the stored map, free the temporary when we exit.
    _map.swap(newMap);
}
