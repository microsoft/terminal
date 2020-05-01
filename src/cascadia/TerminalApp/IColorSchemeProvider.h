/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- IIoProvider.hpp

Abstract:
- TODO

Author(s):
- Mike Griese (migrie) 1 May 2020
--*/
#pragma once

#include "ColorScheme.h"

namespace TerminalApp
{
    class IColorSchemeProvider
    {
    public:
        virtual const ColorScheme* LookupColorScheme(const std::wstring& schemeName) const = 0;
    };
}
