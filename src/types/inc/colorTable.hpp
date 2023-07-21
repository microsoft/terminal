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
    void InitializeColorTable(const std::span<COLORREF> table);
    std::span<const til::color> CampbellColorTable() noexcept;

    std::optional<til::color> ColorFromXOrgAppColorName(const std::wstring_view wstr) noexcept;
}
