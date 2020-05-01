/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- IColorSchemeProvider.h

Abstract:
- This provides an abstraction to enable us to expose the ability to lookup a
  colorscheme by name, without having to expose the full implementation of
  whoever is implementing this.
- This was added in GH#5690 to allow GlobalAppSettings to provide a way to do a
  case-insensitive lookup of color schemes by name.

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
