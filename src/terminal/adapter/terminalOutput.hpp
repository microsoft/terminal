/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- terminalOutput.hpp

Abstract:
- Provides a set of functions for translating certain characters into other
    characters. There are special VT modes where the display characters (values
    x20 - x7f) should be displayed as other characters. This module provides an
    componentization of that logic.

Author(s):
- Mike Griese (migrie) 03-Mar-2016
--*/
#pragma once

#include "termDispatch.hpp"

namespace Microsoft::Console::VirtualTerminal
{
    class TerminalOutput sealed
    {
    public:
        TerminalOutput() noexcept;

        wchar_t TranslateKey(const wchar_t wch) const noexcept;
        bool Designate94Charset(const size_t gsetNumber, const VTID charset);
        bool Designate96Charset(const size_t gsetNumber, const VTID charset);
        void SetDrcs94Designation(const VTID charset);
        void SetDrcs96Designation(const VTID charset);
        VTID GetCharsetId(const size_t gsetNumber) const;
        size_t GetCharsetSize(const size_t gsetNumber) const;
        bool LockingShift(const size_t gsetNumber);
        bool LockingShiftRight(const size_t gsetNumber);
        bool SingleShift(const size_t gsetNumber) noexcept;
        size_t GetLeftSetNumber() const noexcept;
        size_t GetRightSetNumber() const noexcept;
        bool IsSingleShiftPending(const size_t gsetNumber) const noexcept;
        bool NeedToTranslate() const noexcept;
        void EnableGrTranslation(boolean enabled);

    private:
        const std::wstring_view _LookupTranslationTable94(const VTID charset) const;
        const std::wstring_view _LookupTranslationTable96(const VTID charset) const;
        bool _SetTranslationTable(const size_t gsetNumber, const std::wstring_view translationTable);
        void _ReplaceDrcsTable(const std::wstring_view oldTable, const std::wstring_view newTable);

        std::array<std::wstring_view, 4> _gsetTranslationTables;
        std::array<VTID, 4> _gsetIds;
        size_t _glSetNumber = 0;
        size_t _grSetNumber = 2;
        std::wstring_view _glTranslationTable;
        std::wstring_view _grTranslationTable;
        mutable size_t _ssSetNumber = 0;
        boolean _grTranslationEnabled = false;
        VTID _drcsId = 0;
        std::wstring_view _drcsTranslationTable;
    };
}
