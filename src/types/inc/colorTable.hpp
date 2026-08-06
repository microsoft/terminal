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
    void InitializeColorTable(const std::span<COLORREF> table) noexcept;
    void InitializeANSIColorTable(const std::span<COLORREF> table) noexcept;
    void InitializeVT340ColorTable(const std::span<COLORREF> table) noexcept;
    void InitializeExtendedColorTable(const std::span<COLORREF> table, const bool monochrome = false) noexcept;
    std::span<const til::color> CampbellColorTable() noexcept;

    std::optional<til::color> ColorFromXOrgAppColorName(const std::wstring_view wstr) noexcept;
}
