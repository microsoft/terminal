// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "TerminalDispatch.hpp"
using namespace ::Microsoft::Terminal::Core;
using namespace ::Microsoft::Console::VirtualTerminal;

// NOTE:
// Functions related to Set Graphics Renditions (SGR) are in
//      TerminalDispatchGraphics.cpp, not this file

TerminalDispatch::TerminalDispatch(ITerminalApi& terminalApi) :
    _terminalApi{ terminalApi }
{
}

void TerminalDispatch::Execute(const wchar_t wchControl)
{
    _terminalApi.ExecuteChar(wchControl);
}

void TerminalDispatch::Print(const wchar_t wchPrintable)
{
    _terminalApi.PrintString({ &wchPrintable, 1 });
}

void TerminalDispatch::PrintString(const wchar_t* const rgwch, const size_t cch)
{
    _terminalApi.PrintString({ rgwch, cch });
}

bool TerminalDispatch::CursorPosition(const unsigned int uiLine,
                                      const unsigned int uiColumn)
{
    const auto columnInBufferSpace = uiColumn - 1;
    const auto lineInBufferSpace = uiLine - 1;
    short x = static_cast<short>(uiColumn - 1);
    short y = static_cast<short>(uiLine - 1);
    return _terminalApi.SetCursorPosition(x, y);
}

bool TerminalDispatch::CursorForward(const unsigned int uiDistance)
{
    const auto cursorPos = _terminalApi.GetCursorPosition();
    const COORD newCursorPos{ cursorPos.X + gsl::narrow<short>(uiDistance), cursorPos.Y };
    return _terminalApi.SetCursorPosition(newCursorPos.X, newCursorPos.Y);
}

bool TerminalDispatch::EraseCharacters(const unsigned int uiNumChars)
{
    return _terminalApi.EraseCharacters(uiNumChars);
}

bool TerminalDispatch::SetWindowTitle(std::wstring_view title)
{
    return _terminalApi.SetWindowTitle(title);
}

// Method Description:
// - Sets a single entry of the colortable to a new value
// Arguments:
// - tableIndex: The VT color table index
// - dwColor: The new RGB color value to use.
// Return Value:
// True if handled successfully. False otherwise.
bool TerminalDispatch::SetColorTableEntry(const size_t tableIndex,
                                          const DWORD dwColor)
{
    return _terminalApi.SetColorTableEntry(tableIndex, dwColor);
}

bool TerminalDispatch::SetCursorStyle(const DispatchTypes::CursorStyle cursorStyle)
{
    return _terminalApi.SetCursorStyle(cursorStyle);
}

// Method Description:
// - Sets the default foreground color to a new value
// Arguments:
// - dwColor: The new RGB color value to use, in 0x00BBGGRR form
// Return Value:
// True if handled successfully. False otherwise.
bool TerminalDispatch::SetDefaultForeground(const DWORD dwColor)
{
    return _terminalApi.SetDefaultForeground(dwColor);
}

// Method Description:
// - Sets the default background color to a new value
// Arguments:
// - dwColor: The new RGB color value to use, in 0x00BBGGRR form
// Return Value:
// True if handled successfully. False otherwise.
bool TerminalDispatch::SetDefaultBackground(const DWORD dwColor)
{
    return _terminalApi.SetDefaultBackground(dwColor);
}
