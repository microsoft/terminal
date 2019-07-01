/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- screenInfoUiaProvider.hpp

Abstract:
- This module provides UI Automation access to the screen buffer to
  support both automation tests and accessibility (screen reading)
  applications.
- Implements the virtual functions requested in Types::IScreenInfoUiaProvider
- Based on examples, sample code, and guidance from
  https://msdn.microsoft.com/en-us/library/windows/desktop/ee671596(v=vs.85).aspx

Author(s):
- Carlos Zamora (CaZamor)   2019
--*/

#pragma once

#include "../types/inc/viewport.hpp"
#include "../types/IConsoleWindow.hpp"
#include "../types/IScreenInfoUiaProvider.h"

class ScreenInfoUiaProvider final :
    public Microsoft::Console::Types::IScreenInfoUiaProvider
{
protected:
    const COORD _getScreenBufferCoords() const override;
    const TextBuffer& _getTextBuffer() const override;
    const Microsoft::Console::Types::Viewport _getViewport() const override;
    void _LockConsole() noexcept override;
    void _UnlockConsole() noexcept override;
};
