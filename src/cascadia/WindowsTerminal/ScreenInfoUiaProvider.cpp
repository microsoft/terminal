#include "pch.h"
#include "ScreenInfoUiaProvider.h"

// TODO: be able to extract all of this data from WindowsTerminal's Terminal

const COORD ScreenInfoUiaProvider::_getScreenBufferCoords() const
{
    const CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    return gci.GetScreenBufferSize();
}

const TextBuffer& ScreenInfoUiaProvider::_getTextBuffer() const
{
    return _getScreenInfo().GetTextBuffer();
}

const Microsoft::Console::Types::Viewport ScreenInfoUiaProvider::_getViewport() const
{
    return _getScreenInfo().GetViewport();
}

void ScreenInfoUiaProvider::_LockConsole() noexcept
{
    ServiceLocator::LocateGlobals().getConsoleInformation().LockConsole();
}

void ScreenInfoUiaProvider::_UnlockConsole() noexcept
{
    ServiceLocator::LocateGlobals().getConsoleInformation().UnlockConsole();
}
