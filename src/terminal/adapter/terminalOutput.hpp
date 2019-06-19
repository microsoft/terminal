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
        TerminalOutput();
        ~TerminalOutput();

        wchar_t TranslateKey(const wchar_t wch) const;
        bool DesignateCharset(const wchar_t wchNewCharset);
        bool NeedToTranslate() const;

    private:
        wchar_t _wchCurrentCharset = DispatchTypes::VTCharacterSets::USASCII;

        // The tables only ever change the values x20 - x7f (96 display characters)
        static const unsigned int s_uiNumDisplayCharacters = 96;
        static const wchar_t s_rgDECSpecialGraphicsTranslations[s_uiNumDisplayCharacters];

        const wchar_t* _GetTranslationTable() const;
    };
}
