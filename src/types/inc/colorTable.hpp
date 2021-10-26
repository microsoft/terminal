/*++
Copyright (c) Microsoft Corporation

Module Name:
- colorTable.hpp

Abstract:
- Helper for color tables
--*/

#pragma once

namespace Microsoft::Console::Utils
{
    void InitializeColorTable(const gsl::span<COLORREF> table);
    gsl::span<const til::color> CampbellColorTable();

    std::optional<til::color> ColorFromXOrgAppColorName(const std::wstring_view wstr) noexcept;

    // Function Description:
    // - Fill the alpha byte of the colors in a given color table with the given value.
    // Arguments:
    // - table: a color table
    // - newAlpha: the new value to use as the alpha for all the entries in that table.
    // Return Value:
    // - <none>
    constexpr void SetColorTableAlpha(const gsl::span<COLORREF> table, const BYTE newAlpha) noexcept
    {
        const auto shiftedAlpha = newAlpha << 24;
        for (auto& color : table)
        {
            WI_UpdateFlagsInMask(color, 0xff000000, shiftedAlpha);
        }
    }
}
