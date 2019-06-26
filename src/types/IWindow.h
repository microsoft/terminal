/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- IWindow.hpp

Abstract:
- Defines the methods and properties of a window that can be used for
  accessibility in WindowUiaProvider.
- Based on src\interactivity\inc\IConsoleWindow.hpp

Author(s):
- Carlos Zamora (CaZamor) July 2019
--*/

#pragma once

#include "precomp.h"

namespace Microsoft::Console::Types
{
    class IWindow
    {
    public:
        virtual HWND GetWindowHandle() const = 0;
        virtual RECT GetWindowRect() const = 0;
    };
}
