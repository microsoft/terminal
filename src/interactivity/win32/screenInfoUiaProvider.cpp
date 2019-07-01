// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "screenInfoUiaProvider.hpp"
#include "../../host/screenInfo.hpp"
#include "../inc/ServiceLocator.hpp"

#include "../types/IConsoleWindow.hpp"
#include "../types/WindowUiaProvider.h"
#include "../types/inc/viewport.hpp"

using namespace Microsoft::Console::Types;
using namespace Microsoft::Console::Interactivity;
using namespace Microsoft::Console::Interactivity::Win32;

SCREEN_INFORMATION& Microsoft::Console::Interactivity::Win32::ScreenInfoUiaProvider::_getScreenInfo()
{
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    THROW_HR_IF(E_POINTER, !gci.HasActiveOutputBuffer());
    return gci.GetActiveOutputBuffer();
}

Microsoft::Console::Types::IConsoleWindow* const Microsoft::Console::Interactivity::Win32::ScreenInfoUiaProvider::_getIConsoleWindow()
{
    return ServiceLocator::LocateConsoleWindow();
}

const COORD Microsoft::Console::Interactivity::Win32::ScreenInfoUiaProvider::_getScreenBufferCoords() const
{
    const CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    return gci.GetScreenBufferSize();
}

const TextBuffer& Microsoft::Console::Interactivity::Win32::ScreenInfoUiaProvider::_getTextBuffer() const
{
    return _getScreenInfo().GetTextBuffer();
}

const Viewport Microsoft::Console::Interactivity::Win32::ScreenInfoUiaProvider::_getViewport() const
{
    return _getScreenInfo().GetViewport();
}

void Microsoft::Console::Interactivity::Win32::ScreenInfoUiaProvider::_LockConsole() noexcept
{
    ServiceLocator::LocateGlobals().getConsoleInformation().LockConsole();
}

void Microsoft::Console::Interactivity::Win32::ScreenInfoUiaProvider::_UnlockConsole() noexcept
{
    ServiceLocator::LocateGlobals().getConsoleInformation().UnlockConsole();
}
