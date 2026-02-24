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
<<<<<<< Updated upstream
    std::span<const til::color> CampbellColorTable() noexcept;
||||||| Stash base
    void InitializeExtendedColorTableDynamic(std::span<COLORREF> table, COLORREF defaultBackground, COLORREF defaultForeground) noexcept;
    std::span<const til::color> CampbellColorTable() noexcept;
=======
    void InitializeExtendedColorTableDynamic(std::span<COLORREF> table, COLORREF defaultBackground, COLORREF defaultForeground) noexcept;
    std::span<const til::color> DefaultColorTable() noexcept;
>>>>>>> Stashed changes

    std::optional<til::color> ColorFromXOrgAppColorName(const std::wstring_view wstr) noexcept;
}
