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
        bool DesignateCharset(const size_t gsetNumber, const wchar_t charset);
        bool LockingShift(const size_t gsetNumber);
        bool SingleShift(const size_t gsetNumber);
        bool NeedToTranslate() const noexcept;

    private:
        bool _SetTranslationTable(const size_t gsetNumber, const std::wstring_view translationTable);

        std::array<std::wstring_view, 4> _gsetTranslationTables;
        size_t _glSetNumber = 0;
        std::wstring_view _glTranslationTable;
        mutable std::wstring_view _ssTranslationTable;
    };
}
