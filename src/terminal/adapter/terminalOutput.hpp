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
        TerminalOutput() = default;

        wchar_t TranslateKey(const wchar_t wch) const noexcept;
        bool DesignateCharset(const wchar_t wchNewCharset) noexcept;
        bool NeedToTranslate() const noexcept;

    private:
        wchar_t _currentCharset = DispatchTypes::VTCharacterSets::USASCII;

        const std::wstring_view _GetTranslationTable() const noexcept;
    };
}
