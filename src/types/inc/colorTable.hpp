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
}
