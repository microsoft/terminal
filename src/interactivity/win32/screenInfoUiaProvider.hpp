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

#include "precomp.h"
#include "../types/IScreenInfoUiaProvider.h"

// Forward declare, prevent circular ref.
class SCREEN_INFORMATION;

namespace Microsoft::Console::Interactivity::Win32
{
    class Window;

    class ScreenInfoUiaProvider final :
        public Microsoft::Console::Types::IScreenInfoUiaProvider
    {
    protected:
        IConsoleWindow* const _getIConsoleWindow() override;
        const COORD _getScreenBufferCoords() const override;
        const TextBuffer& _getTextBuffer() const override;
        const Viewport _getViewport() const override;
        void _LockConsole() noexcept override;
        void _UnlockConsole() noexcept override;

    private:
        static SCREEN_INFORMATION& _getScreenInfo();
    };
}
